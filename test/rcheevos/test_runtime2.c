#include "rc_runtime2.h"

#include "rc_internal.h"
#include "rc_runtime2_internal.h"
#include "rc_version.h"

#include "mock_memory.h"

#include "../test_framework.h"

static rc_runtime2_t* g_runtime;

#define GENERIC_ACHIEVEMENT_JSON(id, memaddr) "{\"ID\":" id ",\"Title\":\"Achievement " id "\"," \
      "\"Description\":\"Desc " id "\",\"Flags\":3,\"Points\":5,\"MemAddr\":\"" memaddr "\"," \
      "\"Author\":\"User1\",\"BadgeName\":\"00" id "\",\"Created\":1367266583,\"Modified\":1376929305}"

static const char* patchdata_2ach_1lbd = "{\"Success\":true,\"PatchData\":{"
    "\"ID\":1234,\"Title\":\"Sample Game\",\"ConsoleID\":17,\"ImageIcon\":\"/Images/112233.png\","
    "\"Achievements\":["
     "{\"ID\":5501,\"Title\":\"Ach1\",\"Description\":\"Desc1\",\"Flags\":3,\"Points\":5,"
      "\"MemAddr\":\"0xH0001=1_0xH0002=7\",\"Author\":\"User1\",\"BadgeName\":\"00234\","
      "\"Created\":1367266583,\"Modified\":1376929305},"
     "{\"ID\":5502,\"Title\":\"Ach2\",\"Description\":\"Desc2\",\"Flags\":3,\"Points\":2,"
      "\"MemAddr\":\"0xH0001=2_0x0002=9\",\"Author\":\"User1\",\"BadgeName\":\"00235\","
      "\"Created\":1376970283,\"Modified\":1376970283}"
    "],"
    "\"Leaderboards\":["
     "{\"ID\":4401,\"Title\":\"Leaderboard1\",\"Description\":\"Desc1\","
      "\"Mem\":\"STA:0xH1234=1::CAN:0xH5555=5::SUB:0xH1234=2::VAL:0xH2345\",\"Format\":\"SCORE\"}"
    "]"
    "}}";

static const char* patchdata_bounds_check_system = "{\"Success\":true,\"PatchData\":{"
    "\"ID\":1234,\"Title\":\"Sample Game\",\"ConsoleID\":7,\"ImageIcon\":\"/Images/112233.png\","
    "\"Achievements\":["
      GENERIC_ACHIEVEMENT_JSON("1", "0xH0000=5") ","
      GENERIC_ACHIEVEMENT_JSON("2", "0xHFFFF=5") ","
      GENERIC_ACHIEVEMENT_JSON("3", "0xH10000=5") ","
      GENERIC_ACHIEVEMENT_JSON("4", "0x FFFE=5") ","
      GENERIC_ACHIEVEMENT_JSON("5", "0x FFFF=5") ","
      GENERIC_ACHIEVEMENT_JSON("6", "0x 10000=5")
    "],"
    "\"Leaderboards\":[]"
    "}}";

static const char* patchdata_bounds_check_8 = "{\"Success\":true,\"PatchData\":{"
    "\"ID\":1234,\"Title\":\"Sample Game\",\"ConsoleID\":7,\"ImageIcon\":\"/Images/112233.png\","
    "\"Achievements\":["
      GENERIC_ACHIEVEMENT_JSON("408", "0xH0004=5") ","
      GENERIC_ACHIEVEMENT_JSON("508", "0xH0005=5") ","
      GENERIC_ACHIEVEMENT_JSON("608", "0xH0006=5") ","
      GENERIC_ACHIEVEMENT_JSON("708", "0xH0007=5") ","
      GENERIC_ACHIEVEMENT_JSON("808", "0xH0008=5") ","
      GENERIC_ACHIEVEMENT_JSON("416", "0x 0004=5") ","
      GENERIC_ACHIEVEMENT_JSON("516", "0x 0005=5") ","
      GENERIC_ACHIEVEMENT_JSON("616", "0x 0006=5") ","
      GENERIC_ACHIEVEMENT_JSON("716", "0x 0007=5") ","
      GENERIC_ACHIEVEMENT_JSON("816", "0x 0008=5") ","
      GENERIC_ACHIEVEMENT_JSON("424", "0xW0004=5") ","
      GENERIC_ACHIEVEMENT_JSON("524", "0xW0005=5") ","
      GENERIC_ACHIEVEMENT_JSON("624", "0xW0006=5") ","
      GENERIC_ACHIEVEMENT_JSON("724", "0xW0007=5") ","
      GENERIC_ACHIEVEMENT_JSON("824", "0xW0008=5") ","
      GENERIC_ACHIEVEMENT_JSON("432", "0xX0004=5") ","
      GENERIC_ACHIEVEMENT_JSON("532", "0xX0005=5") ","
      GENERIC_ACHIEVEMENT_JSON("632", "0xX0006=5") ","
      GENERIC_ACHIEVEMENT_JSON("732", "0xX0007=5") ","
      GENERIC_ACHIEVEMENT_JSON("832", "0xX0008=5")
    "],"
    "\"Leaderboards\":[]"
    "}}";

static const char* patchdata_exhaustive = "{\"Success\":true,\"PatchData\":{"
    "\"ID\":1234,\"Title\":\"Sample Game\",\"ConsoleID\":7,\"ImageIcon\":\"/Images/112233.png\","
    "\"Achievements\":["
      GENERIC_ACHIEVEMENT_JSON("5", "0xH0005=5") ","
      GENERIC_ACHIEVEMENT_JSON("6", "M:0xH0006=6") ","
      GENERIC_ACHIEVEMENT_JSON("7", "T:0xH0007=7_0xH0001=1") ","
      GENERIC_ACHIEVEMENT_JSON("8", "0xH0008=8") ","
      GENERIC_ACHIEVEMENT_JSON("9", "0xH0009=9") ","
      GENERIC_ACHIEVEMENT_JSON("70", "M:0xX0010=100000") ","
      GENERIC_ACHIEVEMENT_JSON("71", "G:0xX0010=100000")
    "],"
    "\"Leaderboards\":[]"
    "}}";

static const char* no_unlocks = "{\"Success\":true,\"UserUnlocks\":[]}";

static const char* unlock_5501 = "{\"Success\":true,\"UserUnlocks\":[5501]}";
static const char* unlock_5502 = "{\"Success\":true,\"UserUnlocks\":[5502]}";
static const char* unlock_5501_and_5502 = "{\"Success\":true,\"UserUnlocks\":[5501,5502]}";

static const char* response_429 =
    "<html>\n"
    "<head><title>429 Too Many Requests</title></head>\n"
    "<body>\n"
    "<center><h1>429 Too Many Requests</h1></center>\n"
    "<hr><center>nginx</center>\n"
    "</body>\n"
    "</html>";

/* ----- helpers ----- */

static void _assert_achievement_state(rc_runtime2_t* runtime, uint32_t id, int expected_state)
{
  const rc_runtime2_achievement_t* achievement = rc_runtime2_get_achievement_info(runtime, id);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->state, expected_state);
}
#define assert_achievement_state(runtime, id, expected_state) ASSERT_HELPER(_assert_achievement_state(runtime, id, expected_state), "assert_achievement_state")

static rc_runtime2_event_t events[16];
static int event_count = 0;

static void rc_runtime2_event_handler(const rc_runtime2_event_t* e)
{
  memcpy(&events[event_count++], e, sizeof(rc_runtime2_event_t));
}

