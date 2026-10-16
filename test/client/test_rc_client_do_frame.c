#include "rc_client.h"

#include "../../src/runtime/rc_condition.h"
#include "../../src/runtime/rc_condset.h"
#include "../../src/runtime/rc_lboard.h"
#include "../../src/runtime/rc_trigger.h"


#include "../../src/client/rc_client_game.h"
#include "../../src/client/rc_client_internal.h"

#include "client_data.h"
#include "mock_client.h"

#include "../test_framework.h"

/* ----- helpers ----- */

static void _assert_achievement_state(rc_client_t* client, uint32_t id, int expected_state)
{
  const rc_client_achievement_t* achievement = rc_client_get_achievement_info(client, id);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->state, expected_state);
}
#define assert_achievement_state(client, id, expected_state) ASSERT_HELPER(_assert_achievement_state(client, id, expected_state), "assert_achievement_state")

/* ----- do frame ----- */

static void test_do_frame_bounds_check_system(void)
{
  const uint32_t memory_size = 0x10010; /* provide more memory than system expects */
  uint8_t* memory = (uint8_t*)calloc(1, memory_size);
  ASSERT_PTR_NOT_NULL(memory);

  g_client = mock_client_game_loaded(patchdata_bounds_check_system, no_unlocks);

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
  const rc_trigger_t* trigger;
  uint8_t memory[8] = { 0,0,0,0,0,0,0,0 };
  g_client = mock_client_game_loaded(patchdata_bounds_check_8, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    /* all addresses are valid according to the system, so no achievements should be disabled yet. */
    assert_achievement_state(g_client, 808, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    trigger = ((rc_client_achievement_info_t*)rc_client_get_achievement_info(g_client, 808))->trigger;
    ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_WAITING);

    /* limit the memory that's actually exposed and try to process a frame */
    mock_memory(memory, sizeof(memory));
    rc_client_do_frame(g_client);

    assert_achievement_state(g_client, 408, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_client, 508, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_client, 608, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_client, 708, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    assert_achievement_state(g_client, 808, RC_CLIENT_ACHIEVEMENT_STATE_DISABLED); /* out of bounds*/
    ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_DISABLED);

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

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=8&h=1&m=0123456789ABCDEF&v=da80b659c2b858e13ddd97077647b217",
        "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":11}");

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    memory[8] = 8;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 8);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 8));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 1);
    ASSERT_NUM_EQUALS(g_client->user.score, 5432);
    ASSERT_NUM_EQUALS(g_client->user.score_casual, 777);

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_trigger_already_awarded(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=8&h=1&m=0123456789ABCDEF&v=da80b659c2b858e13ddd97077647b217",
        "{\"Success\":false,\"Error\":\"User already has hardcore and regular achievements awarded.\",\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":11}");

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    memory[8] = 8;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 8);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 8));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 1);
    ASSERT_NUM_EQUALS(g_client->user.score, 5432);
    ASSERT_NUM_EQUALS(g_client->user.score_casual, 777);

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_trigger_server_error(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=8&h=1&m=0123456789ABCDEF&v=da80b659c2b858e13ddd97077647b217",
        "{\"Success\":false,\"Error\":\"Achievement not found\"}");

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    memory[8] = 8;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 2);

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
    ASSERT_NUM_EQUALS(event->server_error->result, RC_API_FAILURE);
    ASSERT_NUM_EQUALS(event->server_error->related_id, 8);

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_trigger_while_spectating(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    ASSERT_FALSE(rc_client_get_spectator_mode_enabled(g_client));
    rc_client_set_spectator_mode_enabled(g_client, 1);
    ASSERT_TRUE(rc_client_get_spectator_mode_enabled(g_client));

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=8&h=1&m=0123456789ABCDEF&v=da80b659c2b858e13ddd97077647b217",
        "{\"Success\":false,\"Error\":\"Achievement should not have been unlocked in spectating mode\"}");

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    memory[8] = 8;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

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

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    rc_client_set_spectator_mode_enabled(g_client, 0);
    ASSERT_FALSE(rc_client_get_spectator_mode_enabled(g_client));
  }

  rc_client_destroy(g_client);
}

static int rc_client_callback_deny_unlock(uint32_t achievement_id, rc_client_t* client)
{
  (void)achievement_id;
  (void)client;

  return 0;
}

static int rc_client_callback_allow_unlock(uint32_t achievement_id, rc_client_t* client)
{
  (void)achievement_id;
  (void)client;

  return 1;
}

static void test_do_frame_achievement_trigger_blocked(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  uint32_t num_active;
  const char* api_call8 = "r=awardachievement&u=Username&t=ApiToken&a=8&h=1&m=0123456789ABCDEF&v=da80b659c2b858e13ddd97077647b217";
  const char* api_call9 = "r=awardachievement&u=Username&t=ApiToken&a=9&h=1&m=0123456789ABCDEF&v=6d989ee0f408660a87d6440a13563bf6";
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  g_client->callbacks.can_submit_achievement_unlock = rc_client_callback_deny_unlock;

  ASSERT_PTR_NOT_NULL(g_client->game);
  mock_memory(memory, sizeof(memory));
  num_active = g_client->game->runtime.trigger_count;

  mock_api_response(api_call8,
    "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":11}");
  mock_api_response(api_call9,
    "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":11}");

  reset_mock_events();
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

  memory[8] = 8;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

  event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 8);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
  ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
  ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
  ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
  ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 8));

  ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 1);
  ASSERT_NUM_EQUALS(g_client->user.score, 12350); /* 12345+5 - not updated by server response that didn't happen */
  ASSERT_NUM_EQUALS(g_client->user.score_casual, 0);

  reset_mock_events();
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

  assert_api_not_called(api_call8);

  g_client->callbacks.can_submit_achievement_unlock = rc_client_callback_allow_unlock;

  memory[9] = 9;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

  event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 9);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
  ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
  ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
  ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
  ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 9));

  ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 2);
  ASSERT_NUM_EQUALS(g_client->user.score, 5432);
  ASSERT_NUM_EQUALS(g_client->user.score_casual, 777);

  assert_api_called(api_call9);

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_trigger_warning(void)
{
  rc_client_event_t* event;

  g_client = mock_client_game_loaded(patchdata_warning, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    rc_client_achievement_info_t* ach = (rc_client_achievement_info_t*)rc_client_get_achievement_info(g_client, 101000001);

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    ach->trigger->requirement->conditions->current_hits = 299; /* will trigger at 300 */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 101000001);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, &ach->public_);

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 1);
    ASSERT_NUM_EQUALS(g_client->user.score, 12345);
    ASSERT_NUM_EQUALS(g_client->user.score_casual, 0);

    assert_api_not_called("r=awardachievement&u=Username&t=ApiToken&a=101000001&h=1&m=0123456789ABCDEF&v=589baefac51bd5931234fa9ade42460f");

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_trigger_automatic_retry(const char* response, int status_code)
{
  const char* unlock_request_params = "r=awardachievement&u=Username&t=ApiToken&a=5501&h=1&m=0123456789ABCDEF&v=9b9bdf5501eb6289a6655affbcc695e6";
  const char* unlock_request_params1 = "r=awardachievement&u=Username&t=ApiToken&a=5501&h=1&m=0123456789ABCDEF&o=1&v=4b4af09722aeb0e8a16c152f9a646281";
  const char* unlock_request_params3 = "r=awardachievement&u=Username&t=ApiToken&a=5501&h=1&m=0123456789ABCDEF&o=3&v=d5bc287a030084aa77a911b6c2c5fc96";
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);
  mock_client_enable_async(g_client);

  /* discard the queued ping to make finding the retry easier */
  g_client->state.scheduled_callbacks = NULL;

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    memory[1] = 3;
    memory[2] = 7;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 5501);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 5501));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 1);

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* first failure will immediately requeue the request */
    async_api_error(unlock_request_params, response, status_code);
    assert_api_pending(unlock_request_params);
    ASSERT_PTR_NULL(g_client->state.scheduled_callbacks);
    rc_client_idle(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* second failure will queue it */
    async_api_error(unlock_request_params, response, status_code);
    assert_api_call_count(unlock_request_params, 0);
    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);

    rc_client_idle(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_DISCONNECTED, 0));
    reset_mock_events();

    /* advance time so queued request gets processed */
    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 1 * 1000);
    g_now += 1 * 1000;
    rc_client_idle(g_client);
    assert_api_pending(unlock_request_params1);
    ASSERT_PTR_NULL(g_client->state.scheduled_callbacks);

    /* third failure will requeue it */
    async_api_error(unlock_request_params1, response, status_code);
    assert_api_call_count(unlock_request_params, 0);
    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);

    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 2 * 1000);
    g_now += 2 * 1000;

    rc_client_idle(g_client);
    assert_api_pending(unlock_request_params3);
    ASSERT_PTR_NULL(g_client->state.scheduled_callbacks);

    /* success should not requeue it and update player score */
    async_api_response(unlock_request_params3, "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":11}");
    ASSERT_PTR_NULL(g_client->state.scheduled_callbacks);

    ASSERT_NUM_EQUALS(g_client->user.score, 5432);
    ASSERT_NUM_EQUALS(g_client->user.score_casual, 777);

    /* reconnected event should be pending, watch for it */
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);
    rc_client_idle(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_RECONNECTED, 0));
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_trigger_automatic_retry_empty(void)
{
  test_do_frame_achievement_trigger_automatic_retry("", 200);
}

static void test_do_frame_achievement_trigger_automatic_retry_429(void)
{
  test_do_frame_achievement_trigger_automatic_retry(response_429, 429);
}

static void test_do_frame_achievement_trigger_automatic_retry_502(void)
{
  test_do_frame_achievement_trigger_automatic_retry(response_502, 502);
}

static void test_do_frame_achievement_trigger_automatic_retry_503(void)
{
  test_do_frame_achievement_trigger_automatic_retry(response_503, 503);
}

