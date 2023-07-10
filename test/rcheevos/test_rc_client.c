#include "rc_client.h"

#include "rc_consoles.h"
#include "rc_internal.h"
#include "rc_client_internal.h"
#include "rc_version.h"

#include "../rhash/data.h"
#include "../test_framework.h"

static rc_client_t* g_client;
static void* g_callback_userdata = &g_client; /* dummy object to use for callback userdata validation */

#define GENERIC_ACHIEVEMENT_JSON(id, memaddr) "{\"ID\":" id ",\"Title\":\"Achievement " id "\"," \
      "\"Description\":\"Desc " id "\",\"Flags\":3,\"Points\":5,\"MemAddr\":\"" memaddr "\"," \
      "\"Author\":\"User1\",\"BadgeName\":\"00" id "\",\"Created\":1367266583,\"Modified\":1376929305}"

#define GENERIC_LEADERBOARD_JSON(id, memaddr, format) "{\"ID\":" id ",\"Title\":\"Leaderboard " id "\"," \
      "\"Description\":\"Desc " id "\",\"Mem\":\"" memaddr "\",\"Format\":\"" format "\"}"

static const char* patchdata_empty = "{\"Success\":true,\"PatchData\":{"
    "\"ID\":1234,\"Title\":\"Sample Game\",\"ConsoleID\":17,\"ImageIcon\":\"/Images/112233.png\","
    "\"Achievements\":[],"
    "\"Leaderboards\":[]"
    "}}";

static const char* patchdata_2ach_0lbd = "{\"Success\":true,\"PatchData\":{"
    "\"ID\":1234,\"Title\":\"Sample Game\",\"ConsoleID\":17,\"ImageIcon\":\"/Images/112233.png\","
    "\"Achievements\":["
     "{\"ID\":5501,\"Title\":\"Ach1\",\"Description\":\"Desc1\",\"Flags\":3,\"Points\":5,"
      "\"MemAddr\":\"0xH0001=3_0xH0002=7\",\"Author\":\"User1\",\"BadgeName\":\"00234\","
      "\"Created\":1367266583,\"Modified\":1376929305},"
     "{\"ID\":5502,\"Title\":\"Ach2\",\"Description\":\"Desc2\",\"Flags\":3,\"Points\":2,"
      "\"MemAddr\":\"0xH0001=2_0x0002=9\",\"Author\":\"User1\",\"BadgeName\":\"00235\","
      "\"Created\":1376970283,\"Modified\":1376970283}"
    "],"
    "\"Leaderboards\":[]"
    "}}";

static const char* patchdata_2ach_1lbd = "{\"Success\":true,\"PatchData\":{"
    "\"ID\":1234,\"Title\":\"Sample Game\",\"ConsoleID\":17,\"ImageIcon\":\"/Images/112233.png\","
    "\"Achievements\":["
     "{\"ID\":5501,\"Title\":\"Ach1\",\"Description\":\"Desc1\",\"Flags\":3,\"Points\":5,"
      "\"MemAddr\":\"0xH0001=3_0xH0002=7\",\"Author\":\"User1\",\"BadgeName\":\"00234\","
      "\"Created\":1367266583,\"Modified\":1376929305},"
     "{\"ID\":5502,\"Title\":\"Ach2\",\"Description\":\"Desc2\",\"Flags\":3,\"Points\":2,"
      "\"MemAddr\":\"0xH0001=2_0x0002=9\",\"Author\":\"User1\",\"BadgeName\":\"00235\","
      "\"Created\":1376970283,\"Modified\":1376970283}"
    "],"
    "\"Leaderboards\":["
     "{\"ID\":4401,\"Title\":\"Leaderboard1\",\"Description\":\"Desc1\","
      "\"Mem\":\"STA:0xH000C=1::CAN:0xH000D=1::SUB:0xH000D=2::VAL:0x 000E\",\"Format\":\"SCORE\"}"
    "]"
    "}}";

static const char* patchdata_rich_presence_only = "{\"Success\":true,\"PatchData\":{"
    "\"ID\":1234,\"Title\":\"Sample Game\",\"ConsoleID\":17,\"ImageIcon\":\"/Images/112233.png\","
    "\"Achievements\":[],"
    "\"Leaderboards\":[],"
    "\"RichPresencePatch\":\"Display:\\r\\n@Number(0xH0001)\""
    "}}";

static const char* patchdata_leaderboard_only = "{\"Success\":true,\"PatchData\":{"
    "\"ID\":1234,\"Title\":\"Sample Game\",\"ConsoleID\":17,\"ImageIcon\":\"/Images/112233.png\","
    "\"Achievements\":[],"
    "\"Leaderboards\":["
      GENERIC_LEADERBOARD_JSON("44", "STA:0xH000B=1::CAN:0xH000C=1::SUB:0xH000D=1::VAL:0x 000E", "SCORE")
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
      GENERIC_ACHIEVEMENT_JSON("6", "0x 10000=5") ","
      GENERIC_ACHIEVEMENT_JSON("7", "I:0xH0000_0xHFFFF=5")
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
    "\"Leaderboards\":["
      GENERIC_LEADERBOARD_JSON("44", "STA:0xH000B=1::CAN:0xH000C=1::SUB:0xH000D=1::VAL:0x 000E", "SCORE") ","
      GENERIC_LEADERBOARD_JSON("45", "STA:0xH000A=1::CAN:0xH000C=2::SUB:0xH000D=1::VAL:0xH000E", "SCORE") ","   /* different size */
      GENERIC_LEADERBOARD_JSON("46", "STA:0xH000A=1::CAN:0xH000C=3::SUB:0xH000D=1::VAL:0x 000E", "VALUE") ","   /* different format */
      GENERIC_LEADERBOARD_JSON("47", "STA:0xH000A=1::CAN:0xH000C=4::SUB:0xH000D=2::VAL:0x 000E", "SCORE") ","   /* different submit */
      GENERIC_LEADERBOARD_JSON("48", "STA:0xH000A=2::CAN:0xH000C=5::SUB:0xH000D=1::VAL:0x 000E", "SCORE") ","   /* different start */
      GENERIC_LEADERBOARD_JSON("51", "STA:0xH000A=3::CAN:0xH000C=6::SUB:0xH000D=1::VAL:M:0xH0009=1", "VALUE") "," /* hit count */
      GENERIC_LEADERBOARD_JSON("52", "STA:0xH000B=3::CAN:0xH000C=7::SUB:0xH000D=1::VAL:M:0xH0009=1", "VALUE")     /* hit count */
    "],"
    "\"RichPresencePatch\":\"Display:\\r\\nPoints:@Number(0xH0003)\\r\\n\""
    "}}";

static const char* patchdata_unofficial_unsupported = "{\"Success\":true,\"PatchData\":{"
    "\"ID\":1234,\"Title\":\"Sample Game\",\"ConsoleID\":17,\"ImageIcon\":\"/Images/112233.png\","
    "\"Achievements\":["
     "{\"ID\":5501,\"Title\":\"Ach1\",\"Description\":\"Desc1\",\"Flags\":3,\"Points\":5,"
      "\"MemAddr\":\"0xH0001=1_0xH0002=7\",\"Author\":\"User1\",\"BadgeName\":\"00234\","
      "\"Created\":1367266583,\"Modified\":1376929305},"
     "{\"ID\":5502,\"Title\":\"Ach2\",\"Description\":\"Desc2\",\"Flags\":5,\"Points\":2,"
      "\"MemAddr\":\"0xH0001=2_0x0002=9\",\"Author\":\"User1\",\"BadgeName\":\"00235\","
      "\"Created\":1376970283,\"Modified\":1376970283},"
     "{\"ID\":5503,\"Title\":\"Ach3\",\"Description\":\"Desc3\",\"Flags\":3,\"Points\":2,"
      "\"MemAddr\":\"0xHFEFEFEFE=2_0x0002=9\",\"Author\":\"User1\",\"BadgeName\":\"00236\","
      "\"Created\":1376971283,\"Modified\":1376971283}"
    "],"
    "\"Leaderboards\":["
     "{\"ID\":4401,\"Title\":\"Leaderboard1\",\"Description\":\"Desc1\","
      "\"Mem\":\"STA:0xH000C=1::CAN:0xH000D=1::SUB:0xHFEFEFEFE=2::VAL:0x 000E\",\"Format\":\"SCORE\"}"
    "]"
    "}}";

static const char* patchdata_subset = "{\"Success\":true,\"PatchData\":{"
    "\"ID\":2345,\"Title\":\"Sample Game [Subset - Bonus]\",\"ConsoleID\":17,\"ImageIcon\":\"/Images/112234.png\","
    "\"Achievements\":["
      GENERIC_ACHIEVEMENT_JSON("7", "0xH0007=7") ","
      GENERIC_ACHIEVEMENT_JSON("8", "0xH0008=8") ","
      GENERIC_ACHIEVEMENT_JSON("9", "0xH0009=9")
    "],"
    "\"Leaderboards\":["
      GENERIC_LEADERBOARD_JSON("81", "STA:0xH0008=1::CAN:0xH000C=1::SUB:0xH000D=1::VAL:0x 000E", "SCORE") ","
      GENERIC_LEADERBOARD_JSON("82", "STA:0xH0008=2::CAN:0xH000C=1::SUB:0xH000D=1::VAL:0x 000E", "SCORE")
    "]"
    "}}";

static const char* patchdata_subset2 = "{\"Success\":true,\"PatchData\":{"
    "\"ID\":2345,\"Title\":\"Sample Game [Subset - Multi]\",\"ConsoleID\":17,\"ImageIcon\":\"/Images/112234.png\","
    "\"Achievements\":["
      GENERIC_ACHIEVEMENT_JSON("5501", "0xH0017=7") ","
      GENERIC_ACHIEVEMENT_JSON("5502", "0xH0018=8") ","
      GENERIC_ACHIEVEMENT_JSON("5503", "0xH0019=9")
    "],"
    "\"Leaderboards\":["
    "]"
    "}}";

static const char* no_unlocks = "{\"Success\":true,\"UserUnlocks\":[]}";

static const char* unlock_5501 = "{\"Success\":true,\"UserUnlocks\":[5501]}";
static const char* unlock_5502 = "{\"Success\":true,\"UserUnlocks\":[5502]}";
static const char* unlock_5501_and_5502 = "{\"Success\":true,\"UserUnlocks\":[5501,5502]}";
static const char* unlock_8 = "{\"Success\":true,\"UserUnlocks\":[8]}";
static const char* unlock_6_8_and_9 = "{\"Success\":true,\"UserUnlocks\":[6,8,9]}";

static const char* response_429 =
    "<html>\n"
    "<head><title>429 Too Many Requests</title></head>\n"
    "<body>\n"
    "<center><h1>429 Too Many Requests</h1></center>\n"
    "<hr><center>nginx</center>\n"
    "</body>\n"
    "</html>";

/* ----- helpers ----- */

static void _assert_achievement_state(rc_client_t* client, uint32_t id, int expected_state)
{
  const rc_client_achievement_t* achievement = rc_client_get_achievement_info(client, id);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->state, expected_state);
}
#define assert_achievement_state(client, id, expected_state) ASSERT_HELPER(_assert_achievement_state(client, id, expected_state), "assert_achievement_state")

static rc_client_event_t events[16];
static int event_count = 0;

static void rc_client_event_handler(const rc_client_event_t* e, rc_client_t* client)
{
  memcpy(&events[event_count++], e, sizeof(rc_client_event_t));

  if (e->type == RC_CLIENT_EVENT_SERVER_ERROR) {
    static char event_server_error_message[128];
    static rc_client_server_error_t event_server_error;

    /* server error data is not maintained out of scope, copy it too */
    memcpy(&event_server_error, e->server_error, sizeof(event_server_error));
    strcpy_s(event_server_error_message, sizeof(event_server_error_message), e->server_error->error_message);
    event_server_error.error_message = event_server_error_message;
    events[event_count - 1].server_error = &event_server_error;
  }
}

static rc_client_event_t* find_event(uint8_t type, uint32_t id)
{
  int i;

  for (i = 0; i < event_count; ++i) {
    if (events[i].type == type) {
      switch (type) {
        case RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED:
        case RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW:
        case RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE:
        case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW:
          if (events[i].achievement->id == id)
            return &events[i];
          break;

        case RC_CLIENT_EVENT_LEADERBOARD_STARTED:
        case RC_CLIENT_EVENT_LEADERBOARD_FAILED:
        case RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED:
          if (events[i].leaderboard->id == id)
            return &events[i];
          break;

        case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW:
        case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE:
        case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE:
          if (events[i].leaderboard_tracker->id == id)
            return &events[i];
          break;

        case RC_CLIENT_EVENT_GAME_COMPLETED:
        case RC_CLIENT_EVENT_RESET:
        case RC_CLIENT_EVENT_SERVER_ERROR:
          return &events[i];

        default:
          break;
      }
    }
  }

  return NULL;
}

static uint8_t* g_memory = NULL;
static uint32_t g_memory_size = 0;

static void mock_memory(uint8_t* memory, uint32_t size)
{
  g_memory = memory;
  g_memory_size = size;
}

static uint32_t rc_client_read_memory(uint32_t address, uint8_t* buffer, uint32_t num_bytes, rc_client_t* client)
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
  rc_api_server_response_t server_response;
  int seen;
  rc_client_server_callback_t async_callback;
  void* async_callback_data;
} rc_mock_api_response;

static rc_mock_api_response g_mock_api_responses[12];
static int g_num_mock_api_responses = 0;

static void rc_client_server_call(const rc_api_request_t* request, rc_client_server_callback_t callback, void* callback_data, rc_client_t* client)
{
  rc_api_server_response_t server_response;

  int i;
  for (i = 0; i < g_num_mock_api_responses; i++) {
    if (strcmp(g_mock_api_responses[i].request_params, request->post_data) == 0) {
      g_mock_api_responses[i].seen++;
      callback(&g_mock_api_responses[i].server_response, callback_data);
      return;
    }
  }

  ASSERT_FAIL("No API response for: %s", request->post_data);

  /* still call the callback to prevent memory leak */
  memset(&server_response, 0, sizeof(server_response));
  server_response.body = "";
  server_response.http_status_code = 500;
  callback(&server_response, callback_data);
}

static void rc_client_server_call_async(const rc_api_request_t* request, rc_client_server_callback_t callback, void* callback_data, rc_client_t* client)
{
  g_mock_api_responses[g_num_mock_api_responses].request_params = strdup(request->post_data);
  g_mock_api_responses[g_num_mock_api_responses].async_callback = callback;
  g_mock_api_responses[g_num_mock_api_responses].async_callback_data = callback_data;
  g_mock_api_responses[g_num_mock_api_responses].seen = -1;
  g_num_mock_api_responses++;
}

static void _async_api_response(const char* request_params, const char* response_body, int http_status_code)
{
  int i;
  for (i = 0; i < g_num_mock_api_responses; i++)
  {
    if (g_mock_api_responses[i].request_params && strcmp(g_mock_api_responses[i].request_params, request_params) == 0)
    {
      g_mock_api_responses[i].seen++;
      g_mock_api_responses[i].server_response.body = response_body;
      g_mock_api_responses[i].server_response.body_length = strlen(response_body);
      g_mock_api_responses[i].server_response.http_status_code = http_status_code;
      g_mock_api_responses[i].async_callback(&g_mock_api_responses[i].server_response, g_mock_api_responses[i].async_callback_data);
      free((void*)g_mock_api_responses[i].request_params);
      g_mock_api_responses[i].request_params = NULL;

      while (g_num_mock_api_responses > 0 && g_mock_api_responses[g_num_mock_api_responses - 1].request_params == NULL)
        --g_num_mock_api_responses;
	    return;
    }
  }

  ASSERT_FAIL("No pending API request for: %s", request_params);
}

static void async_api_response(const char* request_params, const char* response_body)
{
  _async_api_response(request_params, response_body, 200);
}

static void async_api_error(const char* request_params, const char* response_body, int http_status_code)
{
  _async_api_response(request_params, response_body, http_status_code);
}

static void _assert_api_called(const char* request_params, int count)
{
  int i;
  for (i = 0; i < g_num_mock_api_responses; i++) {
    if (g_mock_api_responses[i].request_params &&
        strcmp(g_mock_api_responses[i].request_params, request_params) == 0) {
      ASSERT_NUM_EQUALS(g_mock_api_responses[i].seen, count);
      return;
    }
  }

  ASSERT_NUM_EQUALS(0, count);
}
#define assert_api_called(request_params) ASSERT_HELPER(_assert_api_called(request_params, 1), "assert_api_called")
#define assert_api_not_called(request_params) ASSERT_HELPER(_assert_api_called(request_params, 0), "assert_api_not_called")
#define assert_api_call_count(request_params, num) ASSERT_HELPER(_assert_api_called(request_params, num), "assert_api_call_count")
#define assert_api_pending(request_params) ASSERT_HELPER(_assert_api_called(request_params, -1), "assert_api_pending")

static void reset_mock_api_handlers(void)
{
  g_num_mock_api_responses = 0;
  memset(g_mock_api_responses, 0, sizeof(g_mock_api_responses));
}

static void mock_api_response(const char* request_params, const char* response_body)
{
  g_mock_api_responses[g_num_mock_api_responses].request_params = request_params;
  g_mock_api_responses[g_num_mock_api_responses].server_response.body = response_body;
  g_mock_api_responses[g_num_mock_api_responses].server_response.body_length = strlen(response_body);
  g_mock_api_responses[g_num_mock_api_responses].server_response.http_status_code = 200;
  g_num_mock_api_responses++;
}

static void mock_api_error(const char* request_params, const char* response_body, int http_status_code)
{
  g_mock_api_responses[g_num_mock_api_responses].request_params = request_params;
  g_mock_api_responses[g_num_mock_api_responses].server_response.body = response_body;
  g_mock_api_responses[g_num_mock_api_responses].server_response.body_length = strlen(response_body);
  g_mock_api_responses[g_num_mock_api_responses].server_response.http_status_code = http_status_code;
  g_num_mock_api_responses++;
}

static void rc_client_callback_expect_success(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_OK);
  ASSERT_PTR_NULL(error_message);
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void rc_client_callback_expect_uncalled(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_FAIL("Callback should not have been called.");
}

static rc_client_t* mock_client_not_logged_in(void)
{
  mock_memory(NULL, 0);
  rc_api_set_host(NULL);
  reset_mock_api_handlers();
  return rc_client_create(rc_client_read_memory, rc_client_server_call);
}

static rc_client_t* mock_client_not_logged_in_async(void)
{
  mock_memory(NULL, 0);
  rc_api_set_host(NULL);
  return rc_client_create(rc_client_read_memory, rc_client_server_call_async);
}

static rc_client_t* mock_client_logged_in(void)
{
  rc_client_t* client = rc_client_create(rc_client_read_memory, rc_client_server_call);
  client->user.username = "Username";
  client->user.display_name = "DisplayName";
  client->user.token = "ApiToken";
  client->user.score = 12345;
  client->state.user = RC_CLIENT_USER_STATE_LOGGED_IN;

  rc_client_set_event_handler(client, rc_client_event_handler);
  reset_mock_api_handlers();

  mock_memory(NULL, 0);
  rc_api_set_host(NULL);
  return client;
}

static void mock_client_load_game(const char* patchdata, const char* hardcore_unlocks, const char* softcore_unlocks)
{
  reset_mock_api_handlers();
  mock_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");
  mock_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata);
  mock_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", softcore_unlocks);
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", hardcore_unlocks);

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_success, g_callback_userdata);

  if (!g_client->game)
    ASSERT_MESSAGE("client->game is NULL");
}

static rc_client_t* mock_client_game_loaded(const char* patchdata, const char* hardcore_unlocks, const char* softcore_unlocks)
{
  g_client = mock_client_logged_in();

  mock_client_load_game(patchdata, hardcore_unlocks, softcore_unlocks);

  return g_client;
}

static void mock_client_load_subset(const char* patchdata, const char* hardcore_unlocks, const char* softcore_unlocks)
{
  mock_api_response("r=patch&u=Username&t=ApiToken&g=2345", patchdata);
  mock_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=2345&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=2345&h=0", softcore_unlocks);
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=2345&h=1", hardcore_unlocks);

  rc_client_begin_load_subset(g_client, 2345, rc_client_callback_expect_success, g_callback_userdata);
}