static rc_runtime2_event_t* find_event(uint8_t type, uint32_t id)
{
  int i;

  for (i = 0; i < event_count; ++i) {
    if (events[i].type == type) {
      switch (type) {
        case RC_RUNTIME2_EVENT_ACHIEVEMENT_TRIGGERED:
        case RC_RUNTIME2_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW:
        case RC_RUNTIME2_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE:
        case RC_RUNTIME2_EVENT_ACHIEVEMENT_PROGRESS_UPDATED:
          if (events[i].achievement->id == id)
            return &events[i];
          break;

        case RC_RUNTIME2_EVENT_LBOARD_STARTED:
        case RC_RUNTIME2_EVENT_LBOARD_FAILED:
        case RC_RUNTIME2_EVENT_LBOARD_SUBMITTED:
          if (events[i].leaderboard->id == id)
            return &events[i];
          break;

        case RC_RUNTIME2_EVENT_LBOARD_TRACKER_SHOW:
        case RC_RUNTIME2_EVENT_LBOARD_TRACKER_HIDE:
        case RC_RUNTIME2_EVENT_LBOARD_TRACKER_UPDATE:
          if (events[i].leaderboard_tracker->id == id)
            return &events[i];
          break;

        case RC_RUNTIME2_EVENT_RESET:
        case RC_RUNTIME2_EVENT_GAME_COMPLETED:
          return &events[i];

        default:
          break;
      }
    }
  }

  return NULL;
}

static void _assert_event(uint8_t type, uint32_t id)
{
  if (find_event(type, id) != NULL)
    return;

  ASSERT_FAIL("expected event not found");
}
#define assert_event(type, id, value) ASSERT_HELPER(_assert_event(type, id, value), "assert_event")

static uint8_t* g_memory = NULL;
static uint32_t g_memory_size = 0;

static void mock_memory(uint8_t* memory, uint32_t size)
{
  g_memory = memory;
  g_memory_size = size;
}

static uint32_t rc_runtime2_read_memory(uint32_t address, uint8_t* buffer, uint32_t num_bytes, rc_runtime2_t* runtime)
{
  if (g_memory_size > 0) {
    if (address >= g_memory_size)
      return 0;

    uint32_t num_avail = g_memory_size - address;
    if (num_avail < num_bytes)
      num_bytes = num_avail;

    memcpy(buffer, &g_memory[address], num_bytes);
    return num_bytes;
  }

  memset(&buffer, 0, num_bytes);
  return num_bytes;
}

/* ----- API mocking ----- */

typedef struct rc_mock_api_response
{
  const char* request_params;
  const char* response_body;
  int http_status_code;
  rc_runtime2_server_callback_t async_callback;
  void* async_callback_data;
} rc_mock_api_response;

static rc_mock_api_response g_mock_api_responses[8];
static int g_num_mock_api_responses = 0;

static void rc_runtime2_server_call(const rc_api_request_t* request, rc_runtime2_server_callback_t callback, void* callback_data, rc_runtime2_t* runtime)
{
  int i;
  for (i = 0; i < g_num_mock_api_responses; i++)
  {
    if (strcmp(g_mock_api_responses[i].request_params, request->post_data) == 0)
	{
      callback(g_mock_api_responses[i].response_body, g_mock_api_responses[i].http_status_code, callback_data);
	  return;
	}
  }

  ASSERT_FAIL("No API response for: %s", request->post_data);

  // still call the callback to prevent memory leak
  callback("", 500, callback_data);
}

static void rc_runtime2_server_call_async(const rc_api_request_t* request, rc_runtime2_server_callback_t callback, void* callback_data, rc_runtime2_t* runtime)
{
  g_mock_api_responses[g_num_mock_api_responses].request_params = request->post_data;
  g_mock_api_responses[g_num_mock_api_responses].async_callback = callback;
  g_mock_api_responses[g_num_mock_api_responses].async_callback_data = callback_data;
  g_num_mock_api_responses++;
}

static void async_api_response(const char* request_params, const char* response_body)
{
  int i;
  for (i = 0; i < g_num_mock_api_responses; i++)
  {
    if (strcmp(g_mock_api_responses[i].request_params, request_params) == 0)
    {
      g_mock_api_responses[i].async_callback(response_body, 200, g_mock_api_responses[i].async_callback_data);
	    return;
    }
  }

  ASSERT_FAIL("No pending API requst for: %s", request_params);
}

static void reset_mock_api_handlers(void)
{
  g_num_mock_api_responses = 0;
  memset(g_mock_api_responses, 0, sizeof(g_mock_api_responses));
}

static void mock_api_response(const char* request_params, const char* response_body)
{
  g_mock_api_responses[g_num_mock_api_responses].request_params = request_params;
  g_mock_api_responses[g_num_mock_api_responses].response_body = response_body;
  g_mock_api_responses[g_num_mock_api_responses].http_status_code = 200;
  g_num_mock_api_responses++;
}

static void mock_api_error(const char* request_params, const char* response_body, int http_status_code)
{
  g_mock_api_responses[g_num_mock_api_responses].request_params = request_params;
  g_mock_api_responses[g_num_mock_api_responses].response_body = response_body;
  g_mock_api_responses[g_num_mock_api_responses].http_status_code = http_status_code;
  g_num_mock_api_responses++;
}

static void rc_runtime2_callback_expect_success(int result, const char* error_message, rc_runtime2_t* runtime)
{
  ASSERT_NUM_EQUALS(result, RC_OK);
  ASSERT_PTR_NULL(error_message);
  ASSERT_PTR_EQUALS(runtime, g_runtime);
}

static rc_runtime2_t* mock_runtime2_not_logged_in(void)
{
  mock_memory(NULL, 0);
  return rc_runtime2_create(rc_runtime2_read_memory, rc_runtime2_server_call);
}

static rc_runtime2_t* mock_runtime2_not_logged_in_async(void)
{
  mock_memory(NULL, 0);
  return rc_runtime2_create(rc_runtime2_read_memory, rc_runtime2_server_call_async);
}

static rc_runtime2_t* mock_runtime2_logged_in(void)
{
  rc_runtime2_t* runtime = rc_runtime2_create(rc_runtime2_read_memory, rc_runtime2_server_call);
  runtime->user.username = "Username";
  runtime->user.display_name = "DisplayName";
  runtime->user.token = "ApiToken";
  runtime->user.score = 12345;
  runtime->state.user = RC_RUNTIME2_USER_STATE_LOGGED_IN;

  rc_runtime2_set_event_handler(runtime, rc_runtime2_event_handler);

  mock_memory(NULL, 0);
  return runtime;
}

static void mock_runtime2_load_game(const char* patchdata, const char* hardcore_unlocks, const char* softcore_unlocks)
{
  reset_mock_api_handlers();
  mock_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");
  mock_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata);
  mock_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", softcore_unlocks);
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", hardcore_unlocks);

  rc_runtime2_begin_load_game(g_runtime, "0123456789ABCDEF", rc_runtime2_callback_expect_success);

  if (!g_runtime->game)
    ASSERT_MESSAGE("runtime->game is NULL");
}

static rc_runtime2_t* mock_runtime2_game_loaded(const char* patchdata, const char* hardcore_unlocks, const char* softcore_unlocks)
{
  g_runtime = mock_runtime2_logged_in();

  mock_runtime2_load_game(patchdata, hardcore_unlocks, softcore_unlocks);

  return g_runtime;
}

