#include "rc_runtime2.h"

#include "rc_internal.h"
#include "rc_runtime2_internal.h"
#include "rc_version.h"

#include "mock_memory.h"

#include "../test_framework.h"

static rc_runtime2_t* g_runtime;

static const char* patchdata_2ach_1lbd = "{\"Success\":true,\"PatchData\":{"
    "\"ID\":1234,\"Title\":\"Sample Game\",\"ConsoleID\":17,\"ImageIcon\":\"/Images/112233.png\","
    "\"Achievements\":["
     "{\"ID\":5501,\"Title\":\"Ach1\",\"Description\":\"Desc1\",\"Flags\":3,\"Points\":5,"
      "\"MemAddr\":\"0xH1234=1_0xH2345=7\",\"Author\":\"User1\",\"BadgeName\":\"00234\","
      "\"Created\":1367266583,\"Modified\":1376929305},"
     "{\"ID\":5502,\"Title\":\"Ach2\",\"Description\":\"Desc2\",\"Flags\":3,\"Points\":2,"
      "\"MemAddr\":\"0xH1234!=d0xH1234_0xH3456=9\",\"Author\":\"User1\",\"BadgeName\":\"00235\","
      "\"Created\":1376970283,\"Modified\":1376970283}"
    "],"
    "\"Leaderboards\":["
     "{\"ID\":4401,\"Title\":\"Leaderboard1\",\"Description\":\"Desc1\","
      "\"Mem\":\"STA:0xH1234=1::CAN:0xH5555=5::SUB:0xH1234=2::VAL:0xH2345\",\"Format\":\"SCORE\"}"
    "]"
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

static unsigned rc_runtime2_read_memory(unsigned address, uint8_t* buffer, unsigned num_bytes, rc_runtime2_t* runtime)
{
  return 0;
}

// --- API mocking ---

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

static void rc_runtime2_async_api_response(const char* request_params, const char* response_body)
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

static void rc_runtime2_reset_api_handlers(void)
{
  g_num_mock_api_responses = 0;
  memset(g_mock_api_responses, 0, sizeof(g_mock_api_responses));
}

static void rc_runtime2_mock_api_response(const char* request_params, const char* response_body)
{
  g_mock_api_responses[g_num_mock_api_responses].request_params = request_params;
  g_mock_api_responses[g_num_mock_api_responses].response_body = response_body;
  g_mock_api_responses[g_num_mock_api_responses].http_status_code = 200;
  g_num_mock_api_responses++;
}

static void rc_runtime2_mock_api_error(const char* request_params, const char* response_body, int http_status_code)
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
  return rc_runtime2_create(rc_runtime2_read_memory, rc_runtime2_server_call);
}

static rc_runtime2_t* mock_runtime2_not_logged_in_async(void)
{
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

  return runtime;
}

static rc_runtime2_t* mock_runtime2_game_loaded(const char* patchdata, const char* hardcore_unlocks, const char* softcore_unlocks)
{
  g_runtime = mock_runtime2_logged_in();

  rc_runtime2_reset_api_handlers();
  rc_runtime2_mock_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");
  rc_runtime2_mock_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata);
  rc_runtime2_mock_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  rc_runtime2_mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", softcore_unlocks);
  rc_runtime2_mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", hardcore_unlocks);

  rc_runtime2_begin_load_game(g_runtime, "0123456789ABCDEF", rc_runtime2_callback_expect_success);

  if (!g_runtime->game)
    ASSERT_MESSAGE("runtime->game is NULL");

  return g_runtime;
}

/* ----- login ----- */

static void test_login_with_password(void)
{
  const rc_runtime2_user_t* user;

  g_runtime = mock_runtime2_not_logged_in();
  rc_runtime2_reset_api_handlers();
  rc_runtime2_mock_api_response("r=login&u=User&p=Pa%24%24word",
	  "{\"Success\":true,\"User\":\"User\",\"Token\":\"ApiToken\",\"Score\":12345,\"SoftcoreScore\":123,\"Messages\":2,\"Permissions\":1,\"AccountType\":\"Registered\"}");

  rc_runtime2_begin_login_with_password(g_runtime, "User", "Pa$$word", rc_runtime2_callback_expect_success);

  user = rc_runtime2_user_info(g_runtime);
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
  rc_runtime2_reset_api_handlers();
  rc_runtime2_mock_api_response("r=login&u=User&t=ApiToken",
	  "{\"Success\":true,\"User\":\"User\",\"DisplayName\":\"Display\",\"Token\":\"ApiToken\",\"Score\":12345,\"Messages\":2}");

  rc_runtime2_begin_login_with_token(g_runtime, "User", "ApiToken", rc_runtime2_callback_expect_success);

  user = rc_runtime2_user_info(g_runtime);
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
  rc_runtime2_reset_api_handlers();
  rc_runtime2_mock_api_error("r=login&u=User&p=Pa%24%24word", "{\"Success\":false,\"Error\":\"Invalid User/Password combination. Please try again\"}", 403);

  rc_runtime2_begin_login_with_password(g_runtime, "User", "Pa$$word", rc_runtime2_callback_expect_credentials_error);

  ASSERT_PTR_NULL(rc_runtime2_user_info(g_runtime));

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
  rc_runtime2_reset_api_handlers();
  rc_runtime2_mock_api_response("r=login&u=User&p=Pa%24%24word", "{\"Success\":true,\"User\":\"Username\"}");

  rc_runtime2_begin_login_with_password(g_runtime, "User", "Pa$$word", rc_runtime2_callback_expect_missing_token);

  ASSERT_PTR_NULL(rc_runtime2_user_info(g_runtime));

  rc_runtime2_destroy(g_runtime);
}