/* ----- login ----- */

static void test_login_with_password(void)
{
  const rc_client_user_t* user;

  g_client = mock_client_not_logged_in();
  reset_mock_api_handlers();
  mock_api_response("r=login&u=User&p=Pa%24%24word",
	  "{\"Success\":true,\"User\":\"User\",\"Token\":\"ApiToken\",\"Score\":12345,\"SoftcoreScore\":123,\"Messages\":2,\"Permissions\":1,\"AccountType\":\"Registered\"}");

  rc_client_begin_login_with_password(g_client, "User", "Pa$$word", rc_client_callback_expect_success, g_callback_userdata);

  user = rc_client_get_user_info(g_client);
  ASSERT_PTR_NOT_NULL(user);
  ASSERT_STR_EQUALS(user->username, "User");
  ASSERT_STR_EQUALS(user->display_name, "User");
  ASSERT_STR_EQUALS(user->token, "ApiToken");
  ASSERT_NUM_EQUALS(user->score, 12345);
  ASSERT_NUM_EQUALS(user->score_softcore, 123);
  ASSERT_NUM_EQUALS(user->num_unread_messages, 2);

  rc_client_destroy(g_client);
}

static void test_login_with_token(void)
{
  const rc_client_user_t* user;

  g_client = mock_client_not_logged_in();
  reset_mock_api_handlers();
  mock_api_response("r=login&u=User&t=ApiToken",
	  "{\"Success\":true,\"User\":\"User\",\"DisplayName\":\"Display\",\"Token\":\"ApiToken\",\"Score\":12345,\"Messages\":2}");

  rc_client_begin_login_with_token(g_client, "User", "ApiToken", rc_client_callback_expect_success, g_callback_userdata);

  user = rc_client_get_user_info(g_client);
  ASSERT_PTR_NOT_NULL(user);
  ASSERT_STR_EQUALS(user->username, "User");
  ASSERT_STR_EQUALS(user->display_name, "Display");
  ASSERT_STR_EQUALS(user->token, "ApiToken");
  ASSERT_NUM_EQUALS(user->score, 12345);
  ASSERT_NUM_EQUALS(user->num_unread_messages, 2);

  rc_client_destroy(g_client);
}

static void rc_client_callback_expect_username_required(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_INVALID_STATE);
  ASSERT_STR_EQUALS(error_message, "username is required");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void rc_client_callback_expect_password_required(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_INVALID_STATE);
  ASSERT_STR_EQUALS(error_message, "password is required");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void rc_client_callback_expect_token_required(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_INVALID_STATE);
  ASSERT_STR_EQUALS(error_message, "token is required");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void test_login_required_fields(void)
{
  g_client = mock_client_not_logged_in();

  rc_client_begin_login_with_password(g_client, "User", "", rc_client_callback_expect_password_required, g_callback_userdata);
  rc_client_begin_login_with_password(g_client, "", "Pa$$word", rc_client_callback_expect_username_required, g_callback_userdata);
  rc_client_begin_login_with_password(g_client, "", "", rc_client_callback_expect_username_required, g_callback_userdata);

  rc_client_begin_login_with_token(g_client, "User", "", rc_client_callback_expect_token_required, g_callback_userdata);
  rc_client_begin_login_with_token(g_client, "", "ApiToken", rc_client_callback_expect_username_required, g_callback_userdata);
  rc_client_begin_login_with_token(g_client, "", "", rc_client_callback_expect_username_required, g_callback_userdata);

  ASSERT_NUM_EQUALS(g_client->state.user, RC_CLIENT_USER_STATE_NONE);

  rc_client_destroy(g_client);
}

static void rc_client_callback_expect_credentials_error(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_API_FAILURE);
  ASSERT_STR_EQUALS(error_message, "Invalid User/Password combination. Please try again");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void test_login_with_incorrect_password(void)
{
  g_client = mock_client_not_logged_in();
  reset_mock_api_handlers();
  mock_api_error("r=login&u=User&p=Pa%24%24word", "{\"Success\":false,\"Error\":\"Invalid User/Password combination. Please try again\"}", 403);

  rc_client_begin_login_with_password(g_client, "User", "Pa$$word", rc_client_callback_expect_credentials_error, g_callback_userdata);

  ASSERT_PTR_NULL(rc_client_get_user_info(g_client));

  rc_client_destroy(g_client);
}

static void rc_client_callback_expect_missing_token(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_MISSING_VALUE);
  ASSERT_STR_EQUALS(error_message, "Token not found in response");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void test_login_incomplete_response(void)
{
  g_client = mock_client_not_logged_in();
  reset_mock_api_handlers();
  mock_api_response("r=login&u=User&p=Pa%24%24word", "{\"Success\":true,\"User\":\"Username\"}");

  rc_client_begin_login_with_password(g_client, "User", "Pa$$word", rc_client_callback_expect_missing_token, g_callback_userdata);

  ASSERT_PTR_NULL(rc_client_get_user_info(g_client));

  rc_client_destroy(g_client);
}

static void test_login_with_password_async(void)
{
  const rc_client_user_t* user;

  g_client = mock_client_not_logged_in_async();
  reset_mock_api_handlers();

  rc_client_begin_login_with_password(g_client, "User", "Pa$$word", rc_client_callback_expect_success, g_callback_userdata);

  user = rc_client_get_user_info(g_client);
  ASSERT_PTR_NULL(user);

  async_api_response("r=login&u=User&p=Pa%24%24word",
	    "{\"Success\":true,\"User\":\"User\",\"Token\":\"ApiToken\",\"Score\":12345,\"SoftcoreScore\":123,\"Messages\":2,\"Permissions\":1,\"AccountType\":\"Registered\"}");

  user = rc_client_get_user_info(g_client);
  ASSERT_PTR_NOT_NULL(user);
  ASSERT_STR_EQUALS(user->username, "User");
  ASSERT_STR_EQUALS(user->display_name, "User");
  ASSERT_STR_EQUALS(user->token, "ApiToken");
  ASSERT_NUM_EQUALS(user->score, 12345);
  ASSERT_NUM_EQUALS(user->num_unread_messages, 2);

  rc_client_destroy(g_client);
}

static void test_login_with_password_async_aborted(void)
{
  const rc_client_user_t* user;
  rc_client_async_handle_t* handle;

  g_client = mock_client_not_logged_in_async();
  reset_mock_api_handlers();

  handle = rc_client_begin_login_with_password(g_client, "User", "Pa$$word",
      rc_client_callback_expect_uncalled, g_callback_userdata);

  user = rc_client_get_user_info(g_client);
  ASSERT_PTR_NULL(user);

  rc_client_abort_async(g_client, handle);

  async_api_response("r=login&u=User&p=Pa%24%24word",
    "{\"Success\":true,\"User\":\"User\",\"Token\":\"ApiToken\",\"Score\":12345,\"SoftcoreScore\":123,\"Messages\":2,\"Permissions\":1,\"AccountType\":\"Registered\"}");

  user = rc_client_get_user_info(g_client);
  ASSERT_PTR_NULL(user);

  rc_client_destroy(g_client);
}

static void rc_client_callback_expect_login_required(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_LOGIN_REQUIRED);
  ASSERT_STR_EQUALS(error_message, "Login required");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void test_logout(void)
{
  const rc_client_user_t* user;

  g_client = mock_client_logged_in();

  user = rc_client_get_user_info(g_client);
  ASSERT_PTR_NOT_NULL(user);

  rc_client_logout(g_client);
  ASSERT_PTR_NULL(rc_client_get_user_info(g_client));

  /* reference pointer should be NULLed out */
  ASSERT_PTR_NULL(user->display_name);
  ASSERT_PTR_NULL(user->username);
  ASSERT_PTR_NULL(user->token);

  /* attempt to load game should fail */
  reset_mock_api_handlers();
  mock_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_login_required, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);
  
  rc_client_destroy(g_client);
}

static void test_logout_with_game_loaded(void)
{
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks, no_unlocks);

  ASSERT_PTR_NOT_NULL(rc_client_get_user_info(g_client));
  ASSERT_PTR_NOT_NULL(rc_client_get_game_info(g_client));

  rc_client_logout(g_client);

  ASSERT_PTR_NULL(rc_client_get_user_info(g_client));
  ASSERT_PTR_NULL(rc_client_get_game_info(g_client));

  rc_client_destroy(g_client);
}

static void rc_client_callback_expect_login_aborted(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_ABORTED);
  ASSERT_STR_EQUALS(error_message, "Login aborted");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void test_logout_during_login(void)
{
  g_client = mock_client_not_logged_in();
  g_client->callbacks.server_call = rc_client_server_call_async;

  rc_client_begin_login_with_password(g_client, "User", "Pa$$word", rc_client_callback_expect_login_aborted, g_callback_userdata);
  rc_client_logout(g_client);

  async_api_response("r=login&u=User&p=Pa%24%24word",
    "{\"Success\":true,\"User\":\"User\",\"Token\":\"ApiToken\",\"Score\":12345,\"SoftcoreScore\":123,\"Messages\":2,\"Permissions\":1,\"AccountType\":\"Registered\"}");

  ASSERT_PTR_NULL(rc_client_get_user_info(g_client));

  rc_client_destroy(g_client);
}

static void rc_client_callback_expect_no_longer_active(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_ABORTED);
  ASSERT_STR_EQUALS(error_message, "The requested game is no longer active");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void test_logout_during_fetch_game(void)
{
  g_client = mock_client_logged_in();
  g_client->callbacks.server_call = rc_client_server_call_async;

  reset_mock_api_handlers();

  rc_client_begin_load_game(g_client, "0123456789ABCDEF",
    rc_client_callback_expect_no_longer_active, g_callback_userdata);

  async_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");
  async_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata_2ach_1lbd);
  async_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  rc_client_logout(g_client);

  async_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", "{\"Success\":true,\"UserUnlocks\":[]}");
  async_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", "{\"Success\":true,\"UserUnlocks\":[]}");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);
  ASSERT_PTR_NULL(rc_client_get_user_info(g_client));

  rc_client_destroy(g_client);
}

static void test_user_get_image_url(void)
{
  char buffer[256];
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks, no_unlocks);

  ASSERT_NUM_EQUALS(rc_client_user_get_image_url(rc_client_get_user_info(g_client), buffer, sizeof(buffer)), RC_OK);
  ASSERT_STR_EQUALS(buffer, "https://media.retroachievements.org/UserPic/DisplayName.png");

  rc_client_destroy(g_client);
}

static void test_get_user_game_summary(void)
{
  rc_client_user_game_summary_t summary;

  g_client = mock_client_logged_in();
  rc_client_set_unofficial_enabled(g_client, 1);
  mock_client_load_game(patchdata_exhaustive, unlock_8, unlock_6_8_and_9);

  rc_client_get_user_game_summary(g_client, &summary);
  ASSERT_NUM_EQUALS(summary.num_core_achievements, 7);
  ASSERT_NUM_EQUALS(summary.num_unofficial_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 1);

  ASSERT_NUM_EQUALS(summary.points_core, 35);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 5);

  rc_client_destroy(g_client);
}

static void test_get_user_game_summary_softcore(void)
{
  rc_client_user_game_summary_t summary;

  g_client = mock_client_logged_in();
  rc_client_set_unofficial_enabled(g_client, 1);
  mock_client_load_game(patchdata_exhaustive, unlock_8, unlock_6_8_and_9);
  rc_client_set_hardcore_enabled(g_client, 0);

  rc_client_get_user_game_summary(g_client, &summary);
  ASSERT_NUM_EQUALS(summary.num_core_achievements, 7);
  ASSERT_NUM_EQUALS(summary.num_unofficial_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 3);

  ASSERT_NUM_EQUALS(summary.points_core, 35);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 15);

  rc_client_destroy(g_client);
}

static void test_get_user_game_summary_encore_mode(void)
{
  rc_client_user_game_summary_t summary;

  g_client = mock_client_logged_in();
  rc_client_set_unofficial_enabled(g_client, 1);
  reset_mock_api_handlers();
  mock_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");
  mock_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata_exhaustive);
  mock_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", unlock_6_8_and_9);
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", unlock_8);

  rc_client_set_encore_mode_enabled(g_client, 1);
  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_success, g_callback_userdata);

  rc_client_get_user_game_summary(g_client, &summary);
  ASSERT_NUM_EQUALS(summary.num_core_achievements, 7);
  ASSERT_NUM_EQUALS(summary.num_unofficial_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 1);

  ASSERT_NUM_EQUALS(summary.points_core, 35);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 5);

  rc_client_destroy(g_client);
}

static void test_get_user_game_summary_with_unsupported_and_unofficial(void)
{
  rc_client_user_game_summary_t summary;

  g_client = mock_client_logged_in();
  rc_client_set_unofficial_enabled(g_client, 1);
  mock_client_load_game(patchdata_unofficial_unsupported, no_unlocks, no_unlocks);

  rc_client_get_user_game_summary(g_client, &summary);
  ASSERT_NUM_EQUALS(summary.num_core_achievements, 2);
  ASSERT_NUM_EQUALS(summary.num_unofficial_achievements, 1);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 1);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 0);

  ASSERT_NUM_EQUALS(summary.points_core, 7);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 0);

  rc_client_destroy(g_client);
}


/* ----- load game ----- */

static void rc_client_callback_expect_hash_required(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_INVALID_STATE);
  ASSERT_STR_EQUALS(error_message, "hash is required");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void test_load_game_required_fields(void)
{
  g_client = mock_client_logged_in();

  rc_client_begin_load_game(g_client, NULL, rc_client_callback_expect_hash_required, g_callback_userdata);
  rc_client_begin_load_game(g_client, "", rc_client_callback_expect_hash_required, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
}

static void rc_client_callback_expect_unknown_game(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_NO_GAME_LOADED);
  ASSERT_STR_EQUALS(error_message, "Unknown game");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void test_load_game_unknown_hash(void)
{
  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":0}");

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_unknown_game, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public);

    ASSERT_NUM_EQUALS(g_client->game->public.id, 0);
    ASSERT_NUM_EQUALS(g_client->game->public.console_id, RC_CONSOLE_UNKNOWN);
    ASSERT_STR_EQUALS(g_client->game->public.title, "Unknown Game");
    ASSERT_STR_EQUALS(g_client->game->public.hash, "0123456789ABCDEF");
    ASSERT_STR_EQUALS(g_client->game->public.badge_name, "");
  }
  rc_client_destroy(g_client);
}

static void test_load_game_not_logged_in(void)
{
  g_client = mock_client_not_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_login_required, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
}

static void test_load_game(void)
{
  rc_client_achievement_info_t* achievement;
  rc_client_leaderboard_info_t* leaderboard;
  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");
  mock_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata_2ach_1lbd);
  mock_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", no_unlocks);
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", no_unlocks);

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public);

    ASSERT_NUM_EQUALS(g_client->game->public.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public.hash, "0123456789ABCDEF");
    ASSERT_STR_EQUALS(g_client->game->public.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_leaderboards, 1);

    achievement = &g_client->game->subsets->achievements[0];
    ASSERT_NUM_EQUALS(achievement->public.id, 5501);
    ASSERT_STR_EQUALS(achievement->public.title, "Ach1");
    ASSERT_STR_EQUALS(achievement->public.description, "Desc1");
    ASSERT_STR_EQUALS(achievement->public.badge_name, "00234");
    ASSERT_NUM_EQUALS(achievement->public.points, 5);
    ASSERT_NUM_EQUALS(achievement->public.unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->public.state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->public.category, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE);
    ASSERT_PTR_NOT_NULL(achievement->trigger);

    achievement = &g_client->game->subsets->achievements[1];
    ASSERT_NUM_EQUALS(achievement->public.id, 5502);
    ASSERT_STR_EQUALS(achievement->public.title, "Ach2");
    ASSERT_STR_EQUALS(achievement->public.description, "Desc2");
    ASSERT_STR_EQUALS(achievement->public.badge_name, "00235");
    ASSERT_NUM_EQUALS(achievement->public.points, 2);
    ASSERT_NUM_EQUALS(achievement->public.unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->public.state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->public.category, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE);
    ASSERT_PTR_NOT_NULL(achievement->trigger);

    leaderboard = &g_client->game->subsets->leaderboards[0];
    ASSERT_NUM_EQUALS(leaderboard->public.id, 4401);
    ASSERT_STR_EQUALS(leaderboard->public.title, "Leaderboard1");
    ASSERT_STR_EQUALS(leaderboard->public.description, "Desc1");
    ASSERT_NUM_EQUALS(leaderboard->public.state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(leaderboard->format, RC_FORMAT_SCORE);
    ASSERT_PTR_NOT_NULL(leaderboard->lboard);
    ASSERT_NUM_NOT_EQUALS(leaderboard->value_djb2, 0);
    ASSERT_PTR_NULL(leaderboard->tracker);
  }

  rc_client_destroy(g_client);
}

static void test_load_game_async_login(void)
{
  g_client = mock_client_not_logged_in_async();
  reset_mock_api_handlers();

  rc_client_begin_login_with_password(g_client, "Username", "Pa$$word", rc_client_callback_expect_success, g_callback_userdata);
  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_success, g_callback_userdata);

  async_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");
  /* game load process will stop here waiting for the login to complete */
  assert_api_not_called("r=patch&u=Username&t=ApiToken&g=1234");

  /* login completion will trigger process to continue */
  async_api_response("r=login&u=Username&p=Pa%24%24word",
	    "{\"Success\":true,\"User\":\"Username\",\"Token\":\"ApiToken\",\"Score\":12345,\"SoftcoreScore\":123,\"Messages\":2,\"Permissions\":1,\"AccountType\":\"Registered\"}");
  assert_api_pending("r=patch&u=Username&t=ApiToken&g=1234");

  async_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata_2ach_1lbd);
  async_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  async_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", no_unlocks);
  async_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", no_unlocks);

  ASSERT_STR_EQUALS(g_client->user.username, "Username");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public);

    ASSERT_NUM_EQUALS(g_client->game->public.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public.hash, "0123456789ABCDEF");
    ASSERT_STR_EQUALS(g_client->game->public.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
}

static void test_load_game_async_login_with_incorrect_password(void)
{
  g_client = mock_client_not_logged_in_async();
  reset_mock_api_handlers();

  rc_client_begin_login_with_password(g_client, "Username", "Pa$$word", rc_client_callback_expect_credentials_error, g_callback_userdata);
  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_login_required, g_callback_userdata);

  async_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");
  /* game load process will stop here waiting for the login to complete */
  assert_api_not_called("r=patch&u=Username&t=ApiToken&g=1234");

  /* login failure will trigger process to continue */
  async_api_error("r=login&u=Username&p=Pa%24%24word",
      "{\"Success\":false,\"Error\":\"Invalid User/Password combination. Please try again\"}", 403);
  assert_api_not_called("r=patch&u=Username&t=ApiToken&g=1234");

  ASSERT_PTR_NULL(g_client->user.username);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
}

static void rc_client_callback_expect_too_many_requests(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_INVALID_JSON);
  ASSERT_STR_EQUALS(error_message, "429 Too Many Requests");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void test_load_game_gameid_failure(void)
{
  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_error("r=gameid&m=0123456789ABCDEF", response_429, 429);
  mock_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata_2ach_1lbd);
  mock_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", "{\"Success\":true,\"UserUnlocks\":[]}");
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", "{\"Success\":true,\"UserUnlocks\":[]}");

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_too_many_requests, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
}

static void test_load_game_patch_failure(void)
{
  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");
  mock_api_error("r=patch&u=Username&t=ApiToken&g=1234", response_429, 429);
  mock_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", "{\"Success\":true,\"UserUnlocks\":[]}");
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", "{\"Success\":true,\"UserUnlocks\":[]}");

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_too_many_requests, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
}

static void test_load_game_postactivity_failure(void)
{
  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");
  mock_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata_2ach_1lbd);
  mock_api_error("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, response_429, 429);
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", "{\"Success\":true,\"UserUnlocks\":[]}");
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", "{\"Success\":true,\"UserUnlocks\":[]}");

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_too_many_requests, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
}

static void test_load_game_softcore_unlocks_failure(void)
{
  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");
  mock_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata_2ach_1lbd);
  mock_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  mock_api_error("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", response_429, 429);
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", "{\"Success\":true,\"UserUnlocks\":[]}");

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_too_many_requests, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
}