static void test_do_frame_achievement_trigger_automatic_retry_custom_timeout(void)
{
  test_do_frame_achievement_trigger_automatic_retry("Request has timed out.", RC_API_SERVER_RESPONSE_RETRYABLE_CLIENT_ERROR);
}

static void test_do_frame_achievement_trigger_automatic_retry_generic_empty_response(void)
{
  test_do_frame_achievement_trigger_automatic_retry("", 0);
}

static void test_do_frame_achievement_trigger_subset(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_subset, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=8&h=1&m=0123456789ABCDEF&v=da80b659c2b858e13ddd97077647b217",
        "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":11}");

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    memory[8] = 8;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 8);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 8));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 1);
    ASSERT_NUM_EQUALS(g_client->user.score, 5432);
    ASSERT_NUM_EQUALS(g_client->user.score_casual, 777);

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=5501&h=1&m=0123456789ABCDEF&v=9b9bdf5501eb6289a6655affbcc695e6",
        "{\"Success\":true,\"Score\":5437,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":11}");

    memory[0x17] = 7;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 5501);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 5501));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 2);
    ASSERT_NUM_EQUALS(g_client->user.score, 5437);
    ASSERT_NUM_EQUALS(g_client->user.score_casual, 777);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_trigger_base_and_subset(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_subset, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=8&h=1&m=0123456789ABCDEF&v=da80b659c2b858e13ddd97077647b217",
      "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":11}");
    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=5501&h=1&m=0123456789ABCDEF&v=9b9bdf5501eb6289a6655affbcc695e6",
      "{\"Success\":true,\"Score\":5437,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":11}");

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    memory[8] = 8; /* trigger achievement 8 */
    memory[0x17] = 7; /* trigger achievement 5501 */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 2);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 8);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 8));

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 5501);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 5501));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 2);
    ASSERT_NUM_EQUALS(g_client->user.score, 5437);
    ASSERT_NUM_EQUALS(g_client->user.score_casual, 777);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_trigger_rarity(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive_typed, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game)
  {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=8&h=1&m=0123456789ABCDEF&v=da80b659c2b858e13ddd97077647b217",
      "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":11}");

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    memory[8] = 8;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 8);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->type, RC_CLIENT_ACHIEVEMENT_TYPE_PROGRESSION);
    ASSERT_FLOAT_EQUALS(event->achievement->rarity, 86.0f);
    ASSERT_FLOAT_EQUALS(event->achievement->rarity_hardcore, 73.1f);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 8));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 1);
    ASSERT_NUM_EQUALS(g_client->user.score, 5432);
    ASSERT_NUM_EQUALS(g_client->user.score_casual, 777);

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_measured(void)
{
  const rc_client_achievement_t* achievement;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=70&h=1&m=0123456789ABCDEF&v=61e40027573e2cde88b49d27f6804879",
        "{\"Success\":true,\"Score\":5432,\"AchievementID\":70,\"AchievementsRemaining\":11}");
    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=71&h=1&m=0123456789ABCDEF&v=3a8d55b81d391557d5111306599a2b0d",
        "{\"Success\":true,\"Score\":5432,\"AchievementID\":71,\"AchievementsRemaining\":11}");

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    memory[0x10] = 0x39; memory[0x11] = 0x30; /* 12345 */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1); /* one PROGRESS_INDICATOR_SHOW event */

    achievement = rc_client_get_achievement_info(g_client, 70);
    ASSERT_PTR_NOT_NULL(achievement);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_STR_EQUALS(achievement->measured_progress, "12,345/100,000");

    achievement = rc_client_get_achievement_info(g_client, 71);
    ASSERT_PTR_NOT_NULL(achievement);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_STR_EQUALS(achievement->measured_progress, "12%");

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active);

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* increment measured value - raw counter will report progress change, percentage will not */
    memory[0x10] = 0x3A; /* 12346 */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1); /* one PROGRESS_INDICATOR_SHOW event */

    achievement = rc_client_get_achievement_info(g_client, 70);
    ASSERT_PTR_NOT_NULL(achievement);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_STR_EQUALS(achievement->measured_progress, "12,346/100,000");

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* increment measured value - raw counter will report progress change, percentage will not */
    memory[0x11] = 0x33; /* 13114 */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1); /* one PROGRESS_INDICATOR_SHOW event */

    achievement = rc_client_get_achievement_info(g_client, 70);
    ASSERT_PTR_NOT_NULL(achievement);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_STR_EQUALS(achievement->measured_progress, "13,114/100,000");

    achievement = rc_client_get_achievement_info(g_client, 71);
    ASSERT_PTR_NOT_NULL(achievement);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_STR_EQUALS(achievement->measured_progress, "13%");

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active);

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* trigger measured achievements - progress becomes blank */
    memory[0x10] = 0xA0; memory[0x11] = 0x86; memory[0x12] = 0x01; /* 100000 */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 2); /* two TRIGGERED events, and no PROGRESS_INDICATOR_SHOW events */

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

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=6&h=1&m=0123456789ABCDEF&v=65206f4290098ecd30c7845e895057d0",
        "{\"Success\":true,\"Score\":5432,\"AchievementID\":6,\"AchievementsRemaining\":11}");

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    memory[0x06] = 3;                         /* 3/6 */
    memory[0x11] = 0xC3; memory[0x10] = 0x4F; /* 49999/100000 */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

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
    ASSERT_STR_EQUALS(achievement->measured_progress, "49,999/100,000");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 49.999);

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* any change will trigger the popup - even dropping */
    memory[0x10] = 0x4E; /* 49998 */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_UPDATE, 70);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 70));
    ASSERT_STR_EQUALS(event->achievement->measured_progress, "49,998/100,000");

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* don't trigger popup when value changes to 0 as the measured_progress string will be blank */
    memory[0x06] = 0; /* 0 */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    achievement = rc_client_get_achievement_info(g_client, 6);
    ASSERT_STR_EQUALS(achievement->measured_progress, "");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 0.0);

    /* both at 50%, only report first */
    memory[0x06] = 3;                         /* 3/6 */
    memory[0x11] = 0xC3; memory[0x10] = 0x50; /* 50000/100000 */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_UPDATE, 6);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 6));
    ASSERT_STR_EQUALS(event->achievement->measured_progress, "3/6");

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* second slightly ahead */
    memory[0x6] = 4;                                             /* 4/6 */
    memory[0x12] = 1;  memory[0x11] = 0x04; memory[0x10] = 0x6B; /* 66667/100000 */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_UPDATE, 70);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 70));
    ASSERT_STR_EQUALS(event->achievement->measured_progress, "66,667/100,000");
    
    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* don't show popup on trigger */
    memory[0x06] = 6;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 6);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 6));
    ASSERT_STR_EQUALS(event->achievement->measured_progress, "");

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    ASSERT_PTR_NOT_NULL(g_client->game->progress_tracker.hide_callback);
    g_now += 2 * 1000;

    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE, 0));
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_measured_progress_reshown(void)
{
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  ASSERT_PTR_NOT_NULL(g_client->game);
  mock_memory(memory, sizeof(memory));

  reset_mock_events();
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

  memory[0x06] = 3;                         /* 3/6 */
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 1);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW, 6));
  reset_mock_events();

  /* should be two callbacks queued - hiding the progress indicator, and the rich presence update */
  ASSERT_PTR_EQUALS(g_client->state.scheduled_callbacks, g_client->game->progress_tracker.hide_callback);
  ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks->next);
  ASSERT_PTR_NULL(g_client->state.scheduled_callbacks->next->next);

  /* advance time to hide the progress indicator */
  g_now = g_client->game->progress_tracker.hide_callback->when;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 1);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE, 0));
  reset_mock_events();

  /* only the rich presence update should be scheduled */
  ASSERT_PTR_NULL(g_client->state.scheduled_callbacks->next);

  /* advance time to just before the rich presence update */
  g_now = g_client->state.scheduled_callbacks->when - 10;

  /* reschedule the progress indicator */
  memory[0x06] = 4;                         /* 4/6 */
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 1);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW, 6));
  reset_mock_events();

  /* should be two callbacks queued - rich presence update, then hiding the progress indicator */
  ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
  ASSERT_PTR_EQUALS(g_client->state.scheduled_callbacks->next, g_client->game->progress_tracker.hide_callback);
  ASSERT_PTR_NULL(g_client->state.scheduled_callbacks->next->next);

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_challenge_indicator(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=7&h=1&m=0123456789ABCDEF&v=c39308ba325ba4a72919b081fb18fdd4",
        "{\"Success\":true,\"Score\":5432,\"AchievementID\":7,\"AchievementsRemaining\":11}");

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    memory[1] = 1; /* show indicator */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 7));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active);

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    memory[1] = 0; /* hide indicator */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE, 7);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 7));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active);

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    memory[1] = 1; /* show indicator */
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
    ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 7));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active);

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* trigger achievement - expect both hide and trigger events. both should have triggered achievement data */
    memory[7] = 7;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 2);

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

static void test_do_frame_achievement_challenge_indicator_primed_while_reset(void)
{
  static const char* patchdata = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"\",\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":["
       "{\"ID\":7,\"Title\":\"Ach1\",\"Description\":\"Desc1\",\"Flags\":3,\"Points\":5,"
        "\"MemAddr\":\"0xH0001=3_T:0xH0002=4_R:0xH0003=3\",\"Author\":\"User1\",\"BadgeName\":\"00234\","
        "\"Created\":1367266583,\"Modified\":1376929305}"
      "],"
      "\"Leaderboards\":[]"
    "}]}";

  rc_client_event_t* event;
  rc_client_achievement_info_t* achievement;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata, no_unlocks);
  mock_memory(memory, sizeof(memory));

  mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=7&h=1&m=0123456789ABCDEF&v=c39308ba325ba4a72919b081fb18fdd4",
    "{\"Success\":true,\"Score\":5432,\"AchievementID\":7,\"AchievementsRemaining\":11}");

  reset_mock_events();
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

  achievement = (rc_client_achievement_info_t*)rc_client_get_achievement_info(g_client, 7);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->trigger->state, RC_TRIGGER_STATE_ACTIVE);

  memory[1] = 3; /* primed */
  memory[3] = 3; /* but reset */
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);
  ASSERT_NUM_EQUALS(achievement->trigger->state, RC_TRIGGER_STATE_ACTIVE);

  memory[3] = 0; /* disable reset */

  rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(get_pending_event_count(), 1); /* show indicator event */
  event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
  ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
  ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE);
  ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 7));

  ASSERT_NUM_EQUALS(achievement->trigger->state, RC_TRIGGER_STATE_PRIMED);

  memory[3] = 3; /* reset active */

  reset_mock_events();
  rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(get_pending_event_count(), 1); /* hide indicator event */
  event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE, 7);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
  ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
  ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
  ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 7));

  rc_client_destroy(g_client);
}

