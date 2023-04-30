#include "rc_runtime2.h"

#include "rc_internal.h"
#include "rc_runtime2_internal.h"

#include "mock_memory.h"

#include "../test_framework.h"

static rc_runtime2_t* g_runtime;

static unsigned rc_runtime2_peek(unsigned address, unsigned num_bytes, rc_runtime2_t* runtime)
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
  return rc_runtime2_create(rc_runtime2_peek, rc_runtime2_server_call);
}

static rc_runtime2_t* mock_runtime2_not_logged_in_async(void)
{
  return rc_runtime2_create(rc_runtime2_peek, rc_runtime2_server_call_async);
}

static rc_runtime2_t* mock_runtime2_logged_in(void)
{
  rc_runtime2_t* runtime = rc_runtime2_create(rc_runtime2_peek, rc_runtime2_server_call);
  runtime->user.username = "Username";
  runtime->user.display_name = "DisplayName";
  runtime->user.token = "ApiToken";
  runtime->user.score = 12345;
  runtime->state.user = RC_RUNTIME2_USER_STATE_LOGGED_IN;

  return runtime;
}

/* ----- login ----- */

static void test_login_with_password(void)
{
  const rc_runtime2_user_t* user;

  g_runtime = mock_runtime2_not_logged_in();
  rc_runtime2_reset_api_handlers();
  rc_runtime2_mock_api_response("r=login&u=User&p=Pa%24%24word",
	  "{\"Success\":true,\"User\":\"User\",\"Token\":\"ApiToken\",\"Score\":12345,\"SoftcoreScore\":123,\"Messages\":2,\"Permissions\":1,\"AccountType\":\"Registered\"}");

  rc_runtime2_start_login_with_password(g_runtime, "User", "Pa$$word", rc_runtime2_callback_expect_success);

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

  rc_runtime2_start_login_with_token(g_runtime, "User", "ApiToken", rc_runtime2_callback_expect_success);

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

  rc_runtime2_start_login_with_password(g_runtime, "User", "", rc_runtime2_callback_expect_password_required);
  rc_runtime2_start_login_with_password(g_runtime, "", "Pa$$word", rc_runtime2_callback_expect_username_required);
  rc_runtime2_start_login_with_password(g_runtime, "", "", rc_runtime2_callback_expect_username_required);

  rc_runtime2_start_login_with_token(g_runtime, "User", "", rc_runtime2_callback_expect_token_required);
  rc_runtime2_start_login_with_token(g_runtime, "", "ApiToken", rc_runtime2_callback_expect_username_required);
  rc_runtime2_start_login_with_token(g_runtime, "", "", rc_runtime2_callback_expect_username_required);

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

  rc_runtime2_start_login_with_password(g_runtime, "User", "Pa$$word", rc_runtime2_callback_expect_credentials_error);

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

  rc_runtime2_start_login_with_password(g_runtime, "User", "Pa$$word", rc_runtime2_callback_expect_missing_token);

  ASSERT_PTR_NULL(rc_runtime2_user_info(g_runtime));

  rc_runtime2_destroy(g_runtime);
}

static void test_login_with_password_async(void)
{
  const rc_runtime2_user_t* user;

  g_runtime = mock_runtime2_not_logged_in_async();
  rc_runtime2_reset_api_handlers();

  rc_runtime2_start_login_with_password(g_runtime, "User", "Pa$$word", rc_runtime2_callback_expect_success);

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

  rc_runtime2_start_load_game(g_runtime, NULL, rc_runtime2_callback_expect_hash_required);
  rc_runtime2_start_load_game(g_runtime, "", rc_runtime2_callback_expect_hash_required);

  ASSERT_PTR_NULL(g_runtime->state.load);
  ASSERT_NUM_EQUALS(g_runtime->game.public.id, 0);

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

  rc_runtime2_start_load_game(g_runtime, "0123456789ABCDEF", rc_runtime2_callback_expect_unknown_game);

  ASSERT_PTR_NULL(g_runtime->state.load);
  ASSERT_NUM_EQUALS(g_runtime->game.public.id, 0);

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

  rc_runtime2_start_load_game(g_runtime, "0123456789ABCDEF", rc_runtime2_callback_expect_login_required);

  ASSERT_PTR_NULL(g_runtime->state.load);
  ASSERT_NUM_EQUALS(g_runtime->game.public.id, 0);

  rc_runtime2_destroy(g_runtime);
}

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

  TEST_SUITE_END();
}