/* ----- login ----- */

static void test_login_with_password(void)
{
  const rc_runtime2_user_t* user;

  g_runtime = mock_runtime2_not_logged_in();
  reset_mock_api_handlers();
  mock_api_response("r=login&u=User&p=Pa%24%24word",
	  "{\"Success\":true,\"User\":\"User\",\"Token\":\"ApiToken\",\"Score\":12345,\"SoftcoreScore\":123,\"Messages\":2,\"Permissions\":1,\"AccountType\":\"Registered\"}");

  rc_runtime2_begin_login_with_password(g_runtime, "User", "Pa$$word", rc_runtime2_callback_expect_success);

  user = rc_runtime2_get_user_info(g_runtime);
  ASSERT_PTR_NOT_NULL(user);
  ASSERT_STR_EQUALS(user->username, "User");
  ASSERT_STR_EQUALS(user->display_name, "User");
  ASSERT_STR_EQUALS(user->token, "ApiToken");
  ASSERT_NUM_EQUALS(user->score, 12345);
  ASSERT_NUM_EQUALS(user->num_unread_messages, 2);

  rc_runtime2_destroy(g_runtime);
}

static void test_login_with_token(void)
{
  const rc_runtime2_user_t* user;

  g_runtime = mock_runtime2_not_logged_in();
  reset_mock_api_handlers();
  mock_api_response("r=login&u=User&t=ApiToken",
	  "{\"Success\":true,\"User\":\"User\",\"DisplayName\":\"Display\",\"Token\":\"ApiToken\",\"Score\":12345,\"Messages\":2}");

  rc_runtime2_begin_login_with_token(g_runtime, "User", "ApiToken", rc_runtime2_callback_expect_success);

  user = rc_runtime2_get_user_info(g_runtime);
  ASSERT_PTR_NOT_NULL(user);
  ASSERT_STR_EQUALS(user->username, "User");
  ASSERT_STR_EQUALS(user->display_name, "Display");
  ASSERT_STR_EQUALS(user->token, "ApiToken");
  ASSERT_NUM_EQUALS(user->score, 12345);
  ASSERT_NUM_EQUALS(user->num_unread_messages, 2);

  rc_runtime2_destroy(g_runtime);
}

static void rc_runtime2_callback_expect_username_required(int result, const char* error_message, rc_runtime2_t* runtime)
{
  ASSERT_NUM_EQUALS(result, RC_INVALID_STATE);
  ASSERT_STR_EQUALS(error_message, "username is required");
  ASSERT_PTR_EQUALS(runtime, g_runtime);
}

static void rc_runtime2_callback_expect_password_required(int result, const char* error_message, rc_runtime2_t* runtime)
{
  ASSERT_NUM_EQUALS(result, RC_INVALID_STATE);
  ASSERT_STR_EQUALS(error_message, "password is required");
  ASSERT_PTR_EQUALS(runtime, g_runtime);
}

static void rc_runtime2_callback_expect_token_required(int result, const char* error_message, rc_runtime2_t* runtime)
{
  ASSERT_NUM_EQUALS(result, RC_INVALID_STATE);
  ASSERT_STR_EQUALS(error_message, "token is required");
  ASSERT_PTR_EQUALS(runtime, g_runtime);
}

static void test_login_required_fields(void)
{
  g_runtime = mock_runtime2_not_logged_in();

  rc_runtime2_begin_login_with_password(g_runtime, "User", "", rc_runtime2_callback_expect_password_required);
  rc_runtime2_begin_login_with_password(g_runtime, "", "Pa$$word", rc_runtime2_callback_expect_username_required);
  rc_runtime2_begin_login_with_password(g_runtime, "", "", rc_runtime2_callback_expect_username_required);

  rc_runtime2_begin_login_with_token(g_runtime, "User", "", rc_runtime2_callback_expect_token_required);
  rc_runtime2_begin_login_with_token(g_runtime, "", "ApiToken", rc_runtime2_callback_expect_username_required);
  rc_runtime2_begin_login_with_token(g_runtime, "", "", rc_runtime2_callback_expect_username_required);

  ASSERT_NUM_EQUALS(g_runtime->state.user, RC_RUNTIME2_USER_STATE_NONE);

  rc_runtime2_destroy(g_runtime);
}

static void rc_runtime2_callback_expect_credentials_error(int result, const char* error_message, rc_runtime2_t* runtime)
{
  ASSERT_NUM_EQUALS(result, RC_API_FAILURE);
  ASSERT_STR_EQUALS(error_message, "Invalid User/Password combination. Please try again");
  ASSERT_PTR_EQUALS(runtime, g_runtime);
}

static void test_login_with_incorrect_password(void)
{
  g_runtime = mock_runtime2_not_logged_in();
  reset_mock_api_handlers();
  mock_api_error("r=login&u=User&p=Pa%24%24word", "{\"Success\":false,\"Error\":\"Invalid User/Password combination. Please try again\"}", 403);

  rc_runtime2_begin_login_with_password(g_runtime, "User", "Pa$$word", rc_runtime2_callback_expect_credentials_error);

  ASSERT_PTR_NULL(rc_runtime2_get_user_info(g_runtime));

  rc_runtime2_destroy(g_runtime);
}

static void rc_runtime2_callback_expect_missing_token(int result, const char* error_message, rc_runtime2_t* runtime)
{
  ASSERT_NUM_EQUALS(result, RC_MISSING_VALUE);
  ASSERT_STR_EQUALS(error_message, "Token not found in response");
  ASSERT_PTR_EQUALS(runtime, g_runtime);
}

static void test_login_incomplete_response(void)
{
  g_runtime = mock_runtime2_not_logged_in();
  reset_mock_api_handlers();
  mock_api_response("r=login&u=User&p=Pa%24%24word", "{\"Success\":true,\"User\":\"Username\"}");

  rc_runtime2_begin_login_with_password(g_runtime, "User", "Pa$$word", rc_runtime2_callback_expect_missing_token);

  ASSERT_PTR_NULL(rc_runtime2_get_user_info(g_runtime));

  rc_runtime2_destroy(g_runtime);
}

static void test_login_with_password_async(void)
{
  const rc_runtime2_user_t* user;

  g_runtime = mock_runtime2_not_logged_in_async();
  reset_mock_api_handlers();

  rc_runtime2_begin_login_with_password(g_runtime, "User", "Pa$$word", rc_runtime2_callback_expect_success);

  user = rc_runtime2_get_user_info(g_runtime);
  ASSERT_PTR_NULL(user);

  async_api_response("r=login&u=User&p=Pa%24%24word",
	    "{\"Success\":true,\"User\":\"User\",\"Token\":\"ApiToken\",\"Score\":12345,\"SoftcoreScore\":123,\"Messages\":2,\"Permissions\":1,\"AccountType\":\"Registered\"}");

  user = rc_runtime2_get_user_info(g_runtime);
  ASSERT_PTR_NOT_NULL(user);
  ASSERT_STR_EQUALS(user->username, "User");
  ASSERT_STR_EQUALS(user->display_name, "User");
  ASSERT_STR_EQUALS(user->token, "ApiToken");
  ASSERT_NUM_EQUALS(user->score, 12345);
  ASSERT_NUM_EQUALS(user->num_unread_messages, 2);

  rc_runtime2_destroy(g_runtime);
}

