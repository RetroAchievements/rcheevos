#include "internal.h"

#include "mock_memory.h"

#include "../test_framework.h"

static rc_runtime_event_t events[16];
static int event_count = 0;

static void event_handler(const rc_runtime_event_t* e)
{
  memcpy(&events[event_count++], e, sizeof(rc_runtime_event_t));
}

static void _assert_event(char type, int id, int value)
{
  int i;

  for (i = 0; i < event_count; ++i) {
    if (events[i].id == id && events[i].type == type && events[i].value == value)
      return;
  }

  ASSERT_FAIL("expected event not found");
}
#define assert_event(type, id, value) ASSERT_HELPER(_assert_event(type, id, value), "assert_event")

static void _assert_activate_achievement(rc_runtime_t* runtime, unsigned int id, const char* memaddr)
{
  int result = rc_runtime_activate_achievement(runtime, id, memaddr, NULL, 0);
  ASSERT_NUM_EQUALS(result, RC_OK);
}
#define assert_activate_achievement(runtime, id, memaddr) ASSERT_HELPER(_assert_activate_achievement(runtime, id, memaddr), "assert_activate_achievement")

static void _assert_activate_lboard(rc_runtime_t* runtime, unsigned int id, const char* memaddr)
{
  int result = rc_runtime_activate_lboard(runtime, id, memaddr, NULL, 0);
  ASSERT_NUM_EQUALS(result, RC_OK);
}
#define assert_activate_lboard(runtime, id, memaddr) ASSERT_HELPER(_assert_activate_lboard(runtime, id, memaddr), "assert_activate_lboard")

static void _assert_activate_richpresence(rc_runtime_t* runtime, const char* script)
{
  int result = rc_runtime_activate_richpresence(runtime, script, NULL, 0);
  ASSERT_NUM_EQUALS(result, RC_OK);
}
#define assert_activate_richpresence(runtime, script) ASSERT_HELPER(_assert_activate_richpresence(runtime, script), "assert_activate_richpresence")

static void assert_do_frame(rc_runtime_t* runtime, memory_t* memory)
{
  event_count = 0;
  rc_runtime_do_frame(runtime, event_handler, peek, memory, NULL);
}

static void test_two_achievements_activate_and_trigger(void)
{
  unsigned char ram[] = { 0, 10, 10 };
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "0xH0001=10");
  assert_activate_achievement(&runtime, 2, "0xH0002=10");

  /* both achievements are true, should remain in waiting state */
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* both achievements are false, should activate */
  ram[1] = ram[2] = 9;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(event_count, 2);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 1, 0);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 2, 0);

  /* second achievement is true, should trigger */
  ram[2] = 10;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_TRIGGERED);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED, 2, 0);

  /* first achievement is true, should trigger. second is already triggered */
  ram[1] = 10;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_TRIGGERED);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_TRIGGERED);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED, 1, 0);

  /* reset second achievement, should go back to WAITING and stay there */
  rc_reset_trigger(runtime.triggers[1].trigger);
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_TRIGGERED);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* both achievements are false again. second should active, first should be ignored */
  ram[1] = ram[2] = 9;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_TRIGGERED);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 2, 0);

  rc_runtime_destroy(&runtime);
}

static void test_deactivate_achievements(void)
{
  unsigned char ram[] = { 0, 10, 10 };
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "0xH0001=10");
  assert_activate_achievement(&runtime, 2, "0xH0002=10");

  /* both achievements are true, should remain in waiting state */
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* deactivate the first. it owns shared memrefs, so can't be deallocated */
  rc_runtime_deactivate_achievement(&runtime, 1);
  ASSERT_NUM_EQUALS(runtime.trigger_count, 2);
  ASSERT_PTR_NULL(runtime.triggers[0].trigger);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_WAITING);

  /* both achievements are false, deactivated one should not activate */
  ram[1] = ram[2] = 9;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 2, 0);

  /* both achievements are true, deactivated one should not trigger */
  ram[1] = ram[2] = 10;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED, 2, 0);

  /* reactivate achievement. definition didn't change, should reactivate in-place */
  assert_activate_achievement(&runtime, 1, "0xH0001=10");
  ASSERT_NUM_EQUALS(runtime.trigger_count, 2);
  ASSERT_PTR_NOT_NULL(runtime.triggers[0].trigger);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_TRIGGERED);

  /* reactivated achievement is waiting and should not trigger */
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_TRIGGERED);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* both achievements are false. first should activate, second should be ignored */
  ram[1] = ram[2] = 9;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_TRIGGERED);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 1, 0);

  rc_runtime_destroy(&runtime);
}