static void test_do_frame_achievement_challenge_indicator_primed_while_reset_next(void)
{
  static const char* patchdata = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"\",\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":["
       "{\"ID\":7,\"Title\":\"Ach1\",\"Description\":\"Desc1\",\"Flags\":3,\"Points\":5,"
        "\"MemAddr\":\"0xH0001=3_T:0xH0002=4_Z:0xH0003=3_P:0xH0006=1.10.\",\"Author\":\"User1\",\"BadgeName\":\"00234\","
        "\"Created\":1367266583,\"Modified\":1376929305}"
      "],"
      "\"Leaderboards\":[]"
    "}]}";

  rc_client_event_t* event;
  rc_client_achievement_info_t* achievement;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata, no_unlocks);
  mock_memory(memory, sizeof(memory));

  mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=7&h=1&m=0123456789ABCDEF&v=c39308ba325ba4a72919b081fb18fdd4",
    "{\"Success\":true,\"Score\":5432,\"AchievementID\":7,\"AchievementsRemaining\":11}");

  memory[6] = 1; /* pause condition will gain a hit, but not enough to pause it */
  reset_mock_events();
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

  achievement = (rc_client_achievement_info_t*)rc_client_get_achievement_info(g_client, 7);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->trigger->state, RC_TRIGGER_STATE_ACTIVE);

  /* ResetNextIf will clear hits on condition 4, _and_ the trigger will be primed at the same time.
   * The ResetNextIf will cause rc_evaluate_trigger to return RESET, but we still want to raise a
   * PRIMED event out of rc_client. */
  memory[1] = 3; /* primed */
  memory[3] = 3; /* reset pause condition */
  rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(get_pending_event_count(), 1); /* show indicator event */
  event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE);
  ASSERT_NUM_EQUALS(event->achievement->unlock_time, 0);
  ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE);
  ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 7));

  ASSERT_NUM_EQUALS(achievement->trigger->state, RC_TRIGGER_STATE_PRIMED);

  memory[3] = 0; /* disable reset, pause counter increases */
  reset_mock_events();
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

  memory[3] = 0; /* re-enable reset. don't expect any events */

  rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

  rc_client_destroy(g_client);
}

static void test_do_frame_mastery(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  mock_client_enable_async(g_client);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    memory[8] = 8;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 8);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 8));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 1);
    ASSERT_NUM_EQUALS(g_client->user.score, 12345+5);
    ASSERT_NUM_EQUALS(g_client->user.score_casual, 0);

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    async_api_response("r=awardachievement&u=Username&t=ApiToken&a=8&h=1&m=0123456789ABCDEF&v=da80b659c2b858e13ddd97077647b217",
        "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":0}");

    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);
    ASSERT_NUM_EQUALS(g_client->user.score, 5432);
    ASSERT_NUM_EQUALS(g_client->user.score_casual, 777);

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

    event = find_event(RC_CLIENT_EVENT_GAME_COMPLETED, 1234);
    ASSERT_PTR_NOT_NULL(event);

    memory[9] = 9;
    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 9);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 9));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 2);
    ASSERT_NUM_EQUALS(g_client->user.score, 5432+5);
    ASSERT_NUM_EQUALS(g_client->user.score_casual, 777);

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    async_api_response("r=awardachievement&u=Username&t=ApiToken&a=9&h=1&m=0123456789ABCDEF&v=6d989ee0f408660a87d6440a13563bf6",
        "{\"Success\":false,\"Error\":\"User already has hardcore and regular achievements awarded.\",\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":9,\"AchievementsRemaining\":0}");

    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);
    ASSERT_NUM_EQUALS(g_client->user.score, 5432);
    ASSERT_NUM_EQUALS(g_client->user.score_casual, 777);

    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_mastery_encore(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  mock_client_enable_async(g_client);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    memory[8] = 8;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 8);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 8));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 1);
    ASSERT_NUM_EQUALS(g_client->user.score, 12345+5);
    ASSERT_NUM_EQUALS(g_client->user.score_casual, 0);

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    async_api_response("r=awardachievement&u=Username&t=ApiToken&a=8&h=1&m=0123456789ABCDEF&v=da80b659c2b858e13ddd97077647b217",
        "{\"Success\":false,\"Error\":\"User already has hardcore and regular achievements awarded.\",\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":0}");

    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);
    ASSERT_NUM_EQUALS(g_client->user.score, 5432);
    ASSERT_NUM_EQUALS(g_client->user.score_casual, 777);

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

    event = find_event(RC_CLIENT_EVENT_GAME_COMPLETED, 1234);
    ASSERT_PTR_NOT_NULL(event);

    memory[9] = 9;
    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 9);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 9));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 2);
    ASSERT_NUM_EQUALS(g_client->user.score, 5432+5);
    ASSERT_NUM_EQUALS(g_client->user.score_casual, 777);

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    async_api_response("r=awardachievement&u=Username&t=ApiToken&a=9&h=1&m=0123456789ABCDEF&v=6d989ee0f408660a87d6440a13563bf6",
        "{\"Success\":false,\"Error\":\"User already has hardcore and regular achievements awarded.\",\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":9,\"AchievementsRemaining\":0}");

    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);
    ASSERT_NUM_EQUALS(g_client->user.score, 5432);
    ASSERT_NUM_EQUALS(g_client->user.score_casual, 777);

    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_mastery_subset(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_subset, unlock_5501_and_5502);
  mock_client_enable_async(g_client);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game)
  {
    const uint32_t num_active = g_client->game->runtime.trigger_count;
    mock_memory(memory, sizeof(memory));

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    memory[0x19] = 9;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

    event = find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 5503);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(event->achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_NOT_EQUALS(event->achievement->unlock_time, 0);
    ASSERT_NUM_EQUALS(event->achievement->bucket, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_PTR_EQUALS(event->achievement, rc_client_get_achievement_info(g_client, 5503));

    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, num_active - 1);
    ASSERT_NUM_EQUALS(g_client->user.score, 12345 + 5);
    ASSERT_NUM_EQUALS(g_client->user.score_casual, 0);

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    async_api_response("r=awardachievement&u=Username&t=ApiToken&a=5503&h=1&m=0123456789ABCDEF&v=50486c3ea9e2e32bb3683017b1488f88",
      "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":5503,\"AchievementsRemaining\":0}");

    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);
    ASSERT_NUM_EQUALS(g_client->user.score, 5432);
    ASSERT_NUM_EQUALS(g_client->user.score_casual, 777);

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

    event = find_event(RC_CLIENT_EVENT_SUBSET_COMPLETED, 2345);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_PTR_EQUALS(event->subset, rc_client_get_subset_info(g_client, 2345));
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_leaderboard_started(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    mock_memory(memory, sizeof(memory));

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    memory[0x0B] = 1;
    memory[0x0E] = 17;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 2);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 44);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000017");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000017");

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_leaderboard_update(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    mock_memory(memory, sizeof(memory));

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* start the leaderboard */
    memory[0x0B] = 1;
    memory[0x0E] = 17;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 2);
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 44));
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* update the leaderboard */
    memory[0x0E] = 18;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000018");

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_leaderboard_failed(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    mock_memory(memory, sizeof(memory));

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* start the leaderboard */
    memory[0x0B] = 1;
    memory[0x0E] = 17;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 2);
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 44));
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* cancel the leaderboard */
    memory[0x0C] = 1;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 2);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_FAILED, 44);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000017");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000017");

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_leaderboard_submit(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=submitlbentry&u=Username&t=ApiToken&i=44&s=17&m=0123456789ABCDEF&v=a27fa205f7f30c8d13d74806ea5425b6",
        "{\"Success\":true,\"Response\":{\"Score\":17,\"BestScore\":23,"
        "\"TopEntries\":[{\"User\":\"Player1\",\"Score\":44,\"Rank\":1},{\"User\":\"Username\",\"Score\":23,\"Rank\":2}],"
        "\"RankInfo\":{\"Rank\":2,\"NumEntries\":\"2\"}}}");

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* start the leaderboard */
    memory[0x0B] = 1;
    memory[0x0E] = 17;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 2);
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 44));
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* submit the leaderboard */
    memory[0x0D] = 1;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 3);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED, 44);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000017");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000017");

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_SCOREBOARD, 44);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->leaderboard_id, 44);
    ASSERT_STR_EQUALS(event->leaderboard_scoreboard->submitted_score, "000017");
    ASSERT_STR_EQUALS(event->leaderboard_scoreboard->best_score, "000023");
    ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->new_rank, 2);
    ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->num_entries, 2);
    ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->num_top_entries, 2);
    ASSERT_STR_EQUALS(event->leaderboard_scoreboard->top_entries[0].username, "Player1");
    ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->top_entries[0].rank, 1);
    ASSERT_STR_EQUALS(event->leaderboard_scoreboard->top_entries[0].score, "000044");
    ASSERT_STR_EQUALS(event->leaderboard_scoreboard->top_entries[1].username, "Username");
    ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->top_entries[1].rank, 2);
    ASSERT_STR_EQUALS(event->leaderboard_scoreboard->top_entries[1].score, "000023");
    ASSERT_PTR_NOT_NULL(event->leaderboard_scoreboard->top_entries);
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_leaderboard_submit_server_error(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=submitlbentry&u=Username&t=ApiToken&i=44&s=17&m=0123456789ABCDEF&v=a27fa205f7f30c8d13d74806ea5425b6",
        "{\"Success\":false,\"Error\":\"Leaderboard not found\"}");

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* start the leaderboard */
    memory[0x0B] = 1;
    memory[0x0E] = 17;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 2);
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 44));
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* submit the leaderboard */
    memory[0x0D] = 1;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 3);

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
    ASSERT_NUM_EQUALS(event->server_error->result, RC_API_FAILURE);
    ASSERT_NUM_EQUALS(event->server_error->related_id, 44);

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_leaderboard_submit_while_spectating(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    mock_memory(memory, sizeof(memory));

    ASSERT_FALSE(rc_client_get_spectator_mode_enabled(g_client));
    rc_client_set_spectator_mode_enabled(g_client, 1);
    ASSERT_TRUE(rc_client_get_spectator_mode_enabled(g_client));

    mock_api_response("r=submitlbentry&u=Username&t=ApiToken&i=44&s=17&m=0123456789ABCDEF&v=a27fa205f7f30c8d13d74806ea5425b6",
        "{\"Success\":false,\"Error\":\"Leaderboard entry should not have been submitted in spectating mode\"}");

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* start the leaderboard */
    memory[0x0B] = 1;
    memory[0x0E] = 17;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 2);
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 44));
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* submit the leaderboard */
    memory[0x0D] = 1;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 2);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED, 44);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000017");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000017");

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* expect API not called */
    assert_api_not_called("r=submitlbentry&u=Username&t=ApiToken&i=44&s=17&m=0123456789ABCDEF&v=a27fa205f7f30c8d13d74806ea5425b6");
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_leaderboard_submit_immediate(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_leaderboard_immediate_submit, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    mock_memory(memory, sizeof(memory));

    mock_api_response("r=submitlbentry&u=Username&t=ApiToken&i=44&s=17&m=0123456789ABCDEF&v=a27fa205f7f30c8d13d74806ea5425b6",
      "{\"Success\":true,\"Response\":{\"Score\":17,\"BestScore\":23,"
      "\"TopEntries\":[{\"User\":\"Player1\",\"Score\":44,\"Rank\":1},{\"User\":\"Username\",\"Score\":23,\"Rank\":2}],"
      "\"RankInfo\":{\"Rank\":2,\"NumEntries\":\"2\"}}}");

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* start the leaderboard (it will immediately submit) */
    memory[0x0B] = 1;
    memory[0x0E] = 17;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 2); /* don't expect start or tracker events - only submit and scoreboard */

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED, 44);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000017");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_SCOREBOARD, 44);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->leaderboard_id, 44);
    ASSERT_STR_EQUALS(event->leaderboard_scoreboard->submitted_score, "000017");
    ASSERT_STR_EQUALS(event->leaderboard_scoreboard->best_score, "000023");
    ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->new_rank, 2);
    ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->num_entries, 2);
    ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->num_top_entries, 2);
    ASSERT_STR_EQUALS(event->leaderboard_scoreboard->top_entries[0].username, "Player1");
    ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->top_entries[0].rank, 1);
    ASSERT_STR_EQUALS(event->leaderboard_scoreboard->top_entries[0].score, "000044");
    ASSERT_STR_EQUALS(event->leaderboard_scoreboard->top_entries[1].username, "Username");
    ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->top_entries[1].rank, 2);
    ASSERT_STR_EQUALS(event->leaderboard_scoreboard->top_entries[1].score, "000023");
    ASSERT_PTR_NOT_NULL(event->leaderboard_scoreboard->top_entries);
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_leaderboard_submit_hidden(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_leaderboards_hidden, no_unlocks);

  /* hidden leaderboards should still start/track/submit normally. they just don't appear in list */

  ASSERT_PTR_NOT_NULL(g_client->game);
  mock_memory(memory, sizeof(memory));

  mock_api_response("r=submitlbentry&u=Username&t=ApiToken&i=48&s=17&m=0123456789ABCDEF&v=468a1f9e9475d8c4d862f48cc8806018",
    "{\"Success\":true,\"Response\":{\"Score\":17,\"BestScore\":23,"
    "\"TopEntries\":[{\"User\":\"Player1\",\"Score\":44,\"Rank\":1},{\"User\":\"Username\",\"Score\":23,\"Rank\":2}],"
    "\"RankInfo\":{\"Rank\":2,\"NumEntries\":\"2\"}}}");

  reset_mock_events();
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

  /* start the leaderboard */
  memory[0x0A] = 2;
  memory[0x0E] = 17;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 2);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 48));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

  reset_mock_events();
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

  /* submit the leaderboard */
  memory[0x0D] = 1;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 3);

  event = find_event(RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED, 48);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
  ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000017");
  ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 48));

  event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
  ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000017");

  event = find_event(RC_CLIENT_EVENT_LEADERBOARD_SCOREBOARD, 48);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->leaderboard_id, 48);
  ASSERT_STR_EQUALS(event->leaderboard_scoreboard->submitted_score, "000017");
  ASSERT_STR_EQUALS(event->leaderboard_scoreboard->best_score, "000023");
  ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->new_rank, 2);
  ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->num_entries, 2);
  ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->num_top_entries, 2);
  ASSERT_STR_EQUALS(event->leaderboard_scoreboard->top_entries[0].username, "Player1");
  ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->top_entries[0].rank, 1);
  ASSERT_STR_EQUALS(event->leaderboard_scoreboard->top_entries[0].score, "000044");
  ASSERT_STR_EQUALS(event->leaderboard_scoreboard->top_entries[1].username, "Username");
  ASSERT_NUM_EQUALS(event->leaderboard_scoreboard->top_entries[1].rank, 2);
  ASSERT_STR_EQUALS(event->leaderboard_scoreboard->top_entries[1].score, "000023");
  ASSERT_PTR_NOT_NULL(event->leaderboard_scoreboard->top_entries);
  ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 48));

  reset_mock_events();
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

  rc_client_destroy(g_client);
}

