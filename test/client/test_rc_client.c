#include "rc_client.h"

#include "../../src/runtime/rc_trigger.h"

#include "../../src/client/rc_client_game.h"
#include "../../src/client/rc_client_internal.h"
#include "../../src/util/rc_version.h"

#include "client_data.h"
#include "mock_client.h"

#include "../test_framework.h"

#if defined(_WIN32)
#include <Windows.h>
#elif defined(__unix__) && __STDC_VERSION__ >= 199309L
#include <unistd.h>
#else
#define RC_NO_SLEEP
#endif

/* ----- fetch hash library ----- */

static void test_fetch_hash_library_response(int result, const char* error_message,
  rc_client_hash_library_t* list, rc_client_t* client, void* callback_userdata)
{
  rc_client_callback_expect_success(result, error_message, client, callback_userdata);
  if (result != RC_OK)
    return;

  ASSERT_NUM_EQUALS(list->num_entries, 3);
  ASSERT_NUM_EQUALS(list->entries[0].game_id, 1);
  ASSERT_STR_EQUALS(list->entries[0].hash, "aabbccddeeff00112233445566778899");
  ASSERT_NUM_EQUALS(list->entries[1].game_id, 1);
  ASSERT_STR_EQUALS(list->entries[1].hash, "99aabbccddeeff001122334455667788");
  ASSERT_NUM_EQUALS(list->entries[2].game_id, 2);
  ASSERT_STR_EQUALS(list->entries[2].hash, "8899aabbccddeeff0011223344556677");

  rc_client_destroy_hash_library(list);
}

static void test_fetch_hash_library(void)
{
  g_client = mock_client_not_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=hashlibrary&c=1", "{\"Success\":true,\"MD5List\":{"
    "\"aabbccddeeff00112233445566778899\":1,"
    "\"99aabbccddeeff001122334455667788\":1,"
    "\"8899aabbccddeeff0011223344556677\":2"
    "}}");

  rc_client_begin_fetch_hash_library(g_client, 1, test_fetch_hash_library_response, g_callback_userdata);
  rc_client_destroy(g_client);
}

/* ----- fetch game titles ----- */

static void test_fetch_game_titles_response(int result, const char* error_message,
  rc_client_game_title_list_t* list, rc_client_t* client, void* callback_userdata)
{
  rc_client_callback_expect_success(result, error_message, client, callback_userdata);
  if (result != RC_OK)
    return;

  ASSERT_NUM_EQUALS(list->num_entries, 3);
  ASSERT_NUM_EQUALS(list->entries[0].game_id, 3);
  ASSERT_STR_EQUALS(list->entries[0].title, "Game Name 3");
  ASSERT_STR_EQUALS(list->entries[0].badge_name, "010003");
  ASSERT_STR_EQUALS(list->entries[0].badge_url, "https://media.retroachievements.org/Images/010003.png"); /* default media host */
  ASSERT_NUM_EQUALS(list->entries[1].game_id, 4);
  ASSERT_STR_EQUALS(list->entries[1].title, "Game Name 4");
  ASSERT_STR_EQUALS(list->entries[1].badge_name, "010004");
  ASSERT_STR_EQUALS(list->entries[1].badge_url, "http://media.retroachievements.org/Images/010004.png");
  ASSERT_NUM_EQUALS(list->entries[2].game_id, 7);
  ASSERT_STR_EQUALS(list->entries[2].title, "Game Name 7");
  ASSERT_STR_EQUALS(list->entries[2].badge_name, "010007");
  ASSERT_STR_EQUALS(list->entries[2].badge_url, "localhost/Images/010007.png");

  rc_client_destroy_game_title_list(list);
}

static void test_fetch_game_titles(void)
{
  const uint32_t game_ids[] = { 3, 4, 7 };
  g_client = mock_client_not_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=gameinfolist&g=3,4,7", "{\"Success\":true,\"Response\":["
    "{\"ID\": 3, \"Title\":\"Game Name 3\", \"ImageIcon\": \"/Images/010003.png\"}," /* ImageUrl explicitly not returned to test auto-populating logic */
    "{\"ID\": 4, \"Title\":\"Game Name 4\", \"ImageIcon\": \"/Images/010004.png\", \"ImageUrl\": \"http:\\/\\/media.retroachievements.org\\/Images\\/010004.png\"},"
    "{\"ID\": 7, \"Title\":\"Game Name 7\", \"ImageIcon\": \"/Images/010007.png\", \"ImageUrl\": \"localhost\\/Images\\/010007.png\"}"
    "]}");

  rc_client_begin_fetch_game_titles(g_client, game_ids, 3, test_fetch_game_titles_response, g_callback_userdata);
  rc_client_destroy(g_client);
}

