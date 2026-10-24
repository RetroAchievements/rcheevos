#include "mock_client.h"

#include "../../src/client/rc_client_internal.h"

#include "../../src/util/rc_compat.h"
#include "../../src/util/rc_version.h"

#include <stdlib.h>
#include <string.h>

struct rc_client_t* g_client;
void* g_callback_userdata = &g_client; /* dummy object to use for callback userdata validation */

typedef struct rc_client_captured_event_t {
  rc_client_event_t event;
  rc_client_server_error_t server_error; /* server_error goes out of scope, it needs to be copied too */
  uint32_t id;
} rc_client_captured_event_t;

/* ----- Event mocking ----- */

static rc_client_captured_event_t events[16];
static int event_count = 0;

static void rc_client_event_handler(const rc_client_event_t* e, rc_client_t* client)
{
  memcpy(&events[event_count], e, sizeof(rc_client_event_t));
  memset(&events[event_count].server_error, 0, sizeof(events[event_count].server_error));

  switch (e->type) {
    case RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED:
    case RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW:
    case RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE:
    case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW:
    case RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_UPDATE:
      events[event_count].id = e->achievement->id;
      break;

    case RC_CLIENT_EVENT_LEADERBOARD_STARTED:
    case RC_CLIENT_EVENT_LEADERBOARD_FAILED:
    case RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED:
      events[event_count].id = e->leaderboard->id;
      break;

    case RC_CLIENT_EVENT_LEADERBOARD_SCOREBOARD: {
      /* scoreboard is not maintained out of scope, copy it */
      static rc_client_leaderboard_scoreboard_t scoreboard;
      static rc_client_leaderboard_scoreboard_entry_t scoreboard_entries[2];
      static char scoreboard_top_usernames[2][128];
      uint32_t i;

      /* cap the entries at 2, none of the mocked responses have anything larger */
      memcpy(&scoreboard, e->leaderboard_scoreboard, sizeof(scoreboard));
      scoreboard.num_top_entries = (scoreboard.num_top_entries > 2) ? 2 : scoreboard.num_top_entries;
      memcpy(scoreboard_entries, e->leaderboard_scoreboard->top_entries, scoreboard.num_top_entries * sizeof(scoreboard_entries[0]));
      for (i = 0; i < scoreboard.num_top_entries; i++) {
        strcpy_s(scoreboard_top_usernames[i], sizeof(scoreboard_top_usernames[i]), scoreboard_entries[i].username);
        scoreboard_entries[i].username = scoreboard_top_usernames[i];
      }
      scoreboard.top_entries = scoreboard_entries;

      events[event_count].id = e->leaderboard_scoreboard->leaderboard_id;
      events[event_count].event.leaderboard_scoreboard = &scoreboard;
      break;
    }

    case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW:
    case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE:
    case RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE:
      events[event_count].id = e->leaderboard_tracker->id;
      break;

    case RC_CLIENT_EVENT_GAME_COMPLETED:
      events[event_count].id = rc_client_get_game_info(client)->id;
      break;

    case RC_CLIENT_EVENT_SERVER_ERROR: {
      static char event_server_error_message[128];

      /* server error data is not maintained out of scope, copy it */
      memcpy(&events[event_count].server_error, e->server_error, sizeof(events[event_count].server_error));
      strcpy_s(event_server_error_message, sizeof(event_server_error_message), e->server_error->error_message);
      events[event_count].server_error.error_message = event_server_error_message;
      events[event_count].event.server_error = &events[event_count].server_error;
      events[event_count].id = 0;
      break;
    }

    case RC_CLIENT_EVENT_SUBSET_COMPLETED:
      events[event_count].id = e->subset->id;
      break;

    default:
      events[event_count].id = 0;
      break;
  }

  ++event_count;
}

rc_client_event_t* find_event(uint8_t type, uint32_t id)
{
  int i;

  for (i = 0; i < event_count; ++i) {
    if (events[i].id == id && events[i].event.type == type)
      return &events[i].event;
  }

  return NULL;
}

int get_pending_event_count(void)
{
  return event_count;
}

