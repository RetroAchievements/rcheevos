#include "rc_client.h"

#include "rc_consoles.h"
#include "../../src/runtime/rc_condition.h"
#include "../../src/runtime/rc_condset.h"
#include "../../src/runtime/rc_format.h"
#include "../../src/runtime/rc_lboard.h"
#include "../../src/runtime/rc_trigger.h"
#include "rc_api_runtime.h"

#include "../../src/client/rc_client_internal.h"
#include "../../src/client/rc_client_external.h"
#include "../../src/util/rc_version.h"

#include "../test_framework.h"

#include "client_data.h"
#include "mock_client.h"

#if defined(_WIN32)
#include <Windows.h>
#elif defined(__unix__) && __STDC_VERSION__ >= 199309L
#include <unistd.h>
#else
#define RC_NO_SLEEP
#endif

/* ----- login ----- */

static void test_login_with_password(void)
{
  const rc_client_user_t* user;

  g_client = mock_client_not_logged_in();
  reset_mock_api_handlers();
  mock_api_response("r=login2&u=User&p=Pa%24%24word",
	  "{\"Success\":true,\"User\":\"User\",\"Token\":\"ApiToken\",\"Score\":12345,\"SoftcoreScore\":123,\"Messages\":2,\"Permissions\":1,\"AccountType\":\"Registered\"}");

  rc_client_begin_login_with_password(g_client, "User", "Pa$$word", rc_client_callback_expect_success, g_callback_userdata);

  user = rc_client_get_user_info(g_client);
  ASSERT_PTR_NOT_NULL(user);
  ASSERT_STR_EQUALS(user->username, "User");
  ASSERT_STR_EQUALS(user->display_name, "User");
  ASSERT_STR_EQUALS(user->token, "ApiToken");
  ASSERT_NUM_EQUALS(user->score, 12345);
  ASSERT_NUM_EQUALS(user->score_casual, 123);
  ASSERT_NUM_EQUALS(user->num_unread_messages, 2);

  rc_client_destroy(g_client);
}