static void test_shared_memref(void)
{
  unsigned char ram[] = { 0, 10, 10 };
  memory_t memory;
  rc_runtime_t runtime;
  rc_memref_value_t* memref1;
  rc_memref_value_t* memref2;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "0xH0001=10");
  assert_activate_achievement(&runtime, 2, "0xH0001=12");

  memref1 = runtime.triggers[0].trigger->requirement->conditions->operand1.value.memref;
  memref2 = runtime.triggers[1].trigger->requirement->conditions->operand1.value.memref;
  ASSERT_PTR_EQUALS(memref1, memref2);

  /* first is true, should remain waiting. second should activate */
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 2, 0);

  /* deactivate second one. it doesn't have any unique memrefs, so can be free'd */
  rc_runtime_deactivate_achievement(&runtime, 2);
  ASSERT_NUM_EQUALS(runtime.trigger_count, 1);
  ASSERT_PTR_NOT_NULL(runtime.triggers[0].trigger);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_WAITING);

  /* second is true, but no longer in runtime. first should activate, expect nothing from second */
  ram[1] = 12;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 1, 0);

  /* first is true and should trigger */
  ram[1] = 10;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED, 1, 0);

  /* reactivate achievement. old definition was free'd so should be recreated */
  assert_activate_achievement(&runtime, 2, "0xH0001=12");
  ASSERT_NUM_EQUALS(runtime.trigger_count, 2);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_TRIGGERED);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_WAITING);

  /* reactivated achievement is waiting and false. should activate */
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 2, 0);

  /* deactivate first achievement. memrefs used by second - cannot be free'd */
  rc_runtime_deactivate_achievement(&runtime, 1);
  ASSERT_NUM_EQUALS(runtime.trigger_count, 2);
  ASSERT_PTR_NULL(runtime.triggers[0].trigger);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_ACTIVE);

  /* second achievement is true. should trigger using memrefs from first */
  ram[1] = 12;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED, 2, 0);

  rc_runtime_destroy(&runtime);
}

static void test_replace_active_trigger(void)
{
  unsigned char ram[] = { 0, 10, 10 };
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "0xH0001=10");
  assert_activate_achievement(&runtime, 1, "0xH0002=10");

  /* both are true, but first should have been overwritten by second */
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.trigger_count, 2);
  ASSERT_PTR_NULL(runtime.triggers[0].trigger);
  ASSERT_NUM_EQUALS(runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* both are false. only second should be getting processed, expect single event */
  ram[1] = ram[2] = 9;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 1, 0);

  /* first is true, but should not trigger */
  ram[1] = 10;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* second is true and should trigger */
  ram[2] = 10;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED, 1, 0);

  /* switch back to original definition. since the memrefs kept the buffer alive, it should be recycled */
  assert_activate_achievement(&runtime, 1, "0xH0001=10");
  ASSERT_NUM_EQUALS(runtime.trigger_count, 2);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_WAITING);
  ASSERT_PTR_NULL(runtime.triggers[1].trigger);

  rc_runtime_destroy(&runtime);
}

static void test_reset_event(void) 
{
  unsigned char ram[] = { 0, 10, 10 };
  memory_t memory;
  rc_runtime_t runtime;
  rc_condition_t* cond;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "0xH0001=10.2._R:0xH0002=10");
  cond = runtime.triggers[0].trigger->requirement->conditions;

  /* reset is true, so achievement is false and should activate, but not notify reset */
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 1, 0);
  ASSERT_NUM_EQUALS(cond->current_hits, 0);

  /* reset is still true, but since no hits were accumulated there shouldn't be a reset event */
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* reset is not true, hits should increment */
  ram[2] = 9;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 0);
  ASSERT_NUM_EQUALS(cond->current_hits, 1);

  /* reset is true. hits will reset. expect event */
  ram[2] = 10;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_RESET, 1, 0);
  ASSERT_NUM_EQUALS(cond->current_hits, 0);

  /* reset is still true, but since hits were previously reset there shouldn't be a reset event */
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* reset is not true, hits should increment */
  ram[2] = 9;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 0);
  ASSERT_NUM_EQUALS(cond->current_hits, 1);

  /* reset is not true, hits should increment, causing achievement to trigger */
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED, 1, 0);
  ASSERT_NUM_EQUALS(cond->current_hits, 2);

  /* reset is true, but hits shouldn't reset as achievement is no longer active */
  ram[2] = 10;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 0);
  ASSERT_NUM_EQUALS(cond->current_hits, 2);

  rc_runtime_destroy(&runtime);
}