void reset_mock_events(void)
{
  event_count = 0;
}

/* ----- Memory mocking ----- */

static uint8_t* g_memory = NULL;
static uint32_t g_memory_size = 0;

void mock_memory(uint8_t* memory, uint32_t size)
{
  g_memory = memory;
  g_memory_size = size;
}

static uint32_t rc_client_read_memory(uint32_t address, uint8_t* buffer, uint32_t num_bytes, rc_client_t* client)
{
  (void)client;

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

rc_clock_t g_now;

static rc_clock_t rc_client_get_now_millisecs(const rc_client_t* client)
{
  (void)client;

  return g_now;
}

/* ----- API mocking ----- */

typedef struct rc_mock_api_response {
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
  (void)client;

  rc_api_server_response_t server_response;

  int i;
  for (i = 0; i < g_num_mock_api_responses; i++) {
    if (strcmp(g_mock_api_responses[i].request_params, request->post_data) == 0) {
      g_mock_api_responses[i].seen++;
      callback(&g_mock_api_responses[i].server_response, callback_data);
      return;
    }
  }

  ASSERT_MESSAGE("No API response for: %s", request->post_data);

  /* still call the callback to prevent memory leak */
  memset(&server_response, 0, sizeof(server_response));
  server_response.body = "";
  server_response.http_status_code = 500;
  callback(&server_response, callback_data);
}

static void rc_client_server_call_async(const rc_api_request_t* request, rc_client_server_callback_t callback, void* callback_data, rc_client_t* client)
{
  (void)client;

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
      /* request_params are set to NULL, so we can't find the requests to get a count anyway */
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

void async_api_response(const char* request_params, const char* response_body)
{
  _async_api_response(request_params, response_body, 200);
}

void async_api_error(const char* request_params, const char* response_body, int http_status_code)
{
  _async_api_response(request_params, response_body, http_status_code);
}

void mock_client_enable_async(struct rc_client_t* client)
{
  client->callbacks.server_call = rc_client_server_call_async;
}

void _assert_api_called(const char* request_params, int count)
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

void reset_mock_api_handlers(void)
{
  g_num_mock_api_responses = 0;
  memset(g_mock_api_responses, 0, sizeof(g_mock_api_responses));
}

void mock_api_response(const char* request_params, const char* response_body)
{
  g_mock_api_responses[g_num_mock_api_responses].request_params = request_params;
  g_mock_api_responses[g_num_mock_api_responses].server_response.body = response_body;
  g_mock_api_responses[g_num_mock_api_responses].server_response.body_length = strlen(response_body);
  g_mock_api_responses[g_num_mock_api_responses].server_response.http_status_code = 200;
  g_num_mock_api_responses++;
}

void mock_api_error(const char* request_params, const char* response_body, int http_status_code)
{
  mock_api_error_with_length(request_params, response_body, strlen(response_body), http_status_code);
}

void mock_api_error_with_length(const char* request_params, const char* response_body, size_t response_body_length, int http_status_code)
{
  g_mock_api_responses[g_num_mock_api_responses].request_params = request_params;
  g_mock_api_responses[g_num_mock_api_responses].server_response.body = response_body;
  g_mock_api_responses[g_num_mock_api_responses].server_response.body_length = response_body_length;
  g_mock_api_responses[g_num_mock_api_responses].server_response.http_status_code = http_status_code;
  g_num_mock_api_responses++;
}

/* ----- client mocking ----- */

struct rc_client_t* mock_client_not_logged_in(void)
{
  rc_client_t* client = rc_client_create(rc_client_read_memory, rc_client_server_call);
  rc_client_set_event_handler(client, rc_client_event_handler);
  rc_client_set_get_time_millisecs_function(client, rc_client_get_now_millisecs);

  mock_memory(NULL, 0);
  rc_api_set_host(NULL);
  reset_mock_api_handlers();
  g_now = 100000;

  return client;
}

struct rc_client_t* mock_client_not_logged_in_async(void)
{
  rc_client_t* client = mock_client_not_logged_in();
  client->callbacks.server_call = rc_client_server_call_async;
  return client;
}

struct rc_client_t* mock_client_logged_in(void)
{
  rc_client_t* client = mock_client_not_logged_in();
  client->user.username = "Username";
  client->user.display_name = "DisplayName";
  client->user.token = "ApiToken";
  client->user.score = 12345;
  client->user.avatar_url = "http://server/UserPic/Username.png";
  client->state.user = RC_CLIENT_USER_STATE_LOGGED_IN;

  return client;
}

struct rc_client_t* mock_client_logged_in_async(void)
{
  rc_client_t* client = mock_client_logged_in();
  client->callbacks.server_call = rc_client_server_call_async;
  return client;
}

void mock_client_load_game(const char* patchdata, const char* unlocks)
{
  reset_mock_api_handlers();
  event_count = 0;
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2", patchdata);
  mock_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING, unlocks);

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_success, g_callback_userdata);

  if (!g_client->game)
    ASSERT_MESSAGE("client->game is NULL");
}