static void test_load_game_hardcore_unlocks_failure(void)
{
  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");
  mock_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata_2ach_1lbd);
  mock_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", "{\"Success\":true,\"UserUnlocks\":[]}");
  mock_api_error("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", response_429, 429);

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_too_many_requests, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
}

static void test_load_game_gameid_aborted(void)
{
  rc_client_async_handle_t* handle;

  g_client = mock_client_logged_in();
  g_client->callbacks.server_call = rc_client_server_call_async;

  reset_mock_api_handlers();

  handle = rc_client_begin_load_game(g_client, "0123456789ABCDEF",
    rc_client_callback_expect_uncalled, g_callback_userdata);

  rc_client_abort_async(g_client, handle);

  async_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");
  assert_api_not_called("r=patch&u=Username&t=ApiToken&g=1234");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
}

static void test_load_game_patch_aborted(void)
{
  rc_client_async_handle_t* handle;

  g_client = mock_client_logged_in();
  g_client->callbacks.server_call = rc_client_server_call_async;

  reset_mock_api_handlers();

  handle = rc_client_begin_load_game(g_client, "0123456789ABCDEF",
    rc_client_callback_expect_uncalled, g_callback_userdata);

  async_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");

  rc_client_abort_async(g_client, handle);

  async_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata_2ach_1lbd);
  assert_api_not_called("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
}

static void test_load_game_postactivity_aborted(void)
{
  rc_client_async_handle_t* handle;

  g_client = mock_client_logged_in();
  g_client->callbacks.server_call = rc_client_server_call_async;

  reset_mock_api_handlers();

  handle = rc_client_begin_load_game(g_client, "0123456789ABCDEF",
    rc_client_callback_expect_uncalled, g_callback_userdata);

  async_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");
  async_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata_2ach_1lbd);

  rc_client_abort_async(g_client, handle);

  async_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  async_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", "{\"Success\":true,\"UserUnlocks\":[]}");
  async_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", "{\"Success\":true,\"UserUnlocks\":[]}");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
}

static void test_load_game_softcore_unlocks_aborted(void)
{
  rc_client_async_handle_t* handle;

  g_client = mock_client_logged_in();
  g_client->callbacks.server_call = rc_client_server_call_async;

  reset_mock_api_handlers();

  handle = rc_client_begin_load_game(g_client, "0123456789ABCDEF",
    rc_client_callback_expect_uncalled, g_callback_userdata);

  async_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");
  async_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata_2ach_1lbd);
  async_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  rc_client_abort_async(g_client, handle);

  async_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", "{\"Success\":true,\"UserUnlocks\":[]}");
  async_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", "{\"Success\":true,\"UserUnlocks\":[]}");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
}

static void test_load_game_hardcore_unlocks_aborted(void)
{
  rc_client_async_handle_t* handle;

  g_client = mock_client_logged_in();
  g_client->callbacks.server_call = rc_client_server_call_async;

  reset_mock_api_handlers();

  handle = rc_client_begin_load_game(g_client, "0123456789ABCDEF",
    rc_client_callback_expect_uncalled, g_callback_userdata);

  async_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");
  async_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata_2ach_1lbd);
  async_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  async_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", "{\"Success\":true,\"UserUnlocks\":[]}");

  rc_client_abort_async(g_client, handle);

  async_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", "{\"Success\":true,\"UserUnlocks\":[]}");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
}

static void test_load_game_while_spectating(void)
{
  rc_client_achievement_info_t* achievement;
  rc_client_leaderboard_info_t* leaderboard;
  g_client = mock_client_logged_in();
  rc_client_set_spectator_mode_enabled(g_client, 1);

  reset_mock_api_handlers();
  mock_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");
  mock_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata_2ach_1lbd);
  /* spectator mode should not start a session or fetch unlocks */

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public);

    ASSERT_NUM_EQUALS(g_client->game->public.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public.hash, "0123456789ABCDEF");
    ASSERT_STR_EQUALS(g_client->game->public.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_leaderboards, 1);

    achievement = &g_client->game->subsets->achievements[0];
    ASSERT_NUM_EQUALS(achievement->public.id, 5501);
    ASSERT_STR_EQUALS(achievement->public.title, "Ach1");
    ASSERT_STR_EQUALS(achievement->public.description, "Desc1");
    ASSERT_STR_EQUALS(achievement->public.badge_name, "00234");
    ASSERT_NUM_EQUALS(achievement->public.points, 5);
    ASSERT_NUM_EQUALS(achievement->public.unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->public.state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->public.category, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE);
    ASSERT_PTR_NOT_NULL(achievement->trigger);

    achievement = &g_client->game->subsets->achievements[1];
    ASSERT_NUM_EQUALS(achievement->public.id, 5502);
    ASSERT_STR_EQUALS(achievement->public.title, "Ach2");
    ASSERT_STR_EQUALS(achievement->public.description, "Desc2");
    ASSERT_STR_EQUALS(achievement->public.badge_name, "00235");
    ASSERT_NUM_EQUALS(achievement->public.points, 2);
    ASSERT_NUM_EQUALS(achievement->public.unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->public.state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->public.category, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE);
    ASSERT_PTR_NOT_NULL(achievement->trigger);

    leaderboard = &g_client->game->subsets->leaderboards[0];
    ASSERT_NUM_EQUALS(leaderboard->public.id, 4401);
    ASSERT_STR_EQUALS(leaderboard->public.title, "Leaderboard1");
    ASSERT_STR_EQUALS(leaderboard->public.description, "Desc1");
    ASSERT_NUM_EQUALS(leaderboard->public.state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(leaderboard->format, RC_FORMAT_SCORE);
    ASSERT_PTR_NOT_NULL(leaderboard->lboard);
    ASSERT_NUM_NOT_EQUALS(leaderboard->value_djb2, 0);
    ASSERT_PTR_NULL(leaderboard->tracker);
  }

  /* spectator mode cannot be disabled if it was enabled before loading the game */
  rc_client_set_spectator_mode_enabled(g_client, 0);
  ASSERT_TRUE(rc_client_get_spectator_mode_enabled(g_client));

  rc_client_unload_game(g_client);

  /* spectator mode can be disabled after unloading game */
  rc_client_set_spectator_mode_enabled(g_client, 0);
  ASSERT_FALSE(rc_client_get_spectator_mode_enabled(g_client));

  rc_client_destroy(g_client);
}

/* ----- identify and load game ----- */

static void rc_client_callback_expect_data_or_file_path_required(int result, const char* error_message, rc_client_t* client, void* callback_data)
{
  ASSERT_NUM_EQUALS(result, RC_INVALID_STATE);
  ASSERT_STR_EQUALS(error_message, "either data or file_path is required");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_data, g_callback_userdata);
}

static void test_identify_and_load_game_required_fields(void)
{
  g_client = mock_client_logged_in();

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_UNKNOWN, NULL, NULL, 0,
      rc_client_callback_expect_data_or_file_path_required, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
}

static void test_identify_and_load_game_console_specified(void)
{
  size_t image_size;
  uint8_t* image = generate_nes_file(32, 1, &image_size);

  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":1234}");
  mock_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata_2ach_1lbd);
  mock_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", no_unlocks);
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", no_unlocks);

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_NINTENDO, "foo.zip#foo.nes",
      image, image_size, rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public);

    ASSERT_NUM_EQUALS(g_client->game->public.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
  free(image);
}

static void test_identify_and_load_game_console_not_specified(void)
{
  size_t image_size;
  uint8_t* image = generate_nes_file(32, 1, &image_size);

  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":1234}");
  mock_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata_2ach_1lbd);
  mock_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", no_unlocks);
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", no_unlocks);

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_UNKNOWN, "foo.zip#foo.nes",
      image, image_size, rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public);

    ASSERT_NUM_EQUALS(g_client->game->public.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
  free(image);
}

static void test_identify_and_load_game_unknown_hash(void)
{
  size_t image_size;
  uint8_t* image = generate_nes_file(32, 1, &image_size);

  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":0}");

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_UNKNOWN, "foo.zip#foo.nes",
      image, image_size, rc_client_callback_expect_unknown_game, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public);

    ASSERT_NUM_EQUALS(g_client->game->public.id, 0);
    ASSERT_NUM_EQUALS(g_client->game->public.console_id, RC_CONSOLE_NINTENDO);
    ASSERT_STR_EQUALS(g_client->game->public.title, "Unknown Game");
    ASSERT_STR_EQUALS(g_client->game->public.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public.badge_name, "");
  }

  rc_client_destroy(g_client);
  free(image);
}

static void test_identify_and_load_game_multihash(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":1234}");
  mock_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata_2ach_1lbd);
  mock_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", no_unlocks);
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", no_unlocks);

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_UNKNOWN, "abc.dsk",
      image, image_size, rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public);

    ASSERT_NUM_EQUALS(g_client->game->public.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
  free(image);
}

static void test_identify_and_load_game_multihash_unknown_game(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":0}");

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_UNKNOWN, "abc.dsk",
      image, image_size, rc_client_callback_expect_unknown_game, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public);

    ASSERT_NUM_EQUALS(g_client->game->public.id, 0);
    ASSERT_NUM_EQUALS(g_client->game->public.console_id, RC_CONSOLE_APPLE_II);
    ASSERT_STR_EQUALS(g_client->game->public.title, "Unknown Game");
    ASSERT_STR_EQUALS(g_client->game->public.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public.badge_name, "");
  }

  /* same hash generated for all dsk consoles - only one server call should be made */
  assert_api_call_count("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", 1);

  rc_client_destroy(g_client);
  free(image);
}

static void test_identify_and_load_game_multihash_differ(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_logged_in();
  g_client->callbacks.server_call = rc_client_server_call_async;

  reset_mock_api_handlers();

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_UNKNOWN, "abc.dsk",
      image, image_size, rc_client_callback_expect_success, g_callback_userdata);

  /* modify the checksum so callback for first lookup will generate a new lookup */
  memset(&image[256], 0, 32);

  /* first lookup fails */
  async_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":0}");
  ASSERT_PTR_NOT_NULL(g_client->state.load);

  /* second lookup should succeed */
  async_api_response("r=gameid&m=4989b063a40dcfa28291ff8d675050e3", "{\"Success\":true,\"GameID\":1234}");
  async_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata_2ach_1lbd);
  async_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  async_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", no_unlocks);
  async_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", no_unlocks);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public);

    ASSERT_NUM_EQUALS(g_client->game->public.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public.hash, "4989b063a40dcfa28291ff8d675050e3");
    ASSERT_STR_EQUALS(g_client->game->public.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
  free(image);
}

/* ----- change media ----- */

static void test_change_media_required_fields(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks, no_unlocks);

  rc_client_begin_change_media(g_client, NULL, NULL, 0,
      rc_client_callback_expect_data_or_file_path_required, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public);

    ASSERT_NUM_EQUALS(g_client->game->public.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public.hash, "0123456789ABCDEF");
    ASSERT_STR_EQUALS(g_client->game->public.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
  free(image);
}

static void rc_client_callback_expect_no_game_loaded(int result, const char* error_message, rc_client_t* client, void* callback_data)
{
  ASSERT_NUM_EQUALS(result, RC_NO_GAME_LOADED);
  ASSERT_STR_EQUALS(error_message, "No game loaded");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_data, g_callback_userdata);
}

static void test_change_media_no_game_loaded(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_logged_in();

  rc_client_begin_change_media(g_client, "foo.zip#foo.nes", image, image_size,
      rc_client_callback_expect_no_game_loaded, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
  free(image);
}

static void test_change_media_same_game(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks, no_unlocks);

  mock_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":1234}");

  /* changing known discs within a game set is expected to succeed */
  rc_client_begin_change_media(g_client, "foo.zip#foo.nes", image, image_size,
      rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public);

    ASSERT_NUM_EQUALS(g_client->game->public.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_leaderboards, 1);
  }

  /* resetting with a disc from the current game is allowed */
  rc_client_reset(g_client);
  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public);

    ASSERT_NUM_EQUALS(g_client->game->public.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
  free(image);
}

static void test_change_media_known_game(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks, no_unlocks);

  mock_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":5555}");

  /* changing to a known disc from another game is allowed */
  rc_client_begin_change_media(g_client, "foo.zip#foo.nes", image, image_size,
      rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public);

    ASSERT_NUM_EQUALS(g_client->game->public.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_leaderboards, 1);
  }

  /* resetting with a disc from another game will disable the client */
  rc_client_reset(g_client);
  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
  free(image);
}

static void rc_client_callback_expect_hardcore_disabled_undentified_media(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_HARDCORE_DISABLED);
  ASSERT_STR_EQUALS(error_message, "Hardcore disabled. Unidentified media inserted.");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void test_change_media_unknown_game(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks, no_unlocks);
  ASSERT_TRUE(rc_client_get_hardcore_enabled(g_client));

  mock_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":0}");

  /* changing to an unknown disc is not allowed - could be a hacked version of one of the game's discs */
  rc_client_begin_change_media(g_client, "foo.zip#foo.nes", image, image_size,
      rc_client_callback_expect_hardcore_disabled_undentified_media, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public);

    ASSERT_NUM_EQUALS(g_client->game->public.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_leaderboards, 1);
  }

  ASSERT_FALSE(rc_client_get_hardcore_enabled(g_client));

  /* resetting with a disc not from the current game will disable the client */
  rc_client_reset(g_client);
  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
  free(image);
}

static void test_change_media_unhashable(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks, no_unlocks);

  /* N64 hash will fail with Not a Nintendo 64 ROM */
  g_client->game->public.console_id = RC_CONSOLE_NINTENDO_64;

  /* changing to a disc not supported by the system is allowed */
  rc_client_begin_change_media(g_client, "foo.zip#foo.nes", image, image_size,
      rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public);

    ASSERT_NUM_EQUALS(g_client->game->public.id, 1234);
    ASSERT_STR_EQUALS(g_client->game->public.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public.hash, "[NO HASH]");
    ASSERT_STR_EQUALS(g_client->game->public.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_leaderboards, 1);
  }

  /* resetting with a disc not from the current game will disable the client */
  rc_client_reset(g_client);
  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
  free(image);
}

static void test_change_media_back_and_forth(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);
  uint8_t* image2 = generate_generic_file(image_size);
  memset(&image2[256], 0, 32); /* force image2 to be different */

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks, no_unlocks);

  mock_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":1234}");
  mock_api_response("r=gameid&m=4989b063a40dcfa28291ff8d675050e3", "{\"Success\":true,\"GameID\":1234}");

  rc_client_begin_change_media(g_client, "foo.zip#foo.nes", image, image_size,
      rc_client_callback_expect_success, g_callback_userdata);
  rc_client_begin_change_media(g_client, "foo.zip#foo2.nes", image2, image_size,
      rc_client_callback_expect_success, g_callback_userdata);
  rc_client_begin_change_media(g_client, "foo.zip#foo.nes", image, image_size,
      rc_client_callback_expect_success, g_callback_userdata);
  rc_client_begin_change_media(g_client, "foo.zip#foo2.nes", image2, image_size,
      rc_client_callback_expect_success, g_callback_userdata);

  assert_api_call_count("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", 1);
  assert_api_call_count("r=gameid&m=4989b063a40dcfa28291ff8d675050e3", 1);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public);

    ASSERT_NUM_EQUALS(g_client->game->public.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public.hash, "4989b063a40dcfa28291ff8d675050e3");
    ASSERT_STR_EQUALS(g_client->game->public.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
  free(image2);
  free(image);
}

static void test_change_media_while_loading(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_logged_in();
  g_client->callbacks.server_call = rc_client_server_call_async;

  reset_mock_api_handlers();

  rc_client_begin_load_game(g_client, "4989b063a40dcfa28291ff8d675050e3",
      rc_client_callback_expect_success, g_callback_userdata);
  rc_client_begin_change_media(g_client, "foo.zip#foo.nes", image, image_size,
      rc_client_callback_expect_success, g_callback_userdata);

  /* load game lookup */
  async_api_response("r=gameid&m=4989b063a40dcfa28291ff8d675050e3", "{\"Success\":true,\"GameID\":1234}");

  /* media request won't occur until patch data is received */
  assert_api_not_called("r=gameid&m=6a2305a2b6675a97ff792709be1ca857");
  async_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata_2ach_1lbd);
  assert_api_not_called("r=gameid&m=6a2305a2b6675a97ff792709be1ca857");

  /* finish loading game */
  async_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  assert_api_not_called("r=gameid&m=6a2305a2b6675a97ff792709be1ca857");
  async_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", no_unlocks);
  assert_api_not_called("r=gameid&m=6a2305a2b6675a97ff792709be1ca857");
  async_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", no_unlocks);

  /* secondary hash resolution does not occur until game is fully loaded or hash can't be compared to loaded game */
  assert_api_pending("r=gameid&m=6a2305a2b6675a97ff792709be1ca857");
  async_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":1234}");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public);

    ASSERT_NUM_EQUALS(g_client->game->public.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
  free(image);
}

static void test_change_media_while_loading_later(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_logged_in();
  g_client->callbacks.server_call = rc_client_server_call_async;

  reset_mock_api_handlers();

  rc_client_begin_load_game(g_client, "4989b063a40dcfa28291ff8d675050e3",
      rc_client_callback_expect_success, g_callback_userdata);

  /* get past fetching the patch data so there's a valid console for the change media call */
  async_api_response("r=gameid&m=4989b063a40dcfa28291ff8d675050e3", "{\"Success\":true,\"GameID\":1234}");
  async_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata_2ach_1lbd);

  /* change_media should immediately attempt to resolve the new hash */
  rc_client_begin_change_media(g_client, "foo.zip#foo.nes", image, image_size,
      rc_client_callback_expect_success, g_callback_userdata);
  assert_api_pending("r=gameid&m=6a2305a2b6675a97ff792709be1ca857");

  /* finish loading game */
  async_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  async_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", no_unlocks);
  async_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", no_unlocks);
  async_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":1234}");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public);

    ASSERT_NUM_EQUALS(g_client->game->public.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
  free(image);
}

static void test_change_media_aborted(void)
{
  rc_client_async_handle_t* handle;
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks, no_unlocks);
  g_client->callbacks.server_call = rc_client_server_call_async;

  reset_mock_api_handlers();

  /* changing known discs within a game set is expected to succeed */
  handle = rc_client_begin_change_media(g_client, "foo.zip#foo.nes", image, image_size,
    rc_client_callback_expect_uncalled, g_callback_userdata);

  rc_client_abort_async(g_client, handle);

  async_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":1234}");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public);

    ASSERT_NUM_EQUALS(g_client->game->public.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public.hash, "0123456789ABCDEF"); /* old hash retained */
    ASSERT_STR_EQUALS(g_client->game->public.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_leaderboards, 1);
  }

  /* hash should still have been captured and lookup should succeed without having to call server again */
  reset_mock_api_handlers();

  rc_client_begin_change_media(g_client, "foo.zip#foo.nes", image, image_size,
    rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_client->game->public.hash, "6a2305a2b6675a97ff792709be1ca857");
  assert_api_not_called("r=gameid&m=6a2305a2b6675a97ff792709be1ca857");

  rc_client_destroy(g_client);
  free(image);
}

/* ----- get game image ----- */

static void test_game_get_image_url(void)
{
  char buffer[256];
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks, no_unlocks);

  ASSERT_NUM_EQUALS(rc_client_game_get_image_url(rc_client_get_game_info(g_client), buffer, sizeof(buffer)), RC_OK);
  ASSERT_STR_EQUALS(buffer, "https://media.retroachievements.org/Images/112233.png");

  rc_client_destroy(g_client);
}

static void test_game_get_image_url_non_ssl(void)
{
  char buffer[256];
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks, no_unlocks);
  rc_client_set_host(g_client, "http://retroachievements.org");

  ASSERT_NUM_EQUALS(rc_client_game_get_image_url(rc_client_get_game_info(g_client), buffer, sizeof(buffer)), RC_OK);
  ASSERT_STR_EQUALS(buffer, "http://media.retroachievements.org/Images/112233.png");

  rc_client_destroy(g_client);
}