static void test_login_with_password_async(void)
{
  const rc_runtime2_user_t* user;

  g_runtime = mock_runtime2_not_logged_in_async();
  rc_runtime2_reset_api_handlers();

  rc_runtime2_begin_login_with_password(g_runtime, "User", "Pa$$word", rc_runtime2_callback_expect_success);

  user = rc_runtime2_user_info(g_runtime);
  ASSERT_PTR_NULL(user);

  rc_runtime2_async_api_response("r=login&u=User&p=Pa%24%24word",
	"{\"Success\":true,\"User\":\"User\",\"Token\":\"ApiToken\",\"Score\":12345,\"SoftcoreScore\":123,\"Messages\":2,\"Permissions\":1,\"AccountType\":\"Registered\"}");

  user = rc_runtime2_user_info(g_runtime);
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

  rc_runtime2_reset_api_handlers();
  rc_runtime2_mock_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":0}");

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

  rc_runtime2_reset_api_handlers();
  rc_runtime2_mock_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");

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

  rc_runtime2_reset_api_handlers();
  rc_runtime2_mock_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");
  rc_runtime2_mock_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata_2ach_1lbd);
  rc_runtime2_mock_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  rc_runtime2_mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", no_unlocks);
  rc_runtime2_mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", no_unlocks);

  rc_runtime2_begin_load_game(g_runtime, "0123456789ABCDEF", rc_runtime2_callback_expect_success);

  ASSERT_PTR_NULL(g_runtime->state.load);
  ASSERT_PTR_NOT_NULL(g_runtime->game);
  if (g_runtime->game) {
    ASSERT_PTR_EQUALS(rc_runtime2_game_info(g_runtime), &g_runtime->game->public);

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

  rc_runtime2_reset_api_handlers();
  rc_runtime2_mock_api_error("r=gameid&m=0123456789ABCDEF", response_429, 429);
  rc_runtime2_mock_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata_2ach_1lbd);
  rc_runtime2_mock_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  rc_runtime2_mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", "{\"Success\":true,\"UserUnlocks\":[]}");
  rc_runtime2_mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", "{\"Success\":true,\"UserUnlocks\":[]}");

  rc_runtime2_begin_load_game(g_runtime, "0123456789ABCDEF", rc_runtime2_callback_expect_too_many_requests);

  ASSERT_PTR_NULL(g_runtime->state.load);
  ASSERT_PTR_NULL(g_runtime->game);

  rc_runtime2_destroy(g_runtime);
}

static void test_load_game_patch_failure(void)
{
  g_runtime = mock_runtime2_logged_in();

  rc_runtime2_reset_api_handlers();
  rc_runtime2_mock_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");
  rc_runtime2_mock_api_error("r=patch&u=Username&t=ApiToken&g=1234", response_429, 429);
  rc_runtime2_mock_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  rc_runtime2_mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", "{\"Success\":true,\"UserUnlocks\":[]}");
  rc_runtime2_mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", "{\"Success\":true,\"UserUnlocks\":[]}");

  rc_runtime2_begin_load_game(g_runtime, "0123456789ABCDEF", rc_runtime2_callback_expect_too_many_requests);

  ASSERT_PTR_NULL(g_runtime->state.load);
  ASSERT_PTR_NULL(g_runtime->game);

  rc_runtime2_destroy(g_runtime);
}

static void test_load_game_postactivity_failure(void)
{
  g_runtime = mock_runtime2_logged_in();

  rc_runtime2_reset_api_handlers();
  rc_runtime2_mock_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");
  rc_runtime2_mock_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata_2ach_1lbd);
  rc_runtime2_mock_api_error("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, response_429, 429);
  rc_runtime2_mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", "{\"Success\":true,\"UserUnlocks\":[]}");
  rc_runtime2_mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", "{\"Success\":true,\"UserUnlocks\":[]}");

  rc_runtime2_begin_load_game(g_runtime, "0123456789ABCDEF", rc_runtime2_callback_expect_too_many_requests);

  ASSERT_PTR_NULL(g_runtime->state.load);
  ASSERT_PTR_NULL(g_runtime->game);

  rc_runtime2_destroy(g_runtime);
}

static void test_load_game_softcore_unlocks_failure(void)
{
  g_runtime = mock_runtime2_logged_in();

  rc_runtime2_reset_api_handlers();
  rc_runtime2_mock_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");
  rc_runtime2_mock_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata_2ach_1lbd);
  rc_runtime2_mock_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  rc_runtime2_mock_api_error("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", response_429, 429);
  rc_runtime2_mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", "{\"Success\":true,\"UserUnlocks\":[]}");

  rc_runtime2_begin_load_game(g_runtime, "0123456789ABCDEF", rc_runtime2_callback_expect_too_many_requests);

  ASSERT_PTR_NULL(g_runtime->state.load);
  ASSERT_PTR_NULL(g_runtime->game);

  rc_runtime2_destroy(g_runtime);
}

static void test_load_game_hardcore_unlocks_failure(void)
{
  g_runtime = mock_runtime2_logged_in();

  rc_runtime2_reset_api_handlers();
  rc_runtime2_mock_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");
  rc_runtime2_mock_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata_2ach_1lbd);
  rc_runtime2_mock_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  rc_runtime2_mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", "{\"Success\":true,\"UserUnlocks\":[]}");
  rc_runtime2_mock_api_error("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", response_429, 429);

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

  TEST_SUITE_END();
}