/* ----- load game ----- */

static void rc_runtime2_callback_expect_hash_required(int result, const char* error_message, rc_runtime2_t* runtime)
{
  ASSERT_NUM_EQUALS(result, RC_INVALID_STATE);
  ASSERT_STR_EQUALS(error_message, "hash is required");
  ASSERT_PTR_EQUALS(runtime, g_runtime);
}

static void test_load_game_required_fields(void)
{
  g_runtime = mock_runtime2_logged_in();

  rc_runtime2_begin_load_game(g_runtime, NULL, rc_runtime2_callback_expect_hash_required);
  rc_runtime2_begin_load_game(g_runtime, "", rc_runtime2_callback_expect_hash_required);

  ASSERT_PTR_NULL(g_runtime->state.load);
  ASSERT_PTR_NULL(g_runtime->game);

  rc_runtime2_destroy(g_runtime);
}

static void rc_runtime2_callback_expect_unknown_game(int result, const char* error_message, rc_runtime2_t* runtime)
{
  ASSERT_NUM_EQUALS(result, RC_NO_GAME_LOADED);
  ASSERT_STR_EQUALS(error_message, "Unknown game");
  ASSERT_PTR_EQUALS(runtime, g_runtime);
}

static void test_load_game_unknown_hash(void)
{
  g_runtime = mock_runtime2_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":0}");

  rc_runtime2_begin_load_game(g_runtime, "0123456789ABCDEF", rc_runtime2_callback_expect_unknown_game);

  ASSERT_PTR_NULL(g_runtime->state.load);
  ASSERT_PTR_NULL(g_runtime->game);

  rc_runtime2_destroy(g_runtime);
}

static void rc_runtime2_callback_expect_login_required(int result, const char* error_message, rc_runtime2_t* runtime)
{
  ASSERT_NUM_EQUALS(result, RC_LOGIN_REQUIRED);
  ASSERT_STR_EQUALS(error_message, "Login required");
  ASSERT_PTR_EQUALS(runtime, g_runtime);
}

static void test_load_game_not_logged_in(void)
{
  g_runtime = mock_runtime2_not_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");

  rc_runtime2_begin_load_game(g_runtime, "0123456789ABCDEF", rc_runtime2_callback_expect_login_required);

  ASSERT_PTR_NULL(g_runtime->state.load);
  ASSERT_PTR_NULL(g_runtime->game);

  rc_runtime2_destroy(g_runtime);
}

static void test_load_game(void)
{
  rc_runtime2_achievement_info_t* achievement;
  rc_runtime2_leaderboard_info_t* leaderboard;
  g_runtime = mock_runtime2_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");
  mock_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata_2ach_1lbd);
  mock_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", no_unlocks);
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", no_unlocks);

  rc_runtime2_begin_load_game(g_runtime, "0123456789ABCDEF", rc_runtime2_callback_expect_success);

  ASSERT_PTR_NULL(g_runtime->state.load);
  ASSERT_PTR_NOT_NULL(g_runtime->game);
  if (g_runtime->game) {
    ASSERT_PTR_EQUALS(rc_runtime2_get_game_info(g_runtime), &g_runtime->game->public);

    ASSERT_NUM_EQUALS(g_runtime->game->public.id, 1234);
    ASSERT_NUM_EQUALS(g_runtime->game->public.console_id, 17);
    ASSERT_STR_EQUALS(g_runtime->game->public.title, "Sample Game");
    ASSERT_STR_EQUALS(g_runtime->game->public.hash, "0123456789ABCDEF");
    ASSERT_STR_EQUALS(g_runtime->game->public.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_runtime->game->public.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_runtime->game->public.num_leaderboards, 1);

    achievement = &g_runtime->game->achievements[0];
    ASSERT_NUM_EQUALS(achievement->public.id, 5501);
    ASSERT_STR_EQUALS(achievement->public.title, "Ach1");
    ASSERT_STR_EQUALS(achievement->public.description, "Desc1");
    ASSERT_STR_EQUALS(achievement->public.badge_name, "00234");
    ASSERT_NUM_EQUALS(achievement->public.points, 5);
    ASSERT_NUM_EQUALS(achievement->public.unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->public.state, RC_RUNTIME2_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->public.category, RC_RUNTIME2_ACHIEVEMENT_CATEGORY_CORE);
    ASSERT_PTR_NOT_NULL(achievement->trigger);

    achievement = &g_runtime->game->achievements[1];
    ASSERT_NUM_EQUALS(achievement->public.id, 5502);
    ASSERT_STR_EQUALS(achievement->public.title, "Ach2");
    ASSERT_STR_EQUALS(achievement->public.description, "Desc2");
    ASSERT_STR_EQUALS(achievement->public.badge_name, "00235");
    ASSERT_NUM_EQUALS(achievement->public.points, 2);
    ASSERT_NUM_EQUALS(achievement->public.unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->public.state, RC_RUNTIME2_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->public.category, RC_RUNTIME2_ACHIEVEMENT_CATEGORY_CORE);
    ASSERT_PTR_NOT_NULL(achievement->trigger);

    leaderboard = &g_runtime->game->leaderboards[0];
    ASSERT_NUM_EQUALS(leaderboard->public.id, 4401);
    ASSERT_STR_EQUALS(leaderboard->public.title, "Leaderboard1");
    ASSERT_STR_EQUALS(leaderboard->public.description, "Desc1");
    ASSERT_NUM_EQUALS(leaderboard->public.format, RC_FORMAT_SCORE);
    ASSERT_NUM_EQUALS(leaderboard->public.state, RC_RUNTIME2_LEADERBOARD_STATE_ACTIVE);
    ASSERT_PTR_NOT_NULL(leaderboard->lboard);
    ASSERT_NUM_NOT_EQUALS(leaderboard->value_djb2, 0);
    ASSERT_NUM_EQUALS(leaderboard->tracker_id, RC_RUNTIME2_LEADERBOARD_TRACKER_UNASSIGNED);
  }

  rc_runtime2_destroy(g_runtime);
}

static void rc_runtime2_callback_expect_too_many_requests(int result, const char* error_message, rc_runtime2_t* runtime)
{
  ASSERT_NUM_EQUALS(result, RC_INVALID_JSON);
  ASSERT_STR_EQUALS(error_message, "429 Too Many Requests");
  ASSERT_PTR_EQUALS(runtime, g_runtime);
}

static void test_load_game_gameid_failure(void)
{
  g_runtime = mock_runtime2_logged_in();

  reset_mock_api_handlers();
  mock_api_error("r=gameid&m=0123456789ABCDEF", response_429, 429);
  mock_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata_2ach_1lbd);
  mock_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", "{\"Success\":true,\"UserUnlocks\":[]}");
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", "{\"Success\":true,\"UserUnlocks\":[]}");

  rc_runtime2_begin_load_game(g_runtime, "0123456789ABCDEF", rc_runtime2_callback_expect_too_many_requests);

  ASSERT_PTR_NULL(g_runtime->state.load);
  ASSERT_PTR_NULL(g_runtime->game);

  rc_runtime2_destroy(g_runtime);
}