void mock_client_load_game_casual(const char* patchdata, const char* unlocks)
{
  reset_mock_api_handlers();
  event_count = 0;
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2", patchdata);
  mock_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=0&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING, unlocks);

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_success, g_callback_userdata);

  if (!g_client->game)
    ASSERT_MESSAGE("client->game is NULL");
}

struct rc_client_t* mock_client_game_loaded(const char* patchdata, const char* unlocks)
{
  g_client = mock_client_logged_in();

  mock_client_load_game(patchdata, unlocks);

  return g_client;
}

/* ----- client callbacks ----- */

void rc_client_callback_expect_access_denied(int result, const char* error_message, struct rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_ACCESS_DENIED);
  ASSERT_STR_EQUALS(error_message, "Access denied.");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

void rc_client_callback_expect_credentials_error(int result, const char* error_message, struct rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_INVALID_CREDENTIALS);
  ASSERT_STR_EQUALS(error_message, "Invalid User/Password combination. Please try again");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

void rc_client_callback_expect_game_no_longer_active(int result, const char* error_message, struct rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_ABORTED);
  ASSERT_STR_EQUALS(error_message, "The requested game is no longer active");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

void rc_client_callback_expect_login_aborted(int result, const char* error_message, struct rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_ABORTED);
  ASSERT_STR_EQUALS(error_message, "Login aborted");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

void rc_client_callback_expect_login_required(int result, const char* error_message, struct rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_LOGIN_REQUIRED);
  ASSERT_STR_EQUALS(error_message, "Login required");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

void rc_client_callback_expect_no_game_loaded(int result, const char* error_message, struct rc_client_t* client, void* callback_data)
{
  ASSERT_NUM_EQUALS(result, RC_NO_GAME_LOADED);
  ASSERT_STR_EQUALS(error_message, "No game loaded");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_data, g_callback_userdata);
}

void rc_client_callback_expect_no_internet(int result, const char* error_message, struct rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_NO_RESPONSE);
  ASSERT_STR_EQUALS(error_message, "Internet not available.");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

void rc_client_callback_expect_success(int result, const char* error_message, struct rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_OK);
  ASSERT_PTR_NULL(error_message);
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

void rc_client_callback_expect_timeout(int result, const char* error_message, struct rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_NO_RESPONSE);
  ASSERT_STR_EQUALS(error_message, "Request has timed out.");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

void rc_client_callback_expect_token_error(int result, const char* error_message, struct rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_INVALID_CREDENTIALS);
  ASSERT_STR_EQUALS(error_message, "Invalid User/Token combination.");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

void rc_client_callback_expect_uncalled(int result, const char* error_message, struct rc_client_t* client, void* callback_userdata)
{
  (void)result;
  (void)error_message;
  (void)client;
  (void)callback_userdata;

  ASSERT_FAIL("Callback should not have been called.");
}

void rc_client_callback_expect_unknown_game(int result, const char* error_message, struct rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_NO_GAME_LOADED);
  ASSERT_STR_EQUALS(error_message, "Unknown game");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}