static int rc_client_callback_deny_leaderboard(uint32_t leaderboard_id, rc_client_t* client)
{
  (void)leaderboard_id;
  (void)client;

  return 0;
}

static int rc_client_callback_allow_leaderboard(uint32_t leaderboard_id, rc_client_t* client)
{
  (void)leaderboard_id;
  (void)client;

  return 1;
}

static void test_do_frame_leaderboard_submit_blocked(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  const char* api_call = "r=submitlbentry&u=Username&t=ApiToken&i=44&s=17&m=0123456789ABCDEF&v=a27fa205f7f30c8d13d74806ea5425b6";
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  g_client->callbacks.can_submit_leaderboard_entry = rc_client_callback_deny_leaderboard;

  ASSERT_PTR_NOT_NULL(g_client->game);
  mock_memory(memory, sizeof(memory));

  mock_api_response(api_call,
    "{\"Success\":true,\"Response\":{\"Score\":17,\"BestScore\":23,"
    "\"TopEntries\":[{\"User\":\"Player1\",\"Score\":44,\"Rank\":1},{\"User\":\"Username\",\"Score\":23,\"Rank\":2}],"
    "\"RankInfo\":{\"Rank\":2,\"NumEntries\":\"2\"}}}");

  reset_mock_events();
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

  /* start the leaderboard */
  memory[0x0B] = 1;
  memory[0x0E] = 17;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 2);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 44));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

  reset_mock_events();
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

  /* submit the leaderboard */
  memory[0x0B] = 0;
  memory[0x0D] = 1;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 2);

  event = find_event(RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED, 44);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
  ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000017");
  ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));

  event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
  ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000017");

  /* but make sure the server wasn't called */
  assert_api_not_called(api_call);

  reset_mock_events();
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

  g_client->callbacks.can_submit_leaderboard_entry = rc_client_callback_allow_leaderboard;

  /* restart the leaderboard - will immediately submit */
  memory[0x0B] = 1;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 2);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED, 44));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_SCOREBOARD, 44));

  /* make sure the server was called */
  assert_api_called(api_call);

  rc_client_destroy(g_client);
}

static void test_do_frame_leaderboard_submit_casual(void)
{
  rc_client_leaderboard_info_t* leaderboard;
  rc_client_event_t* event;
  uint8_t memory[64];
  const char* api_call = "r=submitlbentry&u=Username&t=ApiToken&i=44&s=17&m=0123456789ABCDEF&v=a27fa205f7f30c8d13d74806ea5425b6";
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  rc_client_set_hardcore_enabled(g_client, 0);

  ASSERT_PTR_NOT_NULL(g_client->game);
  mock_memory(memory, sizeof(memory));

  mock_api_response(api_call,
    "{\"Success\":true,\"Response\":{\"Score\":17,\"BestScore\":23,"
    "\"TopEntries\":[{\"User\":\"Player1\",\"Score\":44,\"Rank\":1},{\"User\":\"Username\",\"Score\":23,\"Rank\":2}],"
    "\"RankInfo\":{\"Rank\":2,\"NumEntries\":\"2\"}}}");

  reset_mock_events();
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

  /* start the leaderboard - will be ignored in casual mode */
  memory[0x0B] = 1;
  memory[0x0E] = 17;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

  /* allow leaderboards to be processed in casual mode. have to manually activate as it wasn't activated on game load */
  g_client->state.allow_leaderboards_in_casual = 1;
  leaderboard = (rc_client_leaderboard_info_t*)rc_client_get_leaderboard_info(g_client, 44);
  leaderboard->public_.state = RC_CLIENT_LEADERBOARD_BUCKET_ACTIVE;
  leaderboard->lboard->state = RC_LBOARD_STATE_ACTIVE;

  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 2);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 44));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

  reset_mock_events();
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

  /* submit the leaderboard */
  memory[0x0B] = 0;
  memory[0x0D] = 1;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 2);

  event = find_event(RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED, 44);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
  ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000017");
  ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));

  event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
  ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000017");

  /* but make sure the server wasn't called */
  assert_api_not_called(api_call);

  reset_mock_events();
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

  g_client->state.hardcore = 1;

  /* restart the leaderboard - will immediately submit */
  memory[0x0B] = 1;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 2);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED, 44));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_SCOREBOARD, 44));

  /* make sure the server was called */
  assert_api_called(api_call);

  rc_client_destroy(g_client);
}