static void test_load_game_patch_failure(void)
{
  g_runtime = mock_runtime2_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");
  mock_api_error("r=patch&u=Username&t=ApiToken&g=1234", response_429, 429);
  mock_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", "{\"Success\":true,\"UserUnlocks\":[]}");
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", "{\"Success\":true,\"UserUnlocks\":[]}");

  rc_runtime2_begin_load_game(g_runtime, "0123456789ABCDEF", rc_runtime2_callback_expect_too_many_requests);

  ASSERT_PTR_NULL(g_runtime->state.load);
  ASSERT_PTR_NULL(g_runtime->game);

  rc_runtime2_destroy(g_runtime);
}

static void test_load_game_postactivity_failure(void)
{
  g_runtime = mock_runtime2_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");
  mock_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata_2ach_1lbd);
  mock_api_error("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, response_429, 429);
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", "{\"Success\":true,\"UserUnlocks\":[]}");
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", "{\"Success\":true,\"UserUnlocks\":[]}");

  rc_runtime2_begin_load_game(g_runtime, "0123456789ABCDEF", rc_runtime2_callback_expect_too_many_requests);

  ASSERT_PTR_NULL(g_runtime->state.load);
  ASSERT_PTR_NULL(g_runtime->game);

  rc_runtime2_destroy(g_runtime);
}

static void test_load_game_softcore_unlocks_failure(void)
{
  g_runtime = mock_runtime2_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");
  mock_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata_2ach_1lbd);
  mock_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  mock_api_error("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", response_429, 429);
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", "{\"Success\":true,\"UserUnlocks\":[]}");

  rc_runtime2_begin_load_game(g_runtime, "0123456789ABCDEF", rc_runtime2_callback_expect_too_many_requests);

  ASSERT_PTR_NULL(g_runtime->state.load);
  ASSERT_PTR_NULL(g_runtime->game);

  rc_runtime2_destroy(g_runtime);
}

static void test_load_game_hardcore_unlocks_failure(void)
{
  g_runtime = mock_runtime2_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");
  mock_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata_2ach_1lbd);
  mock_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", "{\"Success\":true,\"UserUnlocks\":[]}");
  mock_api_error("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", response_429, 429);

  rc_runtime2_begin_load_game(g_runtime, "0123456789ABCDEF", rc_runtime2_callback_expect_too_many_requests);

  ASSERT_PTR_NULL(g_runtime->state.load);
  ASSERT_PTR_NULL(g_runtime->game);

  rc_runtime2_destroy(g_runtime);
}

static void test_achievement_list_simple(void)
{
  rc_runtime2_achievement_list_t* list;
  rc_runtime2_achievement_t** iter;
  rc_runtime2_achievement_t* achievement;

  g_runtime = mock_runtime2_game_loaded(patchdata_2ach_1lbd, no_unlocks, no_unlocks);

  ASSERT_NUM_EQUALS(rc_runtime2_get_achievement_count(g_runtime, RC_RUNTIME2_ACHIEVEMENT_CATEGORY_CORE), 2);
  ASSERT_NUM_EQUALS(rc_runtime2_get_achievement_count(g_runtime, RC_RUNTIME2_ACHIEVEMENT_CATEGORY_UNOFFICIAL), 0);
  ASSERT_NUM_EQUALS(rc_runtime2_get_achievement_count(g_runtime, RC_RUNTIME2_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL), 2);

  list = rc_runtime2_get_achievement_list(g_runtime, RC_RUNTIME2_ACHIEVEMENT_CATEGORY_CORE, RC_RUNTIME2_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].id, RC_RUNTIME2_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 2);

    iter = list->buckets[0].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5501);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5502);

    rc_runtime2_destroy_achievement_list(list);
  }

  list = rc_runtime2_get_achievement_list(g_runtime, RC_RUNTIME2_ACHIEVEMENT_CATEGORY_UNOFFICIAL, RC_RUNTIME2_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 0);
    rc_runtime2_destroy_achievement_list(list);
  }

  rc_runtime2_destroy(g_runtime);
}

static void test_achievement_list_simple_with_unlocks(void)
{
  rc_runtime2_achievement_list_t* list;
  rc_runtime2_achievement_t** iter;
  rc_runtime2_achievement_t* achievement;

  g_runtime = mock_runtime2_game_loaded(patchdata_2ach_1lbd, unlock_5501, unlock_5501_and_5502);

  list = rc_runtime2_get_achievement_list(g_runtime, RC_RUNTIME2_ACHIEVEMENT_CATEGORY_CORE, RC_RUNTIME2_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    /* in hardcore mode, 5501 should be unlocked, but 5502 will be locked */
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].id, RC_RUNTIME2_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].id, RC_RUNTIME2_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);

    iter = list->buckets[0].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5502);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_RUNTIME2_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    iter = list->buckets[1].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5501);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_RUNTIME2_ACHIEVEMENT_UNLOCKED_BOTH);

    rc_runtime2_destroy_achievement_list(list);
  }

  g_runtime->state.hardcore = 0;

  list = rc_runtime2_get_achievement_list(g_runtime, RC_RUNTIME2_ACHIEVEMENT_CATEGORY_CORE, RC_RUNTIME2_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    /* in softcore mode, both should be unlocked */
    ASSERT_NUM_EQUALS(list->num_buckets, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].id, RC_RUNTIME2_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 2);

    iter = list->buckets[0].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5501);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_RUNTIME2_ACHIEVEMENT_UNLOCKED_BOTH);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5502);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_RUNTIME2_ACHIEVEMENT_UNLOCKED_SOFTCORE);

    rc_runtime2_destroy_achievement_list(list);
  }

  rc_runtime2_destroy(g_runtime);
}

static void test_do_frame_bounds_check_system(void)
{
  g_runtime = mock_runtime2_game_loaded(patchdata_bounds_check_system, no_unlocks, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_runtime->game);
  if (g_runtime->game) {
    assert_achievement_state(g_runtime, 1, RC_RUNTIME2_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_runtime, 2, RC_RUNTIME2_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_runtime, 3, RC_RUNTIME2_ACHIEVEMENT_STATE_DISABLED); /* 0x10000 out of range for system */
    assert_achievement_state(g_runtime, 4, RC_RUNTIME2_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_runtime, 5, RC_RUNTIME2_ACHIEVEMENT_STATE_ACTIVE); /* cannot read two bytes from 0xFFFF, but size isn't enforced until do_frame */
    assert_achievement_state(g_runtime, 6, RC_RUNTIME2_ACHIEVEMENT_STATE_DISABLED); /* 0x10000 out of range for system */
  }

  rc_runtime2_destroy(g_runtime);
}