static void test_paused_event(void) 
{
  unsigned char ram[] = { 0, 10, 10 };
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "0xH0001=10.2._P:0xH0002=10");

  /* pause is true, so achievement is false and should activate, but only notify pause */
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_PAUSED, 1, 0);

  /* pause is still true, but previously paused, so no event */
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* pause is not true, expect activate event */
  ram[2] = 9;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 1, 0);

  /* pause is true. expect event */
  ram[2] = 10;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_PAUSED, 1, 0);

  /* pause is still true, but previously paused, so no event */
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* pause is not true, expect trigger*/
  ram[2] = 9;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED, 1, 0);

  /* pause is true, but shouldn't notify as achievement is no longer active */
  ram[2] = 10;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 0);

  rc_runtime_destroy(&runtime);
}

static void test_primed_event(void) 
{
  unsigned char ram[] = { 0, 1, 0, 1, 0 };
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  /* byte(0)==1 && trigger(byte(1)==1) && byte(2)==1 && trigger(byte(3)==1) && byte(4)==1 */
  assert_activate_achievement(&runtime, 1, "0xH0000=1_T:0xH0001=1_0xH0002=1_T:0xH0003=1_0xH0004=1");

  /* trigger conditions are true, but nothing else */
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 1, 0);

  /* primed */
  ram[1] = ram[3] = 0;
  ram[0] = ram[2] = ram[4] = 1;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_PRIMED, 1, 0);

  /* no longer primed */
  ram[0] = 0;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 1, 0);

  /* primed */
  ram[0] = 1;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_PRIMED, 1, 0);

  /* all conditions are true */
  ram[1] = ram[3] = 1;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(event_count, 1);
  assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED, 1, 0);

  rc_runtime_destroy(&runtime);
}

