#include "rc_client.h"

#include "rc_consoles.h"
#include "rc_error.h"
#include "rc_api_request.h"
#include "rc_api_runtime.h"

#include "../../src/client/rc_client_game.h"
#include "../../src/client/rc_client_internal.h"
#include "../../src/client/rc_client_load.h"
#include "../../src/runtime/rc_format.h"
#include "../../src/util/rc_version.h"

#include "client_data.h"
#include "mock_client.h"

#include "../test_framework.h"

#ifdef RC_CLIENT_SUPPORTS_HASH
#include "rc_hash.h"
#include "../hash/hash_data.h"
#include "../hash/mock_filereader.h"
#endif

static rc_hash_iterator_t* rc_client_get_load_state_hash_iterator(rc_client_t* client)
{
  if (client && client->state.load)
    return &client->state.load->hash_iterator;

  return NULL;
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

static void test_load_game_unknown_hash(void)
{
  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2", patchdata_not_found);

  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_NONE);
  ASSERT_NUM_EQUALS(rc_client_is_game_loaded(g_client), 0);

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_unknown_game, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_DONE);
  ASSERT_NUM_EQUALS(rc_client_is_game_loaded(g_client), 0);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 0);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, RC_CONSOLE_UNKNOWN);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Unknown Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "0123456789ABCDEF");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_url, default_game_badge);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 0);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 0);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.id, 0);
    ASSERT_STR_EQUALS(g_client->game->subsets->public_.title, "");
    ASSERT_NUM_EQUALS(g_client->game->subsets->active, 0);
  }
  rc_client_destroy(g_client);
}

static void test_load_game_unknown_hash_repeated(void)
{
  rc_client_async_handle_t* handle;

  g_client = mock_client_logged_in();
  mock_client_enable_async(g_client);

  reset_mock_api_handlers();

  /* first request should resolve the hash asynchronously */
  handle = rc_client_begin_load_game(g_client,
      "0123456789ABCDEF", rc_client_callback_expect_unknown_game, g_callback_userdata);
  ASSERT_PTR_NOT_NULL(handle);
  ASSERT_PTR_NOT_NULL(g_client->state.load);

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2", patchdata_not_found);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

  ASSERT_NUM_EQUALS(g_client->game->public_.id, 0);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, RC_CONSOLE_UNKNOWN);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Unknown Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "0123456789ABCDEF");
  ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "");
  ASSERT_STR_EQUALS(g_client->game->public_.badge_url, default_game_badge);

  /* second request should use the hash cache and not need an asynchronous call */
  handle = rc_client_begin_load_game(g_client,
      "0123456789ABCDEF", rc_client_callback_expect_unknown_game, g_callback_userdata);
  ASSERT_PTR_NULL(handle);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

  ASSERT_NUM_EQUALS(g_client->game->public_.id, 0);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, RC_CONSOLE_UNKNOWN);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Unknown Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "0123456789ABCDEF");
  ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "");
  ASSERT_STR_EQUALS(g_client->game->public_.badge_url, default_game_badge);

  rc_client_destroy(g_client);
}

static void test_load_game_unknown_hash_multiple(void)
{
  rc_client_async_handle_t* handle;

  g_client = mock_client_logged_in();
  mock_client_enable_async(g_client);

  reset_mock_api_handlers();

  /* first request */
  handle = rc_client_begin_load_game(g_client, "0123456789ABCDEF",
                                     rc_client_callback_expect_unknown_game, g_callback_userdata);
  ASSERT_PTR_NOT_NULL(handle);
  ASSERT_PTR_NOT_NULL(g_client->state.load);

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2", patchdata_not_found);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

  ASSERT_NUM_EQUALS(g_client->game->public_.id, 0);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, RC_CONSOLE_UNKNOWN);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Unknown Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "0123456789ABCDEF");
  ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "");
  ASSERT_STR_EQUALS(g_client->game->public_.badge_url, default_game_badge);

  /* second request */
  handle = rc_client_begin_load_game(g_client, "FEDCBA9876543210",
                                     rc_client_callback_expect_unknown_game, g_callback_userdata);
  ASSERT_PTR_NOT_NULL(handle);
  ASSERT_PTR_NOT_NULL(g_client->state.load);

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=FEDCBA9876543210&v=2", patchdata_not_found);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

  ASSERT_NUM_EQUALS(g_client->game->public_.id, 0);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, RC_CONSOLE_UNKNOWN);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Unknown Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "FEDCBA9876543210");
  ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "");
  ASSERT_STR_EQUALS(g_client->game->public_.badge_url, default_game_badge);

  rc_client_destroy(g_client);
}

static void test_load_game_not_logged_in(void)
{
  g_client = mock_client_not_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2", "{\"Success\":true,\"GameID\":1234}");

  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_NONE);

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_login_required, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_NONE);

  rc_client_destroy(g_client);
}

static void test_load_game(void)
{
  rc_client_achievement_info_t* achievement;
  rc_client_leaderboard_info_t* leaderboard;
  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2", patchdata_2ach_1lbd);
  mock_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_NONE);
  ASSERT_NUM_EQUALS(rc_client_is_game_loaded(g_client), 0);

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_DONE);
  ASSERT_NUM_EQUALS(rc_client_is_game_loaded(g_client), 1);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "0123456789ABCDEF");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_url, "http://server/Images/112233.png");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);

    achievement = &g_client->game->subsets->achievements[0];
    ASSERT_NUM_EQUALS(achievement->public_.id, 5501);
    ASSERT_STR_EQUALS(achievement->public_.title, "Ach1");
    ASSERT_STR_EQUALS(achievement->public_.description, "Desc1");
    ASSERT_STR_EQUALS(achievement->public_.badge_name, "00234");
    ASSERT_STR_EQUALS(achievement->public_.badge_url, "https://media.retroachievements.org/Badge/00234.png");
    ASSERT_STR_EQUALS(achievement->public_.badge_locked_url, "https://media.retroachievements.org/Badge/00234_lock.png");
    ASSERT_NUM_EQUALS(achievement->public_.points, 5);
    ASSERT_NUM_EQUALS(achievement->public_.unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->public_.state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->public_.category, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED);
    ASSERT_PTR_NOT_NULL(achievement->trigger);

    achievement = &g_client->game->subsets->achievements[1];
    ASSERT_NUM_EQUALS(achievement->public_.id, 5502);
    ASSERT_STR_EQUALS(achievement->public_.title, "Ach2");
    ASSERT_STR_EQUALS(achievement->public_.description, "Desc2");
    ASSERT_STR_EQUALS(achievement->public_.badge_name, "00235");
    ASSERT_STR_EQUALS(achievement->public_.badge_url, "https://media.retroachievements.org/Badge/00235.png");
    ASSERT_STR_EQUALS(achievement->public_.badge_locked_url, "https://media.retroachievements.org/Badge/00235_lock.png");
    ASSERT_NUM_EQUALS(achievement->public_.points, 2);
    ASSERT_NUM_EQUALS(achievement->public_.unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->public_.state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->public_.category, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED);
    ASSERT_PTR_NOT_NULL(achievement->trigger);

    leaderboard = &g_client->game->subsets->leaderboards[0];
    ASSERT_NUM_EQUALS(leaderboard->public_.id, 4401);
    ASSERT_STR_EQUALS(leaderboard->public_.title, "Leaderboard1");
    ASSERT_STR_EQUALS(leaderboard->public_.description, "Desc1");
    ASSERT_NUM_EQUALS(leaderboard->public_.state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(leaderboard->format, RC_FORMAT_SCORE);
    ASSERT_PTR_NOT_NULL(leaderboard->lboard);
    ASSERT_NUM_NOT_EQUALS(leaderboard->value_djb2, 0);
    ASSERT_PTR_NULL(leaderboard->tracker);
  }

  rc_client_destroy(g_client);
}