static void test_do_frame_leaderboard_tracker_sharing(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    mock_memory(memory, sizeof(memory));

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* start one leaderboard (one tracker) */
    memory[0x0B] = 1;
    memory[0x0E] = 17;
    memory[0x0F] = 1;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 2);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 44);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000273");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000273");

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* start additional leaderboards (45,46,47) - 45 and 46 should generate new trackers */
    memory[0x0A] = 1;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 5);

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

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* start additional leaderboard (48) - should share tracker with 44 */
    memory[0x0A] = 2;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 48);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000273");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 48));
    ASSERT_NUM_EQUALS(g_client->game->leaderboard_trackers->reference_count, 3); /* 44,47,48 */

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* cancel leaderboard 44 */
    memory[0x0C] = 1;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_FAILED, 44);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000273");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));
    ASSERT_NUM_EQUALS(g_client->game->leaderboard_trackers->reference_count, 2); /* 47,48 */

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* cancel leaderboard 45 */
    memory[0x0C] = 2;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 2);

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

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* cancel leaderboard 46 */
    memory[0x0C] = 3;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 2);

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

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);
    
    /* cancel 47, start 51 */
    memory[0x0A] = 3;
    memory[0x0B] = 0;
    memory[0x0C] = 4;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 3);

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

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* cancel 48 */
    memory[0x0C] = 5;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 2);

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

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_leaderboard_tracker_sharing_hits(void)
{
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    mock_memory(memory, sizeof(memory));

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* start leaderboards 51,52 (share tracker) */
    memory[0x0A] = 3;
    memory[0x0B] = 3;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 3);

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
    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "1");

    /* cancel leaderboard 51 */
    memory[0x0C] = 6;
    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 2);

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
    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "3");

    /* restart leaderboard 51 - hit count differs, can't share */
    memory[0x0A] = 3;
    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 3);

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
  const char* submit_entry_params1 = "r=submitlbentry&u=Username&t=ApiToken&i=44&s=17&m=0123456789ABCDEF&o=1&v=659685352e6d8e14923ea32da6f8c3e4";
  const char* submit_entry_params3 = "r=submitlbentry&u=Username&t=ApiToken&i=44&s=17&m=0123456789ABCDEF&o=3&v=91debb749e90c2beff4f21430716dac9";
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  mock_client_enable_async(g_client);

  /* discard the queued ping to make finding the retry easier */
  g_client->state.scheduled_callbacks = NULL;

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    mock_memory(memory, sizeof(memory));

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* start the leaderboard */
    memory[0x0B] = 1;
    memory[0x0E] = 17;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 2);
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 44));
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* submit the leaderboard */
    memory[0x0D] = 1;
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 2);

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED, 44);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_STR_EQUALS(event->leaderboard->tracker_value, "000017");
    ASSERT_PTR_EQUALS(event->leaderboard, rc_client_get_leaderboard_info(g_client, 44));

    event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1);
    ASSERT_PTR_NOT_NULL(event);
    ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
    ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000017");

    reset_mock_events();
    rc_client_do_frame(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* first failure will immediately requeue the request */
    async_api_response(submit_entry_params, "");
    assert_api_pending(submit_entry_params);
    ASSERT_PTR_NULL(g_client->state.scheduled_callbacks);

    rc_client_idle(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

    /* second failure will queue it for one second later */
    async_api_response(submit_entry_params, "");
    assert_api_not_pending(submit_entry_params);
    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);

    /* disconnected event should be raised after retry is queued */
    rc_client_idle(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_DISCONNECTED, 0));
    reset_mock_events();

    /* advance time and process scheduled callbacks */
    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 1 * 1000);
    g_now += 1 * 1000;

    rc_client_idle(g_client);
    assert_api_pending(submit_entry_params1);
    ASSERT_PTR_NULL(g_client->state.scheduled_callbacks);

    /* third failure will requeue it for two seconds later */
    async_api_response(submit_entry_params1, "");
    assert_api_not_pending(submit_entry_params1);
    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);

    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 2 * 1000);
    g_now += 2 * 1000;

    rc_client_idle(g_client);
    assert_api_pending(submit_entry_params3);
    ASSERT_PTR_NULL(g_client->state.scheduled_callbacks);

    /* success should not requeue it and update player score */
    async_api_response(submit_entry_params3,
        "{\"Success\":true,\"Response\":{\"Score\":17,\"BestScore\":23,"
        "\"TopEntries\":[{\"User\":\"Player1\",\"Score\":44,\"Rank\":1},{\"User\":\"Username\",\"Score\":23,\"Rank\":2}],"
        "\"RankInfo\":{\"Rank\":2,\"NumEntries\":\"2\"}}}");
    ASSERT_PTR_NULL(g_client->state.scheduled_callbacks);

    /* reconnected event should be pending, watch for it */
    ASSERT_NUM_EQUALS(get_pending_event_count(), 0);
    rc_client_idle(g_client);
    ASSERT_NUM_EQUALS(get_pending_event_count(), 1);
    ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_RECONNECTED, 0));
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_multiple_automatic_retry(void)
{
  const char* unlock_5501_request_params = "r=awardachievement&u=Username&t=ApiToken&a=5501&h=1&m=0123456789ABCDEF&v=9b9bdf5501eb6289a6655affbcc695e6";
  const char* unlock_5501_request_params1 = "r=awardachievement&u=Username&t=ApiToken&a=5501&h=1&m=0123456789ABCDEF&o=1&v=4b4af09722aeb0e8a16c152f9a646281";
  const char* unlock_5501_request_params4 = "r=awardachievement&u=Username&t=ApiToken&a=5501&h=1&m=0123456789ABCDEF&o=4&v=63646fa1b30797144cc87881f7d95932";
  const char* unlock_5502_request_params = "r=awardachievement&u=Username&t=ApiToken&a=5502&h=1&m=0123456789ABCDEF&v=8d7405f5aacc9b4b334619a0dae62a56";
  const char* unlock_5502_request_params1 = "r=awardachievement&u=Username&t=ApiToken&a=5502&h=1&m=0123456789ABCDEF&o=1&v=46d77e89548126a88c0cdda1980c4dd4";
  const char* submit_entry_params = "r=submitlbentry&u=Username&t=ApiToken&i=4401&s=17&m=0123456789ABCDEF&v=13b4cdfe295a97783e78bb48c8910867";
  const char* submit_entry_params1 = "r=submitlbentry&u=Username&t=ApiToken&i=4401&s=17&m=0123456789ABCDEF&o=1&v=16fe62616f0e39d0a2620b50cc004928";
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);
  mock_client_enable_async(g_client);

  /* discard the queued ping to make finding the retry easier */
  g_client->state.scheduled_callbacks = NULL;

  ASSERT_PTR_NOT_NULL(g_client->game);

  mock_memory(memory, sizeof(memory));

  reset_mock_events();
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

  g_now = 100000; /* time 0 */
  memory[1] = 3; /* trigger 5501 */
  memory[2] = 7; /* trigger 5501 */
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 1);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 5501));
  reset_mock_events();

  /* first failure will immediately requeue the request */
  async_api_response(unlock_5501_request_params, "");
  assert_api_pending(unlock_5501_request_params);

  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

  /* second failure will queue it for one second later (101000) */
  async_api_response(unlock_5501_request_params, "");
  assert_api_not_pending(unlock_5501_request_params);

  /* disconnected event should be raised after retry is queued */
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 1);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_DISCONNECTED, 0));
  reset_mock_events();

  /* advance time 500ms. trigger second achievement and start leaderboard */
  g_now += 500; /* 100500 */
  memory[1] = 2; /* trigger 5502 */
  memory[2] = 9; /* trigger 5502 */
  memory[0x0C] = 1; /* start leaderboard 1 */
  memory[0x0E] = 17; /* leaderboard 1 value */

  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 3);
  /* disconnected event should not be raised again */
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 5502));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 4401));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));
  reset_mock_events();

  /* first failure will immediately requeue the request */
  async_api_response(unlock_5502_request_params, "");
  assert_api_pending(unlock_5502_request_params);

  /* second failure will queue it for one second later (101500) */
  async_api_response(unlock_5502_request_params, "");
  assert_api_not_pending(unlock_5502_request_params);

  /* advance time 250ms. submit leaderboard */
  g_now += 250; /* 100750 */
  memory[0x0D] = 2; /* submit leaderboard 1 */

  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 2);
  /* disconnected event should not be raised again */
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED, 4401));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1));
  reset_mock_events();

  /* first failure will immediately requeue the request */
  async_api_response(submit_entry_params, "");
  assert_api_pending(submit_entry_params);

  /* second failure will queue it for one second later (101750) */
  async_api_response(submit_entry_params, "");
  assert_api_not_pending(submit_entry_params);

  /* advance time. first callback will fail again and be queued for two seconds later (103000) */
  g_now += 350; /* 101100 */
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

  assert_api_pending(unlock_5501_request_params1);
  async_api_response(unlock_5501_request_params1, "");
  assert_api_not_pending(unlock_5501_request_params1);

  /* advance time. second callback will succeed */
  g_now += 500; /* 101600 */
  rc_client_do_frame(g_client);
  assert_api_pending(unlock_5502_request_params1);
  async_api_response(unlock_5502_request_params1, "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":11}");
  assert_api_not_pending(unlock_5502_request_params1);

  /* reconnected event should not be raised until all pending requests are handled */
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

  /* advance time. third callback will succeed */
  g_now += 500; /* 102100 */
  rc_client_do_frame(g_client);
  assert_api_pending(submit_entry_params1);
  async_api_response(submit_entry_params1,
    "{\"Success\":true,\"Response\":{\"Score\":17,\"BestScore\":23,"
    "\"TopEntries\":[{\"User\":\"Player1\",\"Score\":44,\"Rank\":1},{\"User\":\"Username\",\"Score\":23,\"Rank\":2}],"
    "\"RankInfo\":{\"Rank\":2,\"NumEntries\":\"2\"}}}");
  assert_api_not_pending(submit_entry_params);

  /* reconnected event should not be raised until all pending requests are handled */
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

  /* advance time. first callback will finally succeed */
  g_now += 2000; /* 104100 */
  rc_client_do_frame(g_client);
  assert_api_pending(unlock_5501_request_params4);
  async_api_response(unlock_5501_request_params4, "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":11}");
  assert_api_not_pending(unlock_5501_request_params4);

  /* reconnected event should be pending, watch for it */
  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);
  rc_client_idle(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 1);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_RECONNECTED, 0));

  rc_client_destroy(g_client);
}

