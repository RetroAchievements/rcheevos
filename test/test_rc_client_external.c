#include "rc_client.h"

#include "../src/rc_client_internal.h"

#include "test_framework.h"

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL

static rc_client_t* g_client;
static const char* g_external_event;
static int g_external_int = 0;
static void* g_callback_userdata = &g_client; /* dummy object to use for callback userdata validation */

/* begin from test_rc_client.c */

extern void rc_client_server_call(const rc_api_request_t* request, rc_client_server_callback_t callback, void* callback_data, rc_client_t* client);
extern void reset_mock_api_handlers(void);
extern void mock_api_response(const char* request_params, const char* response_body);
extern void mock_api_error(const char* request_params, const char* response_body, int http_status_code);

/* end from test_rc_client.c */

static uint32_t rc_client_read_memory(uint32_t address, uint8_t* buffer, uint32_t num_bytes, rc_client_t* client)
{
  return 0;
}

static rc_client_t* mock_client_with_external()
{
  rc_client_t* client = rc_client_create(rc_client_read_memory, rc_client_server_call);
  client->state.external_client = (rc_client_external_t*)
      rc_buffer_alloc(&client->state.buffer, sizeof(*client->state.external_client));
  memset(client->state.external_client, 0, sizeof(*client->state.external_client));

  rc_api_set_host(NULL);
  reset_mock_api_handlers();
  g_external_event = "none";
  g_external_int = 0;

  return client;
}

static void rc_client_callback_expect_success(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_OK);
  ASSERT_PTR_NULL(error_message);
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

/* ----- settings ----- */

static int rc_client_external_get_int(void)
{
  return g_external_int;
}

static void rc_client_external_set_int(int value)
{
  g_external_int = value;
}

static void test_hardcore_enabled(void)
{
  g_client = mock_client_with_external();
  g_client->state.external_client->get_hardcore_enabled = rc_client_external_get_int;
  g_client->state.external_client->set_hardcore_enabled = rc_client_external_set_int;

  g_external_int = 0;
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 0);

  g_external_int = 1;
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);

  rc_client_set_hardcore_enabled(g_client, 0);
  ASSERT_NUM_EQUALS(g_external_int, 0);

  rc_client_set_hardcore_enabled(g_client, 1);
  ASSERT_NUM_EQUALS(g_external_int, 1);
}

static void test_unofficial_enabled(void)
{
  g_client = mock_client_with_external();
  g_client->state.external_client->get_unofficial_enabled = rc_client_external_get_int;
  g_client->state.external_client->set_unofficial_enabled = rc_client_external_set_int;

  g_external_int = 0;
  ASSERT_NUM_EQUALS(rc_client_get_unofficial_enabled(g_client), 0);

  g_external_int = 1;
  ASSERT_NUM_EQUALS(rc_client_get_unofficial_enabled(g_client), 1);

  rc_client_set_unofficial_enabled(g_client, 0);
  ASSERT_NUM_EQUALS(g_external_int, 0);

  rc_client_set_unofficial_enabled(g_client, 1);
  ASSERT_NUM_EQUALS(g_external_int, 1);
}

static void test_encore_mode_enabled(void)
{
  g_client = mock_client_with_external();
  g_client->state.external_client->get_encore_mode_enabled = rc_client_external_get_int;
  g_client->state.external_client->set_encore_mode_enabled = rc_client_external_set_int;

  g_external_int = 0;
  ASSERT_NUM_EQUALS(rc_client_get_encore_mode_enabled(g_client), 0);

  g_external_int = 1;
  ASSERT_NUM_EQUALS(rc_client_get_encore_mode_enabled(g_client), 1);

  rc_client_set_encore_mode_enabled(g_client, 0);
  ASSERT_NUM_EQUALS(g_external_int, 0);

  rc_client_set_encore_mode_enabled(g_client, 1);
  ASSERT_NUM_EQUALS(g_external_int, 1);
}

static void test_spectator_mode_enabled(void)
{
  g_client = mock_client_with_external();
  g_client->state.external_client->get_spectator_mode_enabled = rc_client_external_get_int;
  g_client->state.external_client->set_spectator_mode_enabled = rc_client_external_set_int;

  g_external_int = 0;
  ASSERT_NUM_EQUALS(rc_client_get_spectator_mode_enabled(g_client), 0);

  g_external_int = 1;
  ASSERT_NUM_EQUALS(rc_client_get_spectator_mode_enabled(g_client), 1);

  rc_client_set_spectator_mode_enabled(g_client, 0);
  ASSERT_NUM_EQUALS(g_external_int, 0);

  rc_client_set_spectator_mode_enabled(g_client, 1);
  ASSERT_NUM_EQUALS(g_external_int, 1);
}

/* ----- login ----- */

typedef struct v1_rc_client_user_t {
  const char* display_name;
  const char* username;
  const char* token;
  uint32_t score;
  uint32_t score_softcore;
  uint32_t num_unread_messages;
} v1_rc_client_user_t;