static void test_login_with_token(void)
{
  const rc_client_user_t* user;

  g_client = mock_client_not_logged_in();
  reset_mock_api_handlers();
  mock_api_response("r=login2&u=User&t=ApiToken",
	  "{\"Success\":true,\"User\":\"User\",\"AvatarUrl\":\"http://server/UserPic/USER.png\",\"Token\":\"ApiToken\",\"Score\":12345,\"Messages\":2}");

  rc_client_begin_login_with_token(g_client, "User", "ApiToken", rc_client_callback_expect_success, g_callback_userdata);

  user = rc_client_get_user_info(g_client);
  ASSERT_PTR_NOT_NULL(user);
  ASSERT_STR_EQUALS(user->username, "User");
  ASSERT_STR_EQUALS(user->display_name, "User");
  ASSERT_STR_EQUALS(user->token, "ApiToken");
  ASSERT_STR_EQUALS(user->avatar_url, "http://server/UserPic/USER.png");
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

static void test_login_with_incorrect_password(void)
{
  g_client = mock_client_not_logged_in();
  reset_mock_api_handlers();
  mock_api_error("r=login2&u=User&p=Pa%24%24word",
      "{\"Success\":false,\"Error\":\"Invalid User/Password combination. Please try again\","
       "\"Status\":401,\"Code\":\"invalid_credentials\"}", 401);

  rc_client_begin_login_with_password(g_client, "User", "Pa$$word", rc_client_callback_expect_credentials_error, g_callback_userdata);

  ASSERT_PTR_NULL(rc_client_get_user_info(g_client));

  rc_client_destroy(g_client);
}

static void test_login_with_incorrect_token(void)
{
  g_client = mock_client_not_logged_in();
  reset_mock_api_handlers();
  mock_api_error("r=login2&u=User&t=TOKEN",
      "{\"Success\":false,\"Error\":\"Invalid User/Token combination.\","
      "\"Status\":401,\"Code\":\"invalid_credentials\"}", 401);

  rc_client_begin_login_with_token(g_client, "User", "TOKEN", rc_client_callback_expect_token_error, g_callback_userdata);

  ASSERT_PTR_NULL(rc_client_get_user_info(g_client));

  rc_client_destroy(g_client);
}

static void rc_client_callback_expect_expired_token(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_EXPIRED_TOKEN);
  ASSERT_STR_EQUALS(error_message, "The access token has expired. Please log in again.");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void test_login_with_expired_token(void)
{
  g_client = mock_client_not_logged_in();
  reset_mock_api_handlers();
  mock_api_error("r=login2&u=User&t=EXPIRED",
      "{\"Success\":false,\"Error\":\"The access token has expired. Please log in again.\","
      "\"Status\":401,\"Code\":\"expired_token\"}", 403);

  rc_client_begin_login_with_token(g_client, "User", "EXPIRED", rc_client_callback_expect_expired_token, g_callback_userdata);

  ASSERT_PTR_NULL(rc_client_get_user_info(g_client));

  rc_client_destroy(g_client);
}

static void test_login_with_banned_account(void)
{
  g_client = mock_client_not_logged_in();
  reset_mock_api_handlers();
  mock_api_error("r=login2&u=User&p=Pa%24%24word",
      "{\"Success\":false,\"Error\":\"Access denied.\","
      "\"Status\":403,\"Code\":\"access_denied\"}", 403);

  rc_client_begin_login_with_password(g_client, "User", "Pa$$word", rc_client_callback_expect_access_denied, g_callback_userdata);

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
  mock_api_response("r=login2&u=User&p=Pa%24%24word", "{\"Success\":true,\"User\":\"Username\"}");

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

  async_api_response("r=login2&u=User&p=Pa%24%24word",
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

  async_api_response("r=login2&u=User&p=Pa%24%24word",
    "{\"Success\":true,\"User\":\"User\",\"Token\":\"ApiToken\",\"Score\":12345,\"SoftcoreScore\":123,\"Messages\":2,\"Permissions\":1,\"AccountType\":\"Registered\"}");

  user = rc_client_get_user_info(g_client);
  ASSERT_PTR_NULL(user);

  rc_client_destroy(g_client);
}

static void test_login_with_password_async_destroyed(void)
{
  const rc_client_user_t* user;

  g_client = mock_client_not_logged_in_async();
  reset_mock_api_handlers();

  rc_client_begin_login_with_password(g_client, "User", "Pa$$word",
    rc_client_callback_expect_uncalled, g_callback_userdata);

  user = rc_client_get_user_info(g_client);
  ASSERT_PTR_NULL(user);

  rc_client_destroy(g_client);

  async_api_response("r=login2&u=User&p=Pa%24%24word",
    "{\"Success\":true,\"User\":\"User\",\"Token\":\"ApiToken\",\"Score\":12345,\"SoftcoreScore\":123,\"Messages\":2,\"Permissions\":1,\"AccountType\":\"Registered\"}");
}

static void test_login_with_password_client_error(void)
{
  const rc_client_user_t* user;
  rc_client_async_handle_t* handle;

  g_client = mock_client_not_logged_in();

  mock_api_error("r=login2&u=User&p=Pa%24%24word", "Internet not available.", RC_API_SERVER_RESPONSE_CLIENT_ERROR);

  handle = rc_client_begin_login_with_password(g_client, "User", "Pa$$word",
      rc_client_callback_expect_no_internet, g_callback_userdata);

  user = rc_client_get_user_info(g_client);
  ASSERT_PTR_NULL(user);

  ASSERT_PTR_NULL(handle);

  rc_client_destroy(g_client);
}

static void test_login_with_password_client_error_substring(void)
{
  const rc_client_user_t* user;
  rc_client_async_handle_t* handle;

  g_client = mock_client_not_logged_in();

  mock_api_error_with_length("r=login2&u=User&p=Pa%24%24word", "Internet not available. Please try again.", strlen("Internet not available."), RC_API_SERVER_RESPONSE_CLIENT_ERROR);

  handle = rc_client_begin_login_with_password(g_client, "User", "Pa$$word",
    rc_client_callback_expect_no_internet, g_callback_userdata);

  user = rc_client_get_user_info(g_client);
  ASSERT_PTR_NULL(user);

  ASSERT_PTR_NULL(handle);

  rc_client_destroy(g_client);
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
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2", "{\"Success\":true,\"GameID\":1234}");

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_login_required, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);
  
  rc_client_destroy(g_client);
}

static void test_logout_with_game_loaded(void)
{
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);

  ASSERT_PTR_NOT_NULL(rc_client_get_user_info(g_client));
  ASSERT_PTR_NOT_NULL(rc_client_get_game_info(g_client));

  rc_client_logout(g_client);

  ASSERT_PTR_NULL(rc_client_get_user_info(g_client));
  ASSERT_PTR_NULL(rc_client_get_game_info(g_client));

  rc_client_destroy(g_client);
}