static void test_load_game_async_load_different_game(void)
{
  g_client = mock_client_logged_in_async();
  reset_mock_api_handlers();

  /* start loading first game */
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_NONE);
  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_game_no_longer_active, g_callback_userdata);
  assert_api_pending("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2");
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_IDENTIFYING_GAME);

  /* receive data for first game, start session for first game */
  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2", patchdata_2ach_1lbd);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_STARTING_SESSION);

  /* start loading second game*/
  rc_client_begin_load_game(g_client, "ABCDEF0123456789", rc_client_callback_expect_success, g_callback_userdata);
  assert_api_pending("r=achievementsets&u=Username&t=ApiToken&m=ABCDEF0123456789&v=2");
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_IDENTIFYING_GAME);

  /* session started for first game, should abort */
  async_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_IDENTIFYING_GAME);

  /* receive data for second game, start session for second game */
  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=ABCDEF0123456789&v=2", patchdata_alternate);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_STARTING_SESSION);

  /* session started for second game, should succeed */
  async_api_response("r=startsession&u=Username&t=ApiToken&g=2345&h=1&m=ABCDEF0123456789&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_DONE);

  /* verify second game was loaded */
  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game)
  {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 2345);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 7);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Other Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "ABCDEF0123456789");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "555555");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 3);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 0);
  }

  rc_client_destroy(g_client);
}

static void test_load_game_async_login(void)
{
  g_client = mock_client_not_logged_in_async();
  reset_mock_api_handlers();

  rc_client_begin_login_with_password(g_client, "Username", "Pa$$word", rc_client_callback_expect_success, g_callback_userdata);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_NONE);
  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_success, g_callback_userdata);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_AWAIT_LOGIN);

  /* game load process will stop here waiting for the login to complete */
  assert_api_not_called("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2");

  /* login completion will trigger process to continue */
  async_api_response("r=login2&u=Username&p=Pa%24%24word",
	    "{\"Success\":true,\"User\":\"Username\",\"Token\":\"ApiToken\",\"Score\":12345,\"SoftcoreScore\":123,\"Messages\":2,\"Permissions\":1,\"AccountType\":\"Registered\"}");
  assert_api_pending("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2");
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_IDENTIFYING_GAME);

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2", patchdata_2ach_1lbd);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_STARTING_SESSION);

  async_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_DONE);

  ASSERT_STR_EQUALS(g_client->user.username, "Username");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "0123456789ABCDEF");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
}

static void test_load_game_async_login_with_incorrect_password(void)
{
  g_client = mock_client_not_logged_in_async();
  reset_mock_api_handlers();

  rc_client_begin_login_with_password(g_client, "Username", "Pa$$word", rc_client_callback_expect_credentials_error, g_callback_userdata);
  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_login_required, g_callback_userdata);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_AWAIT_LOGIN);

  /* game load process will stop here waiting for the login to complete */
  assert_api_not_called("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2");

  /* login failure will trigger process to continue */
  async_api_error("r=login2&u=Username&p=Pa%24%24word",
      "{\"Success\":false,\"Error\":\"Invalid User/Password combination. Please try again\","
      "\"Status\":401,\"Code\":\"invalid_credentials\"}", 401);
  assert_api_not_called("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2");

  ASSERT_PTR_NULL(g_client->user.username);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_NONE);

  rc_client_destroy(g_client);
}

static void test_load_game_async_login_logout(void)
{
  g_client = mock_client_not_logged_in_async();
  reset_mock_api_handlers();

  rc_client_begin_login_with_password(g_client, "Username", "Pa$$word", rc_client_callback_expect_login_aborted, g_callback_userdata);
  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_login_aborted, g_callback_userdata);

  /* game load process will stop here waiting for the login to complete */
  assert_api_not_called("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2");

  /* logout will cancel login and allow game load to proceed with failure */
  rc_client_logout(g_client);
  async_api_response("r=login2&u=Username&p=Pa%24%24word",
    "{\"Success\":true,\"User\":\"Username\",\"Token\":\"ApiToken\",\"Score\":12345,\"SoftcoreScore\":123,\"Messages\":2,\"Permissions\":1,\"AccountType\":\"Registered\"}");
  assert_api_not_called("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2");

  ASSERT_PTR_NULL(g_client->user.username);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_NONE);

  rc_client_destroy(g_client);
}

static void test_load_game_async_login_aborted(void)
{
  rc_client_async_handle_t* handle;

  g_client = mock_client_not_logged_in_async();
  reset_mock_api_handlers();

  handle = rc_client_begin_login_with_password(g_client, "Username", "Pa$$word", rc_client_callback_expect_uncalled, g_callback_userdata);
  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_login_aborted, g_callback_userdata);

  /* game load process will stop here waiting for the login to complete */
  assert_api_not_called("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2");

  /* login abort will trigger game load process to continue */
  rc_client_abort_async(g_client, handle);
  async_api_response("r=login2&u=Username&p=Pa%24%24word",
    "{\"Success\":true,\"User\":\"Username\",\"Token\":\"ApiToken\",\"Score\":12345,\"SoftcoreScore\":123,\"Messages\":2,\"Permissions\":1,\"AccountType\":\"Registered\"}");
  assert_api_not_called("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2");

  ASSERT_PTR_NULL(g_client->user.username);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_NONE);

  rc_client_destroy(g_client);
}

static void rc_client_callback_expect_too_many_requests(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_INVALID_JSON);
  ASSERT_STR_EQUALS(error_message, "429 Too Many Requests");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void test_load_game_patch_failure(void)
{
  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_error("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2", response_429, 429);
  mock_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_too_many_requests, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_NONE);

  rc_client_destroy(g_client);
}

static void test_load_game_startsession_failure(void)
{
  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2", patchdata_2ach_1lbd);
  mock_api_error("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING, response_429, 429);

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_too_many_requests, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_NONE);

  rc_client_destroy(g_client);
}

static void test_load_game_startsession_timeout(void)
{
  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2", patchdata_2ach_1lbd);
  mock_api_error("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING, "", 504);

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_timeout, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_NONE);

  rc_client_destroy(g_client);
}

static void test_load_game_startsession_custom_timeout(void)
{
  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2", patchdata_2ach_1lbd);
  mock_api_error("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING,
    "Request has timed out.", RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR);

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_timeout, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_NONE);

  rc_client_destroy(g_client);
}