static void assert_login_with_password(rc_client_t* client, const char* username, const char* password)
{
  ASSERT_PTR_EQUALS(client, g_client);

  ASSERT_STR_EQUALS(username, "User");
  ASSERT_STR_EQUALS(password, "Pa$$word");
}

static rc_client_async_handle_t* rc_client_external_login_with_password(rc_client_t* client,
    const char* username, const char* password, rc_client_callback_t callback, void* callback_userdata)
{
  assert_login_with_password(client, username, password);

  g_external_event = "login";

  callback(RC_OK, NULL, client, callback_userdata);
  return NULL;
}

static const rc_client_user_t* rc_client_external_get_user_info(void)
{
  v1_rc_client_user_t* user = (v1_rc_client_user_t*)
      rc_buffer_alloc(&g_client->state.buffer, sizeof(v1_rc_client_user_t));

  memset(user, 0, sizeof(*user));
  user->display_name = "User";
  user->username = "User";
  user->token = "ApiToken";
  user->score = 12345;
  user->score_softcore = 123;
  user->num_unread_messages = 2;

  return (rc_client_user_t*)user;
}

static void test_login_with_password(void)
{
  const v1_rc_client_user_t* user;

  g_client = mock_client_with_external();
  g_client->state.external_client->begin_login_with_password = rc_client_external_login_with_password;
  g_client->state.external_client->get_user_info = rc_client_external_get_user_info;

  rc_client_begin_login_with_password(g_client, "User", "Pa$$word", rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_external_event, "login");

  /* user data should come from external client. validate structure */
  user = (const v1_rc_client_user_t*)rc_client_get_user_info(g_client);
  ASSERT_PTR_NOT_NULL(user);
  ASSERT_STR_EQUALS(user->username, "User");
  ASSERT_STR_EQUALS(user->display_name, "User");
  ASSERT_STR_EQUALS(user->token, "ApiToken");
  ASSERT_NUM_EQUALS(user->score, 12345);
  ASSERT_NUM_EQUALS(user->score_softcore, 123);
  ASSERT_NUM_EQUALS(user->num_unread_messages, 2);

  /* ensure non-external client user was not initialized */
  ASSERT_PTR_NULL(g_client->user.username);

  rc_client_destroy(g_client);
}

static void assert_login_with_token(rc_client_t* client, const char* username, const char* token)
{
  ASSERT_PTR_EQUALS(client, g_client);

  ASSERT_STR_EQUALS(username, "User");
  ASSERT_STR_EQUALS(token, "ApiToken");
}

static rc_client_async_handle_t* rc_client_external_login_with_token(rc_client_t* client,
  const char* username, const char* token, rc_client_callback_t callback, void* callback_userdata)
{
  assert_login_with_token(client, username, token);

  g_external_event = "login";

  callback(RC_OK, NULL, client, callback_userdata);
  return NULL;
}

static void test_login_with_token(void)
{
  const v1_rc_client_user_t* user;

  g_client = mock_client_with_external();
  g_client->state.external_client->begin_login_with_token = rc_client_external_login_with_token;
  g_client->state.external_client->get_user_info = rc_client_external_get_user_info;

  rc_client_begin_login_with_token(g_client, "User", "ApiToken", rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_external_event, "login");

  /* user data should come from external client. validate structure */
  user = (const v1_rc_client_user_t*)rc_client_get_user_info(g_client);
  ASSERT_PTR_NOT_NULL(user);
  ASSERT_STR_EQUALS(user->username, "User");
  ASSERT_STR_EQUALS(user->display_name, "User");
  ASSERT_STR_EQUALS(user->token, "ApiToken");
  ASSERT_NUM_EQUALS(user->score, 12345);
  ASSERT_NUM_EQUALS(user->score_softcore, 123);
  ASSERT_NUM_EQUALS(user->num_unread_messages, 2);

  /* ensure non-external client user was not initialized */
  ASSERT_PTR_NULL(g_client->user.username);

  rc_client_destroy(g_client);
}

static void rc_client_external_logout(void)
{
  g_external_event = "logout";
}

static void test_logout(void)
{
  g_client = mock_client_with_external();
  g_client->state.external_client->logout = rc_client_external_logout;

  /* external client should maintain its own state, but use the singular instance*/
  g_client->state.user = RC_CLIENT_USER_STATE_LOGGED_IN;

  rc_client_logout(g_client);
  ASSERT_STR_EQUALS(g_external_event, "logout");

  /* ensure non-external client user was not modified */
  ASSERT_NUM_EQUALS(g_client->state.user, RC_CLIENT_USER_STATE_LOGGED_IN);

  rc_client_destroy(g_client);
}

/* ----- harness ----- */

void test_client_external(void) {
  TEST_SUITE_BEGIN();

  /* settings */
  TEST(test_hardcore_enabled);
  TEST(test_unofficial_enabled);
  TEST(test_encore_mode_enabled);
  TEST(test_spectator_mode_enabled);

  /* login */
  TEST(test_login_with_password);
  TEST(test_login_with_token);

  TEST(test_logout);

  TEST_SUITE_END();
}

#endif /* RC_CLIENT_SUPPORTS_EXTERNAL */