static void test_do_frame_bounds_check_available(void)
{
  uint8_t memory[8] = { 0,0,0,0,0,0,0,0 };
  g_runtime = mock_runtime2_game_loaded(patchdata_bounds_check_8, no_unlocks, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_runtime->game);
  if (g_runtime->game) {
    /* all addresses are valid according to the system, so no achievements should be disabled yet. */
    assert_achievement_state(g_runtime, 808, RC_RUNTIME2_ACHIEVEMENT_STATE_ACTIVE);

    /* limit the memory that's actually exposed and try to process a frame */
    mock_memory(memory, sizeof(memory));
    rc_runtime2_do_frame(g_runtime);

    assert_achievement_state(g_runtime, 408, RC_RUNTIME2_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_runtime, 508, RC_RUNTIME2_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_runtime, 608, RC_RUNTIME2_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_runtime, 708, RC_RUNTIME2_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_runtime, 808, RC_RUNTIME2_ACHIEVEMENT_STATE_DISABLED); /* out of bounds*/

    assert_achievement_state(g_runtime, 416, RC_RUNTIME2_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_runtime, 516, RC_RUNTIME2_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_runtime, 616, RC_RUNTIME2_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_runtime, 716, RC_RUNTIME2_ACHIEVEMENT_STATE_DISABLED); /* only one byte available */
    assert_achievement_state(g_runtime, 816, RC_RUNTIME2_ACHIEVEMENT_STATE_DISABLED); /* out of bounds*/

    assert_achievement_state(g_runtime, 424, RC_RUNTIME2_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_runtime, 524, RC_RUNTIME2_ACHIEVEMENT_STATE_DISABLED); /* 24-bit read actually fetches 32-bits */
    assert_achievement_state(g_runtime, 624, RC_RUNTIME2_ACHIEVEMENT_STATE_DISABLED); /* only two bytes available */
    assert_achievement_state(g_runtime, 724, RC_RUNTIME2_ACHIEVEMENT_STATE_DISABLED); /* only one byte available */
    assert_achievement_state(g_runtime, 824, RC_RUNTIME2_ACHIEVEMENT_STATE_DISABLED); /* out of bounds*/

    assert_achievement_state(g_runtime, 432, RC_RUNTIME2_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_runtime, 532, RC_RUNTIME2_ACHIEVEMENT_STATE_DISABLED); /* only three bytes available */
    assert_achievement_state(g_runtime, 632, RC_RUNTIME2_ACHIEVEMENT_STATE_DISABLED); /* only two bytes available */
    assert_achievement_state(g_runtime, 732, RC_RUNTIME2_ACHIEVEMENT_STATE_DISABLED); /* only one byte available */
    assert_achievement_state(g_runtime, 832, RC_RUNTIME2_ACHIEVEMENT_STATE_DISABLED); /* out of bounds*/
  }

  rc_runtime2_destroy(g_runtime);
}

static void test_do_frame_achievement_trigger(void)
{
  rc_runtime2_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_runtime = mock_runtime2_game_loaded(patchdata_exhaustive, no_unlocks, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_runtime->game);
  if (g_runtime->game) {
    const uint32_t num_active = g_runtime->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=8&h=1&m=0123456789ABCDEF&v=da80b659c2b858e13ddd97077647b217",
        "{\"Success\":true,\"Score\":5432,\"AchievementID\":8,\"AchievementsRemaining\":11}");

    event_count = 0;
    rc_runtime2_do_frame(g_runtime);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[8] = 8;
    rc_runtime2_do_frame(g_runtime);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_RUNTIME2_EVENT_ACHIEVEMENT_TRIGGERED, 8);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_RUNTIME2_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_RUNTIME2_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_RUNTIME2_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_runtime2_get_achievement_info(g_runtime, 8));

    ASSERT_NUM_EQUALS(g_runtime->game->runtime.trigger_count, num_active - 1);
    ASSERT_NUM_EQUALS(g_runtime->user.score, 5432);

    event_count = 0;
    rc_runtime2_do_frame(g_runtime);
    ASSERT_NUM_EQUALS(event_count, 0);
  }

  rc_runtime2_destroy(g_runtime);
}

static void test_do_frame_achievement_trigger_encore(void)
{
  rc_runtime2_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_runtime = mock_runtime2_game_loaded(patchdata_exhaustive, no_unlocks, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_runtime->game);
  if (g_runtime->game) {
    const uint32_t num_active = g_runtime->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=8&h=1&m=0123456789ABCDEF&v=da80b659c2b858e13ddd97077647b217",
        "{\"Success\":false,\"Error\":\"User already has hardcore and regular achievements awarded.\",\"Score\":5432,\"AchievementID\":8,\"AchievementsRemaining\":11}");

    event_count = 0;
    rc_runtime2_do_frame(g_runtime);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[8] = 8;
    rc_runtime2_do_frame(g_runtime);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_RUNTIME2_EVENT_ACHIEVEMENT_TRIGGERED, 8);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_RUNTIME2_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_RUNTIME2_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_RUNTIME2_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_runtime2_get_achievement_info(g_runtime, 8));

    ASSERT_NUM_EQUALS(g_runtime->game->runtime.trigger_count, num_active - 1);
    ASSERT_NUM_EQUALS(g_runtime->user.score, 5432);

    event_count = 0;
    rc_runtime2_do_frame(g_runtime);
    ASSERT_NUM_EQUALS(event_count, 0);
  }

  rc_runtime2_destroy(g_runtime);
}

static void test_do_frame_achievement_measured(void)
{
  rc_runtime2_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_runtime = mock_runtime2_game_loaded(patchdata_exhaustive, no_unlocks, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_runtime->game);
  if (g_runtime->game) {
    const uint32_t num_active = g_runtime->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=70&h=1&m=0123456789ABCDEF&v=61e40027573e2cde88b49d27f6804879",
        "{\"Success\":true,\"Score\":5432,\"AchievementID\":70,\"AchievementsRemaining\":11}");
    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=71&h=1&m=0123456789ABCDEF&v=3a8d55b81d391557d5111306599a2b0d",
        "{\"Success\":true,\"Score\":5432,\"AchievementID\":71,\"AchievementsRemaining\":11}");

    event_count = 0;
    rc_runtime2_do_frame(g_runtime);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[0x10] = 0x39; memory[0x11] = 0x30; /* 12345 */
    rc_runtime2_do_frame(g_runtime);
    ASSERT_NUM_EQUALS(event_count, 2);

    event = find_event(RC_RUNTIME2_EVENT_ACHIEVEMENT_PROGRESS_UPDATED, 70);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_RUNTIME2_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_RUNTIME2_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_RUNTIME2_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_runtime2_get_achievement_info(g_runtime, 70));
    ASSERT_STR_EQUALS(event->achievement->measured_progress, "12345/100000");

    event = find_event(RC_RUNTIME2_EVENT_ACHIEVEMENT_PROGRESS_UPDATED, 71);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_RUNTIME2_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_RUNTIME2_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_RUNTIME2_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_runtime2_get_achievement_info(g_runtime, 71));
    ASSERT_STR_EQUALS(event->achievement->measured_progress, "12%");

    ASSERT_NUM_EQUALS(g_runtime->game->runtime.trigger_count, num_active);

    event_count = 0;
    rc_runtime2_do_frame(g_runtime);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* increment measured value - raw counter will report progress change, percentage will not */
    memory[0x10] = 0x3A; /* 12346 */
    rc_runtime2_do_frame(g_runtime);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_RUNTIME2_EVENT_ACHIEVEMENT_PROGRESS_UPDATED, 70);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_RUNTIME2_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_RUNTIME2_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_RUNTIME2_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_runtime2_get_achievement_info(g_runtime, 70));
    ASSERT_STR_EQUALS(event->achievement->measured_progress, "12346/100000");

    event_count = 0;
    rc_runtime2_do_frame(g_runtime);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* increment measured value - raw counter will report progress change, percentage will not */
    memory[0x11] = 0x33; /* 13114 */
    rc_runtime2_do_frame(g_runtime);
    ASSERT_NUM_EQUALS(event_count, 2);

    event = find_event(RC_RUNTIME2_EVENT_ACHIEVEMENT_PROGRESS_UPDATED, 70);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_RUNTIME2_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_RUNTIME2_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_RUNTIME2_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_runtime2_get_achievement_info(g_runtime, 70));
    ASSERT_STR_EQUALS(event->achievement->measured_progress, "13114/100000");

    event = find_event(RC_RUNTIME2_EVENT_ACHIEVEMENT_PROGRESS_UPDATED, 71);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_RUNTIME2_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_RUNTIME2_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_RUNTIME2_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_runtime2_get_achievement_info(g_runtime, 71));
    ASSERT_STR_EQUALS(event->achievement->measured_progress, "13%");

    ASSERT_NUM_EQUALS(g_runtime->game->runtime.trigger_count, num_active);

    event_count = 0;
    rc_runtime2_do_frame(g_runtime);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* trigger measured achievements - should get trigger events, but not progress events */
    memory[0x10] = 0xA0; memory[0x11] = 0x86; memory[0x12] = 0x01; /* 100000 */
    rc_runtime2_do_frame(g_runtime);
    ASSERT_NUM_EQUALS(event_count, 2);

    event = find_event(RC_RUNTIME2_EVENT_ACHIEVEMENT_TRIGGERED, 70);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_RUNTIME2_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_RUNTIME2_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_RUNTIME2_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_runtime2_get_achievement_info(g_runtime, 70));
    ASSERT_STR_EQUALS(event->achievement->measured_progress, "");

    event = find_event(RC_RUNTIME2_EVENT_ACHIEVEMENT_TRIGGERED, 71);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_RUNTIME2_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_RUNTIME2_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_RUNTIME2_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_runtime2_get_achievement_info(g_runtime, 71));
    ASSERT_STR_EQUALS(event->achievement->measured_progress, "");

    ASSERT_NUM_EQUALS(g_runtime->game->runtime.trigger_count, num_active - 2);
  }

  rc_runtime2_destroy(g_runtime);
}