static void test_load_game_patch_aborted(void)
{
  rc_client_async_handle_t* handle;

  g_client = mock_client_logged_in();
  mock_client_enable_async(g_client);

  reset_mock_api_handlers();

  handle = rc_client_begin_load_game(g_client, "0123456789ABCDEF",
    rc_client_callback_expect_uncalled, g_callback_userdata);

  rc_client_abort_async(g_client, handle);

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2", patchdata_2ach_1lbd);
  assert_api_not_called("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_NONE);

  rc_client_destroy(g_client);
}

static void test_load_game_startsession_aborted(void)
{
  rc_client_async_handle_t* handle;

  g_client = mock_client_logged_in();
  mock_client_enable_async(g_client);

  reset_mock_api_handlers();

  handle = rc_client_begin_load_game(g_client, "0123456789ABCDEF",
    rc_client_callback_expect_uncalled, g_callback_userdata);

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2", patchdata_2ach_1lbd);

  rc_client_abort_async(g_client, handle);

  async_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_NONE);

  rc_client_destroy(g_client);
}

static void test_load_game_while_spectating(void)
{
  rc_client_achievement_info_t* achievement;
  rc_client_leaderboard_info_t* leaderboard;
  g_client = mock_client_logged_in();
  rc_client_set_spectator_mode_enabled(g_client, 1);

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2", patchdata_2ach_1lbd);
  /* spectator mode should not start a session or fetch unlocks */

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_DONE);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "0123456789ABCDEF");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);

    achievement = &g_client->game->subsets->achievements[0];
    ASSERT_NUM_EQUALS(achievement->public_.id, 5501);
    ASSERT_STR_EQUALS(achievement->public_.title, "Ach1");
    ASSERT_STR_EQUALS(achievement->public_.description, "Desc1");
    ASSERT_STR_EQUALS(achievement->public_.badge_name, "00234");
    ASSERT_NUM_EQUALS(achievement->public_.points, 5);
    ASSERT_NUM_EQUALS(achievement->public_.unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->public_.state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->public_.category, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED);
    ASSERT_PTR_NOT_NULL(achievement->trigger);

    achievement = &g_client->game->subsets->achievements[1];
    ASSERT_NUM_EQUALS(achievement->public_.id, 5502);
    ASSERT_STR_EQUALS(achievement->public_.title, "Ach2");
    ASSERT_STR_EQUALS(achievement->public_.description, "Desc2");
    ASSERT_STR_EQUALS(achievement->public_.badge_name, "00235");
    ASSERT_NUM_EQUALS(achievement->public_.points, 2);
    ASSERT_NUM_EQUALS(achievement->public_.unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->public_.state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->public_.category, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED);
    ASSERT_PTR_NOT_NULL(achievement->trigger);

    leaderboard = &g_client->game->subsets->leaderboards[0];
    ASSERT_NUM_EQUALS(leaderboard->public_.id, 4401);
    ASSERT_STR_EQUALS(leaderboard->public_.title, "Leaderboard1");
    ASSERT_STR_EQUALS(leaderboard->public_.description, "Desc1");
    ASSERT_NUM_EQUALS(leaderboard->public_.state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
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

static int rc_client_callback_process_game_sets_called = 0;
static void rc_client_callback_process_game_sets(const rc_api_server_response_t* server_response,
    struct rc_api_fetch_game_sets_response_t* game_sets_response, rc_client_t* client, void* userdata)
{
  (void)client;
  (void)userdata;

  ASSERT_STR_EQUALS(server_response->body, patchdata_2ach_1lbd);
  ASSERT_NUM_EQUALS(game_sets_response->id, 1234);
  ASSERT_NUM_EQUALS(game_sets_response->num_sets, 1);
  ASSERT_NUM_EQUALS(game_sets_response->sets[0].num_achievements, 2);
  ASSERT_NUM_EQUALS(game_sets_response->sets[0].num_leaderboards, 1);
  rc_client_callback_process_game_sets_called = 1;
}

static void test_load_game_process_game_sets(void)
{
  rc_client_achievement_info_t* achievement;
  rc_client_leaderboard_info_t* leaderboard;
  g_client = mock_client_logged_in();
  g_client->callbacks.post_process_game_sets_response = rc_client_callback_process_game_sets;
  rc_client_callback_process_game_sets_called = 0;

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2", patchdata_2ach_1lbd);
  mock_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

  ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "0123456789ABCDEF");
  ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
  ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
  ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);

  achievement = &g_client->game->subsets->achievements[0];
  ASSERT_NUM_EQUALS(achievement->public_.id, 5501);
  ASSERT_STR_EQUALS(achievement->public_.title, "Ach1");
  ASSERT_STR_EQUALS(achievement->public_.description, "Desc1");
  ASSERT_STR_EQUALS(achievement->public_.badge_name, "00234");
  ASSERT_NUM_EQUALS(achievement->public_.points, 5);
  ASSERT_NUM_EQUALS(achievement->public_.unlock_time, 0);
  ASSERT_NUM_EQUALS(achievement->public_.state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(achievement->public_.category, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED);
  ASSERT_PTR_NOT_NULL(achievement->trigger);

  achievement = &g_client->game->subsets->achievements[1];
  ASSERT_NUM_EQUALS(achievement->public_.id, 5502);
  ASSERT_STR_EQUALS(achievement->public_.title, "Ach2");
  ASSERT_STR_EQUALS(achievement->public_.description, "Desc2");
  ASSERT_STR_EQUALS(achievement->public_.badge_name, "00235");
  ASSERT_NUM_EQUALS(achievement->public_.points, 2);
  ASSERT_NUM_EQUALS(achievement->public_.unlock_time, 0);
  ASSERT_NUM_EQUALS(achievement->public_.state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(achievement->public_.category, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED);
  ASSERT_PTR_NOT_NULL(achievement->trigger);

  leaderboard = &g_client->game->subsets->leaderboards[0];
  ASSERT_NUM_EQUALS(leaderboard->public_.id, 4401);
  ASSERT_STR_EQUALS(leaderboard->public_.title, "Leaderboard1");
  ASSERT_STR_EQUALS(leaderboard->public_.description, "Desc1");
  ASSERT_NUM_EQUALS(leaderboard->public_.state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(leaderboard->format, RC_FORMAT_SCORE);
  ASSERT_PTR_NOT_NULL(leaderboard->lboard);
  ASSERT_NUM_NOT_EQUALS(leaderboard->value_djb2, 0);
  ASSERT_PTR_NULL(leaderboard->tracker);

  ASSERT_NUM_NOT_EQUALS(rc_client_callback_process_game_sets_called, 0);

  rc_client_destroy(g_client);
}

static void test_load_game_destroy_while_fetching_game_data(void)
{
  g_client = mock_client_logged_in();
  mock_client_enable_async(g_client);
  reset_mock_api_handlers();

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_uncalled, g_callback_userdata);

  rc_client_destroy(g_client);

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2", patchdata_2ach_1lbd);
}

static void test_load_unknown_game(void)
{
  const char* hash = "0123456789ABCDEFFEDCBA9876543210";
  g_client = mock_client_logged_in();

  rc_client_load_unknown_game(g_client, hash);

  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_DONE);
  ASSERT_NUM_EQUALS(rc_client_is_game_loaded(g_client), 0);

  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

  ASSERT_NUM_EQUALS(g_client->game->public_.id, 0);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, RC_CONSOLE_UNKNOWN);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Unknown Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, hash);
  ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "");
  ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 0);
  ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 0);
  ASSERT_NUM_EQUALS(g_client->game->subsets->public_.id, 0);
  ASSERT_STR_EQUALS(g_client->game->subsets->public_.title, "");
  ASSERT_NUM_EQUALS(g_client->game->subsets->active, 0);

  rc_client_destroy(g_client);
}