static void test_do_frame_rich_presence_hitcount(void)
{
  char buffer[8];
  const char* patchdata_rich_presence_hit_count = "{\"Success\":true,"
    "\"GameId\":1234,\"Title\":\"Sample Game\",\"ConsoleId\":17,"
    "\"ImageIconUrl\":\"http://server/Images/112233.png\","
    "\"RichPresenceGameId\":1234,\"RichPresencePatch\":\"Display:\\r\\n@Number(M:1=1)\","
    "\"Sets\":[{"
      "\"AchievementSetId\":1111,\"GameId\":1234,\"Title\":null,\"Type\":\"core\","
      "\"ImageIconUrl\":\"http://server/Images/112233.png\","
      "\"Achievements\":[],"
      "\"Leaderboards\":[]"
    "}]}";
  g_client = mock_client_game_loaded(patchdata_rich_presence_hit_count, no_unlocks);
  ASSERT_PTR_NOT_NULL(g_client->game);

  ASSERT_NUM_EQUALS(rc_client_get_rich_presence_message(g_client, buffer, sizeof(buffer)), 1);
  ASSERT_STR_EQUALS(buffer, "0");

  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(rc_client_get_rich_presence_message(g_client, buffer, sizeof(buffer)), 1);
  ASSERT_STR_EQUALS(buffer, "1");

  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(rc_client_get_rich_presence_message(g_client, buffer, sizeof(buffer)), 1);
  ASSERT_STR_EQUALS(buffer, "2");

  rc_client_destroy(g_client);
}

/* ----- ping ----- */

static void test_idle_ping(void)
{
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    rc_client_scheduled_callback_t ping_callback;

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ping_callback = g_client->state.scheduled_callbacks->callback;

    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 30 * 1000);
    g_now += 30 * 1000;

    mock_api_response("r=ping&u=Username&t=ApiToken&g=1234&h=1&x=0123456789ABCDEF", "{\"Success\":true}");

    g_client->state.frames_processed++; /* ping won't get called if no frames have been processed */
    rc_client_idle(g_client);

    assert_api_called("r=ping&u=Username&t=ApiToken&g=1234&h=1&x=0123456789ABCDEF");

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 120 * 1000);
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

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    rc_client_scheduled_callback_t ping_callback;

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ping_callback = g_client->state.scheduled_callbacks->callback;

    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 30 * 1000);
    g_now += 30 * 1000;

    mock_memory(memory, sizeof(memory));
    memory[0x03] = 25;

    /* before rc_client_do_frame, memory will not have been read. all values will be 0 */
    mock_api_response("r=ping&u=Username&t=ApiToken&g=1234&m=Points%3a0&h=1&x=0123456789ABCDEF", "{\"Success\":true}");

    g_client->state.frames_processed++; /* ping won't get called if no frames have been processed */
    rc_client_idle(g_client);

    assert_api_called("r=ping&u=Username&t=ApiToken&g=1234&m=Points%3a0&h=1&x=0123456789ABCDEF");

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ASSERT_PTR_EQUALS(g_client->state.scheduled_callbacks->callback, ping_callback);
    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 120 * 1000);
    g_now += 120 * 1000;

    /* rc_client_do_frame will update the memory, so the message will contain appropriate data */
    mock_api_response("r=ping&u=Username&t=ApiToken&g=1234&m=Points%3a25&h=1&x=0123456789ABCDEF", "{\"Success\":true}");

    rc_client_do_frame(g_client);

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ASSERT_PTR_EQUALS(g_client->state.scheduled_callbacks->callback, ping_callback);
    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 120 * 1000);
    g_now += 120 * 1000;

    assert_api_called("r=ping&u=Username&t=ApiToken&g=1234&m=Points%3a25&h=1&x=0123456789ABCDEF");

    /* change the memory to make sure the rich presence gets updated */
    mock_api_response("r=ping&u=Username&t=ApiToken&g=1234&m=Points%3a75&h=1&x=0123456789ABCDEF", "{\"Success\":true}");
    memory[0x03] = 75;

    rc_client_do_frame(g_client);

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ASSERT_PTR_EQUALS(g_client->state.scheduled_callbacks->callback, ping_callback);
    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 120 * 1000);
    g_now += 120 * 1000;

    assert_api_called("r=ping&u=Username&t=ApiToken&g=1234&m=Points%3a75&h=1&x=0123456789ABCDEF");

    /* no change to rich presence strings. make sure the callback still gets called again */
    rc_client_do_frame(g_client);

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ASSERT_PTR_EQUALS(g_client->state.scheduled_callbacks->callback, ping_callback);
    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 120 * 1000);
    g_now += 120 * 1000;

    assert_api_call_count("r=ping&u=Username&t=ApiToken&g=1234&m=Points%3a75&h=1&x=0123456789ABCDEF", 2);
  }

  rc_client_destroy(g_client);
}

static int rc_client_callback_rich_presence_override_allow(rc_client_t* client, char buffer[], size_t buffersize)
{
  (void)buffersize;
  (void)client;

  memcpy(buffer, "Custom", 7);
  return 0;
}

static int rc_client_callback_rich_presence_override_replace(rc_client_t* client, char buffer[], size_t buffersize)
{
  (void)buffersize;
  (void)client;

  memcpy(buffer, "Custom", 7);
  return 6;
}

static void test_do_frame_ping_rich_presence_override_allowed(void)
{
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  g_client->callbacks.rich_presence_override = rc_client_callback_rich_presence_override_allow;

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game)
  {
    rc_client_scheduled_callback_t ping_callback;

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ping_callback = g_client->state.scheduled_callbacks->callback;

    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 30 * 1000);
    g_now += 30 * 1000;

    mock_memory(memory, sizeof(memory));
    memory[0x03] = 25;
    mock_api_response("r=ping&u=Username&t=ApiToken&g=1234&m=Points%3a25&h=1&x=0123456789ABCDEF", "{\"Success\":true}");

    /* ping won't get called if no frames have been processed. do_frame will increment frames_processed */
    rc_client_do_frame(g_client);

    assert_api_called("r=ping&u=Username&t=ApiToken&g=1234&m=Points%3a25&h=1&x=0123456789ABCDEF");

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ASSERT_PTR_EQUALS(g_client->state.scheduled_callbacks->callback, ping_callback);
    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 120 * 1000);
  }

  rc_client_destroy(g_client);
}

static void test_do_frame_ping_rich_presence_override_replaced(void)
{
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  g_client->callbacks.rich_presence_override = rc_client_callback_rich_presence_override_replace;

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game)
  {
    rc_client_scheduled_callback_t ping_callback;

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ping_callback = g_client->state.scheduled_callbacks->callback;

    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 30 * 1000);
    g_now += 30 * 1000;

    mock_memory(memory, sizeof(memory));
    memory[0x03] = 25;

    /* before rc_client_do_frame, can_submit only ignores the m parameter. ping still occurs */
    mock_api_response("r=ping&u=Username&t=ApiToken&g=1234&m=Custom&h=1&x=0123456789ABCDEF", "{\"Success\":true}");

    /* ping won't get called if no frames have been processed. do_frame will increment frames_processed */
    rc_client_do_frame(g_client);

    assert_api_called("r=ping&u=Username&t=ApiToken&g=1234&m=Custom&h=1&x=0123456789ABCDEF");

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ASSERT_PTR_EQUALS(g_client->state.scheduled_callbacks->callback, ping_callback);
    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 120 * 1000);
    g_now += 120 * 1000;

    /* ping still happens every two minutes, even if message not provided */
    mock_api_response("r=ping&u=Username&t=ApiToken&g=1234&m=Custom&h=1&x=0123456789ABCDEF", "{\"Success\":true}");
  }

  rc_client_destroy(g_client);
}

static int rc_client_callback_rich_presence_override_change_game(rc_client_t* client, char buffer[], size_t buffersize)
{
  (void)buffersize;

  client->game->public_.id++; /* simulate game change */
  memcpy(buffer, "Custom", 7);
  return 6;
}

static void test_do_frame_ping_rich_presence_game_changed(void)
{
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  g_client->callbacks.rich_presence_override = rc_client_callback_rich_presence_override_change_game;

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game)
  {
    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);

    /* ping won't get called if no frames have been processed. do_frame will increment frames_processed */
    rc_client_do_frame(g_client);

    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 30 * 1000);
    g_now += 30 * 1000;

    rc_client_idle(g_client);

    /* make sure the API didn't get called for the old game id or the new game id */
    assert_api_not_called("r=ping&u=Username&t=ApiToken&g=1234&m=Custom&h=1&x=0123456789ABCDEF");
    assert_api_not_called("r=ping&u=Username&t=ApiToken&g=1235&m=Custom&h=1&x=0123456789ABCDEF");

    /* ping should not be rescheduled - expect new ping scheduled from alternate game */
    ASSERT_PTR_NULL(g_client->state.scheduled_callbacks);
  }

  rc_client_destroy(g_client);
}