static void test_game_get_image_url_custom(void)
{
  char buffer[256];
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks, no_unlocks);
  rc_client_set_host(g_client, "localhost");

  ASSERT_NUM_EQUALS(rc_client_game_get_image_url(rc_client_get_game_info(g_client), buffer, sizeof(buffer)), RC_OK);
  ASSERT_STR_EQUALS(buffer, "http://localhost/Images/112233.png");

  rc_client_destroy(g_client);
}

/* ----- subset ----- */

static void test_load_subset(void)
{
  rc_client_achievement_info_t* achievement;
  rc_client_leaderboard_info_t* leaderboard;
  rc_client_subset_info_t* subset_info;
  const rc_client_subset_t* subset;
  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":1234}");
  mock_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata_2ach_1lbd);
  mock_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", no_unlocks);
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", no_unlocks);

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_success, g_callback_userdata);

  mock_api_response("r=patch&u=Username&t=ApiToken&g=2345", patchdata_subset);
  mock_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=2345&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=2345&h=0", no_unlocks);
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=2345&h=1", no_unlocks);

  rc_client_begin_load_subset(g_client, 2345, rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public);

    ASSERT_NUM_EQUALS(g_client->game->public.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public.hash, "0123456789ABCDEF");
    ASSERT_STR_EQUALS(g_client->game->public.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public.num_leaderboards, 1);
  }

  subset = rc_client_get_subset_info(g_client, 2345);
  ASSERT_PTR_NOT_NULL(subset);
  if (subset) {
    subset_info = g_client->game->subsets->next;
    ASSERT_PTR_EQUALS(subset, &subset_info->public);

    ASSERT_NUM_EQUALS(subset->id, 2345);
    ASSERT_STR_EQUALS(subset->title, "Bonus");
    ASSERT_STR_EQUALS(subset->badge_name, "112234");
    ASSERT_NUM_EQUALS(subset->num_achievements, 3);
    ASSERT_NUM_EQUALS(subset->num_leaderboards, 2);

    achievement = &subset_info->achievements[0];
    ASSERT_NUM_EQUALS(achievement->public.id, 7);
    ASSERT_STR_EQUALS(achievement->public.title, "Achievement 7");
    ASSERT_STR_EQUALS(achievement->public.description, "Desc 7");
    ASSERT_STR_EQUALS(achievement->public.badge_name, "007");
    ASSERT_NUM_EQUALS(achievement->public.points, 5);
    ASSERT_NUM_EQUALS(achievement->public.unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->public.state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->public.category, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE);
    ASSERT_PTR_NOT_NULL(achievement->trigger);

    achievement = &subset_info->achievements[1];
    ASSERT_NUM_EQUALS(achievement->public.id, 8);
    ASSERT_STR_EQUALS(achievement->public.title, "Achievement 8");
    ASSERT_STR_EQUALS(achievement->public.description, "Desc 8");
    ASSERT_STR_EQUALS(achievement->public.badge_name, "008");
    ASSERT_NUM_EQUALS(achievement->public.points, 5);
    ASSERT_NUM_EQUALS(achievement->public.unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->public.state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->public.category, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE);
    ASSERT_PTR_NOT_NULL(achievement->trigger);

    achievement = &subset_info->achievements[2];
    ASSERT_NUM_EQUALS(achievement->public.id, 9);
    ASSERT_STR_EQUALS(achievement->public.title, "Achievement 9");
    ASSERT_STR_EQUALS(achievement->public.description, "Desc 9");
    ASSERT_STR_EQUALS(achievement->public.badge_name, "009");
    ASSERT_NUM_EQUALS(achievement->public.points, 5);
    ASSERT_NUM_EQUALS(achievement->public.unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->public.state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->public.category, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE);
    ASSERT_PTR_NOT_NULL(achievement->trigger);

    leaderboard = &subset_info->leaderboards[0];
    ASSERT_NUM_EQUALS(leaderboard->public.id, 81);
    ASSERT_STR_EQUALS(leaderboard->public.title, "Leaderboard 81");
    ASSERT_STR_EQUALS(leaderboard->public.description, "Desc 81");
    ASSERT_NUM_EQUALS(leaderboard->public.state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(leaderboard->format, RC_FORMAT_SCORE);
    ASSERT_PTR_NOT_NULL(leaderboard->lboard);
    ASSERT_NUM_NOT_EQUALS(leaderboard->value_djb2, 0);
    ASSERT_PTR_NULL(leaderboard->tracker);

    leaderboard = &subset_info->leaderboards[1];
    ASSERT_NUM_EQUALS(leaderboard->public.id, 82);
    ASSERT_STR_EQUALS(leaderboard->public.title, "Leaderboard 82");
    ASSERT_STR_EQUALS(leaderboard->public.description, "Desc 82");
    ASSERT_NUM_EQUALS(leaderboard->public.state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(leaderboard->format, RC_FORMAT_SCORE);
    ASSERT_PTR_NOT_NULL(leaderboard->lboard);
    ASSERT_NUM_NOT_EQUALS(leaderboard->value_djb2, 0);
    ASSERT_PTR_NULL(leaderboard->tracker);
  }

  rc_client_destroy(g_client);
}

/* ----- achievement list ----- */

static void test_achievement_list_simple(void)
{
  rc_client_achievement_list_t* list;
  rc_client_achievement_t** iter;
  rc_client_achievement_t* achievement;

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks, no_unlocks);

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 2);

    iter = list->buckets[0].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5501);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5502);

    rc_client_destroy_achievement_list(list);
  }

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_UNOFFICIAL, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 0);
    rc_client_destroy_achievement_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_achievement_list_simple_with_unlocks(void)
{
  rc_client_achievement_list_t* list;
  rc_client_achievement_t** iter;
  rc_client_achievement_t* achievement;

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, unlock_5501, unlock_5501_and_5502);

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    /* in hardcore mode, 5501 should be unlocked, but 5502 will be locked */
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);

    iter = list->buckets[0].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5502);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    iter = list->buckets[1].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5501);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);

    rc_client_destroy_achievement_list(list);
  }

  rc_client_set_hardcore_enabled(g_client, 0);

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    /* in softcore mode, both should be unlocked */
    ASSERT_NUM_EQUALS(list->num_buckets, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 2);

    iter = list->buckets[0].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5501);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5502);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);

    rc_client_destroy_achievement_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_achievement_list_simple_with_unlocks_encore_mode(void)
{
  rc_client_achievement_list_t* list;
  rc_client_achievement_t** iter;
  rc_client_achievement_t* achievement;

  g_client = mock_client_logged_in();
  rc_client_set_encore_mode_enabled(g_client, 1);
  mock_client_load_game(patchdata_2ach_1lbd, unlock_5501, unlock_5501_and_5502);

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    /* in hardcore mode, 5501 should be unlocked, but both will appear locked due to encore mode */
    ASSERT_NUM_EQUALS(list->num_buckets, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 2);

    iter = list->buckets[0].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5501);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5502);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);

    rc_client_destroy_achievement_list(list);
  }

  rc_client_set_hardcore_enabled(g_client, 0);

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    /* in softcore mode, both should be unlocked, but will appear locked due to encore mode */
    ASSERT_NUM_EQUALS(list->num_buckets, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 2);

    iter = list->buckets[0].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5501);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5502);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);

    rc_client_destroy_achievement_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_achievement_list_simple_with_unofficial_and_unsupported(void)
{
  rc_client_achievement_list_t* list;

  g_client = mock_client_logged_in();
  rc_client_set_unofficial_enabled(g_client, 1);
  mock_client_load_game(patchdata_unofficial_unsupported, no_unlocks, no_unlocks);

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 5501);
    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unsupported");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5503);

    rc_client_destroy_achievement_list(list);
  }

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_UNOFFICIAL, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNOFFICIAL);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Unofficial");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 5502);

    rc_client_destroy_achievement_list(list);
  }

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 3);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 5501);
    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNOFFICIAL);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unofficial");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5502);
    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Unsupported");
    ASSERT_NUM_EQUALS(list->buckets[2].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[0]->id, 5503);

    rc_client_destroy_achievement_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_achievement_list_simple_with_unofficial_off(void)
{
  rc_client_achievement_list_t* list;

  g_client = mock_client_logged_in();
  rc_client_set_unofficial_enabled(g_client, 0);
  mock_client_load_game(patchdata_unofficial_unsupported, no_unlocks, no_unlocks);

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 5501);
    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unsupported");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5503);

    rc_client_destroy_achievement_list(list);
  }

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_UNOFFICIAL, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 0);
    rc_client_destroy_achievement_list(list);
  }

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 5501);
    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unsupported");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5503);

    rc_client_destroy_achievement_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_achievement_list_buckets(void)
{
  rc_client_achievement_list_t* list;
  rc_client_achievement_t** iter;
  rc_client_achievement_t* achievement;

  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, unlock_8, unlock_8);
  mock_memory(memory, sizeof(memory));

  rc_client_do_frame(g_client); /* advance achievements out of waiting state */
  event_count = 0;

  mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=5&h=1&m=0123456789ABCDEF&v=732f8e30e9c1eb08948dda098c305d8b",
      "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":5,\"AchievementsRemaining\":6}");

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 6);
    iter = list->buckets[0].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 6);
    ASSERT_STR_EQUALS(achievement->measured_progress, "");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 0.0);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 7);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 9);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 70);
    ASSERT_STR_EQUALS(achievement->measured_progress, "");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 0.0);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 71);
    ASSERT_STR_EQUALS(achievement->measured_progress, "");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 0.0);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 8);

    rc_client_destroy_achievement_list(list);
  }

  memory[5] = 5; /* trigger achievement 5 */
  memory[6] = 2; /* start measuring achievement 6 */
  memory[1] = 1; /* begin challenge achievement 7 */
  memory[0x11] = 100; /* start measuring achievements 70 and 71 */
  rc_client_do_frame(g_client);
  event_count = 0;

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 4);

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Active Challenges");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 7);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Recently Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[2].num_achievements, 4);
    iter = list->buckets[2].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 6);
    ASSERT_STR_EQUALS(achievement->measured_progress, "2/6");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 33.333333);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 9);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 70);
    ASSERT_STR_EQUALS(achievement->measured_progress, "25600/100000");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 25.6);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 71);
    ASSERT_STR_EQUALS(achievement->measured_progress, "25%");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 25.6);

    ASSERT_NUM_EQUALS(list->buckets[3].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[3].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[3].label, "Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[3].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[3].achievements[0]->id, 8);

    rc_client_destroy_achievement_list(list);
  }

  /* recently unlocked achievement no longer recent */
  ((rc_client_achievement_t*)rc_client_get_achievement_info(g_client, 5))->unlock_time -= 15 * 60;
  memory[6] = 5; /* almost there achievement 6 */
  memory[1] = 0; /* stop challenge achievement 7 */
  rc_client_do_frame(g_client);
  event_count = 0;

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 3);

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Almost There");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 6);
    ASSERT_STR_EQUALS(list->buckets[0].achievements[0]->measured_progress, "5/6");
    ASSERT_FLOAT_EQUALS(list->buckets[0].achievements[0] ->measured_percent, 83.333333);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 4);
    iter = list->buckets[1].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 7);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 9);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 70);
    ASSERT_STR_EQUALS(achievement->measured_progress, "25600/100000");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 25.6);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 71);
    ASSERT_STR_EQUALS(achievement->measured_progress, "25%");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 25.6);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[2].num_achievements, 2);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[0]->id, 5);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[1]->id, 8);

    rc_client_destroy_achievement_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_achievement_list_subset_with_unofficial_and_unsupported(void)
{
  rc_client_achievement_list_t* list;

  g_client = mock_client_logged_in();
  rc_client_set_unofficial_enabled(g_client, 1);
  mock_client_load_game(patchdata_unofficial_unsupported, no_unlocks, no_unlocks);
  mock_client_load_subset(patchdata_subset, no_unlocks, no_unlocks);

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 3);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 1234);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Sample Game - Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 5501);
    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 1234);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Sample Game - Unsupported");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5503);
    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Bonus - Locked");
    ASSERT_NUM_EQUALS(list->buckets[2].num_achievements, 3);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[0]->id, 7);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[1]->id, 8);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[2]->id, 9);

    rc_client_destroy_achievement_list(list);
  }

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_UNOFFICIAL, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNOFFICIAL);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 1234);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Sample Game - Unofficial");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 5502);

    rc_client_destroy_achievement_list(list);
  }

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 4);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 1234);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Sample Game - Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 5501);
    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNOFFICIAL);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 1234);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Sample Game - Unofficial");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5502);
    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 1234);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Sample Game - Unsupported");
    ASSERT_NUM_EQUALS(list->buckets[2].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[0]->id, 5503);
    ASSERT_NUM_EQUALS(list->buckets[3].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[3].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[3].label, "Bonus - Locked");
    ASSERT_NUM_EQUALS(list->buckets[3].num_achievements, 3);
    ASSERT_NUM_EQUALS(list->buckets[3].achievements[0]->id, 7);
    ASSERT_NUM_EQUALS(list->buckets[3].achievements[1]->id, 8);
    ASSERT_NUM_EQUALS(list->buckets[3].achievements[2]->id, 9);

    rc_client_destroy_achievement_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_achievement_list_subset_buckets(void)
{
  rc_client_achievement_list_t* list;
  rc_client_achievement_t** iter;
  rc_client_achievement_t* achievement;

  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, unlock_8, unlock_8);
  mock_client_load_subset(patchdata_subset2, unlock_5502, unlock_5502);
  mock_memory(memory, sizeof(memory));

  rc_client_do_frame(g_client); /* advance achievements out of waiting state */
  event_count = 0;

  mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=5&h=1&m=0123456789ABCDEF&v=732f8e30e9c1eb08948dda098c305d8b",
      "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":5,\"AchievementsRemaining\":6}");
  mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=5501&h=1&m=0123456789ABCDEF&v=9b9bdf5501eb6289a6655affbcc695e6",
      "{\"Success\":true,\"Score\":5437,\"SoftcoreScore\":777,\"AchievementID\":5,\"AchievementsRemaining\":6}");

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 4);

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 1234);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Sample Game - Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 6);
    iter = list->buckets[0].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 6);
    ASSERT_STR_EQUALS(achievement->measured_progress, "");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 0.0);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 7);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 9);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 70);
    ASSERT_STR_EQUALS(achievement->measured_progress, "");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 0.0);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 71);
    ASSERT_STR_EQUALS(achievement->measured_progress, "");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 0.0);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 1234);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Sample Game - Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 8);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Multi - Locked");
    ASSERT_NUM_EQUALS(list->buckets[2].num_achievements, 2);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[0]->id, 5501);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[1]->id, 5503);

    ASSERT_NUM_EQUALS(list->buckets[3].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[3].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[3].label, "Multi - Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[3].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[3].achievements[0]->id, 5502);

    rc_client_destroy_achievement_list(list);
  }

  memory[5] = 5; /* trigger achievement 5 */
  memory[6] = 2; /* start measuring achievement 6 */
  memory[1] = 1; /* begin challenge achievement 7 */
  memory[0x11] = 100; /* start measuring achievements 70 and 71 */
  memory[0x17] = 7; /* trigger achievement 5501 */
  rc_client_do_frame(g_client);
  event_count = 0;

  /* set the unlock time for achievement 5 back one second to ensure consistent sorting */
  ((rc_client_achievement_t*)rc_client_get_achievement_info(g_client, 5))->unlock_time--;

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 6);

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Active Challenges");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 7);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Recently Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 2);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5501);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[1]->id, 5);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 1234);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Sample Game - Locked");
    ASSERT_NUM_EQUALS(list->buckets[2].num_achievements, 4);
    iter = list->buckets[2].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 6);
    ASSERT_STR_EQUALS(achievement->measured_progress, "2/6");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 33.333333);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 9);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 70);
    ASSERT_STR_EQUALS(achievement->measured_progress, "25600/100000");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 25.6);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 71);
    ASSERT_STR_EQUALS(achievement->measured_progress, "25%");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 25.6);

    ASSERT_NUM_EQUALS(list->buckets[3].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[3].subset_id, 1234);
    ASSERT_STR_EQUALS(list->buckets[3].label, "Sample Game - Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[3].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[3].achievements[0]->id, 8);

    ASSERT_NUM_EQUALS(list->buckets[4].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[4].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[4].label, "Multi - Locked");
    ASSERT_NUM_EQUALS(list->buckets[4].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[4].achievements[0]->id, 5503);

    ASSERT_NUM_EQUALS(list->buckets[5].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[5].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[5].label, "Multi - Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[5].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[5].achievements[0]->id, 5502);

    rc_client_destroy_achievement_list(list);
  }

  /* recently unlocked achievements no longer recent */
  ((rc_client_achievement_t*)rc_client_get_achievement_info(g_client, 5))->unlock_time -= 15 * 60;
  ((rc_client_achievement_t*)rc_client_get_achievement_info(g_client, 5501))->unlock_time -= 15 * 60;
  memory[6] = 5; /* almost there achievement 6 */
  memory[1] = 0; /* stop challenge achievement 7 */
  rc_client_do_frame(g_client);
  event_count = 0;

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 5);

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Almost There");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 6);
    ASSERT_STR_EQUALS(list->buckets[0].achievements[0]->measured_progress, "5/6");
    ASSERT_FLOAT_EQUALS(list->buckets[0].achievements[0] ->measured_percent, 83.333333);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 1234);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Sample Game - Locked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 4);
    iter = list->buckets[1].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 7);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 9);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 70);
    ASSERT_STR_EQUALS(achievement->measured_progress, "25600/100000");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 25.6);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 71);
    ASSERT_STR_EQUALS(achievement->measured_progress, "25%");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 25.6);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 1234);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Sample Game - Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[2].num_achievements, 2);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[0]->id, 5);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[1]->id, 8);

    ASSERT_NUM_EQUALS(list->buckets[3].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[3].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[3].label, "Multi - Locked");
    ASSERT_NUM_EQUALS(list->buckets[3].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[3].achievements[0]->id, 5503);

    ASSERT_NUM_EQUALS(list->buckets[4].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[4].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[4].label, "Multi - Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[4].num_achievements, 2);
    ASSERT_NUM_EQUALS(list->buckets[4].achievements[0]->id, 5501);
    ASSERT_NUM_EQUALS(list->buckets[4].achievements[1]->id, 5502);

    rc_client_destroy_achievement_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_achievement_list_subset_buckets_subset_first(void)
{
  rc_client_achievement_list_t* list;
  rc_client_achievement_t** iter;
  rc_client_achievement_t* achievement;

  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_logged_in();
  reset_mock_api_handlers();
  mock_api_response("r=gameid&m=0123456789ABCDEF", "{\"Success\":true,\"GameID\":2345}");
  mock_api_response("r=patch&u=Username&t=ApiToken&g=2345", patchdata_subset2);
  mock_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=2345&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=2345&h=0", unlock_5502);
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=2345&h=1", unlock_5502);
  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_success, g_callback_userdata);

  mock_api_response("r=patch&u=Username&t=ApiToken&g=1234", patchdata_exhaustive);
  mock_api_response("r=postactivity&u=Username&t=ApiToken&a=3&m=1234&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=0", unlock_8);
  mock_api_response("r=unlocks&u=Username&t=ApiToken&g=1234&h=1", unlock_8);
  rc_client_begin_load_subset(g_client, 1234, rc_client_callback_expect_success, g_callback_userdata);

  mock_memory(memory, sizeof(memory));

  rc_client_do_frame(g_client); /* advance achievements out of waiting state */
  event_count = 0;

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 4);

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Multi - Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 5501);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[1]->id, 5503);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Multi - Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5502);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 1234);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Sample Game - Locked");
    ASSERT_NUM_EQUALS(list->buckets[2].num_achievements, 6);
    iter = list->buckets[2].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 6);
    ASSERT_STR_EQUALS(achievement->measured_progress, "");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 0.0);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 7);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 9);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 70);
    ASSERT_STR_EQUALS(achievement->measured_progress, "");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 0.0);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 71);
    ASSERT_STR_EQUALS(achievement->measured_progress, "");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 0.0);

    ASSERT_NUM_EQUALS(list->buckets[3].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[3].subset_id, 1234);
    ASSERT_STR_EQUALS(list->buckets[3].label, "Sample Game - Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[3].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[3].achievements[0]->id, 8);

    rc_client_destroy_achievement_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_achievement_get_image_url(void)
{
  char buffer[256];
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks, no_unlocks);

  ASSERT_NUM_EQUALS(rc_client_achievement_get_image_url(rc_client_get_achievement_info(g_client, 5501),
      RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED, buffer, sizeof(buffer)), RC_OK);
  ASSERT_STR_EQUALS(buffer, "https://media.retroachievements.org/Badge/00234.png");

  ASSERT_NUM_EQUALS(rc_client_achievement_get_image_url(rc_client_get_achievement_info(g_client, 5501),
      RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE, buffer, sizeof(buffer)), RC_OK);
  ASSERT_STR_EQUALS(buffer, "https://media.retroachievements.org/Badge/00234_lock.png");

  ASSERT_NUM_EQUALS(rc_client_achievement_get_image_url(rc_client_get_achievement_info(g_client, 5501),
      RC_CLIENT_ACHIEVEMENT_STATE_DISABLED, buffer, sizeof(buffer)), RC_OK);
  ASSERT_STR_EQUALS(buffer, "https://media.retroachievements.org/Badge/00234_lock.png");

  ASSERT_NUM_EQUALS(rc_client_achievement_get_image_url(rc_client_get_achievement_info(g_client, 5501),
      RC_CLIENT_ACHIEVEMENT_STATE_INACTIVE, buffer, sizeof(buffer)), RC_OK);
  ASSERT_STR_EQUALS(buffer, "https://media.retroachievements.org/Badge/00234_lock.png");

  rc_client_destroy(g_client);
}