static void test_load_unknown_game_multihash(void)
{
  const char* hash = "0123456789ABCDEFFEDCBA9876543210,FEDCBA98765432100123456789ABCDEF";
  g_client = mock_client_logged_in();

  rc_client_load_unknown_game(g_client, hash);

  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_DONE);
  ASSERT_NUM_EQUALS(rc_client_is_game_loaded(g_client), 0);

  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

  ASSERT_NUM_EQUALS(g_client->game->public_.id, 0);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, RC_CONSOLE_UNKNOWN);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Unknown Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, hash);
  ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "");
  ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 0);
  ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 0);
  ASSERT_NUM_EQUALS(g_client->game->subsets->public_.id, 0);
  ASSERT_STR_EQUALS(g_client->game->subsets->public_.title, "");
  ASSERT_NUM_EQUALS(g_client->game->subsets->active, 0);

  rc_client_destroy(g_client);
}

static void test_load_game_dispatched_read_memory(void)
{
  g_client = mock_client_logged_in();
  rc_client_set_allow_background_memory_reads(g_client, 0);

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2", patchdata_2ach_1lbd);
  mock_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_success, g_callback_userdata);
  ASSERT_NUM_EQUALS(rc_client_get_load_game_state(g_client), RC_CLIENT_LOAD_GAME_STATE_STARTING_SESSION);
  ASSERT_PTR_NOT_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_idle(g_client);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "0123456789ABCDEF");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
}

/* ----- unload game ----- */

static void test_unload_game(void)
{
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_PTR_NOT_NULL(rc_client_get_game_info(g_client));
  ASSERT_PTR_NOT_NULL(rc_client_get_user_info(g_client));
  ASSERT_PTR_NOT_NULL(rc_client_get_achievement_info(g_client, 5501));
  ASSERT_PTR_NOT_NULL(rc_client_get_leaderboard_info(g_client, 4401));

  reset_mock_events();
  rc_client_unload_game(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

  ASSERT_PTR_NULL(g_client->game);
  ASSERT_PTR_NULL(rc_client_get_game_info(g_client));
  ASSERT_PTR_NOT_NULL(rc_client_get_user_info(g_client));
  ASSERT_PTR_NULL(rc_client_get_achievement_info(g_client, 5501));
  ASSERT_PTR_NULL(rc_client_get_leaderboard_info(g_client, 4401));

  rc_client_destroy(g_client);
}

static void test_unload_game_hides_ui(void)
{
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  mock_memory(memory, sizeof(memory));

  reset_mock_events();
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

  memory[0x01] = 1;   /* show indicator */
  memory[0x06] = 3;   /* progress tracker */
  memory[0x0B] = 1;   /* start leaderboard */
  memory[0x0E] = 17;  /* leaderboard value */
  rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(get_pending_event_count(), 4);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 44));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW, 6));
  reset_mock_events();

  rc_client_unload_game(g_client);

  ASSERT_NUM_EQUALS(get_pending_event_count(), 3);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE, 0));
  reset_mock_events();

  rc_client_destroy(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);
}

static void test_unload_game_while_fetching_game_data(void)
{
  g_client = mock_client_logged_in();
  mock_client_enable_async(g_client);
  reset_mock_api_handlers();

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_uncalled, g_callback_userdata);

  rc_client_unload_game(g_client);

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2", patchdata_2ach_1lbd);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
}

static void test_unload_game_while_starting_session(void)
{
  g_client = mock_client_logged_in();
  mock_client_enable_async(g_client);
  reset_mock_api_handlers();

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_uncalled, g_callback_userdata);

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2", patchdata_2ach_1lbd);

  rc_client_unload_game(g_client);

  async_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
}

#ifdef RC_CLIENT_SUPPORTS_HASH

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
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857&v=2", patchdata_2ach_1lbd);
  mock_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=6a2305a2b6675a97ff792709be1ca857&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_GAMEBOY, "foo.zip#foo.gb",
      image, image_size, rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
  free(image);
}

static void test_identify_and_load_game_console_not_specified(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857&v=2", patchdata_2ach_1lbd);
  mock_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=6a2305a2b6675a97ff792709be1ca857&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_UNKNOWN, "foo.zip#foo.gb",
      image, image_size, rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
  free(image);
}

#ifndef RC_HASH_NO_ROM
uint8_t* generate_nes_file(size_t kb, int with_header, size_t* image_size);

static void test_identify_and_load_game_multiconsole_first(void)
{
  rc_hash_iterator_t* iterator;
  size_t image_size;
  uint8_t* image = generate_nes_file(32, 1, &image_size);

  g_client = mock_client_logged_in();
  mock_client_enable_async(g_client);

  reset_mock_api_handlers();

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_UNKNOWN, "foo.zip#foo.nes",
    image, image_size, rc_client_callback_expect_success, g_callback_userdata);

  /* first hash lookup should be pending. inject a secondary console into the iterator */
  assert_api_pending("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857&v=2");
  iterator = rc_client_get_load_state_hash_iterator(g_client);
  ASSERT_NUM_EQUALS(iterator->index, 1);
  ASSERT_NUM_EQUALS(iterator->consoles[iterator->index], 0);
  iterator->consoles[iterator->index] = RC_CONSOLE_MEGA_DRIVE; /* full buffer hash */
  iterator->consoles[iterator->index + 1] = 0;

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857&v=2", patchdata_2ach_1lbd);
  async_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=6a2305a2b6675a97ff792709be1ca857&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  assert_api_not_pending("r=achievementsets&u=Username&t=ApiToken&m=64b131c5c7fec32985d9c99700babb7e&v=2");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17); /* actual console ID returned from server */
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
  free(image);
}

