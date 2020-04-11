#include "internal.h"
#include "rurl.h"

#include "smw_snes.h"
#include "galaga_nes.h"

#include "test_framework.h"

#include <stddef.h>
#include <string.h>
#include <assert.h>
#include <string.h> /* memset */

#include "lua.h"
#include "lauxlib.h"

typedef struct {
  unsigned char* ram;
  unsigned size;
}
memory_t;

static unsigned peekb(unsigned address, memory_t* memory) {
  return address < memory->size ? memory->ram[address] : 0;
}

static unsigned peek(unsigned address, unsigned num_bytes, void* ud) {
  memory_t* memory = (memory_t*)ud;

  switch (num_bytes) {
    case 1: return peekb(address, memory);

    case 2: return peekb(address, memory) |
                   peekb(address + 1, memory) << 8;

    case 4: return peekb(address, memory) |
                   peekb(address + 1, memory) << 8 |
                   peekb(address + 2, memory) << 16 |
                   peekb(address + 3, memory) << 24;
  }

  return 0;
}

static rc_condition_t* condset_get_cond(rc_condset_t* condset, int ndx) {
  rc_condition_t* cond = condset->conditions;

  while (ndx-- != 0) {
    assert(cond != NULL);
    cond = cond->next;
  }

  assert(cond != NULL);
  return cond;
}

static rc_condset_t* trigger_get_set(rc_trigger_t* trigger, int ndx) {
  rc_condset_t* condset = trigger->alternative;

  if (ndx-- == 0) {
    assert(trigger->requirement != NULL);
    return trigger->requirement;
  }

  while (ndx-- != 0) {
    condset = condset->next;
    assert(condset != NULL);
  }

  assert(condset != NULL);
  return condset;
}

static rc_runtime_event_t events[16];
static int event_count = 0;

static void event_handler(const rc_runtime_event_t* e)
{
  memcpy(&events[event_count++], e, sizeof(rc_runtime_event_t));
}

static void assert_event(char type, int id, int value)
{
  int i;

  for (i = 0; i < event_count; ++i) {
    if (events[i].id == id && events[i].type == type && events[i].value == value)
      return;
  }

  assert(!"expected event not found");
}