/* ----- fetch game list ----- */

static void test_fetch_game_list_response(int result, const char* error_message,
  rc_client_game_list_t* list, rc_client_t* client, void* callback_userdata)
{
  rc_client_callback_expect_success(result, error_message, client, callback_userdata);
  if (result != RC_OK)
    return;

  ASSERT_NUM_EQUALS(list->num_entries, 2);
  ASSERT_NUM_EQUALS(list->entries[0].id, 3);
  ASSERT_STR_EQUALS(list->entries[0].name, "Game Name 3");
  ASSERT_STR_EQUALS(list->entries[0].image_name, "010003");
  ASSERT_STR_EQUALS(list->entries[0].image_url, "https://media.retroachievements.org/Images/010003.png");
  ASSERT_NUM_EQUALS(list->entries[0].num_achievements, 10);
  ASSERT_NUM_EQUALS(list->entries[0].num_leaderboards, 2);
  ASSERT_NUM_EQUALS(list->entries[0].points, 50);
  ASSERT_NUM_EQUALS(list->entries[0].num_supported_hashes, 2);
  ASSERT_STR_EQUALS(list->entries[0].supported_hashes[0], "00112233445566778899aabbccddeeff");
  ASSERT_STR_EQUALS(list->entries[0].supported_hashes[1], "112233445566778899aabbccddeeff00");
  ASSERT_NUM_EQUALS(list->entries[0].num_unsupported_hashes, 1);
  ASSERT_STR_EQUALS(list->entries[0].unsupported_hashes[0], "2233445566778899aabbccddeeff0011");

  ASSERT_NUM_EQUALS(list->entries[1].id, 4);
  ASSERT_STR_EQUALS(list->entries[1].name, "Game Name 4");
  ASSERT_STR_EQUALS(list->entries[1].image_name, "010004");
  ASSERT_STR_EQUALS(list->entries[1].image_url, "http://media.retroachievements.org/Images/010004.png");
  ASSERT_NUM_EQUALS(list->entries[1].num_achievements, 20);
  ASSERT_NUM_EQUALS(list->entries[1].num_leaderboards, 0);
  ASSERT_NUM_EQUALS(list->entries[1].points, 100);
  ASSERT_NUM_EQUALS(list->entries[1].num_supported_hashes, 1);
  ASSERT_STR_EQUALS(list->entries[1].supported_hashes[0], "33445566778899aabbccddeeff001122");
  ASSERT_NUM_EQUALS(list->entries[1].num_unsupported_hashes, 0);
  ASSERT_PTR_NULL(list->entries[1].unsupported_hashes);

  rc_client_destroy_game_list(list);
}

static void test_fetch_game_list(void)
{
  g_client = mock_client_not_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=systemgames&s=17", "{\"Success\":true,\"Response\":["
    "{\"ID\": 3, \"Title\":\"Game Name 3\", \"ImageIcon\": \"\\/Images\\/010003.png\","
      "\"ImageUrl\": \"https:\\/\\/media.retroachievements.org\\/Images\\/010003.png\","
      "\"NumAchievements\":10,\"NumLeaderboards\":2,\"Points\":50,"
      "\"SupportedHashes\":[\"00112233445566778899aabbccddeeff\",\"112233445566778899aabbccddeeff00\"],"
      "\"UnsupportedHashes\":[\"2233445566778899aabbccddeeff0011\"]},"
    "{\"ID\": 4, \"Title\":\"Game Name 4\", \"ImageIcon\": \"\\/Images\\/010004.png\","
      "\"ImageUrl\": \"http:\\/\\/media.retroachievements.org\\/Images\\/010004.png\","
      "\"NumAchievements\":20,\"NumLeaderboards\":0,\"Points\":100,"
      "\"SupportedHashes\":[\"33445566778899aabbccddeeff001122\"]}"
    "]}");

  rc_client_begin_fetch_game_list(g_client, 17, test_fetch_game_list_response, g_callback_userdata);
  rc_client_destroy(g_client);
}

/* ----- all user progress ----- */