static void test_identify_and_load_game_multiconsole_second(void)
{
  rc_hash_iterator_t* iterator;
  size_t image_size;
  uint8_t* image = generate_nes_file(32, 1, &image_size);

  g_client = mock_client_logged_in();
  mock_client_enable_async(g_client);

  reset_mock_api_handlers();

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_UNKNOWN, "foo.zip#foo.nes",
    image, image_size, rc_client_callback_expect_success, g_callback_userdata);

  /* first hash lookup should be pending. inject a secondary console into the iterator */
  assert_api_pending("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857&v=2");
  iterator = rc_client_get_load_state_hash_iterator(g_client);
  ASSERT_NUM_EQUALS(iterator->index, 1);
  ASSERT_NUM_EQUALS(iterator->consoles[iterator->index], 0);
  iterator->consoles[iterator->index] = RC_CONSOLE_MEGA_DRIVE; /* full buffer hash */
  iterator->consoles[iterator->index + 1] = 0;

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857&v=2", patchdata_not_found);

  assert_api_pending("r=achievementsets&u=Username&t=ApiToken&m=64b131c5c7fec32985d9c99700babb7e&v=2");
  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=64b131c5c7fec32985d9c99700babb7e&v=2" ,patchdata_2ach_1lbd);
  async_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=64b131c5c7fec32985d9c99700babb7e&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17); /* actual console ID returned from server */
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "64b131c5c7fec32985d9c99700babb7e");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
  free(image);
}

#endif /* RC_HASH_NO_ROM */

#ifndef RC_HASH_NO_DISC

static void test_identify_and_load_game_from_disc(void)
{
  rc_hash_callbacks_t hash_callbacks;

  size_t image_size;
  uint8_t* image = generate_psx_bin("SLUS_007.45", 0x07D800, &image_size);
  const char cue_single_track[] =
    "FILE \"game.bin\" BINARY\n"
    "  TRACK 01 MODE2/2352\n"
    "    INDEX 01 00:00:00\n";

  g_client = mock_client_logged_in();

  memset(&hash_callbacks, 0, sizeof(hash_callbacks));
  get_mock_filereader(&hash_callbacks.filereader);
  rc_client_set_hash_callbacks(g_client, &hash_callbacks);

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)cue_single_track, sizeof(cue_single_track));

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=db433fb038cde4fb15c144e8c7dea6e3&v=2", patchdata_2ach_1lbd);
  mock_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=db433fb038cde4fb15c144e8c7dea6e3&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_PLAYSTATION, "game.cue",
    NULL, 0, rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game)
  {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "db433fb038cde4fb15c144e8c7dea6e3");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
  free(image);
}

#endif

static void test_identify_and_load_game_unknown_hash(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857&v=2", patchdata_not_found);

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_UNKNOWN, "foo.zip#foo.gb",
      image, image_size, rc_client_callback_expect_unknown_game, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 0);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, RC_CONSOLE_GAMEBOY);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Unknown Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "");
  }

  rc_client_destroy(g_client);
  free(image);
}

static void test_identify_and_load_game_unknown_hash_repeated(void)
{
  rc_client_async_handle_t* handle;
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_logged_in();
  mock_client_enable_async(g_client);

  reset_mock_api_handlers();

  /* first request should resolve the hash asynchronously */
  handle = rc_client_begin_identify_and_load_game(g_client,
      RC_CONSOLE_UNKNOWN, "foo.zip#foo.gb", image, image_size,
      rc_client_callback_expect_unknown_game, g_callback_userdata);
  ASSERT_PTR_NOT_NULL(handle);
  ASSERT_PTR_NOT_NULL(g_client->state.load);

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857&v=2", patchdata_not_found);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

  ASSERT_NUM_EQUALS(g_client->game->public_.id, 0);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, RC_CONSOLE_GAMEBOY);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Unknown Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
  ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "");

  /* second request should use the hash cache and not need an asynchronous call */
  handle = rc_client_begin_identify_and_load_game(g_client,
      RC_CONSOLE_UNKNOWN, "foo.zip#foo.gb", image, image_size,
      rc_client_callback_expect_unknown_game, g_callback_userdata);
  ASSERT_PTR_NULL(handle);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

  ASSERT_NUM_EQUALS(g_client->game->public_.id, 0);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, RC_CONSOLE_GAMEBOY);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Unknown Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
  ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "");

  rc_client_destroy(g_client);
  free(image);
}

static void test_identify_and_load_game_unknown_hash_multiple(void)
{
  rc_client_async_handle_t* handle;

  const size_t disk_size = 256 * 16 * 35; /* 140KB - Apple II disk size */
  uint8_t* disk = generate_generic_file(disk_size);

  g_client = mock_client_logged_in();
  mock_client_enable_async(g_client);

  reset_mock_api_handlers();

  /* first request */
  handle = rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_APPLE_II,
    "disk1.dsk", disk, disk_size, rc_client_callback_expect_unknown_game, g_callback_userdata);
  ASSERT_PTR_NOT_NULL(handle);
  ASSERT_PTR_NOT_NULL(g_client->state.load);

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=88be638f4d78b4072109e55f13e8a0ac&v=2", patchdata_not_found);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

  ASSERT_NUM_EQUALS(g_client->game->public_.id, 0);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, RC_CONSOLE_APPLE_II);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Unknown Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "88be638f4d78b4072109e55f13e8a0ac");
  ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "");
  ASSERT_STR_EQUALS(g_client->game->public_.badge_url, default_game_badge);

  /* second request - modify file so new hash is generated */
  disk[16]++;
  handle = rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_APPLE_II,
    "disk1.dsk", disk, disk_size, rc_client_callback_expect_unknown_game, g_callback_userdata);
  ASSERT_PTR_NOT_NULL(handle);
  ASSERT_PTR_NOT_NULL(g_client->state.load);

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=8e39c6077108cafd6193d1c649b5d695&v=2", patchdata_not_found);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

  ASSERT_NUM_EQUALS(g_client->game->public_.id, 0);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, RC_CONSOLE_APPLE_II);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Unknown Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "8e39c6077108cafd6193d1c649b5d695");
  ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "");
  ASSERT_STR_EQUALS(g_client->game->public_.badge_url, default_game_badge);

  free(disk);
  rc_client_destroy(g_client);
}

#ifndef RC_HASH_NO_ROM

static void test_identify_and_load_game_unknown_hash_multiconsole(void)
{
  rc_hash_iterator_t* iterator;
  size_t image_size;
  uint8_t* image = generate_nes_file(32, 1, &image_size);

  g_client = mock_client_logged_in();
  mock_client_enable_async(g_client);

  reset_mock_api_handlers();

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_UNKNOWN, "foo.zip#foo.nes",
    image, image_size, rc_client_callback_expect_unknown_game, g_callback_userdata);

  /* first hash lookup should be pending. inject a secondary console into the iterator */
  assert_api_pending("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857&v=2");
  iterator = rc_client_get_load_state_hash_iterator(g_client);
  ASSERT_NUM_EQUALS(iterator->index, 1);
  ASSERT_NUM_EQUALS(iterator->consoles[iterator->index], 0);
  iterator->consoles[iterator->index] = RC_CONSOLE_MEGA_DRIVE; /* full buffer hash */
  iterator->consoles[iterator->index + 1] = 0;

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857&v=2", patchdata_not_found);

  assert_api_pending("r=achievementsets&u=Username&t=ApiToken&m=64b131c5c7fec32985d9c99700babb7e&v=2");
  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=64b131c5c7fec32985d9c99700babb7e&v=2", patchdata_not_found);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    /* when multiple hashes are tried, console will be unknown and hash will be a CSV */
    ASSERT_NUM_EQUALS(g_client->game->public_.id, 0);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, RC_CONSOLE_UNKNOWN);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Unknown Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857,64b131c5c7fec32985d9c99700babb7e");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "");
  }

  rc_client_destroy(g_client);
  free(image);
}