static void test_runtime(void) {
  {
    /*------------------------------------------------------------------------
    TestRuntimeTwoAchievementsActivateAndTrigger
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0, 10, 10 };
    memory_t memory;
    rc_runtime_t runtime;

    memory.ram = ram;
    memory.size = sizeof(ram);

    rc_runtime_init(&runtime);

    assert(rc_runtime_activate_achievement(&runtime, 1, "0xH0001=10", NULL, 0) == RC_OK);
    assert(rc_runtime_activate_achievement(&runtime, 2, "0xH0002=10", NULL, 0) == RC_OK);

    /* both achievements are true, should remain in waiting state */
    event_count = 0;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(event_count == 0);
    assert(runtime.triggers[0].trigger->state == RC_TRIGGER_STATE_WAITING);
    assert(runtime.triggers[1].trigger->state == RC_TRIGGER_STATE_WAITING);

    /* both achievements are false, should activate */
    event_count = 0;
    ram[1] = ram[2] = 9;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(runtime.triggers[0].trigger->state == RC_TRIGGER_STATE_ACTIVE);
    assert(runtime.triggers[1].trigger->state == RC_TRIGGER_STATE_ACTIVE);
    assert(event_count == 2);
    assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 1, 0);
    assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 2, 0);

    /* second achievement is true, should trigger */
    event_count = 0;
    ram[2] = 10;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(runtime.triggers[0].trigger->state == RC_TRIGGER_STATE_ACTIVE);
    assert(runtime.triggers[1].trigger->state == RC_TRIGGER_STATE_TRIGGERED);
    assert(event_count == 1);
    assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED, 2, 0);

    /* first achievement is true, should trigger. second is already triggered */
    event_count = 0;
    ram[1] = 10;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(runtime.triggers[0].trigger->state == RC_TRIGGER_STATE_TRIGGERED);
    assert(runtime.triggers[1].trigger->state == RC_TRIGGER_STATE_TRIGGERED);
    assert(event_count == 1);
    assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED, 1, 0);

    /* reset second achievement, should go back to waiting and stay there */
    event_count = 0;
    rc_reset_trigger(runtime.triggers[1].trigger);
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(runtime.triggers[0].trigger->state == RC_TRIGGER_STATE_TRIGGERED);
    assert(runtime.triggers[1].trigger->state == RC_TRIGGER_STATE_WAITING);
    assert(event_count == 0);

    /* both achievements are false again, second should activate, first should be ignored */
    event_count = 0;
    ram[1] = ram[2] = 9;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(runtime.triggers[0].trigger->state == RC_TRIGGER_STATE_TRIGGERED);
    assert(runtime.triggers[1].trigger->state == RC_TRIGGER_STATE_ACTIVE);
    assert(event_count == 1);
    assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 2, 0);

    rc_runtime_destroy(&runtime);
  }

  {
    /*------------------------------------------------------------------------
    TestRuntimeDeactivateAchievements
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0, 10, 10 };
    memory_t memory;
    rc_runtime_t runtime;

    memory.ram = ram;
    memory.size = sizeof(ram);

    rc_runtime_init(&runtime);

    assert(rc_runtime_activate_achievement(&runtime, 1, "0xH0001=10", NULL, 0) == RC_OK);
    assert(rc_runtime_activate_achievement(&runtime, 2, "0xH0002=10", NULL, 0) == RC_OK);

    /* both achievements are true, should remain in waiting state */
    event_count = 0;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(event_count == 0);
    assert(runtime.triggers[0].trigger->state == RC_TRIGGER_STATE_WAITING);
    assert(runtime.triggers[1].trigger->state == RC_TRIGGER_STATE_WAITING);

    /* deactivate the first one - has memrefs, can't be deallocated */
    rc_runtime_deactivate_achievement(&runtime, 1);
    assert(runtime.trigger_count == 2);
    assert(runtime.triggers[0].trigger == NULL);
    assert(runtime.triggers[1].trigger->state == RC_TRIGGER_STATE_WAITING);

    /* both achievements are false, only active one should activate */
    event_count = 0;
    ram[1] = ram[2] = 9;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(event_count == 1);
    assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 2, 0);

    /* both achievements are true, only active one should trigger */
    event_count = 0;
    ram[1] = ram[2] = 10;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(event_count == 1);
    assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED, 2, 0);

    /* reactivate achievement. definition didn't change, just reactivate in-place */
    assert(rc_runtime_activate_achievement(&runtime, 1, "0xH0001=10", NULL, 0) == RC_OK);
    assert(runtime.trigger_count == 2);
    assert(runtime.triggers[0].trigger->state == RC_TRIGGER_STATE_WAITING);
    assert(runtime.triggers[1].trigger->state == RC_TRIGGER_STATE_TRIGGERED);

    /* reactivated achievement is waiting, should not trigger */
    event_count = 0;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(runtime.triggers[0].trigger->state == RC_TRIGGER_STATE_WAITING);
    assert(runtime.triggers[1].trigger->state == RC_TRIGGER_STATE_TRIGGERED);
    assert(event_count == 0);

    /* both achievements are false again, first should activate, second should be ignored */
    event_count = 0;
    ram[1] = ram[2] = 9;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(runtime.triggers[0].trigger->state == RC_TRIGGER_STATE_ACTIVE);
    assert(runtime.triggers[1].trigger->state == RC_TRIGGER_STATE_TRIGGERED);
    assert(event_count == 1);
    assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 1, 0);

    rc_runtime_destroy(&runtime);
  }

  {
    /*------------------------------------------------------------------------
    TestRuntimeSharedMemRef
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0, 10, 10 };
    memory_t memory;
    rc_runtime_t runtime;

    memory.ram = ram;
    memory.size = sizeof(ram);

    rc_runtime_init(&runtime);

    assert(rc_runtime_activate_achievement(&runtime, 1, "0xH0001=10", NULL, 0) == RC_OK);
    assert(rc_runtime_activate_achievement(&runtime, 2, "0xH0001=12", NULL, 0) == RC_OK);

    assert(condset_get_cond(trigger_get_set(runtime.triggers[0].trigger, 0), 0)->operand1.value.memref ==
           condset_get_cond(trigger_get_set(runtime.triggers[1].trigger, 0), 0)->operand1.value.memref);

    /* first is true, should remain waiting, second should activate */
    event_count = 0;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(event_count == 1);
    assert(runtime.triggers[0].trigger->state == RC_TRIGGER_STATE_WAITING);
    assert(runtime.triggers[1].trigger->state == RC_TRIGGER_STATE_ACTIVE);
    assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 2, 0);

    /* deactivate second one. it doesn't have any unique memrefs, so can be free'd */
    rc_runtime_deactivate_achievement(&runtime, 2);
    assert(runtime.trigger_count == 1);
    assert(runtime.triggers[0].trigger->state == RC_TRIGGER_STATE_WAITING);

    /* second is true, but no longer in runtime. first should activate, expect nothing from second */
    event_count = 0;
    ram[1] = 12;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(event_count == 1);
    assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 1, 0);

    /* first is true and should trigger */
    event_count = 0;
    ram[1] = 10;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(event_count == 1);
    assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED, 1, 0);

    /* reactivate achievement. old definition was free'd, so should be recreated */
    assert(rc_runtime_activate_achievement(&runtime, 2, "0xH0001=12", NULL, 0) == RC_OK);
    assert(runtime.trigger_count == 2);
    assert(runtime.triggers[0].trigger->state == RC_TRIGGER_STATE_TRIGGERED);
    assert(runtime.triggers[1].trigger->state == RC_TRIGGER_STATE_WAITING);

    /* reactivated achievement is waiting and false, should activate */
    event_count = 0;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(event_count == 1);
    assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 2, 0);

    /* deactive first achievement, memrefs used by second - cannot be free'd */
    rc_runtime_deactivate_achievement(&runtime, 1);
    assert(runtime.trigger_count == 2);
    assert(runtime.triggers[0].trigger == NULL);
    assert(runtime.triggers[1].trigger->state == RC_TRIGGER_STATE_ACTIVE);

    /* second achievement is true, should activate using memrefs from first */
    event_count = 0;
    ram[1] = 12;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(event_count == 1);
    assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED, 2, 0);

    rc_runtime_destroy(&runtime);
  }

  {
    /*------------------------------------------------------------------------
    TestRuntimeReplaceActiveTrigger
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0, 10, 10 };
    memory_t memory;
    rc_runtime_t runtime;

    memory.ram = ram;
    memory.size = sizeof(ram);

    rc_runtime_init(&runtime);

    assert(rc_runtime_activate_achievement(&runtime, 1, "0xH0001=10", NULL, 0) == RC_OK);
    assert(rc_runtime_activate_achievement(&runtime, 1, "0xH0002=10", NULL, 0) == RC_OK);

    /* both are true, but first should have been overridden by second */
    event_count = 0;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(runtime.trigger_count == 2);
    assert(runtime.triggers[0].trigger == NULL);
    assert(runtime.triggers[1].trigger->state == RC_TRIGGER_STATE_WAITING);
    assert(event_count == 0);

    /* both are false, but only second should be getting processed, expect single event */
    event_count = 0;
    ram[1] = ram[2] = 9;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(event_count == 1);
    assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 1, 0);

    /* first is true, but should not trigger */
    event_count = 0;
    ram[1] = 10;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(event_count == 0);

    /* second is true, and should trigger */
    event_count = 0;
    ram[2] = 10;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(event_count == 1);
    assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED, 1, 0);

    /* switch back to original definition, since memref kept buffer alive, buffer should be reused */
    assert(rc_runtime_activate_achievement(&runtime, 1, "0xH0001=10", NULL, 0) == RC_OK);
    assert(runtime.trigger_count == 2);
    assert(runtime.triggers[0].trigger->state == RC_TRIGGER_STATE_WAITING);
    assert(runtime.triggers[1].trigger == NULL);

    rc_runtime_destroy(&runtime);
  }

  {
    /*------------------------------------------------------------------------
    TestRuntimeResetIf
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0, 10, 10 };
    memory_t memory;
    rc_runtime_t runtime;

    memory.ram = ram;
    memory.size = sizeof(ram);

    rc_runtime_init(&runtime);

    assert(rc_runtime_activate_achievement(&runtime, 1, "0xH0001=10.2._R:0xH0002=10", NULL, 0) == RC_OK);
    rc_condition_t* cond = condset_get_cond(trigger_get_set(runtime.triggers[0].trigger, 0), 0);

    /* reset is true, so achievement is false, it should activate, but not notify reset */
    event_count = 0;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(event_count == 1);
    assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 1, 0);
    assert(cond->current_hits == 0);

    /* reset is still true, but no hits accumulated, so no event */
    event_count = 0;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(event_count == 0);

    /* reset is not true, hits should increment */
    event_count = 0;
    ram[2] = 9;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(event_count == 0);
    assert(cond->current_hits == 1);

    /* reset is true, hits should reset, expect event */
    event_count = 0;
    ram[2] = 10;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(event_count == 1);
    assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_RESET, 1, 0);
    assert(cond->current_hits == 0);

    /* reset is still true, but no hits accumulated, so no event */
    event_count = 0;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(event_count == 0);

    /* reset is not true, hits should increment */
    event_count = 0;
    ram[2] = 9;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(event_count == 0);
    assert(cond->current_hits == 1);

    /* reset is not true, hits should increment and trigger should fire */
    event_count = 0;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(event_count == 1);
    assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED, 1, 0);
    assert(cond->current_hits == 2);

    /* reset is true, but shouldn't be processed as trigger previously fired */
    event_count = 0;
    ram[2] = 10;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(event_count == 0);
    assert(cond->current_hits == 2);

    rc_runtime_destroy(&runtime);
  }

  {
    /*------------------------------------------------------------------------
    TestRuntimePauseIf
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0, 10, 10 };
    memory_t memory;
    rc_runtime_t runtime;

    memory.ram = ram;
    memory.size = sizeof(ram);

    rc_runtime_init(&runtime);

    assert(rc_runtime_activate_achievement(&runtime, 1, "0xH0001=10.2._P:0xH0002=10", NULL, 0) == RC_OK);

    /* pause is true, so achievement is false, it should activate, but only notify pause */
    event_count = 0;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(event_count == 1);
    assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_PAUSED, 1, 0);

    /* pause is still true, but previously paused, so no event */
    event_count = 0;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(event_count == 0);

    /* pause is not true, expect activate event */
    event_count = 0;
    ram[2] = 9;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(event_count == 1);
    assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 1, 0);

    /* pause is true, expect event */
    event_count = 0;
    ram[2] = 10;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(event_count == 1);
    assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_PAUSED, 1, 0);

    /* pause is still true, but previously paused, so no event */
    event_count = 0;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(event_count == 0);

    /* pause is not true, expect trigger */
    event_count = 0;
    ram[2] = 9;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(event_count == 1);
    assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED, 1, 0);

    /* reset is true, but shouldn't be processed as trigger previously fired */
    event_count = 0;
    ram[2] = 10;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(event_count == 0);

    rc_runtime_destroy(&runtime);
  }

  {
    /*------------------------------------------------------------------------
    TestRuntimeTriggerPrimed
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x00, 0x00, 0x00, 0x00};
    memory_t memory;
    rc_runtime_t runtime;

    memory.ram = ram;
    memory.size = sizeof(ram);

    rc_runtime_init(&runtime);

    assert(rc_runtime_activate_achievement(&runtime, 1, "0xH0000=1_T:0xH0001=1_0xH0002=1_T:0xH0003=1_0xH0004=1", NULL, 0) == RC_OK);

    /* trigger conditions are true, but nothing else */
    event_count = 0;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(event_count == 1);
    assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 1, 0);

    /* primed */
    event_count = 0;
    ram[0] = ram[2] = ram[4] = 1;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(event_count == 1);
    assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_PRIMED, 1, 0);

    /* no longer primed */
    event_count = 0;
    ram[0] = 0;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(event_count == 1);
    assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_ACTIVATED, 1, 0);

    /* primed */
    event_count = 0;
    ram[0] = ram[2] = ram[4] = 1;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(event_count == 1);
    assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_PRIMED, 1, 0);

    /* all conditions are true */
    event_count = 0;
    ram[1] = ram[3] = 1;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(event_count == 1);
    assert_event(RC_RUNTIME_EVENT_ACHIEVEMENT_TRIGGERED, 1, 0);
  }

  {
    /*------------------------------------------------------------------------
    TestRuntimeTwoLeaderboardsActivateAndTrigger
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 2, 10, 10 };
    memory_t memory;
    rc_runtime_t runtime;

    memory.ram = ram;
    memory.size = sizeof(ram);

    rc_runtime_init(&runtime);

    assert(rc_runtime_activate_lboard(&runtime, 1, "STA:0xH0001=10::SUB:0xH0001=11::CAN:0xH0001=12::VAL:0xH0000", NULL, 0) == RC_OK);
    assert(rc_runtime_activate_lboard(&runtime, 2, "STA:0xH0002=10::SUB:0xH0002=11::CAN:0xH0002=12::VAL:0xH0000*2", NULL, 0) == RC_OK);

    /* both start conditions are true, leaderboards will not be active */
    event_count = 0;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(runtime.lboards[0].lboard->state == RC_LBOARD_STATE_WAITING);
    assert(runtime.lboards[1].lboard->state == RC_LBOARD_STATE_WAITING);
    assert(event_count == 0);

    /* both start conditions are false, leaderboards will activate */
    event_count = 0;
    ram[1] = ram[2] = 9;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(runtime.lboards[0].lboard->state == RC_LBOARD_STATE_ACTIVE);
    assert(runtime.lboards[1].lboard->state == RC_LBOARD_STATE_ACTIVE);
    assert(event_count == 0);

    /* both start conditions are true, leaderboards will start */
    event_count = 0;
    ram[1] = ram[2] = 10;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(runtime.lboards[0].lboard->state == RC_LBOARD_STATE_STARTED);
    assert(runtime.lboards[1].lboard->state == RC_LBOARD_STATE_STARTED);
    assert(event_count == 2);
    assert_event(RC_RUNTIME_EVENT_LBOARD_STARTED, 1, 2);
    assert_event(RC_RUNTIME_EVENT_LBOARD_STARTED, 2, 4);

    /* start condition no longer true, leaderboard should continue processing */
    event_count = 0;
    ram[1] = ram[2] = 9;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(runtime.lboards[0].lboard->state == RC_LBOARD_STATE_STARTED);
    assert(runtime.lboards[1].lboard->state == RC_LBOARD_STATE_STARTED);
    assert(event_count == 0);

    /* value changed */
    event_count = 0;
    ram[0] = 3;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(runtime.lboards[0].lboard->state == RC_LBOARD_STATE_STARTED);
    assert(runtime.lboards[1].lboard->state == RC_LBOARD_STATE_STARTED);
    assert(event_count == 2);
    assert_event(RC_RUNTIME_EVENT_LBOARD_UPDATED, 1, 3);
    assert_event(RC_RUNTIME_EVENT_LBOARD_UPDATED, 2, 6);

    /* value changed; first leaderboard submit, second canceled - expect events for submit and cancel, none for update */
    event_count = 0;
    ram[0] = 4;
    ram[1] = 11;
    ram[2] = 12;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(runtime.lboards[0].lboard->state == RC_LBOARD_STATE_TRIGGERED);
    assert(runtime.lboards[1].lboard->state == RC_LBOARD_STATE_CANCELED);
    assert(event_count == 2);
    assert_event(RC_RUNTIME_EVENT_LBOARD_TRIGGERED, 1, 4);
    assert_event(RC_RUNTIME_EVENT_LBOARD_CANCELED, 2, 0);

    /* both start conditions are true, leaderboards will not be active */
    event_count = 0;
    ram[1] = ram[2] = 10;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(runtime.lboards[0].lboard->state == RC_LBOARD_STATE_TRIGGERED);
    assert(runtime.lboards[1].lboard->state == RC_LBOARD_STATE_CANCELED);
    assert(event_count == 0);

    /* both start conditions are false, leaderboards will re-activate */
    event_count = 0;
    ram[1] = ram[2] = 9;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(runtime.lboards[0].lboard->state == RC_LBOARD_STATE_ACTIVE);
    assert(runtime.lboards[1].lboard->state == RC_LBOARD_STATE_ACTIVE);
    assert(event_count == 0);

    /* both start conditions are true, leaderboards will start */
    event_count = 0;
    ram[1] = ram[2] = 10;
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(runtime.lboards[0].lboard->state == RC_LBOARD_STATE_STARTED);
    assert(runtime.lboards[1].lboard->state == RC_LBOARD_STATE_STARTED);
    assert(event_count == 2);
    assert_event(RC_RUNTIME_EVENT_LBOARD_STARTED, 1, 4);
    assert_event(RC_RUNTIME_EVENT_LBOARD_STARTED, 2, 8);

    rc_runtime_destroy(&runtime);
  }
  
  {
    /*------------------------------------------------------------------------
    TestRuntimeRichPresence
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 2, 10, 10 };
    memory_t memory;
    rc_runtime_t runtime;
    int frame_count = 0;

    memory.ram = ram;
    memory.size = sizeof(ram);

    rc_runtime_init(&runtime);

    /* initial value */
    assert(strcmp(rc_runtime_get_richpresence(&runtime), "") == 0);

    /* loading generates display string with uninitialized memrefs - ensures non-empty string if loaded while paused */
    assert(rc_runtime_activate_richpresence(&runtime,
        "Format:Points\nFormatType=VALUE\n\nDisplay:\nScore is @Points(0x 0001) Points", NULL, 0) == RC_OK);
    assert(strcmp(rc_runtime_get_richpresence(&runtime), "Score is 0 Points") == 0);

    /* first frame should update display string */
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(strcmp(rc_runtime_get_richpresence(&runtime), "Score is 2570 Points") == 0);

    /* display string should not update for 60 frames */
    ram[1] = 20;
    for (frame_count = 0; frame_count < 59; ++frame_count) {
      rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
      assert(strcmp(rc_runtime_get_richpresence(&runtime), "Score is 2570 Points") == 0);
    }

    /* string should update on 60th frame */
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(strcmp(rc_runtime_get_richpresence(&runtime), "Score is 2580 Points") == 0);

    rc_runtime_destroy(&runtime);
  }

  {
    /*------------------------------------------------------------------------
    TestRuntimeRichPresenceStartsWithMacro
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 2, 10, 10 };
    memory_t memory;
    rc_runtime_t runtime;
    int frame_count = 0;

    memory.ram = ram;
    memory.size = sizeof(ram);

    rc_runtime_init(&runtime);

    /* initial value */
    assert(strcmp(rc_runtime_get_richpresence(&runtime), "") == 0);

    /* loading generates display string with uninitialized memrefs - ensures non-empty string if loaded while paused */
    assert(rc_runtime_activate_richpresence(&runtime,
        "Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(0x 0001) Points", NULL, 0) == RC_OK);
    assert(strcmp(rc_runtime_get_richpresence(&runtime), "0 Points") == 0);

    /* first frame should update display string */
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(strcmp(rc_runtime_get_richpresence(&runtime), "2570 Points") == 0);

    /* display string should not update for 60 frames */
    ram[1] = 20;
    for (frame_count = 0; frame_count < 59; ++frame_count) {
      rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
      assert(strcmp(rc_runtime_get_richpresence(&runtime), "2570 Points") == 0);
    }

    /* string should update on 60th frame */
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(strcmp(rc_runtime_get_richpresence(&runtime), "2580 Points") == 0);

    rc_runtime_destroy(&runtime);
  }

  {
    /*------------------------------------------------------------------------
    TestRuntimeRichPresenceOnlyMacro
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 2, 10, 10 };
    memory_t memory;
    rc_runtime_t runtime;
    int frame_count = 0;

    memory.ram = ram;
    memory.size = sizeof(ram);

    rc_runtime_init(&runtime);

    /* initial value */
    assert(strcmp(rc_runtime_get_richpresence(&runtime), "") == 0);

    /* loading generates display string with uninitialized memrefs - ensures non-empty string if loaded while paused */
    assert(rc_runtime_activate_richpresence(&runtime,
        "Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(0x 0001)", NULL, 0) == RC_OK);
    assert(strcmp(rc_runtime_get_richpresence(&runtime), "0") == 0);

    /* first frame should update display string */
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(strcmp(rc_runtime_get_richpresence(&runtime), "2570") == 0);

    /* display string should not update for 60 frames */
    ram[1] = 20;
    for (frame_count = 0; frame_count < 59; ++frame_count) {
      rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
      assert(strcmp(rc_runtime_get_richpresence(&runtime), "2570") == 0);
    }

    /* string should update on 60th frame */
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(strcmp(rc_runtime_get_richpresence(&runtime), "2580") == 0);

    rc_runtime_destroy(&runtime);
  }
  
  {
    /*------------------------------------------------------------------------
    TestRuntimeRichPresenceConditional
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 2, 10, 10 };
    memory_t memory;
    rc_runtime_t runtime;
    int frame_count = 0;

    memory.ram = ram;
    memory.size = sizeof(ram);

    rc_runtime_init(&runtime);

    /* initial value */
    assert(strcmp(rc_runtime_get_richpresence(&runtime), "") == 0);

    /* loading generates display string with uninitialized memrefs - ensures non-empty string if loaded while paused */
    assert(rc_runtime_activate_richpresence(&runtime,
        "Format:Points\nFormatType=VALUE\n\nDisplay:\n?0xH0000=2?@Points(0x 0001) points\nScore is @Points(0x 0001) Points", NULL, 0) == RC_OK);
    assert(strcmp(rc_runtime_get_richpresence(&runtime), "Score is 0 Points") == 0);

    /* first frame should update display string */
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(strcmp(rc_runtime_get_richpresence(&runtime), "2570 points") == 0);

    /* display string should not update for 60 frames */
    ram[1] = 20;
    ram[0] = 0;
    for (frame_count = 0; frame_count < 59; ++frame_count) {
      rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
      assert(strcmp(rc_runtime_get_richpresence(&runtime), "2570 points") == 0);
    }

    /* string should update on 60th frame */
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(strcmp(rc_runtime_get_richpresence(&runtime), "Score is 2580 Points") == 0);

    rc_runtime_destroy(&runtime);
  }

  {
    /*------------------------------------------------------------------------
    TestRuntimeRichPresenceReload
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 2, 10, 10 };
    memory_t memory;
    rc_runtime_t runtime;

    memory.ram = ram;
    memory.size = sizeof(ram);

    rc_runtime_init(&runtime);

    /* initial value */
    assert(strcmp(rc_runtime_get_richpresence(&runtime), "") == 0);

    /* loading generates display string with uninitialized memrefs */
    assert(rc_runtime_activate_richpresence(&runtime,
        "Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(0x 0001) Points", NULL, 0) == RC_OK);
    assert(strcmp(rc_runtime_get_richpresence(&runtime), "0 Points") == 0);

    /* first frame should update display string */
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(strcmp(rc_runtime_get_richpresence(&runtime), "2570 Points") == 0);
    ram[1] = 20;

    /* reloading should generate display string with current memrefs */
    assert(rc_runtime_activate_richpresence(&runtime,
        "Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(0x 0001) Bananas", NULL, 0) == RC_OK);
    assert(strcmp(rc_runtime_get_richpresence(&runtime), "2570 Bananas") == 0);

    /* should reuse the memrefs from the first runtime */
    assert(runtime.richpresence->owns_memrefs == 0);
    assert(runtime.richpresence->previous != NULL);

    /* first frame after reloading should update display string */
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(strcmp(rc_runtime_get_richpresence(&runtime), "2580 Bananas") == 0);

    rc_runtime_destroy(&runtime);
  }

  {
    /*------------------------------------------------------------------------
    TestRuntimeStaticRichPresence
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 2, 10, 10 };
    memory_t memory;
    rc_runtime_t runtime;

    memory.ram = ram;
    memory.size = sizeof(ram);

    rc_runtime_init(&runtime);

    /* initial value */
    assert(strcmp(rc_runtime_get_richpresence(&runtime), "") == 0);

    /* static string will be set on first frame */
    assert(rc_runtime_activate_richpresence(&runtime,
        "Display:\nHello, world!", NULL, 0) == RC_OK);
    assert(strcmp(rc_runtime_get_richpresence(&runtime), "Hello, world!") == 0);

    /* first frame should not update display string */
    rc_runtime_do_frame(&runtime, event_handler, peek, &memory, NULL);
    assert(strcmp(rc_runtime_get_richpresence(&runtime), "Hello, world!") == 0);
    assert(runtime.richpresence == NULL); /* this ensures the static string isn't evaluated */

    rc_runtime_destroy(&runtime);
  }
}