static void test_fetch_all_user_progress_response(int result, const char* error_message,
  rc_client_all_user_progress_t* list, rc_client_t* client, void* callback_userdata)
{
  rc_client_callback_expect_success(result, error_message, client, callback_userdata);
  if (result != RC_OK)
    return;

  ASSERT_NUM_EQUALS(list->num_entries, 4);
  ASSERT_NUM_EQUALS(list->entries[0].game_id, 10);
  ASSERT_NUM_EQUALS(list->entries[0].num_achievements, 11);
  ASSERT_NUM_EQUALS(list->entries[0].num_unlocked_achievements, 0);
  ASSERT_NUM_EQUALS(list->entries[0].num_unlocked_achievements_hardcore, 0);
  ASSERT_NUM_EQUALS(list->entries[1].game_id, 20);
  ASSERT_NUM_EQUALS(list->entries[1].num_achievements, 21);
  ASSERT_NUM_EQUALS(list->entries[1].num_unlocked_achievements, 22);
  ASSERT_NUM_EQUALS(list->entries[1].num_unlocked_achievements_hardcore, 0);
  ASSERT_NUM_EQUALS(list->entries[2].game_id, 30);
  ASSERT_NUM_EQUALS(list->entries[2].num_achievements, 31);
  ASSERT_NUM_EQUALS(list->entries[2].num_unlocked_achievements, 32);
  ASSERT_NUM_EQUALS(list->entries[2].num_unlocked_achievements_hardcore, 33);
  ASSERT_NUM_EQUALS(list->entries[3].game_id, 40);
  ASSERT_NUM_EQUALS(list->entries[3].num_achievements, 41);
  ASSERT_NUM_EQUALS(list->entries[3].num_unlocked_achievements, 0);
  ASSERT_NUM_EQUALS(list->entries[3].num_unlocked_achievements_hardcore, 43);

  rc_client_destroy_all_user_progress(list);
}

static void test_fetch_all_user_progress(void)
{
  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=allprogress&u=Username&t=ApiToken&c=1", "{\"Success\":true,\"Response\":{\"10\":{\"Achievements\":11},"
      "\"20\":{\"Achievements\":21,\"Unlocked\":22},"
      "\"30\":{\"Achievements\":31,\"Unlocked\":32,\"UnlockedHardcore\":33},"
      "\"40\":{\"Achievements\":41,\"UnlockedHardcore\":43}}}");

  rc_client_begin_fetch_all_user_progress(g_client, 1, test_fetch_all_user_progress_response, g_callback_userdata);
  rc_client_destroy(g_client);
}

/* ----- clock ----- */

static void test_clock_get_now_millisecs(void)
{
  rc_client_t* client = rc_client_create(NULL, NULL);
  rc_get_time_millisecs_func_t get_millisecs = client->callbacks.get_time_millisecs;

#ifdef RC_NO_SLEEP
  rc_clock_t time1;
  ASSERT_PTR_NOT_NULL(get_millisecs);
  time1 = get_millisecs(client);
  ASSERT_NUM_NOT_EQUALS(time1, 0);
#else
  rc_clock_t time1, time2, diff;
  time_t first = time(NULL), now;

  do {
    ASSERT_PTR_NOT_NULL(get_millisecs);
    time1 = get_millisecs(client);
    ASSERT_NUM_NOT_EQUALS(time1, 0);

#if defined(_WIN32)
    Sleep(50);
#else
    usleep(50000);
#endif

    time2 = get_millisecs(client);
    ASSERT_NUM_NOT_EQUALS(time2, 0);
    diff = time2 - time1;

    ASSERT_NUM_GREATER(diff, 49);
    if (diff < 100)
      break;

    now = time(NULL);
    if (now - first >= 3) {
      ASSERT_FAIL("could not get a 50ms sleep interval within 3 seconds");
      break;
    }

  } while (1);
#endif

  rc_client_destroy(client);
}

/* ----- settings ----- */

static void test_set_hardcore_disable(void)
{
  const rc_client_achievement_t* achievement;
  const rc_client_leaderboard_t* leaderboard;
  const rc_trigger_t* trigger;

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, unlock_5501h_and_5502);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);

  achievement = rc_client_get_achievement_info(g_client, 5501);
  ASSERT_PTR_NOT_NULL(achievement);
  trigger = ((rc_client_achievement_info_t*)achievement)->trigger;
  ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_TRIGGERED);

  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  trigger = ((rc_client_achievement_info_t*)achievement)->trigger;
  ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_CASUAL);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_WAITING);

  ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, 1); /* only 5502 should be active*/

  leaderboard = rc_client_get_leaderboard_info(g_client, 4401);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(g_client->game->runtime.lboard_count, 1);

  rc_client_set_hardcore_enabled(g_client, 0);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 0);
  ASSERT_NUM_EQUALS(g_client->game->waiting_for_reset, 0);

  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  trigger = ((rc_client_achievement_info_t*)achievement)->trigger;

  ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_CASUAL);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_TRIGGERED);
  ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, 0); /* 5502 should not be active*/

  leaderboard = rc_client_get_leaderboard_info(g_client, 4401);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_INACTIVE);
  ASSERT_NUM_EQUALS(g_client->game->runtime.lboard_count, 0);

  rc_client_destroy(g_client);
}