#endif

static void test_identify_and_load_game_unknown_hash_console_specified(void)
{
  rc_hash_iterator_t* iterator;
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_logged_in();
  mock_client_enable_async(g_client);

  reset_mock_api_handlers();

  /* explicitly specify we only want the NES hash processed */
  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_GAMEBOY, "foo.zip#foo.gb",
    image, image_size, rc_client_callback_expect_unknown_game, g_callback_userdata);

  /* first hash lookup should be pending. iterator should not have been initialized */
  assert_api_pending("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857&v=2");
  iterator = rc_client_get_load_state_hash_iterator(g_client);
  ASSERT_NUM_EQUALS(iterator->index, 0);
  ASSERT_NUM_EQUALS(iterator->consoles[iterator->index], 0);

  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857&v=2", patchdata_not_found);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 0);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, RC_CONSOLE_GAMEBOY);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Unknown Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "");
  }

  rc_client_destroy(g_client);
  free(image);
}

static void assert_unknown_hash_parameters(uint32_t console_id, const char* hash, rc_client_t* client, void* callback_data)
{
  ASSERT_NUM_EQUALS(console_id, RC_CONSOLE_GAMEBOY);
  ASSERT_STR_EQUALS(hash, "6a2305a2b6675a97ff792709be1ca857");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_data, g_callback_userdata);
}

static uint32_t rc_client_identify_unknown_hash(uint32_t console_id, const char* hash, rc_client_t* client, void* callback_data)
{
  assert_unknown_hash_parameters(console_id, hash, client, callback_data);
  return 1234;
}

static void test_identify_and_load_game_unknown_hash_client_provided(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_logged_in();
  g_client->callbacks.identify_unknown_hash = rc_client_identify_unknown_hash;

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857&v=2", patchdata_not_found);
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&g=1234&v=2", patchdata_2ach_1lbd);
  mock_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=6a2305a2b6675a97ff792709be1ca857&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_GAMEBOY, "foo.zip#foo.gb",
    image, image_size, rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game)
  {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17); /* patchdata returns 17 */
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
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
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857&v=2", patchdata_2ach_1lbd);
  mock_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=6a2305a2b6675a97ff792709be1ca857&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_UNKNOWN, "abc.dsk",
      image, image_size, rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
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
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857&v=2", patchdata_not_found);

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_UNKNOWN, "abc.dsk",
      image, image_size, rc_client_callback_expect_unknown_game, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 0);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, RC_CONSOLE_APPLE_II);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Unknown Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "");
  }

  /* same hash generated for all dsk consoles - only one server call should be made */
  assert_api_call_count("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857&v=2", 1);

  rc_client_destroy(g_client);
  free(image);
}

static void test_identify_and_load_game_multihash_differ(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_logged_in();
  mock_client_enable_async(g_client);

  reset_mock_api_handlers();

  rc_client_begin_identify_and_load_game(g_client, RC_CONSOLE_UNKNOWN, "abc.dsk",
      image, image_size, rc_client_callback_expect_success, g_callback_userdata);

  /* modify the checksum so callback for first lookup will generate a new lookup */
  memset(&image[256], 0, 32);

  /* first lookup fails */
  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857&v=2", patchdata_not_found);
  ASSERT_PTR_NOT_NULL(g_client->state.load);

  /* second lookup should succeed */
  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=4989b063a40dcfa28291ff8d675050e3&v=2", patchdata_2ach_1lbd);
  async_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=4989b063a40dcfa28291ff8d675050e3&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
 
  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "4989b063a40dcfa28291ff8d675050e3");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
  free(image);
}

#endif /* RC_CLIENT_SUPPORTS_HASH */

/* ----- change media ----- */

static void rc_client_callback_expect_hardcore_disabled_undentified_media(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_HARDCORE_DISABLED);
  ASSERT_STR_EQUALS(error_message, "Hardcore disabled. Unidentified media inserted.");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

#ifdef RC_CLIENT_SUPPORTS_HASH

static void test_change_media_required_fields(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);

  rc_client_begin_identify_and_change_media(g_client, NULL, NULL, 0,
      rc_client_callback_expect_data_or_file_path_required, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "0123456789ABCDEF");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
  free(image);
}

static void test_change_media_no_game_loaded(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_logged_in();

  rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo.gb", image, image_size,
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

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);

  mock_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":1234}");

  /* changing known discs within a game set is expected to succeed */
  rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo.gb", image, image_size,
      rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  /* resetting with a disc from the current game is allowed */
  rc_client_reset(g_client);
  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
  free(image);
}

static void test_change_media_known_game(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);

  mock_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":5555}");

  /* changing to a known disc from another game is allowed */
  rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo.gb", image, image_size,
      rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  /* resetting with a disc from another game will disable the client */
  rc_client_reset(g_client);
  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
  free(image);
}

static void test_change_media_unknown_game(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);
  ASSERT_TRUE(rc_client_get_hardcore_enabled(g_client));

  mock_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":0}");

  /* changing to an unknown disc is not allowed - could be a hacked version of one of the game's discs */
  rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo.gb", image, image_size,
      rc_client_callback_expect_hardcore_disabled_undentified_media, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
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
  const char* original_hash = NULL;
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);
  original_hash = g_client->game->public_.hash;

  /* N64 hash will fail with Not a Nintendo 64 ROM */
  g_client->game->public_.console_id = RC_CONSOLE_NINTENDO_64;

  /* changing to a disc not supported by the system is allowed */
  rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo.gb", image, image_size,
      rc_client_callback_expect_success, g_callback_userdata);

  rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo.gb", image, image_size,
    rc_client_callback_expect_success, g_callback_userdata);
  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "[NO HASH]");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  /* switch back to the original media so we can detect a switch back to the unhashable media */
  rc_client_begin_change_media(g_client, original_hash, rc_client_callback_expect_success, g_callback_userdata);

  /* re-entrant call won't try to regenerate the hash */
  rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo.gb", image, image_size,
      rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "[NO HASH]");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }


  /* resetting with a disc not from the current game will disable the client */
  rc_client_reset(g_client);
  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
  free(image);
}