/* ----- leaderboards ----- */

static void test_leaderboard_list_simple(void)
{
  rc_client_leaderboard_list_t* list;
  rc_client_leaderboard_t** iter;
  rc_client_leaderboard_t* leaderboard;
  uint8_t memory[16] = { 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0 };

  g_client = mock_client_logged_in();
  mock_memory(memory, sizeof(memory));
  mock_client_load_game(patchdata_exhaustive, no_unlocks, no_unlocks);

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_NONE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_ALL);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "All");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 7);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 45);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 48);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 51);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 52);

    rc_client_destroy_leaderboard_list(list);
  }

  memory[0x0A] = 1; /* start 45,46,47 */
  rc_client_do_frame(g_client);

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_NONE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_ALL);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "All");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 7);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 45);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 48);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 51);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 52);

    rc_client_destroy_leaderboard_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_leaderboard_list_simple_with_unsupported(void)
{
  rc_client_leaderboard_list_t* list;
  rc_client_leaderboard_t** iter;
  rc_client_leaderboard_t* leaderboard;
  uint8_t memory[16] = { 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0 };

  g_client = mock_client_logged_in();
  mock_memory(memory, 0x0E); /* 0x0E address is now invalid (44,45,46,47,48)*/
  mock_client_load_game(patchdata_exhaustive, no_unlocks, no_unlocks);

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_NONE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_ALL);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "All");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 2);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 51);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 52);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_UNSUPPORTED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unsupported");
    ASSERT_NUM_EQUALS(list->buckets[1].num_leaderboards, 5);

    iter = list->buckets[1].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 45);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 48);

    rc_client_destroy_leaderboard_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_leaderboard_list_buckets(void)
{
  rc_client_leaderboard_list_t* list;
  rc_client_leaderboard_t** iter;
  rc_client_leaderboard_t* leaderboard;
  uint8_t memory[16] = { 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0 };

  g_client = mock_client_logged_in();
  mock_memory(memory, sizeof(memory));
  mock_client_load_game(patchdata_exhaustive, no_unlocks, no_unlocks);

  rc_client_do_frame(g_client);

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_TRACKING);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Inactive");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 7);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 45);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 48);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 51);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 52);

    rc_client_destroy_leaderboard_list(list);
  }

  memory[0x0A] = 1; /* start 45,46,47 */
  rc_client_do_frame(g_client);

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_TRACKING);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_ACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Active");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 3);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 45);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Inactive");
    ASSERT_NUM_EQUALS(list->buckets[1].num_leaderboards, 4);

    iter = list->buckets[1].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 48);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 51);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 52);

    rc_client_destroy_leaderboard_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_leaderboard_list_buckets_with_unsupported(void)
{
  rc_client_leaderboard_list_t* list;
  rc_client_leaderboard_t** iter;
  rc_client_leaderboard_t* leaderboard;
  uint8_t memory[16] = { 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0 };

  g_client = mock_client_logged_in();
  mock_memory(memory, 0x0E); /* 0x0E address is now invalid (44,45,46,47,48)*/
  mock_client_load_game(patchdata_exhaustive, no_unlocks, no_unlocks);

  rc_client_do_frame(g_client);

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_TRACKING);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Inactive");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 2);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 51);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 52);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_UNSUPPORTED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unsupported");
    ASSERT_NUM_EQUALS(list->buckets[1].num_leaderboards, 5);

    iter = list->buckets[1].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 45);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 48);

    rc_client_destroy_leaderboard_list(list);
  }

  memory[0x0B] = 3; /* start 52 */
  rc_client_do_frame(g_client);

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_TRACKING);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 3);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_ACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Active");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 1);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 52);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Inactive");
    ASSERT_NUM_EQUALS(list->buckets[1].num_leaderboards, 1);

    iter = list->buckets[1].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 51);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_UNSUPPORTED);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Unsupported");
    ASSERT_NUM_EQUALS(list->buckets[2].num_leaderboards, 5);

    iter = list->buckets[2].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 45);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 48);

    rc_client_destroy_leaderboard_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_leaderboard_list_subset(void)
{
  rc_client_leaderboard_list_t* list;
  rc_client_leaderboard_t** iter;
  rc_client_leaderboard_t* leaderboard;
  uint8_t memory[16] = { 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0 };

  g_client = mock_client_logged_in();
  mock_memory(memory, sizeof(memory));
  mock_client_load_game(patchdata_exhaustive, no_unlocks, no_unlocks);
  mock_client_load_subset(patchdata_subset, no_unlocks, no_unlocks);

  rc_client_do_frame(g_client);

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_TRACKING);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 1234);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Sample Game - Inactive");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 7);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 45);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 48);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 51);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 52);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Bonus - Inactive");
    ASSERT_NUM_EQUALS(list->buckets[1].num_leaderboards, 2);

    iter = list->buckets[1].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 81);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 82);

    rc_client_destroy_leaderboard_list(list);
  }

  memory[0x0A] = 1; /* start 45,46,47 */
  memory[0x08] = 2; /* start 82 */
  rc_client_do_frame(g_client);

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_TRACKING);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 3);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_ACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Active");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 4);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 45);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 82);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 1234);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Sample Game - Inactive");
    ASSERT_NUM_EQUALS(list->buckets[1].num_leaderboards, 4);

    iter = list->buckets[1].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 48);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 51);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 52);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Bonus - Inactive");
    ASSERT_NUM_EQUALS(list->buckets[2].num_leaderboards, 1);

    iter = list->buckets[2].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 81);

    rc_client_destroy_leaderboard_list(list);
  }

  rc_client_destroy(g_client);
}

static const char* lbinfo_4401_top_10 = "{\"Success\":true,\"LeaderboardData\":{\"LBID\":4401,\"GameID\":1234,"
    "\"LowerIsBetter\":1,\"LBTitle\":\"Leaderboard1\",\"LBDesc\":\"Desc1\",\"LBFormat\":\"SCORE\","
    "\"LBMem\":\"STA:0xH000C=1::CAN:0xH000D=1::SUB:0xH000D=2::VAL:0x 000E\",\"LBAuthor\":null,"
    "\"LBCreated\":\"2013-10-20 22:12:21\",\"LBUpdated\":\"2021-06-14 08:18:19\","
    "\"Entries\":["
      "{\"User\":\"PlayerG\",\"Score\":3524,\"Rank\":1,\"Index\":1,\"DateSubmitted\":1615654895},"
      "{\"User\":\"PlayerB\",\"Score\":3645,\"Rank\":2,\"Index\":2,\"DateSubmitted\":1615634566},"
      "{\"User\":\"DisplayName\",\"Score\":3754,\"Rank\":3,\"Index\":3,\"DateSubmitted\":1615234553},"
      "{\"User\":\"PlayerC\",\"Score\":3811,\"Rank\":4,\"Index\":4,\"DateSubmitted\":1615653844},"
      "{\"User\":\"PlayerF\",\"Score\":3811,\"Rank\":4,\"Index\":5,\"DateSubmitted\":1615623878},"
      "{\"User\":\"PlayerA\",\"Score\":3811,\"Rank\":4,\"Index\":6,\"DateSubmitted\":1615653284},"
      "{\"User\":\"PlayerI\",\"Score\":3902,\"Rank\":7,\"Index\":7,\"DateSubmitted\":1615632174},"
      "{\"User\":\"PlayerE\",\"Score\":3956,\"Rank\":8,\"Index\":8,\"DateSubmitted\":1616384834},"
      "{\"User\":\"PlayerD\",\"Score\":3985,\"Rank\":9,\"Index\":9,\"DateSubmitted\":1615238383},"
      "{\"User\":\"PlayerH\",\"Score\":4012,\"Rank\":10,\"Index\":10,\"DateSubmitted\":1615638984}"
    "]"
  "}}";

static const char* lbinfo_4401_top_10_no_user = "{\"Success\":true,\"LeaderboardData\":{\"LBID\":4401,\"GameID\":1234,"
    "\"LowerIsBetter\":1,\"LBTitle\":\"Leaderboard1\",\"LBDesc\":\"Desc1\",\"LBFormat\":\"SCORE\","
    "\"LBMem\":\"STA:0xH000C=1::CAN:0xH000D=1::SUB:0xH000D=2::VAL:0x 000E\",\"LBAuthor\":null,"
    "\"LBCreated\":\"2013-10-20 22:12:21\",\"LBUpdated\":\"2021-06-14 08:18:19\","
    "\"Entries\":["
      "{\"User\":\"PlayerG\",\"Score\":3524,\"Rank\":1,\"Index\":1,\"DateSubmitted\":1615654895},"
      "{\"User\":\"PlayerB\",\"Score\":3645,\"Rank\":2,\"Index\":2,\"DateSubmitted\":1615634566},"
      "{\"User\":\"PlayerJ\",\"Score\":3754,\"Rank\":3,\"Index\":3,\"DateSubmitted\":1615234553},"
      "{\"User\":\"PlayerC\",\"Score\":3811,\"Rank\":4,\"Index\":4,\"DateSubmitted\":1615653844},"
      "{\"User\":\"PlayerF\",\"Score\":3811,\"Rank\":4,\"Index\":5,\"DateSubmitted\":1615623878},"
      "{\"User\":\"PlayerA\",\"Score\":3811,\"Rank\":4,\"Index\":6,\"DateSubmitted\":1615653284},"
      "{\"User\":\"PlayerI\",\"Score\":3902,\"Rank\":7,\"Index\":7,\"DateSubmitted\":1615632174},"
      "{\"User\":\"PlayerE\",\"Score\":3956,\"Rank\":8,\"Index\":8,\"DateSubmitted\":1616384834},"
      "{\"User\":\"PlayerD\",\"Score\":3985,\"Rank\":9,\"Index\":9,\"DateSubmitted\":1615238383},"
      "{\"User\":\"PlayerH\",\"Score\":4012,\"Rank\":10,\"Index\":10,\"DateSubmitted\":1615638984}"
    "]"
  "}}";

static const char* lbinfo_4401_near_user = "{\"Success\":true,\"LeaderboardData\":{\"LBID\":4401,\"GameID\":1234,"
    "\"LowerIsBetter\":1,\"LBTitle\":\"Leaderboard1\",\"LBDesc\":\"Desc1\",\"LBFormat\":\"SCORE\","
    "\"LBMem\":\"STA:0xH000C=1::CAN:0xH000D=1::SUB:0xH000D=2::VAL:0x 000E\",\"LBAuthor\":null,"
    "\"LBCreated\":\"2013-10-20 22:12:21\",\"LBUpdated\":\"2021-06-14 08:18:19\","
    "\"Entries\":["
      "{\"User\":\"PlayerG\",\"Score\":3524,\"Rank\":17,\"Index\":17,\"DateSubmitted\":1615654895},"
      "{\"User\":\"PlayerB\",\"Score\":3645,\"Rank\":18,\"Index\":18,\"DateSubmitted\":1615634566},"
      "{\"User\":\"PlayerC\",\"Score\":3811,\"Rank\":19,\"Index\":19,\"DateSubmitted\":1615653844},"
      "{\"User\":\"PlayerF\",\"Score\":3811,\"Rank\":19,\"Index\":20,\"DateSubmitted\":1615623878},"
      "{\"User\":\"DisplayName\",\"Score\":3811,\"Rank\":19,\"Index\":21,\"DateSubmitted\":1615234553},"
      "{\"User\":\"PlayerA\",\"Score\":3811,\"Rank\":19,\"Index\":22,\"DateSubmitted\":1615653284},"
      "{\"User\":\"PlayerI\",\"Score\":3902,\"Rank\":23,\"Index\":23,\"DateSubmitted\":1615632174},"
      "{\"User\":\"PlayerE\",\"Score\":3956,\"Rank\":24,\"Index\":24,\"DateSubmitted\":1616384834},"
      "{\"User\":\"PlayerD\",\"Score\":3985,\"Rank\":25,\"Index\":25,\"DateSubmitted\":1615238383},"
      "{\"User\":\"PlayerH\",\"Score\":4012,\"Rank\":26,\"Index\":26,\"DateSubmitted\":1615638984}"
    "]"
  "}}";

static rc_client_leaderboard_entry_list_t* g_leaderboard_entries = NULL;
static void rc_client_callback_expect_leaderboard_entry_list(int result, const char* error_message, rc_client_leaderboard_entry_list_t* list, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_OK);
  ASSERT_PTR_NULL(error_message);
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);

  ASSERT_PTR_NOT_NULL(list);
  g_leaderboard_entries = list;
}

static void test_fetch_leaderboard_entries(void)
{
  rc_client_leaderboard_entry_t* entry;
  char url[256];

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks, no_unlocks);
  g_leaderboard_entries = NULL;

  mock_api_response("r=lbinfo&i=4401&c=10", lbinfo_4401_top_10);

  rc_client_begin_fetch_leaderboard_entries(g_client, 4401, 1, 10,
      rc_client_callback_expect_leaderboard_entry_list, g_callback_userdata);
  ASSERT_PTR_NOT_NULL(g_leaderboard_entries);

  ASSERT_NUM_EQUALS(g_leaderboard_entries->num_entries, 10);

  entry = g_leaderboard_entries->entries;
  ASSERT_STR_EQUALS(entry->user, "PlayerG");
  ASSERT_STR_EQUALS(entry->display, "003524");
  ASSERT_NUM_EQUALS(entry->index, 1);
  ASSERT_NUM_EQUALS(entry->rank, 1);
  ASSERT_NUM_EQUALS(entry->submitted, 1615654895);

  ASSERT_NUM_EQUALS(rc_client_leaderboard_entry_get_user_image_url(entry, url, sizeof(url)), RC_OK);
  ASSERT_STR_EQUALS(url, "https://media.retroachievements.org/UserPic/PlayerG.png");

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerB");
  ASSERT_STR_EQUALS(entry->display, "003645");
  ASSERT_NUM_EQUALS(entry->index, 2);
  ASSERT_NUM_EQUALS(entry->rank, 2);
  ASSERT_NUM_EQUALS(entry->submitted, 1615634566);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "DisplayName");
  ASSERT_STR_EQUALS(entry->display, "003754");
  ASSERT_NUM_EQUALS(entry->index, 3);
  ASSERT_NUM_EQUALS(entry->rank, 3);
  ASSERT_NUM_EQUALS(entry->submitted, 1615234553);

  ASSERT_NUM_EQUALS(rc_client_leaderboard_entry_get_user_image_url(entry, url, sizeof(url)), RC_OK);
  ASSERT_STR_EQUALS(url, "https://media.retroachievements.org/UserPic/DisplayName.png");

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerC");
  ASSERT_STR_EQUALS(entry->display, "003811");
  ASSERT_NUM_EQUALS(entry->index, 4);
  ASSERT_NUM_EQUALS(entry->rank, 4);
  ASSERT_NUM_EQUALS(entry->submitted, 1615653844);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerF");
  ASSERT_STR_EQUALS(entry->display, "003811");
  ASSERT_NUM_EQUALS(entry->index, 5);
  ASSERT_NUM_EQUALS(entry->rank, 4);
  ASSERT_NUM_EQUALS(entry->submitted, 1615623878);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerA");
  ASSERT_STR_EQUALS(entry->display, "003811");
  ASSERT_NUM_EQUALS(entry->index, 6);
  ASSERT_NUM_EQUALS(entry->rank, 4);
  ASSERT_NUM_EQUALS(entry->submitted, 1615653284);

  ASSERT_NUM_EQUALS(rc_client_leaderboard_entry_get_user_image_url(entry, url, sizeof(url)), RC_OK);
  ASSERT_STR_EQUALS(url, "https://media.retroachievements.org/UserPic/PlayerA.png");

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerI");
  ASSERT_STR_EQUALS(entry->display, "003902");
  ASSERT_NUM_EQUALS(entry->index, 7);
  ASSERT_NUM_EQUALS(entry->rank, 7);
  ASSERT_NUM_EQUALS(entry->submitted, 1615632174);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerE");
  ASSERT_STR_EQUALS(entry->display, "003956");
  ASSERT_NUM_EQUALS(entry->index, 8);
  ASSERT_NUM_EQUALS(entry->rank, 8);
  ASSERT_NUM_EQUALS(entry->submitted, 1616384834);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerD");
  ASSERT_STR_EQUALS(entry->display, "003985");
  ASSERT_NUM_EQUALS(entry->index, 9);
  ASSERT_NUM_EQUALS(entry->rank, 9);
  ASSERT_NUM_EQUALS(entry->submitted, 1615238383);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerH");
  ASSERT_STR_EQUALS(entry->display, "004012");
  ASSERT_NUM_EQUALS(entry->index, 10);
  ASSERT_NUM_EQUALS(entry->rank, 10);
  ASSERT_NUM_EQUALS(entry->submitted, 1615638984);

  ASSERT_NUM_EQUALS(g_leaderboard_entries->user_index, 2);

  rc_client_destroy_leaderboard_entry_list(g_leaderboard_entries);
  rc_client_destroy(g_client);
}

static void test_fetch_leaderboard_entries_no_user(void)
{
  rc_client_leaderboard_entry_t* entry;

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks, no_unlocks);
  g_leaderboard_entries = NULL;

  mock_api_response("r=lbinfo&i=4401&c=10", lbinfo_4401_top_10_no_user);

  rc_client_begin_fetch_leaderboard_entries(g_client, 4401, 1, 10,
      rc_client_callback_expect_leaderboard_entry_list, g_callback_userdata);
  ASSERT_PTR_NOT_NULL(g_leaderboard_entries);

  ASSERT_NUM_EQUALS(g_leaderboard_entries->num_entries, 10);

  entry = g_leaderboard_entries->entries;
  ASSERT_STR_EQUALS(entry->user, "PlayerG");
  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerB");
  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerJ");
  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerC");
  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerF");
  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerA");
  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerI");
  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerE");
  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerD");
  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerH");

  ASSERT_NUM_EQUALS(g_leaderboard_entries->user_index, -1);

  rc_client_destroy_leaderboard_entry_list(g_leaderboard_entries);
  rc_client_destroy(g_client);
}