static void test_lboard(void) 
{
  unsigned char ram[] = { 2, 10, 10 };
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_lboard(&runtime, 1, "STA:0xH0001=10::SUB:0xH0001=11::CAN:0xH0001=12::VAL:0xH0000");
  assert_activate_lboard(&runtime, 2, "STA:0xH0002=10::SUB:0xH0002=11::CAN:0xH0002=12::VAL:0xH0000*2");

  /* both start conditions are true, leaderboards will not be active */
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.lboards[0].lboard->state, RC_LBOARD_STATE_WAITING);
  ASSERT_NUM_EQUALS(runtime.lboards[1].lboard->state, RC_LBOARD_STATE_WAITING);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* both start conditions are false, leaderboards will activate */
  ram[1] = ram[2] = 9;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.lboards[0].lboard->state, RC_LBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(runtime.lboards[1].lboard->state, RC_LBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* both start conditions are true, leaderboards will start */
  ram[1] = ram[2] = 10;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.lboards[0].lboard->state, RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(runtime.lboards[1].lboard->state, RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(event_count, 2);
  assert_event(RC_RUNTIME_EVENT_LBOARD_STARTED, 1, 2);
  assert_event(RC_RUNTIME_EVENT_LBOARD_STARTED, 2, 4);

  /* start condition no longer true, leaderboard should continue processing */
  ram[1] = ram[2] = 9;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.lboards[0].lboard->state, RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(runtime.lboards[1].lboard->state, RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* value changed */
  ram[0] = 3;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.lboards[0].lboard->state, RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(runtime.lboards[1].lboard->state, RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(event_count, 2);
  assert_event(RC_RUNTIME_EVENT_LBOARD_UPDATED, 1, 3);
  assert_event(RC_RUNTIME_EVENT_LBOARD_UPDATED, 2, 6);

  /* value changed; first leaderboard submit, second canceled - expect events for submit and cancel, none for update */
  ram[0] = 4;
  ram[1] = 11;
  ram[2] = 12;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.lboards[0].lboard->state, RC_LBOARD_STATE_TRIGGERED);
  ASSERT_NUM_EQUALS(runtime.lboards[1].lboard->state, RC_LBOARD_STATE_CANCELED);
  ASSERT_NUM_EQUALS(event_count, 2);
  assert_event(RC_RUNTIME_EVENT_LBOARD_TRIGGERED, 1, 4);
  assert_event(RC_RUNTIME_EVENT_LBOARD_CANCELED, 2, 0);

  /* both start conditions are true, leaderboards will not be active */
  ram[1] = ram[2] = 10;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.lboards[0].lboard->state, RC_LBOARD_STATE_TRIGGERED);
  ASSERT_NUM_EQUALS(runtime.lboards[1].lboard->state, RC_LBOARD_STATE_CANCELED);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* both start conditions are false, leaderboards will re-activate */
  ram[1] = ram[2] = 9;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.lboards[0].lboard->state, RC_LBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(runtime.lboards[1].lboard->state, RC_LBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(event_count, 0);

  /* both start conditions are true, leaderboards will start */
  ram[1] = ram[2] = 10;
  assert_do_frame(&runtime, &memory);
  ASSERT_NUM_EQUALS(runtime.lboards[0].lboard->state, RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(runtime.lboards[1].lboard->state, RC_LBOARD_STATE_STARTED);
  ASSERT_NUM_EQUALS(event_count, 2);
  assert_event(RC_RUNTIME_EVENT_LBOARD_STARTED, 1, 4);
  assert_event(RC_RUNTIME_EVENT_LBOARD_STARTED, 2, 8);

  rc_runtime_destroy(&runtime);
}

static void test_richpresence(void)
{
  unsigned char ram[] = { 2, 10, 10 };
  memory_t memory;
  rc_runtime_t runtime;
  int frame_count = 0;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  /* initial value */
  ASSERT_STR_EQUALS(rc_runtime_get_richpresence(&runtime), "");

  /* loading generates a display string with uninitialized memrefs, which ensures a non-empty display string */
  assert_activate_richpresence(&runtime,
      "Format:Points\nFormatType=VALUE\n\nDisplay:\nScore is @Points(0x 0001) Points");
  ASSERT_STR_EQUALS(rc_runtime_get_richpresence(&runtime), "Score is 0 Points");

  /* first frame should update display string with correct memrfs */
  assert_do_frame(&runtime, &memory);
  ASSERT_STR_EQUALS(rc_runtime_get_richpresence(&runtime), "Score is 2570 Points");

  /* display string should not update for 60 frames */
  ram[1] = 20;
  for (frame_count = 0; frame_count < 59; ++frame_count) {
    assert_do_frame(&runtime, &memory);
    ASSERT_STR_EQUALS(rc_runtime_get_richpresence(&runtime), "Score is 2570 Points");
  }

  /* string should update on the 60th frame */
  assert_do_frame(&runtime, &memory);
  ASSERT_STR_EQUALS(rc_runtime_get_richpresence(&runtime), "Score is 2580 Points");

  rc_runtime_destroy(&runtime);
}

static void test_richpresence_starts_with_macro(void)
{
  unsigned char ram[] = { 2, 10, 10 };
  memory_t memory;
  rc_runtime_t runtime;
  
  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_richpresence(&runtime,
      "Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(0x 0001) Points");
  assert_do_frame(&runtime, &memory);
  ASSERT_STR_EQUALS(rc_runtime_get_richpresence(&runtime), "2570 Points");

  rc_runtime_destroy(&runtime);
}

static void test_richpresence_macro_only(void)
{
  unsigned char ram[] = { 2, 10, 10 };
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_richpresence(&runtime,
      "Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(0x 0001)");
  assert_do_frame(&runtime, &memory);
  ASSERT_STR_EQUALS(rc_runtime_get_richpresence(&runtime), "2570");

  rc_runtime_destroy(&runtime);
}

static void test_richpresence_conditional(void)
{
  unsigned char ram[] = { 2, 10, 10 };
  memory_t memory;
  rc_runtime_t runtime;
  int frame_count = 0;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  /* loading generates a display string with uninitialized memrefs, which ensures a non-empty display string */
  assert_activate_richpresence(&runtime,
      "Format:Points\nFormatType=VALUE\n\nDisplay:\n?0xH0000=2?@Points(0x 0001) points\nScore is @Points(0x 0001) Points");
  ASSERT_STR_EQUALS(rc_runtime_get_richpresence(&runtime), "Score is 0 Points");

  /* first frame should update display string with correct memrfs */
  assert_do_frame(&runtime, &memory);
  ASSERT_STR_EQUALS(rc_runtime_get_richpresence(&runtime), "2570 points");

  /* display string should not update for 60 frames */
  ram[0] = 0;
  ram[1] = 20;
  for (frame_count = 0; frame_count < 59; ++frame_count) {
    assert_do_frame(&runtime, &memory);
    ASSERT_STR_EQUALS(rc_runtime_get_richpresence(&runtime), "2570 points");
  }

  /* string should update on the 60th frame */
  assert_do_frame(&runtime, &memory);
  ASSERT_STR_EQUALS(rc_runtime_get_richpresence(&runtime), "Score is 2580 Points");

  rc_runtime_destroy(&runtime);
}

static void test_richpresence_reload(void)
{
  unsigned char ram[] = { 2, 10, 10 };
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  /* loading generates a display string with uninitialized memrefs, which ensures a non-empty display string */
  assert_activate_richpresence(&runtime,
      "Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(0x 0001) Points");
  ASSERT_STR_EQUALS(rc_runtime_get_richpresence(&runtime), "0 Points");

  /* first frame should update display string with correct memrfs */
  assert_do_frame(&runtime, &memory);
  ASSERT_STR_EQUALS(rc_runtime_get_richpresence(&runtime), "2570 Points");

  /* reloading should generate display string with current memrefs */
  ram[1] = 20;
  assert_activate_richpresence(&runtime,
      "Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(0x 0001) Bananas");
  ASSERT_STR_EQUALS(rc_runtime_get_richpresence(&runtime), "2570 Bananas");

  /* memrefs should be reused from first script */
  ASSERT_NUM_EQUALS(runtime.richpresence->owns_memrefs, 0);
  ASSERT_PTR_NOT_NULL(runtime.richpresence->previous);

  /* first frame after reloading should update display string */
  assert_do_frame(&runtime, &memory);
  ASSERT_STR_EQUALS(rc_runtime_get_richpresence(&runtime), "2580 Bananas");

  rc_runtime_destroy(&runtime);
}

static void test_richpresence_reload_addaddress(void)
{
  /* ram[1] must be non-zero */
  unsigned char ram[] = { 1, 10, 10, 10 };
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  /* loading generates a display string with uninitialized memrefs, which ensures a non-empty display string */
  assert_activate_richpresence(&runtime,
      "Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(I:0xH0000_M:0x 0001) Points");
  ASSERT_STR_EQUALS(rc_runtime_get_richpresence(&runtime), "0 Points");

  /* first frame should update display string with correct memrfs */
  assert_do_frame(&runtime, &memory);
  ASSERT_STR_EQUALS(rc_runtime_get_richpresence(&runtime), "2570 Points");

  /* reloading should generate display string with current memrefs */
  /* AddAddress will always generate a new memref for the indirection. */
  /* because the reset doesn't provide a peek, the indirection can't be resolved, and the value will be 0. */
  ram[2] = 20;
  assert_activate_richpresence(&runtime,
      "Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(I:0xH0000_M:0x 0001) Bananas");
  ASSERT_STR_EQUALS(rc_runtime_get_richpresence(&runtime), "0 Bananas");

  /* AddAddress always generates a new memrefs for the indirection. */
  ASSERT_NUM_EQUALS(runtime.richpresence->owns_memrefs, 1);
  ASSERT_PTR_NOT_NULL(runtime.richpresence->previous);

  /* first frame after reloading should update display string */
  assert_do_frame(&runtime, &memory);
  ASSERT_STR_EQUALS(rc_runtime_get_richpresence(&runtime), "2580 Bananas");

  rc_runtime_destroy(&runtime);
}

static void test_richpresence_static(void)
{
  unsigned char ram[] = { 2, 10, 10 };
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_richpresence(&runtime, "Display:\nHello, world!");
  assert_do_frame(&runtime, &memory);
  ASSERT_STR_EQUALS(rc_runtime_get_richpresence(&runtime), "Hello, world!");

  /* first frame won't affect the display string */
  assert_do_frame(&runtime, &memory);
  ASSERT_STR_EQUALS(rc_runtime_get_richpresence(&runtime), "Hello, world!");

  /* this ensures the static string is not evaluated every frame */
  ASSERT_PTR_NULL(runtime.richpresence);

  rc_runtime_destroy(&runtime);
}

void test_runtime(void) {
  TEST_SUITE_BEGIN();

  /* achievements */
  TEST(test_two_achievements_activate_and_trigger);
  TEST(test_deactivate_achievements);

  TEST(test_shared_memref);
  TEST(test_replace_active_trigger);

  /* achievement events */
  TEST(test_reset_event);
  TEST(test_paused_event);
  TEST(test_primed_event);

  /* leaderboards */
  TEST(test_lboard);

  /* rich presence */
  TEST(test_richpresence);
  TEST(test_richpresence_starts_with_macro);
  TEST(test_richpresence_macro_only);
  TEST(test_richpresence_conditional);
  TEST(test_richpresence_reload);
  TEST(test_richpresence_reload_addaddress);
  TEST(test_richpresence_static);

  TEST_SUITE_END();
}