static void test_do_frame_achievement_challenge_indicator(void)
{
  rc_runtime2_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_runtime = mock_runtime2_game_loaded(patchdata_exhaustive, no_unlocks, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_runtime->game);
  if (g_runtime->game) {
    const uint32_t num_active = g_runtime->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=7&h=1&m=0123456789ABCDEF&v=c39308ba325ba4a72919b081fb18fdd4",
        "{\"Success\":true,\"Score\":5432,\"AchievementID\":7,\"AchievementsRemaining\":11}");

    event_count = 0;
    rc_runtime2_do_frame(g_runtime);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[1] = 1; /* show indicator */
    rc_runtime2_do_frame(g_runtime);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_RUNTIME2_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_RUNTIME2_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_RUNTIME2_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_RUNTIME2_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE);
    ASSERT_PTR_EQUALS(event->achievement, rc_runtime2_get_achievement_info(g_runtime, 7));

    ASSERT_NUM_EQUALS(g_runtime->game->runtime.trigger_count, num_active);

    event_count = 0;
    rc_runtime2_do_frame(g_runtime);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[1] = 0; /* hide indicator */
    rc_runtime2_do_frame(g_runtime);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_RUNTIME2_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE, 7);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_RUNTIME2_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_RUNTIME2_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_RUNTIME2_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_runtime2_get_achievement_info(g_runtime, 7));

    ASSERT_NUM_EQUALS(g_runtime->game->runtime.trigger_count, num_active);

    event_count = 0;
    rc_runtime2_do_frame(g_runtime);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[1] = 1; /* show indicator */
    rc_runtime2_do_frame(g_runtime);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_RUNTIME2_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_RUNTIME2_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_RUNTIME2_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_RUNTIME2_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE);
    ASSERT_PTR_EQUALS(event->achievement, rc_runtime2_get_achievement_info(g_runtime, 7));

    ASSERT_NUM_EQUALS(g_runtime->game->runtime.trigger_count, num_active);

    event_count = 0;
    rc_runtime2_do_frame(g_runtime);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* trigger achievement - expect both hide and trigger events. both should have triggered achievement data */
    memory[7] = 7;
    rc_runtime2_do_frame(g_runtime);
    ASSERT_NUM_EQUALS(event_count, 2);

    event = find_event(RC_RUNTIME2_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE, 7);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_RUNTIME2_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_RUNTIME2_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_RUNTIME2_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_runtime2_get_achievement_info(g_runtime, 7));

    event = find_event(RC_RUNTIME2_EVENT_ACHIEVEMENT_TRIGGERED, 7);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_RUNTIME2_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_RUNTIME2_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_RUNTIME2_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_runtime2_get_achievement_info(g_runtime, 7));

    ASSERT_NUM_EQUALS(g_runtime->game->runtime.trigger_count, num_active - 1);
  }

  rc_runtime2_destroy(g_runtime);
}

/* ----- settings ----- */