static void test_change_media_unhashable_without_generation(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);

  /* changing to a disc not supported by the system is allowed */
  rc_client_begin_change_media(g_client, "[NO HASH]",
    rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "[NO HASH]");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
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

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);

  mock_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":1234}");
  mock_api_response("r=gameid&m=4989b063a40dcfa28291ff8d675050e3", "{\"Success\":true,\"GameID\":1234}");

  rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo.gb", image, image_size,
      rc_client_callback_expect_success, g_callback_userdata);
  rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo2.nes", image2, image_size,
      rc_client_callback_expect_success, g_callback_userdata);
  rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo.gb", image, image_size,
      rc_client_callback_expect_success, g_callback_userdata);
  rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo2.nes", image2, image_size,
      rc_client_callback_expect_success, g_callback_userdata);

  assert_api_call_count("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", 1);
  assert_api_call_count("r=gameid&m=4989b063a40dcfa28291ff8d675050e3", 1);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "4989b063a40dcfa28291ff8d675050e3");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
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
  mock_client_enable_async(g_client);

  reset_mock_api_handlers();

  rc_client_begin_load_game(g_client, "4989b063a40dcfa28291ff8d675050e3",
      rc_client_callback_expect_success, g_callback_userdata);
  rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo.gb", image, image_size,
      rc_client_callback_expect_success, g_callback_userdata);

  /* media request won't occur until patch data is received */
  assert_api_not_called("r=gameid&m=6a2305a2b6675a97ff792709be1ca857");
  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=4989b063a40dcfa28291ff8d675050e3&v=2", patchdata_2ach_1lbd);
  assert_api_not_called("r=gameid&m=6a2305a2b6675a97ff792709be1ca857");

  /* finish loading game */
  async_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=4989b063a40dcfa28291ff8d675050e3&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  /* secondary hash resolution does not occur until game is fully loaded or hash can't be compared to loaded game */
  assert_api_pending("r=gameid&m=6a2305a2b6675a97ff792709be1ca857");
  async_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":1234}");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
  free(image);
}

static void test_change_media_while_loading_later(void)
{
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_logged_in();
  mock_client_enable_async(g_client);

  reset_mock_api_handlers();

  rc_client_begin_load_game(g_client, "4989b063a40dcfa28291ff8d675050e3",
      rc_client_callback_expect_success, g_callback_userdata);

  /* get past fetching the patch data so there's a valid console for the change media call */
  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=4989b063a40dcfa28291ff8d675050e3&v=2", patchdata_2ach_1lbd);

  /* change_media should immediately attempt to resolve the new hash */
  rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo.gb", image, image_size,
      rc_client_callback_expect_success, g_callback_userdata);
  assert_api_pending("r=gameid&m=6a2305a2b6675a97ff792709be1ca857");

  /* finish loading game - session will be started with the old hash because the new hash hasn't resolved yet */
  async_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=4989b063a40dcfa28291ff8d675050e3&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  async_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":1234}");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  rc_client_destroy(g_client);
  free(image);
}

static void test_change_media_async_aborted(void)
{
  rc_client_async_handle_t* handle;
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);
  mock_client_enable_async(g_client);

  reset_mock_api_handlers();

  /* changing known discs within a game set is expected to succeed */
  handle = rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo.gb", image, image_size,
    rc_client_callback_expect_uncalled, g_callback_userdata);

  rc_client_abort_async(g_client, handle);

  async_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":1234}");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "0123456789ABCDEF"); /* old hash retained */
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  /* hash should still have been captured and lookup should succeed without having to call server again */
  reset_mock_api_handlers();

  rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo.gb", image, image_size,
    rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
  assert_api_not_called("r=achievementsets&u=Username&t=ApiToken&m=6a2305a2b6675a97ff792709be1ca857&v=2");

  rc_client_destroy(g_client);
  free(image);
}

static void test_change_media_client_error(void)
{
  rc_client_async_handle_t* handle;
  const size_t image_size = 32768;
  uint8_t* image = generate_generic_file(image_size);

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);
  mock_api_error("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "Internet not available.", RC_API_SERVER_RESPONSE_CLIENT_ERROR);

  handle = rc_client_begin_identify_and_change_media(g_client, "foo.zip#foo.gb", image, image_size,
      rc_client_callback_expect_no_internet, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game)
  {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "0123456789ABCDEF"); /* old hash retained */
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 2);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 1);
  }

  ASSERT_PTR_NULL(handle);

  rc_client_destroy(g_client);
  free(image);
}

#endif

static void test_change_media_from_hash_required_fields(void)
{
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);

  rc_client_begin_change_media(g_client, NULL,
    rc_client_callback_expect_hash_required, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "0123456789ABCDEF");

  rc_client_destroy(g_client);
}


static void test_change_media_from_hash_no_game_loaded(void)
{
  g_client = mock_client_logged_in();

  rc_client_begin_change_media(g_client, "6a2305a2b6675a97ff792709be1ca857",
    rc_client_callback_expect_no_game_loaded, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
}

static void test_change_media_from_hash_same_game(void)
{
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);

  mock_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":1234}");

  /* changing known discs within a game set is expected to succeed */
  rc_client_begin_change_media(g_client, "6a2305a2b6675a97ff792709be1ca857",
    rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");

  /* resetting with a disc from the current game is allowed */
  rc_client_reset(g_client);
  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");

  rc_client_destroy(g_client);
}

static void test_change_media_from_hash_known_game(void)
{
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);

  mock_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":5555}");

  /* changing to a known disc from another game is allowed */
  rc_client_begin_change_media(g_client, "6a2305a2b6675a97ff792709be1ca857",
    rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");

  /* resetting with a disc from another game will disable the client */
  rc_client_reset(g_client);
  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
}

static void test_change_media_from_hash_unknown_game(void)
{
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);
  ASSERT_TRUE(rc_client_get_hardcore_enabled(g_client));

  mock_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":0}");

  /* changing to an unknown disc is not allowed - could be a hacked version of one of the game's discs */
  rc_client_begin_change_media(g_client, "6a2305a2b6675a97ff792709be1ca857",
    rc_client_callback_expect_hardcore_disabled_undentified_media, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");

  ASSERT_FALSE(rc_client_get_hardcore_enabled(g_client));

  /* resetting with a disc not from the current game will disable the client */
  rc_client_reset(g_client);
  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NULL(g_client->game);

  rc_client_destroy(g_client);
}

static void test_change_media_from_hash_back_and_forth(void)
{
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);

  mock_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":1234}");
  mock_api_response("r=gameid&m=4989b063a40dcfa28291ff8d675050e3", "{\"Success\":true,\"GameID\":1234}");

  rc_client_begin_change_media(g_client, "6a2305a2b6675a97ff792709be1ca857",
    rc_client_callback_expect_success, g_callback_userdata);
  rc_client_begin_change_media(g_client, "4989b063a40dcfa28291ff8d675050e3",
    rc_client_callback_expect_success, g_callback_userdata);
  rc_client_begin_change_media(g_client, "6a2305a2b6675a97ff792709be1ca857",
    rc_client_callback_expect_success, g_callback_userdata);
  rc_client_begin_change_media(g_client, "4989b063a40dcfa28291ff8d675050e3",
    rc_client_callback_expect_success, g_callback_userdata);

  assert_api_call_count("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", 1);
  assert_api_call_count("r=gameid&m=4989b063a40dcfa28291ff8d675050e3", 1);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "4989b063a40dcfa28291ff8d675050e3");

  rc_client_destroy(g_client);
}