static void test_fetch_leaderboard_entries_around_user(void)
{
  rc_client_leaderboard_entry_t* entry;

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks, no_unlocks);
  g_leaderboard_entries = NULL;

  mock_api_response("r=lbinfo&i=4401&u=Username&c=10", lbinfo_4401_near_user);

  rc_client_begin_fetch_leaderboard_entries_around_user(g_client, 4401, 10,
      rc_client_callback_expect_leaderboard_entry_list, g_callback_userdata);
  ASSERT_PTR_NOT_NULL(g_leaderboard_entries);

  ASSERT_NUM_EQUALS(g_leaderboard_entries->num_entries, 10);

  entry = g_leaderboard_entries->entries;
  ASSERT_STR_EQUALS(entry->user, "PlayerG");
  ASSERT_STR_EQUALS(entry->display, "003524");
  ASSERT_NUM_EQUALS(entry->index, 17);
  ASSERT_NUM_EQUALS(entry->rank, 17);
  ASSERT_NUM_EQUALS(entry->submitted, 1615654895);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerB");
  ASSERT_STR_EQUALS(entry->display, "003645");
  ASSERT_NUM_EQUALS(entry->index, 18);
  ASSERT_NUM_EQUALS(entry->rank, 18);
  ASSERT_NUM_EQUALS(entry->submitted, 1615634566);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerC");
  ASSERT_STR_EQUALS(entry->display, "003811");
  ASSERT_NUM_EQUALS(entry->index, 19);
  ASSERT_NUM_EQUALS(entry->rank, 19);
  ASSERT_NUM_EQUALS(entry->submitted, 1615653844);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerF");
  ASSERT_STR_EQUALS(entry->display, "003811");
  ASSERT_NUM_EQUALS(entry->index, 20);
  ASSERT_NUM_EQUALS(entry->rank, 19);
  ASSERT_NUM_EQUALS(entry->submitted, 1615623878);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "DisplayName");
  ASSERT_STR_EQUALS(entry->display, "003811");
  ASSERT_NUM_EQUALS(entry->index, 21);
  ASSERT_NUM_EQUALS(entry->rank, 19);
  ASSERT_NUM_EQUALS(entry->submitted, 1615234553);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerA");
  ASSERT_STR_EQUALS(entry->display, "003811");
  ASSERT_NUM_EQUALS(entry->index, 22);
  ASSERT_NUM_EQUALS(entry->rank, 19);
  ASSERT_NUM_EQUALS(entry->submitted, 1615653284);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerI");
  ASSERT_STR_EQUALS(entry->display, "003902");
  ASSERT_NUM_EQUALS(entry->index, 23);
  ASSERT_NUM_EQUALS(entry->rank, 23);
  ASSERT_NUM_EQUALS(entry->submitted, 1615632174);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerE");
  ASSERT_STR_EQUALS(entry->display, "003956");
  ASSERT_NUM_EQUALS(entry->index, 24);
  ASSERT_NUM_EQUALS(entry->rank, 24);
  ASSERT_NUM_EQUALS(entry->submitted, 1616384834);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerD");
  ASSERT_STR_EQUALS(entry->display, "003985");
  ASSERT_NUM_EQUALS(entry->index, 25);
  ASSERT_NUM_EQUALS(entry->rank, 25);
  ASSERT_NUM_EQUALS(entry->submitted, 1615238383);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerH");
  ASSERT_STR_EQUALS(entry->display, "004012");
  ASSERT_NUM_EQUALS(entry->index, 26);
  ASSERT_NUM_EQUALS(entry->rank, 26);
  ASSERT_NUM_EQUALS(entry->submitted, 1615638984);

  ASSERT_NUM_EQUALS(g_leaderboard_entries->user_index, 4);

  rc_client_destroy_leaderboard_entry_list(g_leaderboard_entries);
  rc_client_destroy(g_client);
}

static void rc_client_callback_expect_leaderboard_entry_list_login_required(int result, const char* error_message,
    rc_client_leaderboard_entry_list_t* list, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_LOGIN_REQUIRED);
  ASSERT_STR_EQUALS(error_message, "Login required");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
  ASSERT_PTR_NULL(list);
}

static void test_fetch_leaderboard_entries_around_user_not_logged_in(void)
{
  g_client = mock_client_not_logged_in();
  g_leaderboard_entries = NULL;

  mock_api_response("r=lbinfo&i=4401&u=Username&c=10", lbinfo_4401_near_user);

  rc_client_begin_fetch_leaderboard_entries_around_user(g_client, 4401, 10,
      rc_client_callback_expect_leaderboard_entry_list_login_required, g_callback_userdata);
  ASSERT_PTR_NULL(g_leaderboard_entries);

  assert_api_not_called("r=lbinfo&i=4401&u=Username&c=10");

  rc_client_destroy(g_client);
}

static void rc_client_callback_expect_leaderboard_uncalled(int result, const char* error_message,
  rc_client_leaderboard_entry_list_t* list, rc_client_t* client, void* callback_userdata)
{
  ASSERT_FAIL("Callback should not have been called.")
}

static void test_fetch_leaderboard_entries_aborted(void)
{
  rc_client_async_handle_t* handle;

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks, no_unlocks);
  g_client->callbacks.server_call = rc_client_server_call_async;

  g_leaderboard_entries = NULL;

  handle = rc_client_begin_fetch_leaderboard_entries(g_client, 4401, 1, 10,
    rc_client_callback_expect_leaderboard_uncalled, g_callback_userdata);

  rc_client_abort_async(g_client, handle);

  async_api_response("r=lbinfo&i=4401&c=10", lbinfo_4401_top_10);
  ASSERT_PTR_NULL(g_leaderboard_entries);

  rc_client_destroy(g_client);
}

/* ----- do frame ----- */

static void test_do_frame_bounds_check_system(void)
{
  const uint32_t memory_size = 0x10010; /* provide more memory than system expects */
  uint8_t* memory = (uint8_t*)calloc(1, memory_size);
  ASSERT_PTR_NOT_NULL(memory);

  g_client = mock_client_game_loaded(patchdata_bounds_check_system, no_unlocks, no_unlocks);

  mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=7&h=1&m=0123456789ABCDEF&v=c39308ba325ba4a72919b081fb18fdd4",
    "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":7,\"AchievementsRemaining\":4}");

  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_NUM_EQUALS(g_client->game->max_valid_address, 0xFFFF);

  assert_achievement_state(g_client, 1, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  assert_achievement_state(g_client, 2, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  assert_achievement_state(g_client, 3, RC_CLIENT_ACHIEVEMENT_STATE_DISABLED); /* 0x10000 out of range for system */
  assert_achievement_state(g_client, 4, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  assert_achievement_state(g_client, 5, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE); /* cannot read two bytes from 0xFFFF, but size isn't enforced until do_frame */
  assert_achievement_state(g_client, 6, RC_CLIENT_ACHIEVEMENT_STATE_DISABLED); /* 0x10000 out of range for system */
  assert_achievement_state(g_client, 7, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);

  /* verify that reading at the edge of the memory bounds fails */
  mock_memory(memory, 0x10000);
  rc_client_do_frame(g_client);
  assert_achievement_state(g_client, 5, RC_CLIENT_ACHIEVEMENT_STATE_DISABLED); /* cannot read two bytes from 0xFFFF */

  /* set up memory so achievement 7 would trigger if the pointed at address were valid */
  /* achievement should not trigger - invalid address should be ignored */
  memory[0x10000] = 5;
  memory[0x00000] = 1; /* byte(0xFFFF + byte(0x0000)) == 5 */
  rc_client_do_frame(g_client);
  assert_achievement_state(g_client, 7, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);

  /* even if the extra memory is available, it shouldn't try to read beyond the system defined max address */
  mock_memory(memory, memory_size);
  rc_client_do_frame(g_client);
  assert_achievement_state(g_client, 7, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);

  /* change max valid address so memory will be evaluated. achievement should trigger */
  g_client->game->max_valid_address = memory_size - 1;
  rc_client_do_frame(g_client);
  assert_achievement_state(g_client, 7, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);

  rc_client_destroy(g_client);
  free(memory);
}

static void test_do_frame_bounds_check_available(void)
{
  uint8_t memory[8] = { 0,0,0,0,0,0,0,0 };
  g_client = mock_client_game_loaded(patchdata_bounds_check_8, no_unlocks, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    /* all addresses are valid according to the system, so no achievements should be disabled yet. */
    assert_achievement_state(g_client, 808, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);

    /* limit the memory that's actually exposed and try to process a frame */
    mock_memory(memory, sizeof(memory));
    rc_client_do_frame(g_client);

    assert_achievement_state(g_client, 408, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_client, 508, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_client, 608, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_client, 708, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_client, 808, RC_CLIENT_ACHIEVEMENT_STATE_DISABLED); /* out of bounds*/

    assert_achievement_state(g_client, 416, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_client, 516, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_client, 616, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_client, 716, RC_CLIENT_ACHIEVEMENT_STATE_DISABLED); /* only one byte available */
    assert_achievement_state(g_client, 816, RC_CLIENT_ACHIEVEMENT_STATE_DISABLED); /* out of bounds*/

    assert_achievement_state(g_client, 424, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_client, 524, RC_CLIENT_ACHIEVEMENT_STATE_DISABLED); /* 24-bit read actually fetches 32-bits */
    assert_achievement_state(g_client, 624, RC_CLIENT_ACHIEVEMENT_STATE_DISABLED); /* only two bytes available */
    assert_achievement_state(g_client, 724, RC_CLIENT_ACHIEVEMENT_STATE_DISABLED); /* only one byte available */
    assert_achievement_state(g_client, 824, RC_CLIENT_ACHIEVEMENT_STATE_DISABLED); /* out of bounds*/

    assert_achievement_state(g_client, 432, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_client, 532, RC_CLIENT_ACHIEVEMENT_STATE_DISABLED); /* only three bytes available */
    assert_achievement_state(g_client, 632, RC_CLIENT_ACHIEVEMENT_STATE_DISABLED); /* only two bytes available */
    assert_achievement_state(g_client, 732, RC_CLIENT_ACHIEVEMENT_STATE_DISABLED); /* only one byte available */
    assert_achievement_state(g_client, 832, RC_CLIENT_ACHIEVEMENT_STATE_DISABLED); /* out of bounds*/
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_trigger(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=8&h=1&m=0123456789ABCDEF&v=da80b659c2b858e13ddd97077647b217",
        "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":11}");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[8] = 8;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 8);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 8));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 1);
    ASSERT_NUM_EQUALS(g_client->user.score, 5432);
    ASSERT_NUM_EQUALS(g_client->user.score_softcore, 777);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_trigger_already_awarded(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=8&h=1&m=0123456789ABCDEF&v=da80b659c2b858e13ddd97077647b217",
        "{\"Success\":false,\"Error\":\"User already has hardcore and regular achievements awarded.\",\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":11}");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[8] = 8;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 8);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 8));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 1);
    ASSERT_NUM_EQUALS(g_client->user.score, 5432);
    ASSERT_NUM_EQUALS(g_client->user.score_softcore, 777);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_trigger_server_error(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=8&h=1&m=0123456789ABCDEF&v=da80b659c2b858e13ddd97077647b217",
        "{\"Success\":false,\"Error\":\"Achievement not found\"}");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[8] = 8;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);

    /* achievement still counts as triggered */
    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 8);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 8));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 1);
    ASSERT_NUM_EQUALS(g_client->user.score, 12345 + 5); /* score will have been adjusted locally, but not from server */

    /* but an error should have been reported */
    event = find_event(RC_CLIENT_EVENT_SERVER_ERROR, 0);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_STR_EQUALS(event->server_error->api, "award_achievement");
    ASSERT_STR_EQUALS(event->server_error->error_message, "Achievement not found");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_trigger_while_spectating(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    ASSERT_FALSE(rc_client_get_spectator_mode_enabled(g_client));
    rc_client_set_spectator_mode_enabled(g_client, 1);
    ASSERT_TRUE(rc_client_get_spectator_mode_enabled(g_client));

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=8&h=1&m=0123456789ABCDEF&v=da80b659c2b858e13ddd97077647b217",
        "{\"Success\":false,\"Error\":\"Achievement should not have been unlocked in spectating mode\"}");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[8] = 8;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    /* achievement still counts as triggered */
    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 8);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 8));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 1);
    ASSERT_NUM_EQUALS(g_client->user.score, 12345 + 5); /* score will have been adjusted locally, but not from server */

    /* expect API not called */
    assert_api_not_called("r=awardachievement&u=Username&t=ApiToken&a=8&h=1&m=0123456789ABCDEF&v=da80b659c2b858e13ddd97077647b217");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    rc_client_set_spectator_mode_enabled(g_client, 0);
    ASSERT_FALSE(rc_client_get_spectator_mode_enabled(g_client));
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_trigger_automatic_retry(void)
{
  const char* unlock_request_params = "r=awardachievement&u=Username&t=ApiToken&a=5501&h=1&m=0123456789ABCDEF&v=9b9bdf5501eb6289a6655affbcc695e6";
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks, no_unlocks);
  g_client->callbacks.server_call = rc_client_server_call_async;

  /* discard the queued ping to make finding the retry easier */
  g_client->state.scheduled_callbacks = NULL;

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[1] = 3;
    memory[2] = 7;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 5501);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 5501));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 1);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* first failure will immediately requeue the request */
    async_api_response(unlock_request_params, "");
    assert_api_pending(unlock_request_params);
    ASSERT_PTR_NULL(g_client->state.scheduled_callbacks);

    /* second failure will queue it */
    async_api_response(unlock_request_params, "");
    assert_api_call_count(unlock_request_params, 0);
    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);

    g_client->state.scheduled_callbacks->when = 0;
    rc_client_idle(g_client);
    assert_api_pending(unlock_request_params);
    ASSERT_PTR_NULL(g_client->state.scheduled_callbacks);

    /* third failure will requeue it */
    async_api_response(unlock_request_params, "");
    assert_api_call_count(unlock_request_params, 0);
    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);

    g_client->state.scheduled_callbacks->when = 0;
    rc_client_idle(g_client);
    assert_api_pending(unlock_request_params);
    ASSERT_PTR_NULL(g_client->state.scheduled_callbacks);

    /* success should not requeue it and update player score */
    async_api_response(unlock_request_params, "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":11}");
    ASSERT_PTR_NULL(g_client->state.scheduled_callbacks);

    ASSERT_NUM_EQUALS(g_client->user.score, 5432);
    ASSERT_NUM_EQUALS(g_client->user.score_softcore, 777);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_trigger_subset(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks, no_unlocks);
  mock_client_load_subset(patchdata_2ach_1lbd, no_unlocks, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=8&h=1&m=0123456789ABCDEF&v=da80b659c2b858e13ddd97077647b217",
        "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":11}");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[8] = 8;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 8);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 8));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 1);
    ASSERT_NUM_EQUALS(g_client->user.score, 5432);
    ASSERT_NUM_EQUALS(g_client->user.score_softcore, 777);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=5501&h=1&m=0123456789ABCDEF&v=9b9bdf5501eb6289a6655affbcc695e6",
        "{\"Success\":true,\"Score\":5437,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":11}");

    memory[1] = 3;
    memory[2] = 7;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 5501);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 5501));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 2);
    ASSERT_NUM_EQUALS(g_client->user.score, 5437);
    ASSERT_NUM_EQUALS(g_client->user.score_softcore, 777);
  }

  rc_client_destroy(g_client);
}