static void test_idle_ping_while_not_running(void)
{
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);

  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    rc_client_scheduled_callback_t ping_callback;
    memory[0x03] = 25;
    mock_memory(memory, sizeof(memory));

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ping_callback = g_client->state.scheduled_callbacks->callback;

    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 30 * 1000);
    g_now += 30 * 1000;

    mock_api_response("r=ping&u=Username&t=ApiToken&g=1234&m=Points%3a25&h=1&x=0123456789ABCDEF", "{\"Success\":true}");

    /* if no frames have been processed since the last ping, a ping shouldn't be sent */
    g_client->state.frames_processed = g_client->state.frames_at_last_ping;
    rc_client_idle(g_client);

    assert_api_not_called("r=ping&u=Username&t=ApiToken&g=1234&m=Points%3a25&h=1&x=0123456789ABCDEF");

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 120 * 1000);
    ASSERT_PTR_EQUALS(g_client->state.scheduled_callbacks->callback, ping_callback);
    g_now += 120 * 1000;

    /* do_frame will increment frames_processed, and ping will get called */

    rc_client_do_frame(g_client);

    assert_api_called("r=ping&u=Username&t=ApiToken&g=1234&m=Points%3a25&h=1&x=0123456789ABCDEF");

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ASSERT_PTR_EQUALS(g_client->state.scheduled_callbacks->callback, ping_callback);
    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 120 * 1000);
    g_now += 120 * 1000;

    /* no frames processed. ping shouldn't be sent, but still rescheduled */

    rc_client_idle(g_client);

    assert_api_call_count("r=ping&u=Username&t=ApiToken&g=1234&m=Points%3a25&h=1&x=0123456789ABCDEF", 1);

    ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
    ASSERT_NUM_EQUALS(g_client->state.scheduled_callbacks->when, g_now + 120 * 1000);
    ASSERT_PTR_EQUALS(g_client->state.scheduled_callbacks->callback, ping_callback);
  }

  /* unloading game should unschedule ping */
  rc_client_unload_game(g_client);
  ASSERT_PTR_NULL(g_client->state.scheduled_callbacks);

  rc_client_destroy(g_client);
}

/* ----- reset ----- */

static void test_reset_hides_widgets(void)
{
  const rc_client_leaderboard_t* leaderboard;
  const rc_client_achievement_t* achievement;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  mock_memory(memory, sizeof(memory));

  rc_client_do_frame(g_client);

  memory[0x01] = 1; /* challenge indicator for achievement 7 */
  memory[0x06] = 3; /* progress indicator for achievement 6 */
  memory[0x0A] = 2; /* tracker for leaderboard 48 */
  reset_mock_events();
  rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(get_pending_event_count(), 4);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 48));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW, 6));

  reset_mock_events();
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

  achievement = rc_client_get_achievement_info(g_client, 7);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger->state, RC_TRIGGER_STATE_PRIMED);

  leaderboard = rc_client_get_leaderboard_info(g_client, 48);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);
  ASSERT_NUM_EQUALS(((rc_client_leaderboard_info_t*)leaderboard)->lboard->state, RC_LBOARD_STATE_STARTED);

  rc_client_reset(g_client);

  ASSERT_NUM_EQUALS(get_pending_event_count(), 3);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE, 0));

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

static void test_reset_detaches_hide_progress_indicator_event(void)
{
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  mock_memory(memory, sizeof(memory));

  rc_client_do_frame(g_client);

  memory[0x06] = 3; /* progress indicator for achievement 6 */
  reset_mock_events();
  rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(get_pending_event_count(), 1);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW, 6));
  reset_mock_events();

  /* should be two callbacks queued - hiding the progress indicator, and the rich presence update */
  ASSERT_PTR_EQUALS(g_client->state.scheduled_callbacks, g_client->game->progress_tracker.hide_callback);
  ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks->next);
  ASSERT_PTR_NULL(g_client->state.scheduled_callbacks->next->next);

  /* advance time to hide the progress indicator */
  g_now = g_client->game->progress_tracker.hide_callback->when;
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 1);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE, 0));
  reset_mock_events();

  /* only the rich presence update should be scheduled */
  ASSERT_PTR_NULL(g_client->state.scheduled_callbacks->next);

  /* advance time to just before the rich presence update */
  g_now = g_client->state.scheduled_callbacks->when - 10;

  /* reschedule the progress indicator */
  memory[0x06] = 4;                         /* 4/6 */
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 1);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW, 6));
  reset_mock_events();

  /* should be two callbacks queued - rich presence update, then hiding the progress indicator */
  ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
  ASSERT_PTR_EQUALS(g_client->state.scheduled_callbacks->next, g_client->game->progress_tracker.hide_callback);
  ASSERT_PTR_NULL(g_client->state.scheduled_callbacks->next->next);

  rc_client_reset(g_client);

  ASSERT_NUM_EQUALS(get_pending_event_count(), 1);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE, 0));

  /* only the rich presence update should be scheduled */
  ASSERT_PTR_NULL(g_client->state.scheduled_callbacks->next);

  rc_client_destroy(g_client);
}

/* ----- pause ----- */

static void test_can_pause(void)
{
  uint16_t frames_needed, frames_needed2, frames_needed3, frames_needed4;
  uint32_t frames_remaining;
  int i;

  /* pause without client should be allowed */
  ASSERT_NUM_EQUALS(rc_client_can_pause(NULL, NULL), 1);

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);

  rc_client_do_frame(g_client);

  /* first pause should always be allowed */
  ASSERT_NUM_EQUALS(rc_client_can_pause(g_client, &frames_remaining), 1);
  ASSERT_NUM_EQUALS(frames_remaining, 0);
  frames_needed = g_client->state.unpaused_frame_decay;

  /* if no frames have been processed, the client is still paused, so pause is allowed */
  ASSERT_NUM_EQUALS(rc_client_can_pause(g_client, &frames_remaining), 1);
  ASSERT_NUM_EQUALS(frames_remaining, 0);
  ASSERT_NUM_EQUALS(g_client->state.unpaused_frame_decay, frames_needed);

  /* do a few frames (not enough to allow pause) - pause should still not be allowed */
  for (i = 0; i < 10; i++)
    rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(rc_client_can_pause(g_client, &frames_remaining), 0);
  ASSERT_NUM_EQUALS(frames_remaining, 10);
  ASSERT_NUM_EQUALS(g_client->state.unpaused_frame_decay, frames_needed - 10);

  /* do enough frames to allow pause, but not clear out the decay value.
   * pause should be allowed, and the decay value should be reset to a higher value. */
  for (i = 0; i < 20; i++)
    rc_client_do_frame(g_client);

  ASSERT_NUM_GREATER(g_client->state.unpaused_frame_decay, 0);
  ASSERT_NUM_EQUALS(rc_client_can_pause(g_client, &frames_remaining), 1);
  ASSERT_NUM_EQUALS(frames_remaining, 0);
  frames_needed2 = g_client->state.unpaused_frame_decay;
  ASSERT_NUM_GREATER(frames_needed2, frames_needed);

  /* do enough frames to allow pause before - should not allow pause now */
  for (i = 0; i < 25; i++)
    rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(rc_client_can_pause(g_client, &frames_remaining), 0);
  ASSERT_NUM_EQUALS(frames_remaining, 15);
  ASSERT_NUM_EQUALS(g_client->state.unpaused_frame_decay, frames_needed2 - 25);

  /* do enough frames to allow pause, but not clear out the decay value.
   * pause should be allowed, and the decay value should be reset to an even higher value. */
  for (i = 0; i < 35; i++)
    rc_client_do_frame(g_client);

  ASSERT_NUM_GREATER(g_client->state.unpaused_frame_decay, 0);
  ASSERT_NUM_EQUALS(rc_client_can_pause(g_client, &frames_remaining), 1);
  ASSERT_NUM_EQUALS(frames_remaining, 0);
  frames_needed3 = g_client->state.unpaused_frame_decay;
  ASSERT_NUM_GREATER(frames_needed3, frames_needed2);

  /* completely clear out the decay. decay value should drop, but not all the way. */
  for (i = 0; i < frames_needed3; i++)
    rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(rc_client_can_pause(g_client, &frames_remaining), 1);
  ASSERT_NUM_EQUALS(frames_remaining, 0);
  frames_needed4 = g_client->state.unpaused_frame_decay;
  ASSERT_NUM_LESS(frames_needed4, frames_needed3);
  ASSERT_NUM_GREATER(frames_needed4, frames_needed);

  /* completely clear out the decay. decay value should drop back to default
   * have to do this twice to get through the decayed cycles */
  for (i = 0; i < frames_needed4 * 2; i++)
    rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(rc_client_can_pause(g_client, &frames_remaining), 1);
  ASSERT_NUM_EQUALS(frames_remaining, 0);
  ASSERT_NUM_EQUALS(g_client->state.unpaused_frame_decay, frames_needed);

  /* disable hardcore. pause should be allowed immediately */
  rc_client_set_hardcore_enabled(g_client, 0);
  ASSERT_NUM_EQUALS(rc_client_can_pause(g_client, &frames_remaining), 1);
  ASSERT_NUM_EQUALS(frames_remaining, 0);

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

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
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
  memory[0x06] = 4; /* progress indicator for achievement 6*/
  memory[0x0A] = 2; /* tracker for leaderboard 48 */
  memory[0x0E] = 25; /* leaderboard 48 value */
  reset_mock_events();
  rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(get_pending_event_count(), 4);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 48));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW, 6));

  reset_mock_events();
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

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

  /* deserialize current state. expect progress tracker hide */
  ASSERT_NUM_EQUALS(rc_client_deserialize_progress(g_client, serialized2), RC_OK);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 1);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE, 0));
  reset_mock_events();

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
  ASSERT_NUM_EQUALS(get_pending_event_count(), 2);
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
  reset_mock_events();
  ASSERT_NUM_EQUALS(rc_client_deserialize_progress(g_client, serialized2), RC_OK);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 2);
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
  reset_mock_events();
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 1);
  event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE, 1);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000030");

  /* deserialize second state. expect challenge tracker update to old value */
  reset_mock_events();
  ASSERT_NUM_EQUALS(rc_client_deserialize_progress(g_client, serialized2), RC_OK);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 1);
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

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  mock_memory(memory, sizeof(memory));

  rc_client_do_frame(g_client);

  /* activate some widgets */
  memory[0x01] = 1; /* challenge indicator for achievement 7 */
  memory[0x0A] = 2; /* tracker for leaderboard 48 */
  memory[0x0E] = 25; /* leaderboard 48 value */
  reset_mock_events();
  rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(get_pending_event_count(), 3); /* challenge indicator show, leaderboard start, tracker show */
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

  reset_mock_events();
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

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
  ASSERT_NUM_EQUALS(get_pending_event_count(), 2);
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
  reset_mock_events();
  rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(get_pending_event_count(), 3); /* challenge indicator show, leaderboard start, tracker show */
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

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  mock_memory(memory, sizeof(memory));

  rc_client_do_frame(g_client);

  /* activate some widgets */
  memory[0x01] = 1; /* challenge indicator for achievement 7 */
  memory[0x0A] = 2; /* tracker for leaderboard 48 */
  memory[0x0E] = 25; /* leaderboard 48 value */
  reset_mock_events();
  rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(get_pending_event_count(), 3); /* challenge indicator show, leaderboard start, tracker show */
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

  reset_mock_events();
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

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
  ASSERT_NUM_EQUALS(get_pending_event_count(), 2);
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
  reset_mock_events();
  rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(get_pending_event_count(), 3); /* challenge indicator show, leaderboard start, tracker show */
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

  rc_client_destroy(g_client);
}