static void test_logout_during_login(void)
{
  g_client = mock_client_not_logged_in();
  mock_client_enable_async(g_client);

  rc_client_begin_login_with_password(g_client, "User", "Pa$$word", rc_client_callback_expect_login_aborted, g_callback_userdata);
  rc_client_logout(g_client);

  async_api_response("r=login2&u=User&p=Pa%24%24word",
    "{\"Success\":true,\"User\":\"User\",\"Token\":\"ApiToken\",\"Score\":12345,\"SoftcoreScore\":123,\"Messages\":2,\"Permissions\":1,\"AccountType\":\"Registered\"}");

  ASSERT_PTR_NULL(rc_client_get_user_info(g_client));

  rc_client_destroy(g_client);
}

static void test_logout_during_fetch_game(void)
{
  g_client = mock_client_logged_in();
  mock_client_enable_async(g_client);

  reset_mock_api_handlers();

  rc_client_begin_load_game(g_client, "0123456789ABCDEF",
    rc_client_callback_expect_game_no_longer_active, g_callback_userdata);

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2", patchdata_2ach_1lbd);

  rc_client_logout(g_client);

  async_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);
  ASSERT_PTR_NULL(rc_client_get_user_info(g_client));

  rc_client_destroy(g_client);
}

static void test_user_get_image_url(void)
{
  char buffer[256];
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);
  ASSERT_STR_EQUALS(g_client->user.avatar_url, "http://server/UserPic/Username.png");

  ASSERT_NUM_EQUALS(rc_client_user_get_image_url(rc_client_get_user_info(g_client), buffer, sizeof(buffer)), RC_OK);
  ASSERT_STR_EQUALS(buffer, "http://server/UserPic/Username.png");

  ASSERT_NUM_EQUALS(rc_client_user_get_image_url(rc_client_get_user_info(g_client), buffer, 10), RC_INSUFFICIENT_BUFFER);

  rc_client_destroy(g_client);
}

/* ----- harness ----- */

void test_client_user(void) {
  TEST_SUITE_BEGIN();

  /* login */
  TEST(test_login_with_password);
  TEST(test_login_with_token);
  TEST(test_login_required_fields);
  TEST(test_login_with_incorrect_password);
  TEST(test_login_with_incorrect_token);
  TEST(test_login_with_expired_token);
  TEST(test_login_with_banned_account);
  TEST(test_login_incomplete_response);
  TEST(test_login_with_password_async);
  TEST(test_login_with_password_async_aborted);
  TEST(test_login_with_password_async_destroyed);
  TEST(test_login_with_password_client_error);
  TEST(test_login_with_password_client_error_substring);

  /* logout */
  TEST(test_logout);
  TEST(test_logout_with_game_loaded);
  TEST(test_logout_during_login);
  TEST(test_logout_during_fetch_game);

  /* user */
  TEST(test_user_get_image_url);

  TEST_SUITE_END();
}