static void test_do_frame_achievement_measured(void)
{
  const rc_client_achievement_t* achievement;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=70&h=1&m=0123456789ABCDEF&v=61e40027573e2cde88b49d27f6804879",
        "{\"Success\":true,\"Score\":5432,\"AchievementID\":70,\"AchievementsRemaining\":11}");
    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=71&h=1&m=0123456789ABCDEF&v=3a8d55b81d391557d5111306599a2b0d",
        "{\"Success\":true,\"Score\":5432,\"AchievementID\":71,\"AchievementsRemaining\":11}");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[0x10] = 0x39; memory[0x11] = 0x30; /* 12345 */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1); /* one PROGRESS_INDICATOR_SHOW event */

    achievement = rc_client_get_achievement_info(g_client, 70);
    ASSERT_PTR_NOT_NULL(achievement);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_STR_EQUALS(achievement->measured_progress, "12345/100000");

    achievement = rc_client_get_achievement_info(g_client, 71);
    ASSERT_PTR_NOT_NULL(achievement);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_STR_EQUALS(achievement->measured_progress, "12%");

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* increment measured value - raw counter will report progress change, percentage will not */
    memory[0x10] = 0x3A; /* 12346 */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1); /* one PROGRESS_INDICATOR_SHOW event */

    achievement = rc_client_get_achievement_info(g_client, 70);
    ASSERT_PTR_NOT_NULL(achievement);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_STR_EQUALS(achievement->measured_progress, "12346/100000");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* increment measured value - raw counter will report progress change, percentage will not */
    memory[0x11] = 0x33; /* 13114 */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1); /* one PROGRESS_INDICATOR_SHOW event */

    achievement = rc_client_get_achievement_info(g_client, 70);
    ASSERT_PTR_NOT_NULL(achievement);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_STR_EQUALS(achievement->measured_progress, "13114/100000");

    achievement = rc_client_get_achievement_info(g_client, 71);
    ASSERT_PTR_NOT_NULL(achievement);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_STR_EQUALS(achievement->measured_progress, "13%");

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* trigger measured achievements - progress becomes blank */
    memory[0x10] = 0xA0; memory[0x11] = 0x86; memory[0x12] = 0x01; /* 100000 */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2); /* two TRIGGERED events, and no PROGRESS_INDICATOR_SHOW events */

    achievement = rc_client_get_achievement_info(g_client, 70);
    ASSERT_PTR_NOT_NULL(achievement);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_STR_EQUALS(achievement->measured_progress, "");

    achievement = rc_client_get_achievement_info(g_client, 71);
    ASSERT_PTR_NOT_NULL(achievement);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_STR_EQUALS(achievement->measured_progress, "");

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 2);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_measured_progress_event(void)
{
  rc_client_event_t* event;
  const rc_client_achievement_t* achievement;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=6&h=1&m=0123456789ABCDEF&v=65206f4290098ecd30c7845e895057d0",
        "{\"Success\":true,\"Score\":5432,\"AchievementID\":6,\"AchievementsRemaining\":11}");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[0x06] = 3;                         /* 3/6 */
    memory[0x11] = 0xC3; memory[0x10] = 0x4F; /* 49999/100000 */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    /* 3/6 = 50%, 49999/100000 = 49.999% */
    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW, 6);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 6));
    ASSERT_STR_EQUALS(event->achievement->measured_progress, "3/6");

    /* both achievements should have been updated, */
    achievement = rc_client_get_achievement_info(g_client, 6);
    ASSERT_STR_EQUALS(achievement->measured_progress, "3/6");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 50.0);

    achievement = rc_client_get_achievement_info(g_client, 70);
    ASSERT_STR_EQUALS(achievement->measured_progress, "49999/100000");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 49.999);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* any change will trigger the popup - even dropping */
    memory[0x10] = 0x4E; /* 49998 */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW, 70);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 70));
    ASSERT_STR_EQUALS(event->achievement->measured_progress, "49998/100000");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* don't trigger popup when value changes to 0 as the measured_progress string will be blank */
    memory[0x06] = 0; /* 0 */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    achievement = rc_client_get_achievement_info(g_client, 6);
    ASSERT_STR_EQUALS(achievement->measured_progress, "");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 0.0);

    /* both at 50%, only report first */
    memory[0x06] = 3;                         /* 3/6 */
    memory[0x11] = 0xC3; memory[0x10] = 0x50; /* 50000/100000 */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW, 6);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 6));
    ASSERT_STR_EQUALS(event->achievement->measured_progress, "3/6");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* second slightly ahead */
    memory[0x6] = 4;                                             /* 4/6 */
    memory[0x12] = 1;  memory[0x11] = 0x04; memory[0x10] = 0x6B; /* 66667/100000 */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW, 70);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 70));
    ASSERT_STR_EQUALS(event->achievement->measured_progress, "66667/100000");
    
    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* don't show popup on trigger */
    memory[0x06] = 6;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 6);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 6));
    ASSERT_STR_EQUALS(event->achievement->measured_progress, "");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_challenge_indicator(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=7&h=1&m=0123456789ABCDEF&v=c39308ba325ba4a72919b081fb18fdd4",
        "{\"Success\":true,\"Score\":5432,\"AchievementID\":7,\"AchievementsRemaining\":11}");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[1] = 1; /* show indicator */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 7));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[1] = 0; /* hide indicator */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE, 7);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 7));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[1] = 1; /* show indicator */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 7));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* trigger achievement - expect both hide and trigger events. both should have triggered achievement data */
    memory[7] = 7;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE, 7);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 7));

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 7);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 7));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 1);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_mastery(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks, no_unlocks);
  g_client->callbacks.server_call = rc_client_server_call_async;

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[8] = 8;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 8);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 8));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 1);
    ASSERT_NUM_EQUALS(g_client->user.score, 12345+5);
    ASSERT_NUM_EQUALS(g_client->user.score_softcore, 0);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    async_api_response("r=awardachievement&u=Username&t=ApiToken&a=8&h=1&m=0123456789ABCDEF&v=da80b659c2b858e13ddd97077647b217",
        "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":0}");

    ASSERT_NUM_EQUALS(event_count, 0);
    ASSERT_NUM_EQUALS(g_client->user.score, 5432);
    ASSERT_NUM_EQUALS(g_client->user.score_softcore, 777);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_GAME_COMPLETED, 1234);
    ASSERT_PTR_NOT_NULL(event);

    memory[9] = 9;
    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 9);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 9));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 2);
    ASSERT_NUM_EQUALS(g_client->user.score, 5432+5);
    ASSERT_NUM_EQUALS(g_client->user.score_softcore, 777);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    async_api_response("r=awardachievement&u=Username&t=ApiToken&a=9&h=1&m=0123456789ABCDEF&v=6d989ee0f408660a87d6440a13563bf6",
        "{\"Success\":false,\"Error\":\"User already has hardcore and regular achievements awarded.\",\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":9,\"AchievementsRemaining\":0}");

    ASSERT_NUM_EQUALS(event_count, 0);
    ASSERT_NUM_EQUALS(g_client->user.score, 5432);
    ASSERT_NUM_EQUALS(g_client->user.score_softcore, 777);

    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_mastery_encore(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks, no_unlocks);
  g_client->callbacks.server_call = rc_client_server_call_async;

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[8] = 8;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 8);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 8));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 1);
    ASSERT_NUM_EQUALS(g_client->user.score, 12345+5);
    ASSERT_NUM_EQUALS(g_client->user.score_softcore, 0);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    async_api_response("r=awardachievement&u=Username&t=ApiToken&a=8&h=1&m=0123456789ABCDEF&v=da80b659c2b858e13ddd97077647b217",
        "{\"Success\":false,\"Error\":\"User already has hardcore and regular achievements awarded.\",\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":0}");

    ASSERT_NUM_EQUALS(event_count, 0);
    ASSERT_NUM_EQUALS(g_client->user.score, 5432);
    ASSERT_NUM_EQUALS(g_client->user.score_softcore, 777);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_GAME_COMPLETED, 1234);
    ASSERT_PTR_NOT_NULL(event);

    memory[9] = 9;
    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 9);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 9));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 2);
    ASSERT_NUM_EQUALS(g_client->user.score, 5432+5);
    ASSERT_NUM_EQUALS(g_client->user.score_softcore, 777);

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    async_api_response("r=awardachievement&u=Username&t=ApiToken&a=9&h=1&m=0123456789ABCDEF&v=6d989ee0f408660a87d6440a13563bf6",
        "{\"Success\":false,\"Error\":\"User already has hardcore and regular achievements awarded.\",\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":9,\"AchievementsRemaining\":0}");

    ASSERT_NUM_EQUALS(event_count, 0);
    ASSERT_NUM_EQUALS(g_client->user.score, 5432);
    ASSERT_NUM_EQUALS(g_client->user.score_softcore, 777);

    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_leaderboard_started(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    mock_memory(memory, sizeof(memory));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    memory[0x0B] = 1;
    memory[0x0E] = 17;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 44);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000017");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000017");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_leaderboard_update(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    mock_memory(memory, sizeof(memory));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* start the leaderboard */
    memory[0x0B] = 1;
    memory[0x0E] = 17;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 44));
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* update the leaderboard */
    memory[0x0E] = 18;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000018");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_leaderboard_failed(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    mock_memory(memory, sizeof(memory));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* start the leaderboard */
    memory[0x0B] = 1;
    memory[0x0E] = 17;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 44));
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* cancel the leaderboard */
    memory[0x0C] = 1;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_FAILED, 44);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000017");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000017");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_leaderboard_submit(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=submitlbentry&u=Username&t=ApiToken&i=44&s=17&m=0123456789ABCDEF&v=a27fa205f7f30c8d13d74806ea5425b6",
        "{\"Success\":true,\"Response\":{\"Score\":17,\"BestScore\":23,"
        "\"TopEntries\":[{\"User\":\"Player1\",\"Score\":44,\"Rank\":1},{\"User\":\"Username\",\"Score\":23,\"Rank\":2}],"
        "\"RankInfo\":{\"Rank\":2,\"NumEntries\":\"2\"}}}");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* start the leaderboard */
    memory[0x0B] = 1;
    memory[0x0E] = 17;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 44));
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* submit the leaderboard */
    memory[0x0D] = 1;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED, 44);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000017");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000017");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_leaderboard_submit_server_error(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=submitlbentry&u=Username&t=ApiToken&i=44&s=17&m=0123456789ABCDEF&v=a27fa205f7f30c8d13d74806ea5425b6",
        "{\"Success\":false,\"Error\":\"Leaderboard not found\"}");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* start the leaderboard */
    memory[0x0B] = 1;
    memory[0x0E] = 17;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 44));
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* submit the leaderboard */
    memory[0x0D] = 1;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 3);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED, 44);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000017");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000017");

    /* an error should have also been reported */
    event = find_event(RC_CLIENT_EVENT_SERVER_ERROR, 0);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_STR_EQUALS(event->server_error->api, "submit_lboard_entry");
    ASSERT_STR_EQUALS(event->server_error->error_message, "Leaderboard not found");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_leaderboard_submit_while_spectating(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    mock_memory(memory, sizeof(memory));

    ASSERT_FALSE(rc_client_get_spectator_mode_enabled(g_client));
    rc_client_set_spectator_mode_enabled(g_client, 1);
    ASSERT_TRUE(rc_client_get_spectator_mode_enabled(g_client));

    mock_api_response("r=submitlbentry&u=Username&t=ApiToken&i=44&s=17&m=0123456789ABCDEF&v=a27fa205f7f30c8d13d74806ea5425b6",
        "{\"Success\":false,\"Error\":\"Leaderboard entry should not have been submitted in spectating mode\"}");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* start the leaderboard */
    memory[0x0B] = 1;
    memory[0x0E] = 17;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 44));
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* submit the leaderboard */
    memory[0x0D] = 1;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED, 44);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000017");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000017");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* expect API not called */
    assert_api_not_called("r=submitlbentry&u=Username&t=ApiToken&i=44&s=17&m=0123456789ABCDEF&v=a27fa205f7f30c8d13d74806ea5425b6");
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_leaderboard_tracker_sharing(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    mock_memory(memory, sizeof(memory));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* start one leaderboard (one tracker) */
    memory[0x0B] = 1;
    memory[0x0E] = 17;
    memory[0x0F] = 1;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 44);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000273");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000273");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* start additional leaderboards (45,46,47) - 45 and 46 should generate new trackers */
    memory[0x0A] = 1;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 5);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 45);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000017");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 45));
    ASSERT_NUM_EQUALS(g_client->game->leaderboard_trackers->next->reference_count, 1); /* 45 */

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 46);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "273");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 46));
    ASSERT_NUM_EQUALS(g_client->game->leaderboard_trackers->next->reference_count, 1); /* 46 */

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 47);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000273");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 47));
    ASSERT_NUM_EQUALS(g_client->game->leaderboard_trackers->reference_count, 2); /* 44,47 */

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 2);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 2);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000017"); /* 45 has different size */

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 3);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 3);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "273"); /* 46 has different format */

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* start additional leaderboard (48) - should share tracker with 44 */
    memory[0x0A] = 2;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 48);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000273");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 48));
    ASSERT_NUM_EQUALS(g_client->game->leaderboard_trackers->reference_count, 3); /* 44,47,48 */

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* cancel leaderboard 44 */
    memory[0x0C] = 1;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_FAILED, 44);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000273");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));
    ASSERT_NUM_EQUALS(g_client->game->leaderboard_trackers->reference_count, 2); /* 47,48 */

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* cancel leaderboard 45 */
    memory[0x0C] = 2;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_FAILED, 45);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000017");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 45));
    ASSERT_NUM_EQUALS(g_client->game->leaderboard_trackers->next->reference_count, 0); /* */

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 2);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 2);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000017");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* cancel leaderboard 46 */
    memory[0x0C] = 3;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_FAILED, 46);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "273");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 46));
    ASSERT_NUM_EQUALS(g_client->game->leaderboard_trackers->next->reference_count, 0); /* */

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 3);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 3);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "273");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);
    
    /* cancel 47, start 51 */
    memory[0x0A] = 3;
    memory[0x0B] = 0;
    memory[0x0C] = 4;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 3);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_FAILED, 47);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000273");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 47));
    ASSERT_NUM_EQUALS(g_client->game->leaderboard_trackers->reference_count, 1); /* 48 */

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 51);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "0");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 51));
    ASSERT_NUM_EQUALS(g_client->game->leaderboard_trackers->next->reference_count, 1); /* 51 */

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 2);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 2);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "0");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* cancel 48 */
    memory[0x0C] = 5;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_FAILED, 48);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000273");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 48));
    ASSERT_NUM_EQUALS(g_client->game->leaderboard_trackers->reference_count, 0); /*  */

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000273");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_leaderboard_tracker_sharing_hits(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    mock_memory(memory, sizeof(memory));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* start leaderboards 51,52 (share tracker) */
    memory[0x0A] = 3;
    memory[0x0B] = 3;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 3);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 51);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "0");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 51));

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 52);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "0");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 52));

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "0");

    /* hit count ticks */
    memory[0x09] = 1;
    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "1");

    /* cancel leaderboard 51 */
    memory[0x0C] = 6;
    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_FAILED, 51);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "2");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 51));

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "2");

    /* hit count ticks */
    memory[0x0A] = 0;
    memory[0x0C] = 0;
    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 1);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "3");

    /* restart leaderboard 51 - hit count differs, can't share */
    memory[0x0A] = 3;
    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 3);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 51);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "1");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 51));

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "4"); /* 52 */

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 2);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 2);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "1"); /* 51 */
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_leaderboard_submit_automatic_retry(void)
{
  const char* submit_entry_params = "r=submitlbentry&u=Username&t=ApiToken&i=44&s=17&m=0123456789ABCDEF&v=a27fa205f7f30c8d13d74806ea5425b6";
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks, no_unlocks);
  g_client->callbacks.server_call = rc_client_server_call_async;

  /* discard the queued ping to make finding the retry easier */
  g_client->state.scheduled_callbacks = NULL;

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    mock_memory(memory, sizeof(memory));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* start the leaderboard */
    memory[0x0B] = 1;
    memory[0x0E] = 17;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 44));
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* submit the leaderboard */
    memory[0x0D] = 1;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 2);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED, 44);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000017");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000017");

    event_count = 0;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(event_count, 0);

    /* first failure will immediately requeue the request */
    async_api_response(submit_entry_params, "");
    assert_api_pending(submit_entry_params);
    ASSERT_PTR_NULL(g_client->state.scheduled_callbacks);

    /* second failure will queue it */
    async_api_response(submit_entry_params, "");
    assert_api_call_count(submit_entry_params, 0);
    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);

    g_client->state.scheduled_callbacks->when = 0;
    rc_client_idle(g_client);
    assert_api_pending(submit_entry_params);
    ASSERT_PTR_NULL(g_client->state.scheduled_callbacks);

    /* third failure will requeue it */
    async_api_response(submit_entry_params, "");
    assert_api_call_count(submit_entry_params, 0);
    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);

    g_client->state.scheduled_callbacks->when = 0;
    rc_client_idle(g_client);
    assert_api_pending(submit_entry_params);
    ASSERT_PTR_NULL(g_client->state.scheduled_callbacks);

    /* success should not requeue it and update player score */
    async_api_response(submit_entry_params,
        "{\"Success\":true,\"Response\":{\"Score\":17,\"BestScore\":23,"
        "\"TopEntries\":[{\"User\":\"Player1\",\"Score\":44,\"Rank\":1},{\"User\":\"Username\",\"Score\":23,\"Rank\":2}],"
        "\"RankInfo\":{\"Rank\":2,\"NumEntries\":\"2\"}}}");
    ASSERT_PTR_NULL(g_client->state.scheduled_callbacks);
  }

  rc_client_destroy(g_client);
}

/* ----- ping ----- */

static void test_idle_ping(void)
{
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    rc_client_scheduled_callback_t ping_callback;
    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    g_client->state.scheduled_callbacks->when = 0;
    ping_callback = g_client->state.scheduled_callbacks->callback;

    mock_api_response("r=ping&u=Username&t=ApiToken&g=1234", "{\"Success\":true}");

    rc_client_idle(g_client);

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ASSERT_NUM_GREATER(g_client->state.scheduled_callbacks->when, time(NULL) + 100);
    ASSERT_NUM_LESS(g_client->state.scheduled_callbacks->when, time(NULL) + 150);
    ASSERT_PTR_EQUALS(g_client->state.scheduled_callbacks->callback, ping_callback);
  }

  /* unloading game should unschedule ping */
  rc_client_unload_game(g_client);
  ASSERT_PTR_NULL(g_client->state.scheduled_callbacks);

  rc_client_destroy(g_client);
}

static void test_do_frame_ping_rich_presence(void)
{
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    rc_client_scheduled_callback_t ping_callback;
    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    g_client->state.scheduled_callbacks->when = 0;
    ping_callback = g_client->state.scheduled_callbacks->callback;

    mock_memory(memory, sizeof(memory));
    mock_api_response("r=ping&u=Username&t=ApiToken&g=1234&m=Points%3a0", "{\"Success\":true}");

    rc_client_do_frame(g_client);

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ASSERT_NUM_GREATER(g_client->state.scheduled_callbacks->when, time(NULL) + 100);
    ASSERT_PTR_EQUALS(g_client->state.scheduled_callbacks->callback, ping_callback);

    g_client->state.scheduled_callbacks->when = 0;
    mock_api_response("r=ping&u=Username&t=ApiToken&g=1234&m=Points%3a25", "{\"Success\":true}");
    memory[0x03] = 25;

    rc_client_do_frame(g_client);

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ASSERT_NUM_GREATER(g_client->state.scheduled_callbacks->when, time(NULL) + 100);
    ASSERT_PTR_EQUALS(g_client->state.scheduled_callbacks->callback, ping_callback);

    assert_api_called("r=ping&u=Username&t=ApiToken&g=1234&m=Points%3a25");
  }

  rc_client_destroy(g_client);
}

static void test_reset_hides_widgets(void)
{
  const rc_client_leaderboard_t* leaderboard;
  const rc_client_achievement_t* achievement;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks, no_unlocks);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  mock_memory(memory, sizeof(memory));

  rc_client_do_frame(g_client);

  memory[0x01] = 1; /* challenge indicator for achievement 7 */
  memory[0x0A] = 2; /* tracker for leaderboard 48 */
  event_count = 0;
  rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(event_count, 3); /* challenge indicator show, leaderboard start, tracker show */
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

  event_count = 0;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);

  achievement = rc_client_get_achievement_info(g_client, 7);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->state, RC_TRIGGER_STATE_PRIMED);

  leaderboard = rc_client_get_leaderboard_info(g_client, 48);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
  ASSERT_NUM_EQUALS(((rc_client_leaderboard_info_t*)leaderboard)->lboard->state, RC_LBOARD_STATE_STARTED);

  rc_client_reset(g_client);

  ASSERT_NUM_EQUALS(event_count, 2); /* challenge indicator hide, tracker hide */
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1));

  achievement = rc_client_get_achievement_info(g_client, 7);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->state, RC_TRIGGER_STATE_WAITING);

  leaderboard = rc_client_get_leaderboard_info(g_client, 48);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_leaderboard_info_t*)leaderboard)->lboard->state, RC_LBOARD_STATE_WAITING);

  /* non tracked achievements/leaderboards should also be reset to waiting */
  achievement = rc_client_get_achievement_info(g_client, 5);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->state, RC_TRIGGER_STATE_WAITING);

  leaderboard = rc_client_get_leaderboard_info(g_client, 46);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_leaderboard_info_t*)leaderboard)->lboard->state, RC_LBOARD_STATE_WAITING);

  rc_client_destroy(g_client);
}

/* ----- progress ----- */

static void test_deserialize_progress_updates_widgets(void)
{
  const rc_client_leaderboard_t* leaderboard;
  const rc_client_achievement_t* achievement;
  const rc_client_event_t* event;
  uint8_t* serialized1;
  uint8_t* serialized2;
  size_t serialize_size;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks, no_unlocks);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  mock_memory(memory, sizeof(memory));

  rc_client_do_frame(g_client);

  /* create an initial checkpoint */
  serialize_size = rc_client_progress_size(g_client);
  serialized1 = (uint8_t*)malloc(serialize_size);
  serialized2 = (uint8_t*)malloc(serialize_size);
  ASSERT_NUM_EQUALS(rc_client_serialize_progress(g_client, serialized1), RC_OK);

  /* activate some widgets */
  memory[0x01] = 1; /* challenge indicator for achievement 7 */
  memory[0x0A] = 2; /* tracker for leaderboard 48 */
  memory[0x0E] = 25; /* leaderboard 48 value */
  event_count = 0;
  rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(event_count, 3); /* challenge indicator show, leaderboard start, tracker show */
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

  event_count = 0;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);

  achievement = rc_client_get_achievement_info(g_client, 7);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->state, RC_TRIGGER_STATE_PRIMED);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->requirement->conditions->next->current_hits, 2);

  leaderboard = rc_client_get_leaderboard_info(g_client, 48);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
  ASSERT_NUM_EQUALS(((rc_client_leaderboard_info_t*)leaderboard)->lboard->state, RC_LBOARD_STATE_STARTED);

  /* capture the state with the widgets visible */
  ASSERT_NUM_EQUALS(rc_client_serialize_progress(g_client, serialized2), RC_OK);

  /* deserialize current state. expect no changes */
  ASSERT_NUM_EQUALS(rc_client_deserialize_progress(g_client, serialized2), RC_OK);
  ASSERT_NUM_EQUALS(event_count, 0);

  achievement = rc_client_get_achievement_info(g_client, 7);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->state, RC_TRIGGER_STATE_PRIMED);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->requirement->conditions->next->current_hits, 2);

  leaderboard = rc_client_get_leaderboard_info(g_client, 48);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
  ASSERT_NUM_EQUALS(((rc_client_leaderboard_info_t*)leaderboard)->lboard->state, RC_LBOARD_STATE_STARTED);

  /* deserialize original state. expect challenge indicator hide, tracker hide */
  ASSERT_NUM_EQUALS(rc_client_deserialize_progress(g_client, serialized1), RC_OK);
  ASSERT_NUM_EQUALS(event_count, 2);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1));

  achievement = rc_client_get_achievement_info(g_client, 7);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->state, RC_TRIGGER_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->requirement->conditions->next->current_hits, 0);

  leaderboard = rc_client_get_leaderboard_info(g_client, 48);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_leaderboard_info_t*)leaderboard)->lboard->state, RC_LBOARD_STATE_ACTIVE);

  /* deserialize second state. expect challenge indicator show, tracker show */
  event_count = 0;
  ASSERT_NUM_EQUALS(rc_client_deserialize_progress(g_client, serialized2), RC_OK);
  ASSERT_NUM_EQUALS(event_count, 2);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

  achievement = rc_client_get_achievement_info(g_client, 7);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->state, RC_TRIGGER_STATE_PRIMED);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->requirement->conditions->next->current_hits, 2);

  leaderboard = rc_client_get_leaderboard_info(g_client, 48);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
  ASSERT_NUM_EQUALS(((rc_client_leaderboard_info_t*)leaderboard)->lboard->state, RC_LBOARD_STATE_STARTED);

  /* update tracker value */
  memory[0x0E] = 30;
  event_count = 0;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 1);
  event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE, 1);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000030");

  /* deserialize second state. expect challenge tracker update to old value */
  event_count = 0;
  ASSERT_NUM_EQUALS(rc_client_deserialize_progress(g_client, serialized2), RC_OK);
  ASSERT_NUM_EQUALS(event_count, 1);
  event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE, 1);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000025");

  leaderboard = rc_client_get_leaderboard_info(g_client, 48);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
  ASSERT_NUM_EQUALS(((rc_client_leaderboard_info_t*)leaderboard)->lboard->state, RC_LBOARD_STATE_STARTED);

  free(serialized2);
  free(serialized1);
  rc_client_destroy(g_client);
}