static void test_set_hardcore_disable(void)
{
  const rc_runtime2_achievement_t* achievement;
  const rc_runtime2_leaderboard_t* leaderboard;

  g_runtime = mock_runtime2_game_loaded(patchdata_2ach_1lbd, unlock_5501, unlock_5501_and_5502);
  ASSERT_NUM_EQUALS(rc_runtime2_get_hardcore_enabled(g_runtime), 1);

  achievement = rc_runtime2_get_achievement_info(g_runtime, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_RUNTIME2_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    ASSERT_NUM_EQUALS(achievement->state, RC_RUNTIME2_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(g_runtime->game->runtime.trigger_count, 1); /* 5502 should be active*/
  }

  leaderboard = rc_runtime2_get_leaderboard_info(g_runtime, 4401);
  ASSERT_PTR_NOT_NULL(leaderboard);
  if (leaderboard) {
    ASSERT_NUM_EQUALS(leaderboard->state, RC_RUNTIME2_LEADERBOARD_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(g_runtime->game->runtime.lboard_count, 1);
  }

  rc_runtime2_set_hardcore_enabled(g_runtime, 0);
  ASSERT_NUM_EQUALS(rc_runtime2_get_hardcore_enabled(g_runtime), 0);
  ASSERT_NUM_EQUALS(g_runtime->game->waiting_for_reset, 0);

  achievement = rc_runtime2_get_achievement_info(g_runtime, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_RUNTIME2_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    ASSERT_NUM_EQUALS(achievement->state, RC_RUNTIME2_ACHIEVEMENT_STATE_INACTIVE);
    ASSERT_NUM_EQUALS(g_runtime->game->runtime.trigger_count, 0); /* 5502 should not be active*/
  }

  leaderboard = rc_runtime2_get_leaderboard_info(g_runtime, 4401);
  ASSERT_PTR_NOT_NULL(leaderboard);
  if (leaderboard) {
    ASSERT_NUM_EQUALS(leaderboard->state, RC_RUNTIME2_LEADERBOARD_STATE_INACTIVE);
    ASSERT_NUM_EQUALS(g_runtime->game->runtime.lboard_count, 0);
  }

  rc_runtime2_destroy(g_runtime);
}

static void test_set_hardcore_enable(void)
{
  const rc_runtime2_achievement_t* achievement;
  const rc_runtime2_leaderboard_t* leaderboard;

  g_runtime = mock_runtime2_logged_in();
  rc_runtime2_set_hardcore_enabled(g_runtime, 0);
  mock_runtime2_load_game(patchdata_2ach_1lbd, unlock_5501, unlock_5501_and_5502);

  ASSERT_NUM_EQUALS(rc_runtime2_get_hardcore_enabled(g_runtime), 0);

  achievement = rc_runtime2_get_achievement_info(g_runtime, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_RUNTIME2_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    ASSERT_NUM_EQUALS(achievement->state, RC_RUNTIME2_ACHIEVEMENT_STATE_INACTIVE);
    ASSERT_NUM_EQUALS(g_runtime->game->runtime.trigger_count, 0); /* 5502 should not be active*/
  }

  leaderboard = rc_runtime2_get_leaderboard_info(g_runtime, 4401);
  ASSERT_PTR_NOT_NULL(leaderboard);
  if (leaderboard) {
    ASSERT_NUM_EQUALS(leaderboard->state, RC_RUNTIME2_LEADERBOARD_STATE_INACTIVE);
    ASSERT_NUM_EQUALS(g_runtime->game->runtime.lboard_count, 0);
  }

  /* when enabling hardcore, flag waiting_for_reset. this will prevent processing until rc_runtime2_reset is called */
  rc_runtime2_set_hardcore_enabled(g_runtime, 1);
  ASSERT_NUM_EQUALS(rc_runtime2_get_hardcore_enabled(g_runtime), 1);
  ASSERT_NUM_EQUALS(g_runtime->game->waiting_for_reset, 1);

  achievement = rc_runtime2_get_achievement_info(g_runtime, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_RUNTIME2_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    ASSERT_NUM_EQUALS(achievement->state, RC_RUNTIME2_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(g_runtime->game->runtime.trigger_count, 1); /* 5502 should be active*/
  }

  leaderboard = rc_runtime2_get_leaderboard_info(g_runtime, 4401);
  ASSERT_PTR_NOT_NULL(leaderboard);
  if (leaderboard) {
    ASSERT_NUM_EQUALS(leaderboard->state, RC_RUNTIME2_LEADERBOARD_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(g_runtime->game->runtime.lboard_count, 1);
  }

  /* resetting clears waiting_for_reset */
  rc_runtime2_reset(g_runtime);
  ASSERT_NUM_EQUALS(rc_runtime2_get_hardcore_enabled(g_runtime), 1);
  ASSERT_NUM_EQUALS(g_runtime->game->waiting_for_reset, 0);

  /* hardcore already enabled, attempting to set it again shouldn't flag waiting_for_reset */
  rc_runtime2_set_hardcore_enabled(g_runtime, 1);
  ASSERT_NUM_EQUALS(rc_runtime2_get_hardcore_enabled(g_runtime), 1);
  ASSERT_NUM_EQUALS(g_runtime->game->waiting_for_reset, 0);

  rc_runtime2_destroy(g_runtime);
}


static void test_set_hardcore_enable_no_game_loaded(void)
{
  const rc_runtime2_achievement_t* achievement;
  const rc_runtime2_leaderboard_t* leaderboard;

  g_runtime = mock_runtime2_logged_in();
  rc_runtime2_set_hardcore_enabled(g_runtime, 0);

  /* when enabling hardcore, flag waiting_for_reset. this will prevent processing until rc_runtime2_reset is called */
  rc_runtime2_set_hardcore_enabled(g_runtime, 1);
  ASSERT_NUM_EQUALS(rc_runtime2_get_hardcore_enabled(g_runtime), 1);
  ASSERT_NUM_EQUALS(g_runtime->game->waiting_for_reset, 1);
  rc_runtime2_set_hardcore_enabled(g_runtime, 0);

  mock_runtime2_load_game(patchdata_2ach_1lbd, unlock_5501, unlock_5501_and_5502);

  ASSERT_NUM_EQUALS(rc_runtime2_get_hardcore_enabled(g_runtime), 0);

  achievement = rc_runtime2_get_achievement_info(g_runtime, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_RUNTIME2_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    ASSERT_NUM_EQUALS(achievement->state, RC_RUNTIME2_ACHIEVEMENT_STATE_INACTIVE);
    ASSERT_NUM_EQUALS(g_runtime->game->runtime.trigger_count, 0); /* 5502 should not be active*/
  }

  leaderboard = rc_runtime2_get_leaderboard_info(g_runtime, 4401);
  ASSERT_PTR_NOT_NULL(leaderboard);
  if (leaderboard) {
    ASSERT_NUM_EQUALS(leaderboard->state, RC_RUNTIME2_LEADERBOARD_STATE_INACTIVE);
    ASSERT_NUM_EQUALS(g_runtime->game->runtime.lboard_count, 0);
  }

  /* when enabling hardcore, flag waiting_for_reset. this will prevent processing until rc_runtime2_reset is called */
  rc_runtime2_set_hardcore_enabled(g_runtime, 1);
  ASSERT_NUM_EQUALS(rc_runtime2_get_hardcore_enabled(g_runtime), 1);
  ASSERT_NUM_EQUALS(g_runtime->game->waiting_for_reset, 1);

  achievement = rc_runtime2_get_achievement_info(g_runtime, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_RUNTIME2_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    ASSERT_NUM_EQUALS(achievement->state, RC_RUNTIME2_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(g_runtime->game->runtime.trigger_count, 1); /* 5502 should be active*/
  }

  leaderboard = rc_runtime2_get_leaderboard_info(g_runtime, 4401);
  ASSERT_PTR_NOT_NULL(leaderboard);
  if (leaderboard) {
    ASSERT_NUM_EQUALS(leaderboard->state, RC_RUNTIME2_LEADERBOARD_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(g_runtime->game->runtime.lboard_count, 1);
  }

  /* resetting clears waiting_for_reset */
  rc_runtime2_reset(g_runtime);
  ASSERT_NUM_EQUALS(rc_runtime2_get_hardcore_enabled(g_runtime), 1);
  ASSERT_NUM_EQUALS(g_runtime->game->waiting_for_reset, 0);

  /* hardcore already enabled, attempting to set it again shouldn't flag waiting_for_reset */
  rc_runtime2_set_hardcore_enabled(g_runtime, 1);
  ASSERT_NUM_EQUALS(rc_runtime2_get_hardcore_enabled(g_runtime), 1);
  ASSERT_NUM_EQUALS(g_runtime->game->waiting_for_reset, 0);

  rc_runtime2_destroy(g_runtime);
}

/* ----- harness ----- */

void test_runtime2(void) {
  TEST_SUITE_BEGIN();

  /* login */
  TEST(test_login_with_password);
  TEST(test_login_with_token);
  TEST(test_login_required_fields);
  TEST(test_login_with_incorrect_password);
  TEST(test_login_incomplete_response);
  TEST(test_login_with_password_async);

  /* load game */
  TEST(test_load_game_required_fields);
  TEST(test_load_game_unknown_hash);
  TEST(test_load_game_not_logged_in);
  TEST(test_load_game);
  TEST(test_load_game_gameid_failure);
  TEST(test_load_game_patch_failure);
  TEST(test_load_game_postactivity_failure);
  TEST(test_load_game_softcore_unlocks_failure);
  TEST(test_load_game_hardcore_unlocks_failure);

  /* achievements */
  TEST(test_achievement_list_simple);
  TEST(test_achievement_list_simple_with_unlocks);
  // TODO: test buckets

  /* do frame */
  TEST(test_do_frame_bounds_check_system);
  TEST(test_do_frame_bounds_check_available);
  TEST(test_do_frame_achievement_trigger);
  TEST(test_do_frame_achievement_trigger_encore);
  TEST(test_do_frame_achievement_measured);
  TEST(test_do_frame_achievement_challenge_indicator);
  // TODO: test mastery

  /* settings */
  TEST(test_set_hardcore_disable);
  TEST(test_set_hardcore_enable);

  TEST_SUITE_END();
}