static void test_set_hardcore_disable_active_tracker(void)
{
  const rc_client_leaderboard_t* leaderboard;
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, unlock_5501h_and_5502);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  mock_memory(memory, sizeof(memory));

  rc_client_do_frame(g_client);

  memory[0x0C] = 1;
  memory[0x0E] = 25;
  reset_mock_events();
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 2);

  event = find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 4401);
  ASSERT_PTR_NOT_NULL(event);

  event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
  ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000025");

  leaderboard = rc_client_get_leaderboard_info(g_client, 4401);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);

  reset_mock_events();
  rc_client_set_hardcore_enabled(g_client, 0);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 0);
  ASSERT_NUM_EQUALS(g_client->game->waiting_for_reset, 0);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

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
  mock_client_load_game_casual(patchdata_2ach_1lbd, unlock_5501h_and_5502);

  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 0);

  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_CASUAL);
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
  reset_mock_events();
  rc_client_set_hardcore_enabled(g_client, 1);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(g_client->game->waiting_for_reset, 1);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_RESET, 0));

  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_CASUAL);
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
  reset_mock_events();
  rc_client_set_hardcore_enabled(g_client, 1);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

  rc_client_destroy(g_client);
}

static void test_set_hardcore_enable_encore_mode(void)
{
  const rc_client_achievement_t* achievement;
  rc_client_achievement_info_t* achievement_info;

  g_client = mock_client_logged_in();
  rc_client_set_encore_mode_enabled(g_client, 1);
  mock_client_load_game(patchdata_2ach_1lbd, unlock_5501h_and_5502);

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
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_CASUAL);
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
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_CASUAL);
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
  achievement_info->public_.state = RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED;
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
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_CASUAL);
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
  mock_client_load_game(patchdata_2ach_1lbd, unlock_5501h_and_5502);

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
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_CASUAL);
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
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_CASUAL);
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
  mock_client_load_game(patchdata_2ach_1lbd, unlock_5501h_and_5502);

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
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_CASUAL);
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
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_CASUAL);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  }

  rc_client_destroy(g_client);
}

static void test_get_user_agent_clause(void)
{
  char expected_clause[] = "rcheevos/" RCHEEVOS_VERSION_STRING;
  char buffer[64];

  g_client = mock_client_not_logged_in();
  ASSERT_NUM_EQUALS(rc_client_get_user_agent_clause(g_client, buffer, sizeof(buffer)), sizeof(expected_clause) - 1);
  ASSERT_STR_EQUALS(buffer, expected_clause);

  /* snprintf will return the number of characters it wants, even if the buffer is too small,
   * but will only fill as much of the buffer is available */
  ASSERT_NUM_EQUALS(rc_client_get_user_agent_clause(g_client, buffer, 8), sizeof(expected_clause) - 1);
  ASSERT_STR_EQUALS(buffer, "rcheevo");

  rc_client_destroy(g_client);
}

static void test_version(void)
{
  ASSERT_STR_EQUALS(rc_version_string(), RCHEEVOS_VERSION_STRING);
  ASSERT_NUM_EQUALS(rc_version(), RCHEEVOS_VERSION);
}

/* ----- harness ----- */

void test_client(void) {
  TEST_SUITE_BEGIN();

  /* hash library */
  TEST(test_fetch_hash_library);

  /* game titles */
  TEST(test_fetch_game_titles);

  /* game list */
  TEST(test_fetch_game_list);

  /* all user progress */
  TEST(test_fetch_all_user_progress);

  /* clock */
  TEST(test_clock_get_now_millisecs);

  /* settings */
  TEST(test_set_hardcore_disable);
  TEST(test_set_hardcore_disable_active_tracker);
  TEST(test_set_hardcore_enable);
  TEST(test_set_hardcore_enable_no_game_loaded);
  TEST(test_set_hardcore_enable_encore_mode);
  TEST(test_set_encore_mode_enable);
  TEST(test_set_encore_mode_disable);

  TEST(test_get_user_agent_clause);
  TEST(test_version);

  TEST_SUITE_END();
}