static void test_deserialize_progress_null(void)
{
  const rc_client_leaderboard_t* leaderboard;
  const rc_client_achievement_t* achievement;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks, no_unlocks);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  mock_memory(memory, sizeof(memory));

  rc_client_do_frame(g_client);

  /* activate some widgets */
  memory[0x01] = 1; /* challenge indicator for achievement 7 */
  memory[0x0A] = 2; /* tracker for leaderboard 48 */
  memory[0x0E] = 25; /* leaderboard 48 value */
  event_count = 0;
  rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(event_count, 3); /* challenge indicator show, leaderboard start, tracker show */
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

  event_count = 0;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);

  achievement = rc_client_get_achievement_info(g_client, 7);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->state, RC_TRIGGER_STATE_PRIMED);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->requirement->conditions->next->current_hits, 2);

  leaderboard = rc_client_get_leaderboard_info(g_client, 48);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
  ASSERT_NUM_EQUALS(((rc_client_leaderboard_info_t*)leaderboard)->lboard->state, RC_LBOARD_STATE_STARTED);

  /* deserialize null state. expect all widgets to be hidden and achievements reset to waiting */
  ASSERT_NUM_EQUALS(rc_client_deserialize_progress(g_client, NULL), RC_OK);
  ASSERT_NUM_EQUALS(event_count, 2);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1));

  achievement = rc_client_get_achievement_info(g_client, 7);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->requirement->conditions->next->current_hits, 0);

  leaderboard = rc_client_get_leaderboard_info(g_client, 48);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_leaderboard_info_t*)leaderboard)->lboard->state, RC_LBOARD_STATE_WAITING);

  /* must be false before it can be true to change from WAITING to ACTIVE. do so manually */
  ((rc_client_leaderboard_info_t*)leaderboard)->lboard->state = RC_LBOARD_STATE_ACTIVE;

  /* advance frame, challenge indicator and leaderboard tracker should reappear */
  event_count = 0;
  rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(event_count, 3); /* challenge indicator show, leaderboard start, tracker show */
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

  rc_client_destroy(g_client);
}

static void test_deserialize_progress_invalid(void)
{
  const rc_client_leaderboard_t* leaderboard;
  const rc_client_achievement_t* achievement;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks, no_unlocks);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  mock_memory(memory, sizeof(memory));

  rc_client_do_frame(g_client);

  /* activate some widgets */
  memory[0x01] = 1; /* challenge indicator for achievement 7 */
  memory[0x0A] = 2; /* tracker for leaderboard 48 */
  memory[0x0E] = 25; /* leaderboard 48 value */
  event_count = 0;
  rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(event_count, 3); /* challenge indicator show, leaderboard start, tracker show */
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

  event_count = 0;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 0);

  achievement = rc_client_get_achievement_info(g_client, 7);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->state, RC_TRIGGER_STATE_PRIMED);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->requirement->conditions->next->current_hits, 2);

  leaderboard = rc_client_get_leaderboard_info(g_client, 48);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
  ASSERT_NUM_EQUALS(((rc_client_leaderboard_info_t*)leaderboard)->lboard->state, RC_LBOARD_STATE_STARTED);

  /* deserialize null state. expect all widgets to be hidden and achievements reset to waiting */
  ASSERT_NUM_EQUALS(rc_client_deserialize_progress(g_client, memory), RC_INVALID_STATE);
  ASSERT_NUM_EQUALS(event_count, 2);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1));

  achievement = rc_client_get_achievement_info(g_client, 7);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->requirement->conditions->next->current_hits, 0);

  leaderboard = rc_client_get_leaderboard_info(g_client, 48);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_leaderboard_info_t*)leaderboard)->lboard->state, RC_LBOARD_STATE_WAITING);

  /* must be false before it can be true to change from WAITING to ACTIVE. do so manually */
  ((rc_client_leaderboard_info_t*)leaderboard)->lboard->state = RC_LBOARD_STATE_ACTIVE;

  /* advance frame, challenge indicator and leaderboard tracker should reappear */
  event_count = 0;
  rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(event_count, 3); /* challenge indicator show, leaderboard start, tracker show */
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

  rc_client_destroy(g_client);
}

/* ----- processing required ----- */

static void test_processing_required(void)
{
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, unlock_5501, unlock_5501_and_5502);

  ASSERT_TRUE(rc_client_is_processing_required(g_client));

  rc_client_destroy(g_client);
}

static void test_processing_required_empty_game(void)
{
  g_client = mock_client_game_loaded(patchdata_empty, no_unlocks, no_unlocks);

  ASSERT_FALSE(rc_client_is_processing_required(g_client));

  rc_client_destroy(g_client);
}

static void test_processing_required_rich_presence_only(void)
{
  g_client = mock_client_game_loaded(patchdata_rich_presence_only, no_unlocks, no_unlocks);

  ASSERT_TRUE(rc_client_is_processing_required(g_client));

  rc_client_destroy(g_client);
}

static void test_processing_required_leaderboard_only(void)
{
  g_client = mock_client_game_loaded(patchdata_leaderboard_only, no_unlocks, no_unlocks);

  ASSERT_TRUE(rc_client_is_processing_required(g_client));

  rc_client_destroy(g_client);
}

static void test_processing_required_after_mastery(void)
{
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, unlock_5501_and_5502, unlock_5501_and_5502);

  ASSERT_TRUE(rc_client_is_processing_required(g_client));

  rc_client_destroy(g_client);
}

static void test_processing_required_after_mastery_no_leaderboards(void)
{
  g_client = mock_client_game_loaded(patchdata_2ach_0lbd, unlock_5501_and_5502, unlock_5501_and_5502);

  ASSERT_FALSE(rc_client_is_processing_required(g_client));

  rc_client_destroy(g_client);
}

/* ----- settings ----- */

static void test_set_hardcore_disable(void)
{
  const rc_client_achievement_t* achievement;
  const rc_client_leaderboard_t* leaderboard;

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, unlock_5501, unlock_5501_and_5502);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);

  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, 1); /* 5502 should be active*/
  }

  leaderboard = rc_client_get_leaderboard_info(g_client, 4401);
  ASSERT_PTR_NOT_NULL(leaderboard);
  if (leaderboard) {
    ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(g_client->game->runtime.lboard_count, 1);
  }

  rc_client_set_hardcore_enabled(g_client, 0);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 0);
  ASSERT_NUM_EQUALS(g_client->game->waiting_for_reset, 0);

  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, 0); /* 5502 should not be active*/
  }

  leaderboard = rc_client_get_leaderboard_info(g_client, 4401);
  ASSERT_PTR_NOT_NULL(leaderboard);
  if (leaderboard) {
    ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_INACTIVE);
    ASSERT_NUM_EQUALS(g_client->game->runtime.lboard_count, 0);
  }

  rc_client_destroy(g_client);
}

static void test_set_hardcore_disable_active_tracker(void)
{
  const rc_client_leaderboard_t* leaderboard;
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, unlock_5501, unlock_5501_and_5502);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  mock_memory(memory, sizeof(memory));

  rc_client_do_frame(g_client);

  memory[0x0C] = 1;
  memory[0x0E] = 25;
  event_count = 0;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(event_count, 2);

  event = find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 4401);
  ASSERT_PTR_NOT_NULL(event);

  event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
  ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000025");

  leaderboard = rc_client_get_leaderboard_info(g_client, 4401);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);

  event_count = 0;
  rc_client_set_hardcore_enabled(g_client, 0);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 0);
  ASSERT_NUM_EQUALS(g_client->game->waiting_for_reset, 0);
  ASSERT_NUM_EQUALS(event_count, 1);

  leaderboard = rc_client_get_leaderboard_info(g_client, 4401);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_INACTIVE);

  event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);

  rc_client_destroy(g_client);
}

static void test_set_hardcore_enable(void)
{
  const rc_client_achievement_t* achievement;
  const rc_client_leaderboard_t* leaderboard;

  g_client = mock_client_logged_in();
  rc_client_set_hardcore_enabled(g_client, 0);
  mock_client_load_game(patchdata_2ach_1lbd, unlock_5501, unlock_5501_and_5502);

  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 0);

  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, 0); /* 5502 should not be active*/
  }

  leaderboard = rc_client_get_leaderboard_info(g_client, 4401);
  ASSERT_PTR_NOT_NULL(leaderboard);
  if (leaderboard) {
    ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_INACTIVE);
    ASSERT_NUM_EQUALS(g_client->game->runtime.lboard_count, 0);
  }

  /* when enabling hardcore, flag waiting_for_reset. this will prevent processing until rc_client_reset is called */
  event_count = 0;
  rc_client_set_hardcore_enabled(g_client, 1);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(g_client->game->waiting_for_reset, 1);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_RESET, 0));

  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, 1); /* 5502 should be active*/
  }

  leaderboard = rc_client_get_leaderboard_info(g_client, 4401);
  ASSERT_PTR_NOT_NULL(leaderboard);
  if (leaderboard) {
    ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(g_client->game->runtime.lboard_count, 1);
  }

  /* resetting clears waiting_for_reset */
  rc_client_reset(g_client);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(g_client->game->waiting_for_reset, 0);

  /* hardcore already enabled, attempting to set it again shouldn't flag waiting_for_reset */
  rc_client_set_hardcore_enabled(g_client, 1);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(g_client->game->waiting_for_reset, 0);

  rc_client_destroy(g_client);
}

static void test_set_hardcore_enable_no_game_loaded(void)
{
  g_client = mock_client_logged_in();
  rc_client_set_hardcore_enabled(g_client, 0);

  /* enabling hardcore before a game is loaded just toggles the flag  */
  event_count = 0;
  rc_client_set_hardcore_enabled(g_client, 1);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(event_count, 0);

  rc_client_destroy(g_client);
}

static void test_set_hardcore_enable_encore_mode(void)
{
  const rc_client_achievement_t* achievement;
  rc_client_achievement_info_t* achievement_info;

  g_client = mock_client_logged_in();
  rc_client_set_encore_mode_enabled(g_client, 1);
  mock_client_load_game(patchdata_2ach_1lbd, unlock_5501, unlock_5501_and_5502);

  ASSERT_NUM_EQUALS(rc_client_get_encore_mode_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, 2);

  g_client->game->runtime.triggers[0].trigger->state = RC_TRIGGER_STATE_ACTIVE;
  g_client->game->runtime.triggers[1].trigger->state = RC_TRIGGER_STATE_ACTIVE;

  achievement = rc_client_get_achievement_info(g_client, 5501);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH); /* unlock information still tracked */
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);     /* but achievement remains active */
    ASSERT_NUM_EQUALS(g_client->game->runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_ACTIVE);
  }
  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(g_client->game->runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_ACTIVE);
  }

  /* toggle hardcore mode should retain active achievements */
  rc_client_set_hardcore_enabled(g_client, 0);
  ASSERT_NUM_EQUALS(rc_client_get_encore_mode_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 0);
  ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, 2);

  achievement = rc_client_get_achievement_info(g_client, 5501);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(g_client->game->runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_ACTIVE);
  }
  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(g_client->game->runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_ACTIVE);
  }

  /* toggle hardcore mode should retain active achievements */
  rc_client_set_hardcore_enabled(g_client, 1);
  ASSERT_NUM_EQUALS(rc_client_get_encore_mode_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, 2);

  /* trigger an achievement */
  achievement_info = (rc_client_achievement_info_t*)rc_client_get_achievement_info(g_client, 5501);
  achievement_info->public.state = RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED;
  g_client->game->runtime.triggers[0].trigger->state = RC_TRIGGER_STATE_TRIGGERED;

  /* toggle hardcore mode should retain active achievements */
  rc_client_set_hardcore_enabled(g_client, 0);
  ASSERT_NUM_EQUALS(rc_client_get_encore_mode_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 0);
  ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, 1); /* only one active now */

  achievement = rc_client_get_achievement_info(g_client, 5501);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
  }
  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(g_client->game->runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_ACTIVE);
    ASSERT_PTR_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger, g_client->game->runtime.triggers[0].trigger);
  }

  /* toggle hardcore mode should retain active achievements */
  rc_client_set_hardcore_enabled(g_client, 1);
  ASSERT_NUM_EQUALS(rc_client_get_encore_mode_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, 1);

  rc_client_destroy(g_client);
}

static void test_set_encore_mode_enable(void)
{
  const rc_client_achievement_t* achievement;

  g_client = mock_client_logged_in();
  rc_client_set_encore_mode_enabled(g_client, 1);
  mock_client_load_game(patchdata_2ach_1lbd, unlock_5501, unlock_5501_and_5502);

  ASSERT_NUM_EQUALS(rc_client_get_encore_mode_enabled(g_client), 1);

  achievement = rc_client_get_achievement_info(g_client, 5501);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH); /* unlock information still tracked */
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);     /* but achievement remains active */
  }
  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  }

  /* toggle encore mode with a game loaded has no effect */
  rc_client_set_encore_mode_enabled(g_client, 0);
  ASSERT_NUM_EQUALS(rc_client_get_encore_mode_enabled(g_client), 0);

  achievement = rc_client_get_achievement_info(g_client, 5501);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  }
  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  }

  rc_client_destroy(g_client);
}

static void test_set_encore_mode_disable(void)
{
  const rc_client_achievement_t* achievement;

  g_client = mock_client_logged_in();
  rc_client_set_encore_mode_enabled(g_client, 1);
  rc_client_set_encore_mode_enabled(g_client, 0);
  mock_client_load_game(patchdata_2ach_1lbd, unlock_5501, unlock_5501_and_5502);

  ASSERT_NUM_EQUALS(rc_client_get_encore_mode_enabled(g_client), 0);

  achievement = rc_client_get_achievement_info(g_client, 5501);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
  }
  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  }

  /* toggle encore mode with a game loaded has no effect */
  rc_client_set_encore_mode_enabled(g_client, 1);
  ASSERT_NUM_EQUALS(rc_client_get_encore_mode_enabled(g_client), 1);

  achievement = rc_client_get_achievement_info(g_client, 5501);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
  }
  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  }

  rc_client_destroy(g_client);
}

/* ----- harness ----- */

void test_client(void) {
  TEST_SUITE_BEGIN();

  /* login */
  TEST(test_login_with_password);
  TEST(test_login_with_token);
  TEST(test_login_required_fields);
  TEST(test_login_with_incorrect_password);
  TEST(test_login_incomplete_response);
  TEST(test_login_with_password_async);
  TEST(test_login_with_password_async_aborted);

  /* logout */
  TEST(test_logout);
  TEST(test_logout_with_game_loaded);
  TEST(test_logout_during_login);
  TEST(test_logout_during_fetch_game);

  /* user */
  TEST(test_user_get_image_url);

  TEST(test_get_user_game_summary);
  TEST(test_get_user_game_summary_softcore);
  TEST(test_get_user_game_summary_encore_mode);
  TEST(test_get_user_game_summary_with_unsupported_and_unofficial);

  /* load game */
  TEST(test_load_game_required_fields);
  TEST(test_load_game_unknown_hash);
  TEST(test_load_game_not_logged_in);
  TEST(test_load_game);
  TEST(test_load_game_async_login);
  TEST(test_load_game_async_login_with_incorrect_password);
  TEST(test_load_game_gameid_failure);
  TEST(test_load_game_patch_failure);
  TEST(test_load_game_postactivity_failure);
  TEST(test_load_game_softcore_unlocks_failure);
  TEST(test_load_game_hardcore_unlocks_failure);
  TEST(test_load_game_gameid_aborted);
  TEST(test_load_game_patch_aborted);
  TEST(test_load_game_postactivity_aborted);
  TEST(test_load_game_softcore_unlocks_aborted);
  TEST(test_load_game_hardcore_unlocks_aborted);
  TEST(test_load_game_while_spectating);

  /* identify and load game */
  TEST(test_identify_and_load_game_required_fields);
  TEST(test_identify_and_load_game_console_specified);
  TEST(test_identify_and_load_game_console_not_specified);
  TEST(test_identify_and_load_game_unknown_hash);
  TEST(test_identify_and_load_game_multihash);
  TEST(test_identify_and_load_game_multihash_unknown_game);
  TEST(test_identify_and_load_game_multihash_differ);

  /* change media */
  TEST(test_change_media_required_fields);
  TEST(test_change_media_no_game_loaded);
  TEST(test_change_media_same_game);
  TEST(test_change_media_known_game);
  TEST(test_change_media_unknown_game);
  TEST(test_change_media_unhashable);
  TEST(test_change_media_back_and_forth);
  TEST(test_change_media_while_loading);
  TEST(test_change_media_while_loading_later);
  TEST(test_change_media_aborted);

  /* game */
  TEST(test_game_get_image_url);
  TEST(test_game_get_image_url_non_ssl);
  TEST(test_game_get_image_url_custom);

  /* subset */
  TEST(test_load_subset);

  /* achievements */
  TEST(test_achievement_list_simple);
  TEST(test_achievement_list_simple_with_unlocks);
  TEST(test_achievement_list_simple_with_unlocks_encore_mode);
  TEST(test_achievement_list_simple_with_unofficial_and_unsupported);
  TEST(test_achievement_list_simple_with_unofficial_off);
  TEST(test_achievement_list_buckets);
  TEST(test_achievement_list_subset_with_unofficial_and_unsupported);
  TEST(test_achievement_list_subset_buckets);
  TEST(test_achievement_list_subset_buckets_subset_first);

  TEST(test_achievement_get_image_url);

  /* leaderboards */
  TEST(test_leaderboard_list_simple);
  TEST(test_leaderboard_list_simple_with_unsupported);
  TEST(test_leaderboard_list_buckets);
  TEST(test_leaderboard_list_buckets_with_unsupported);
  TEST(test_leaderboard_list_subset);

  TEST(test_fetch_leaderboard_entries);
  TEST(test_fetch_leaderboard_entries_no_user);
  TEST(test_fetch_leaderboard_entries_around_user);
  TEST(test_fetch_leaderboard_entries_around_user_not_logged_in);
  TEST(test_fetch_leaderboard_entries_aborted);

  /* do frame */
  TEST(test_do_frame_bounds_check_system);
  TEST(test_do_frame_bounds_check_available);
  TEST(test_do_frame_achievement_trigger);
  TEST(test_do_frame_achievement_trigger_already_awarded);
  TEST(test_do_frame_achievement_trigger_server_error);
  TEST(test_do_frame_achievement_trigger_while_spectating);
  TEST(test_do_frame_achievement_trigger_automatic_retry);
  TEST(test_do_frame_achievement_trigger_subset);
  TEST(test_do_frame_achievement_measured);
  TEST(test_do_frame_achievement_measured_progress_event);
  TEST(test_do_frame_achievement_challenge_indicator);
  TEST(test_do_frame_mastery);
  TEST(test_do_frame_mastery_encore);
  TEST(test_do_frame_leaderboard_started);
  TEST(test_do_frame_leaderboard_update);
  TEST(test_do_frame_leaderboard_failed);
  TEST(test_do_frame_leaderboard_submit);
  TEST(test_do_frame_leaderboard_submit_server_error);
  TEST(test_do_frame_leaderboard_submit_while_spectating);
  TEST(test_do_frame_leaderboard_tracker_sharing);
  TEST(test_do_frame_leaderboard_tracker_sharing_hits);
  TEST(test_do_frame_leaderboard_submit_automatic_retry);

  TEST(test_idle_ping);
  TEST(test_do_frame_ping_rich_presence);

  TEST(test_reset_hides_widgets);
  TEST(test_deserialize_progress_updates_widgets);
  TEST(test_deserialize_progress_null);
  TEST(test_deserialize_progress_invalid);

  /* processing required */
  TEST(test_processing_required);
  TEST(test_processing_required_empty_game);
  TEST(test_processing_required_rich_presence_only);
  TEST(test_processing_required_leaderboard_only);
  TEST(test_processing_required_after_mastery);
  TEST(test_processing_required_after_mastery_no_leaderboards);

  /* settings */
  TEST(test_set_hardcore_disable);
  TEST(test_set_hardcore_disable_active_tracker);
  TEST(test_set_hardcore_enable);
  TEST(test_set_hardcore_enable_no_game_loaded);
  TEST(test_set_hardcore_enable_encore_mode);
  TEST(test_set_encore_mode_enable);
  TEST(test_set_encore_mode_disable);

  TEST_SUITE_END();
}