static void test_change_media_from_hash_while_loading(void)
{
  g_client = mock_client_logged_in();
  mock_client_enable_async(g_client);

  reset_mock_api_handlers();

  rc_client_begin_load_game(g_client, "4989b063a40dcfa28291ff8d675050e3",
    rc_client_callback_expect_success, g_callback_userdata);
  rc_client_begin_change_media(g_client, "6a2305a2b6675a97ff792709be1ca857",
    rc_client_callback_expect_success, g_callback_userdata);

  /* media request won't occur until patch data is received */
  assert_api_not_called("r=gameid&m=6a2305a2b6675a97ff792709be1ca857");
  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=4989b063a40dcfa28291ff8d675050e3&v=2", patchdata_2ach_1lbd);
  assert_api_not_called("r=gameid&m=6a2305a2b6675a97ff792709be1ca857");

  /* finish loading game */
  async_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=4989b063a40dcfa28291ff8d675050e3&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  /* secondary hash resolution does not occur until game is fully loaded or hash can't be compared to loaded game */
  assert_api_pending("r=gameid&m=6a2305a2b6675a97ff792709be1ca857");
  async_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":1234}");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");

  rc_client_destroy(g_client);
}

static void test_change_media_from_hash_while_loading_later(void)
{
  g_client = mock_client_logged_in();
  mock_client_enable_async(g_client);

  reset_mock_api_handlers();

  rc_client_begin_load_game(g_client, "4989b063a40dcfa28291ff8d675050e3",
    rc_client_callback_expect_success, g_callback_userdata);

  /* get past fetching the patch data so there's a valid console for the change media call */
  async_api_response("r=achievementsets&u=Username&t=ApiToken&m=4989b063a40dcfa28291ff8d675050e3&v=2", patchdata_2ach_1lbd);

  /* change_media should immediately attempt to resolve the new hash */
  rc_client_begin_change_media(g_client, "6a2305a2b6675a97ff792709be1ca857",
    rc_client_callback_expect_success, g_callback_userdata);
  assert_api_pending("r=gameid&m=6a2305a2b6675a97ff792709be1ca857");

  /* finish loading game - session will be started with the old hash because the new hash hasn't resolved yet */
  async_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=4989b063a40dcfa28291ff8d675050e3&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");
  async_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":1234}");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");

  rc_client_destroy(g_client);
}

static void test_change_media_from_hash_async_aborted(void)
{
  rc_client_async_handle_t* handle;

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);
  mock_client_enable_async(g_client);

  reset_mock_api_handlers();

  /* changing known discs within a game set is expected to succeed */
  handle = rc_client_begin_change_media(g_client, "6a2305a2b6675a97ff792709be1ca857",
    rc_client_callback_expect_uncalled, g_callback_userdata);

  rc_client_abort_async(g_client, handle);

  async_api_response("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "{\"Success\":true,\"GameID\":1234}");

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "0123456789ABCDEF"); /* old hash retained */

  /* hash should still have been captured and lookup should succeed without having to call server again */
  reset_mock_api_handlers();

  rc_client_begin_change_media(g_client, "6a2305a2b6675a97ff792709be1ca857",
    rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_client->game->public_.hash, "6a2305a2b6675a97ff792709be1ca857");
  assert_api_not_called("r=gameid&m=6a2305a2b6675a97ff792709be1ca857");

  rc_client_destroy(g_client);
}

static void test_change_media_from_hash_client_error(void)
{
  rc_client_async_handle_t* handle;

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);
  mock_api_error("r=gameid&m=6a2305a2b6675a97ff792709be1ca857", "Internet not available.", RC_API_SERVER_RESPONSE_CLIENT_ERROR);

  handle = rc_client_begin_change_media(g_client, "6a2305a2b6675a97ff792709be1ca857",
    rc_client_callback_expect_no_internet, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
  ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
  ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
  ASSERT_STR_EQUALS(g_client->game->public_.hash, "0123456789ABCDEF"); /* old hash retained */

  ASSERT_PTR_NULL(handle);

  rc_client_destroy(g_client);
}

/* ----- harness ----- */

void test_client_load(void) {
  TEST_SUITE_BEGIN();

  /* load game */
  TEST(test_load_game_required_fields);
  TEST(test_load_game_unknown_hash);
  TEST(test_load_game_unknown_hash_repeated);
  TEST(test_load_game_unknown_hash_multiple);
  TEST(test_load_game_not_logged_in);
  TEST(test_load_game);
  TEST(test_load_game_async_load_different_game);
  TEST(test_load_game_async_login);
  TEST(test_load_game_async_login_with_incorrect_password);
  TEST(test_load_game_async_login_logout);
  TEST(test_load_game_async_login_aborted);
  TEST(test_load_game_patch_failure);
  TEST(test_load_game_startsession_failure);
  TEST(test_load_game_startsession_timeout);
  TEST(test_load_game_startsession_custom_timeout);
  TEST(test_load_game_patch_aborted);
  TEST(test_load_game_startsession_aborted);
  TEST(test_load_game_while_spectating);
  TEST(test_load_game_process_game_sets);
  TEST(test_load_game_destroy_while_fetching_game_data);
  TEST(test_load_unknown_game);
  TEST(test_load_unknown_game_multihash);
  TEST(test_load_game_dispatched_read_memory);

  /* unload game */
  TEST(test_unload_game);
  TEST(test_unload_game_hides_ui);
  TEST(test_unload_game_while_fetching_game_data);
  TEST(test_unload_game_while_starting_session);

#ifdef RC_CLIENT_SUPPORTS_HASH
  /* identify and load game */
  TEST(test_identify_and_load_game_required_fields);
  TEST(test_identify_and_load_game_console_specified);
  TEST(test_identify_and_load_game_console_not_specified);
 #ifndef RC_HASH_NO_ROM
  TEST(test_identify_and_load_game_multiconsole_first);
  TEST(test_identify_and_load_game_multiconsole_second);
 #endif
 #ifndef RC_HASH_NO_DISC
  TEST(test_identify_and_load_game_from_disc);
 #endif
  TEST(test_identify_and_load_game_unknown_hash);
  TEST(test_identify_and_load_game_unknown_hash_repeated);
  TEST(test_identify_and_load_game_unknown_hash_multiple);
 #ifndef RC_HASH_NO_ROM
  TEST(test_identify_and_load_game_unknown_hash_multiconsole);
 #endif
  TEST(test_identify_and_load_game_unknown_hash_console_specified);
  TEST(test_identify_and_load_game_unknown_hash_client_provided);
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
  TEST(test_change_media_unhashable_without_generation);
  TEST(test_change_media_back_and_forth);
  TEST(test_change_media_while_loading);
  TEST(test_change_media_while_loading_later);
  TEST(test_change_media_async_aborted);
  TEST(test_change_media_client_error);
#endif

  TEST(test_change_media_from_hash_required_fields);
  TEST(test_change_media_from_hash_no_game_loaded);
  TEST(test_change_media_from_hash_same_game);
  TEST(test_change_media_from_hash_known_game);
  TEST(test_change_media_from_hash_unknown_game);
  TEST(test_change_media_from_hash_back_and_forth);
  TEST(test_change_media_from_hash_while_loading);
  TEST(test_change_media_from_hash_while_loading_later);
  TEST(test_change_media_from_hash_async_aborted);
  TEST(test_change_media_from_hash_client_error);

  TEST_SUITE_END();
}