static void test_lua(void) {
  {
    /*------------------------------------------------------------------------
    TestLua
    ------------------------------------------------------------------------*/

#ifndef RC_DISABLE_LUA

    lua_State* L;
    const char* luacheevo = "return { test = function(peek, ud) return peek(0, 4, ud) end }";
    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;
    char buffer[2048];

    memory.ram = ram;
    memory.size = sizeof(ram);

    L = luaL_newstate();
    luaL_loadbufferx(L, luacheevo, strlen(luacheevo), "luacheevo.lua", "t");
    lua_call(L, 0, 1);

    memory.ram = ram;
    memory.size = sizeof(ram);

    trigger = rc_parse_trigger(buffer, "@test=0xX0", L, 1);
    assert(rc_test_trigger(trigger, peek, &memory, L) != 0);

#endif /* RC_DISABLE_LUA */
  }
}

extern void test_condition();
extern void test_memref();
extern void test_operand();
extern void test_condset();
extern void test_trigger();
extern void test_value();
extern void test_format();
extern void test_lboard();
extern void test_richpresence();

extern void test_hash();

TEST_FRAMEWORK_DECLARATIONS()

int main(void) {
  TEST_FRAMEWORK_INIT();

  test_memref();
  test_operand();
  test_condition();
  test_condset();
  test_trigger();
  test_value();
  test_format();
  test_lboard();

  test_richpresence();
  test_runtime();
  test_lua();

  test_hash();

  TEST_FRAMEWORK_SHUTDOWN();

  return TEST_FRAMEWORK_PASSED() ? 0 : 1;
}