static void test_deserialize_progress_sized(void)
{
  uint8_t* serialized1;
  uint8_t* serialized2;
  size_t serialize_size;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  mock_memory(memory, sizeof(memory));

  rc_client_do_frame(g_client);

  /* create an initial checkpoint */
  serialize_size = rc_client_progress_size(g_client);
  serialized1 = (uint8_t*)malloc(serialize_size);
  serialized2 = (uint8_t*)malloc(serialize_size);
  ASSERT_NUM_EQUALS(rc_client_serialize_progress_sized(g_client, serialized1, serialize_size - 1), RC_INSUFFICIENT_BUFFER);
  ASSERT_NUM_EQUALS(rc_client_serialize_progress_sized(g_client, serialized1, serialize_size), RC_OK);

  /* activate some widgets */
  memory[0x01] = 1; /* challenge indicator for achievement 7 */
  memory[0x06] = 4; /* progress indicator for achievement 6*/
  memory[0x0A] = 2; /* tracker for leaderboard 48 */
  memory[0x0E] = 25; /* leaderboard 48 value */
  reset_mock_events();
  rc_client_do_frame(g_client);

  ASSERT_NUM_EQUALS(get_pending_event_count(), 4);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 48));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW, 6));
  reset_mock_events();

  /* capture the state with the widgets visible */
  ASSERT_NUM_EQUALS(rc_client_serialize_progress_sized(g_client, serialized2, serialize_size), RC_OK);

  /* deserialize current state. expect progress tracker hide */
  ASSERT_NUM_EQUALS(rc_client_deserialize_progress_sized(g_client, serialized2, serialize_size), RC_OK);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 1);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE, 0));
  reset_mock_events();

  /* deserialize original state with incorrect size. expect challenge indicator hide, tracker hide */
  ASSERT_NUM_EQUALS(rc_client_deserialize_progress_sized(g_client, serialized1, serialize_size - 1), RC_INSUFFICIENT_BUFFER);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 2);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1));

  /* deserialize second state. expect challenge indicator show, tracker show */
  reset_mock_events();
  ASSERT_NUM_EQUALS(rc_client_deserialize_progress_sized(g_client, serialized2, serialize_size), RC_OK);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 2);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW, 7));
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1));

  free(serialized2);
  free(serialized1);
  rc_client_destroy(g_client);
}

static void test_deserialize_progress_unknown_game(void)
{
  uint8_t buffer[128];

  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2", patchdata_not_found);
  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_unknown_game, g_callback_userdata);

  ASSERT_NUM_EQUALS(rc_client_progress_size(g_client), 0);
  ASSERT_NUM_EQUALS(rc_client_serialize_progress_sized(g_client, buffer, sizeof(buffer)), RC_NO_GAME_LOADED);
  ASSERT_NUM_EQUALS(rc_client_deserialize_progress_sized(g_client, buffer, sizeof(buffer)), RC_NO_GAME_LOADED);

  rc_client_destroy(g_client);
}

/* ----- processing required ----- */

static void test_processing_required(void)
{
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, unlock_5501h_and_5502);

  ASSERT_TRUE(rc_client_is_processing_required(g_client));

  rc_client_destroy(g_client);
}

static void test_processing_required_empty_game(void)
{
  g_client = mock_client_game_loaded(patchdata_empty, no_unlocks);

  ASSERT_FALSE(rc_client_is_processing_required(g_client));

  rc_client_destroy(g_client);
}

static void test_processing_required_rich_presence_only(void)
{
  g_client = mock_client_game_loaded(patchdata_rich_presence_only, no_unlocks);

  ASSERT_TRUE(rc_client_is_processing_required(g_client));

  rc_client_destroy(g_client);
}

static void test_processing_required_leaderboard_only(void)
{
  g_client = mock_client_game_loaded(patchdata_leaderboard_only, no_unlocks);

  ASSERT_TRUE(rc_client_is_processing_required(g_client));

  rc_client_destroy(g_client);
}

static void test_processing_required_after_mastery(void)
{
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, unlock_5501_and_5502);

  ASSERT_TRUE(rc_client_is_processing_required(g_client));

  rc_client_destroy(g_client);
}

static void test_processing_required_after_mastery_no_leaderboards(void)
{
  g_client = mock_client_game_loaded(patchdata_2ach_0lbd, unlock_5501_and_5502);

  ASSERT_FALSE(rc_client_is_processing_required(g_client));

  rc_client_destroy(g_client);
}

/* ----- harness ----- */

void test_client_do_frame(void) {
  TEST_SUITE_BEGIN();

  /* do frame */
  TEST(test_do_frame_bounds_check_system);
  TEST(test_do_frame_bounds_check_available);
  TEST(test_do_frame_achievement_trigger);
  TEST(test_do_frame_achievement_trigger_already_awarded);
  TEST(test_do_frame_achievement_trigger_server_error);
  TEST(test_do_frame_achievement_trigger_while_spectating);
  TEST(test_do_frame_achievement_trigger_blocked);
  TEST(test_do_frame_achievement_trigger_warning);
  TEST(test_do_frame_achievement_trigger_automatic_retry_empty);
  TEST(test_do_frame_achievement_trigger_automatic_retry_429);
  TEST(test_do_frame_achievement_trigger_automatic_retry_502);
  TEST(test_do_frame_achievement_trigger_automatic_retry_503);
  TEST(test_do_frame_achievement_trigger_automatic_retry_custom_timeout);
  TEST(test_do_frame_achievement_trigger_automatic_retry_generic_empty_response);
  TEST(test_do_frame_achievement_trigger_subset);
  TEST(test_do_frame_achievement_trigger_base_and_subset);
  TEST(test_do_frame_achievement_trigger_rarity);
  TEST(test_do_frame_achievement_measured);
  TEST(test_do_frame_achievement_measured_progress_event);
  TEST(test_do_frame_achievement_measured_progress_reshown);
  TEST(test_do_frame_achievement_challenge_indicator);
  TEST(test_do_frame_achievement_challenge_indicator_primed_while_reset);
  TEST(test_do_frame_achievement_challenge_indicator_primed_while_reset_next);
  TEST(test_do_frame_mastery);
  TEST(test_do_frame_mastery_encore);
  TEST(test_do_frame_mastery_subset);
  TEST(test_do_frame_leaderboard_started);
  TEST(test_do_frame_leaderboard_update);
  TEST(test_do_frame_leaderboard_failed);
  TEST(test_do_frame_leaderboard_submit);
  TEST(test_do_frame_leaderboard_submit_server_error);
  TEST(test_do_frame_leaderboard_submit_while_spectating);
  TEST(test_do_frame_leaderboard_submit_immediate);
  TEST(test_do_frame_leaderboard_submit_hidden);
  TEST(test_do_frame_leaderboard_submit_blocked);
  TEST(test_do_frame_leaderboard_submit_casual);
  TEST(test_do_frame_leaderboard_tracker_sharing);
  TEST(test_do_frame_leaderboard_tracker_sharing_hits);
  TEST(test_do_frame_leaderboard_submit_automatic_retry);
  TEST(test_do_frame_multiple_automatic_retry);
  TEST(test_do_frame_rich_presence_hitcount);

  /* ping */
  TEST(test_idle_ping);
  TEST(test_do_frame_ping_rich_presence);
  TEST(test_do_frame_ping_rich_presence_override_allowed);
  TEST(test_do_frame_ping_rich_presence_override_replaced);
  TEST(test_do_frame_ping_rich_presence_game_changed);
  TEST(test_idle_ping_while_not_running);

  /* reset */
  TEST(test_reset_hides_widgets);
  TEST(test_reset_detaches_hide_progress_indicator_event);

  /* pause */
  TEST(test_can_pause);

  /* deserialize_progress */
  TEST(test_deserialize_progress_updates_widgets);
  TEST(test_deserialize_progress_null);
  TEST(test_deserialize_progress_invalid);
  TEST(test_deserialize_progress_sized);
  TEST(test_deserialize_progress_unknown_game);

  /* processing required */
  TEST(test_processing_required);
  TEST(test_processing_required_empty_game);
  TEST(test_processing_required_rich_presence_only);
  TEST(test_processing_required_leaderboard_only);
  TEST(test_processing_required_after_mastery);
  TEST(test_processing_required_after_mastery_no_leaderboards);

  TEST_SUITE_END();
}
