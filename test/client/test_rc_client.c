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
