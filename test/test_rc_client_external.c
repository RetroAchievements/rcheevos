#include "rc_client.h"

#include "../src/rc_client_internal.h"
#include "rc_consoles.h"
#include "rhash/data.h"

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
  const rc_client_user_t* user;

  g_client = mock_client_with_external();
  g_client->state.external_client->begin_login_with_password = rc_client_external_login_with_password;
  g_client->state.external_client->get_user_info = rc_client_external_get_user_info;

  rc_client_begin_login_with_password(g_client, "User", "Pa$$word", rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_external_event, "login");

  /* user data should come from external client. validate structure */
  user = rc_client_get_user_info(g_client);
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
  const rc_client_user_t* user;

  g_client = mock_client_with_external();
  g_client->state.external_client->begin_login_with_token = rc_client_external_login_with_token;
  g_client->state.external_client->get_user_info = rc_client_external_get_user_info;

  rc_client_begin_login_with_token(g_client, "User", "ApiToken", rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_external_event, "login");

  /* user data should come from external client. validate structure */
  user = rc_client_get_user_info(g_client);
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

/* ----- load game ----- */

typedef struct v1_rc_client_game_t {
  uint32_t id;
  uint32_t console_id;
  const char* title;
  const char* hash;
  const char* badge_name;
} v1_rc_client_game_t;

static const rc_client_game_t* rc_client_external_get_game_info(void)
{
  v1_rc_client_game_t* game = (v1_rc_client_game_t*)
    rc_buffer_alloc(&g_client->state.buffer, sizeof(v1_rc_client_game_t));

  memset(game, 0, sizeof(*game));
  game->id = 1234;
  game->console_id = RC_CONSOLE_PLAYSTATION;
  game->title = "Game Title";
  game->hash = "GAME_HASH";
  game->badge_name = "BDG001";

  return (const rc_client_game_t*)game;
}

static void assert_identify_and_load_game(rc_client_t* client,
    uint32_t console_id, const char* file_path, const uint8_t* data, size_t data_size)
{
  ASSERT_PTR_EQUALS(client, g_client);

  ASSERT_NUM_EQUALS(console_id, RC_CONSOLE_NINTENDO);
  ASSERT_STR_EQUALS(file_path, "foo.zip#foo.nes");
  ASSERT_PTR_NOT_NULL(data);
  ASSERT_NUM_EQUALS(32784, data_size);
}

static rc_client_async_handle_t* rc_client_external_identify_and_load_game(rc_client_t* client,
    uint32_t console_id, const char* file_path, const uint8_t* data, size_t data_size,
    rc_client_callback_t callback, void* callback_userdata)
{
  assert_identify_and_load_game(client, console_id, file_path, data, data_size);

  g_external_event = "load_game";

  callback(RC_OK, NULL, client, callback_userdata);
  return NULL;
}

static void test_identify_and_load_game(void)
{
  size_t image_size;
  uint8_t* image = generate_nes_file(32, 1, &image_size);
  const rc_client_game_t* game;

  g_client = mock_client_with_external();
  g_client->state.external_client->begin_identify_and_load_game = rc_client_external_identify_and_load_game;
  g_client->state.external_client->get_game_info = rc_client_external_get_game_info;

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_NINTENDO, "foo.zip#foo.nes",
    image, image_size, rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_external_event, "load_game");

  /* user data should come from external client. validate structure */
  game = rc_client_get_game_info(g_client);
  ASSERT_PTR_NOT_NULL(game);
  ASSERT_NUM_EQUALS(game->id, 1234);
  ASSERT_NUM_EQUALS(game->console_id, RC_CONSOLE_PLAYSTATION);
  ASSERT_STR_EQUALS(game->title, "Game Title");
  ASSERT_STR_EQUALS(game->hash, "GAME_HASH");
  ASSERT_STR_EQUALS(game->badge_name, "BDG001");
  /* ensure non-external client user was not initialized */
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
}

static void assert_load_game(rc_client_t* client, const char* hash)
{
  ASSERT_PTR_EQUALS(client, g_client);

  ASSERT_STR_EQUALS(hash, "ABCDEF0123456789");
}

static rc_client_async_handle_t* rc_client_external_load_game(rc_client_t* client,
  const char* hash, rc_client_callback_t callback, void* callback_userdata)
{
  assert_load_game(client, hash);

  g_external_event = "load_game";

  callback(RC_OK, NULL, client, callback_userdata);
  return NULL;
}

static void test_load_game(void)
{
  const rc_client_game_t* game;

  g_client = mock_client_with_external();
  g_client->state.external_client->begin_load_game = rc_client_external_load_game;
  g_client->state.external_client->get_game_info = rc_client_external_get_game_info;

  rc_client_begin_load_game(g_client, "ABCDEF0123456789", rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_external_event, "load_game");

  /* user data should come from external client. validate structure */
  game = rc_client_get_game_info(g_client);
  ASSERT_PTR_NOT_NULL(game);
  ASSERT_NUM_EQUALS(game->id, 1234);
  ASSERT_NUM_EQUALS(game->console_id, RC_CONSOLE_PLAYSTATION);
  ASSERT_STR_EQUALS(game->title, "Game Title");
  ASSERT_STR_EQUALS(game->hash, "GAME_HASH");
  ASSERT_STR_EQUALS(game->badge_name, "BDG001");
  /* ensure non-external client user was not initialized */
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
}

static void rc_client_external_get_user_game_summary(rc_client_user_game_summary_t* summary)
{
  summary->num_core_achievements = 20;
  summary->num_unlocked_achievements = 6;
  summary->num_unofficial_achievements = 3;
  summary->num_unsupported_achievements = 1;
  summary->points_core = 100;
  summary->points_unlocked = 23;
}

static void test_get_user_game_summary(void)
{
  rc_client_user_game_summary_t summary;

  g_client = mock_client_with_external();
  g_client->state.external_client->get_user_game_summary = rc_client_external_get_user_game_summary;

  rc_client_get_user_game_summary(g_client, &summary);

  ASSERT_NUM_EQUALS(summary.num_core_achievements, 20);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 6);
  ASSERT_NUM_EQUALS(summary.num_unofficial_achievements, 3);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 1);
  ASSERT_NUM_EQUALS(summary.points_core, 100);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 23);

  rc_client_destroy(g_client);
}

static void rc_client_external_unload_game(void)
{
  g_external_event = "unload_game";
}

static void test_unload_game(void)
{
  g_client = mock_client_with_external();
  g_client->state.external_client->unload_game = rc_client_external_unload_game;

  rc_client_unload_game(g_client);

  ASSERT_STR_EQUALS(g_external_event, "unload_game");

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

  /* load game */
  TEST(test_identify_and_load_game);
  TEST(test_load_game);
  TEST(test_get_user_game_summary);

  TEST(test_unload_game);

  TEST_SUITE_END();
}

#endif /* RC_CLIENT_SUPPORTS_EXTERNAL */
