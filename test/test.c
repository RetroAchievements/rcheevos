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

static rc_lboard_t* parse_lboard(const char* memaddr, void* buffer) {
  int ret;
  rc_lboard_t* self;

  ret = rc_lboard_size(memaddr);
  assert(ret >= 0);
  memset(buffer, 0xEE, ret + 128);

  self = rc_parse_lboard(buffer, memaddr, NULL, 0);
  assert(self != NULL);
  assert(*((int*)((char*)buffer + ret)) == 0xEEEEEEEE);

  self->state = RC_LBOARD_STATE_ACTIVE;
  return self;
}

static void lboard_check(const char* memaddr, int expected_ret) {
  int ret = rc_lboard_size(memaddr);
  assert(ret == expected_ret);
}

typedef struct {
  int active, submitted;
}
lboard_test_state_t;

static void lboard_reset(rc_lboard_t* lboard, lboard_test_state_t* state) {
  rc_reset_lboard(lboard);
  state->active = state->submitted = 0;
}

static int lboard_evaluate(rc_lboard_t* lboard, lboard_test_state_t* test, memory_t* memory) {
  int value;

  switch (rc_evaluate_lboard(lboard, &value, peek, memory, NULL)) {
    case RC_LBOARD_STATE_STARTED:
      test->active = 1;
      break;

    case RC_LBOARD_STATE_CANCELED:
      test->active = 0;
      break;

    case RC_LBOARD_STATE_TRIGGERED:
      test->active = 0;
      test->submitted = 1;
      break;
  }

  return value;
}

static void test_lboard(void) {
  char buffer[2048];

  {
    /*------------------------------------------------------------------------
    TestSimpleLeaderboard
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_lboard_t* lboard;
    lboard_test_state_t state;
    unsigned value;

    memory.ram = ram;
    memory.size = sizeof(ram);

    lboard = parse_lboard("STA:0xH00=1::CAN:0xH00=2::SUB:0xH00=3::VAL:0xH02", buffer);
    state.active = state.submitted = 0;

    assert(!state.active);
    assert(!state.submitted);

    value = lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(!state.submitted);

    ram[0] = 3; /* submit value, but not active */
    value = lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(!state.submitted);

    ram[0] = 2; /* cancel value, but not active */
    value = lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(!state.submitted);

    ram[0] = 1; /* start value */
    value = lboard_evaluate(lboard, &state, &memory);
    assert(state.active);
    assert(!state.submitted);

    ram[0] = 2; /* cancel value */
    value = lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(!state.submitted);

    ram[0] = 3; /* submit value, but not active */
    value = lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(!state.submitted);

    ram[0] = 1; /* start value */
    value = lboard_evaluate(lboard, &state, &memory);
    assert(state.active);
    assert(!state.submitted);

    ram[0] = 3; /* submit value */
    value = lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(state.submitted);
    assert(value == 0x34U);
  }

  {
    /*------------------------------------------------------------------------
    TestStartAndCancelSameFrame
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_lboard_t* lboard;
    lboard_test_state_t state;

    memory.ram = ram;
    memory.size = sizeof(ram);

    lboard = parse_lboard("STA:0xH00=0::CAN:0xH01=18::SUB:0xH00=3::VAL:0xH02", buffer);
    state.active = state.submitted = 0;

    lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(!state.submitted);

    ram[1] = 0x13; /* disables cancel */
    lboard_evaluate(lboard, &state, &memory);
    assert(state.active);
    assert(!state.submitted);

    ram[1] = 0x12; /* enables cancel */
    lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(!state.submitted);

    ram[1] = 0x13; /* disables cancel, but start condition still true, so it shouldn't restart */
    lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(!state.submitted);

    ram[0] = 0x01; /* disables start; no effect this frame, but next frame can restart */
    lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(!state.submitted);

    ram[0] = 0x00; /* enables start */
    lboard_evaluate(lboard, &state, &memory);
    assert(state.active);
    assert(!state.submitted);
  }

  {
    /*------------------------------------------------------------------------
    TestStartAndSubmitSameFrame
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_lboard_t* lboard;
    lboard_test_state_t state;
    unsigned value;

    memory.ram = ram;
    memory.size = sizeof(ram);

    lboard = parse_lboard("STA:0xH00=0::CAN:0xH01=10::SUB:0xH01=18::VAL:0xH02", buffer);
    state.active = state.submitted = 0;

    value = lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(state.submitted);
    assert(value == 0x34U);

    ram[1] = 0; /* disable submit, value should not be resubmitted, */
    value = lboard_evaluate(lboard, &state, &memory); /* start is still true, but leaderboard should not reactivate */
    assert(!state.active);

    ram[0] = 1; /* disable start */
    value = lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);

    ram[0] = 0; /* reenable start, leaderboard should reactivate */
    value = lboard_evaluate(lboard, &state, &memory);
    assert(state.active);
  }

  {
    /*------------------------------------------------------------------------
    TestProgress
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_lboard_t* lboard;
    lboard_test_state_t state;
    unsigned value;

    memory.ram = ram;
    memory.size = sizeof(ram);

    lboard = parse_lboard("STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::PRO:0xH04::VAL:0xH02", buffer);
    state.active = state.submitted = 0;

    value = lboard_evaluate(lboard, &state, &memory);
    assert(state.active);
    assert(value == 0x56U);

    lboard = parse_lboard("STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::VAL:0xH02", buffer);
    state.active = state.submitted = 0;

    value = lboard_evaluate(lboard, &state, &memory);
    assert(state.active);
    assert(value == 0x34U);
  }

  {
    /*------------------------------------------------------------------------
    TestStartAndCondition
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_lboard_t* lboard;
    lboard_test_state_t state;

    memory.ram = ram;
    memory.size = sizeof(ram);

    lboard = parse_lboard("STA:0xH00=0_0xH01=0::CAN:0xH01=10::SUB:0xH01=18::VAL:0xH02", buffer);
    state.active = state.submitted = 0;

    lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);

    ram[1] = 0; /* second part of start condition is true */
    lboard_evaluate(lboard, &state, &memory);
    assert(state.active);
  }

  {
    /*------------------------------------------------------------------------
    TestStartOrCondition
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_lboard_t* lboard;
    lboard_test_state_t state;

    memory.ram = ram;
    memory.size = sizeof(ram);

    lboard = parse_lboard("STA:S0xH00=1S0xH01=1::CAN:0xH01=10::SUB:0xH01=18::VAL:0xH02", buffer);
    state.active = state.submitted = 0;

    lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);

    ram[1] = 1; /* second part of start condition is true */
    lboard_evaluate(lboard, &state, &memory);
    assert(state.active);

    ram[1] = 0;
    lboard_reset(lboard, &state);
    lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);

    ram[0] = 1; /* first part of start condition is true */
    lboard_evaluate(lboard, &state, &memory);
    assert(state.active);
  }

  {
    /*------------------------------------------------------------------------
    TestCancelOrCondition
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_lboard_t* lboard;
    lboard_test_state_t state;

    memory.ram = ram;
    memory.size = sizeof(ram);

    lboard = parse_lboard("STA:0xH00=0::CAN:S0xH01=12S0xH02=12::SUB:0xH00=3::VAL:0xH02", buffer);
    state.active = state.submitted = 0;

    lboard_evaluate(lboard, &state, &memory);
    assert(state.active);

    ram[2] = 12; /* second part of cancel condition is true */
    lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);

    ram[2] = 0; /* second part of cancel condition is false */
    lboard_reset(lboard, &state);
    lboard->state = RC_LBOARD_STATE_ACTIVE;
    lboard_evaluate(lboard, &state, &memory);
    assert(state.active);

    ram[1] = 12; /* first part of cancel condition is true */
    lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
  }

  {
    /*------------------------------------------------------------------------
    TestSubmitAndCondition
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_lboard_t* lboard;
    lboard_test_state_t state;

    memory.ram = ram;
    memory.size = sizeof(ram);

    lboard = parse_lboard("STA:0xH00=0::CAN:0xH01=10::SUB:0xH01=18_0xH03=18::VAL:0xH02", buffer);
    state.active = state.submitted = 0;

    lboard_evaluate(lboard, &state, &memory);
    assert(state.active);

    ram[3] = 18;
    lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(state.submitted);
  }

  {
    /*------------------------------------------------------------------------
    TestSubmitOrCondition
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_lboard_t* lboard;
    lboard_test_state_t state;

    memory.ram = ram;
    memory.size = sizeof(ram);

    lboard = parse_lboard("STA:0xH00=0::CAN:0xH01=10::SUB:S0xH01=12S0xH03=12::VAL:0xH02", buffer);
    state.active = state.submitted = 0;

    lboard_evaluate(lboard, &state, &memory);
    assert(state.active);

    ram[3] = 12; /* second part of submit condition is true */
    lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(state.submitted);

    ram[3] = 0;
    lboard_reset(lboard, &state);
    lboard->state = RC_LBOARD_STATE_ACTIVE;
    lboard_evaluate(lboard, &state, &memory);
    assert(state.active);

    ram[1] = 12; /* first part of submit condition is true */
    lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(state.submitted);
  }

  {
    /*------------------------------------------------------------------------
    TestValueFromHitCount
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_lboard_t* lboard;
    lboard_test_state_t state;
    unsigned value;

    memory.ram = ram;
    memory.size = sizeof(ram);

    lboard = parse_lboard("STA:0xH00=1::CAN:0xH00=2::SUB:0xH00=3::VAL:M:0xH02!=d0xH02", buffer);
    state.active = state.submitted = 0;

    /* not active, value should not be tallied */
    value = lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(value == 0U);
    ram[2] = 3;
    value = lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(value == 0U);

    /* active, tally will not occur as value hasn't changed */
    ram[0] = 1;
    value = lboard_evaluate(lboard, &state, &memory);
    assert(state.active);
    assert(value == 0U);

    /* active, value changed, expect tally */
    ram[2] = 11;
    value = lboard_evaluate(lboard, &state, &memory);
    assert(state.active);
    assert(value == 1U);

    /* not changed, no tally */
    value = lboard_evaluate(lboard, &state, &memory);
    assert(state.active);
    assert(value == 1U);

    /* changed, tally */
    ram[2] = 12;
    value = lboard_evaluate(lboard, &state, &memory);
    assert(state.active);
    assert(value == 2U);

    /* cancelled, no tally */
    ram[0] = 2;
    value = lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(value == 0U);
    ram[2] = 13;
    value = lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(value == 0U);

    /* reactivated, tally should be reset */
    ram[0] = 1;
    value = lboard_evaluate(lboard, &state, &memory);
    assert(state.active);
    assert(value == 0U);

    /* active, value changed, expect tally */
    ram[2] = 11;
    value = lboard_evaluate(lboard, &state, &memory);
    assert(state.active);
    assert(value == 1U);
  }

  {
    /*------------------------------------------------------------------------
    TestValueFromHitCountAddHits
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_lboard_t* lboard;
    lboard_test_state_t state;
    unsigned value;

    memory.ram = ram;
    memory.size = sizeof(ram);

    lboard = parse_lboard("STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::VAL:C:0xH03=1_M:0xH02=1", buffer);
    state.active = state.submitted = 0;

    /* active, nothing to tally */
    value = lboard_evaluate(lboard, &state, &memory);
    assert(state.active);
    assert(value == 0U);

    /* second value tallied */
    ram[2] = 1;
    value = lboard_evaluate(lboard, &state, &memory);
    assert(state.active);
    assert(value == 1U);

    /* both values tallied */
    ram[3] = 1;
    value = lboard_evaluate(lboard, &state, &memory);
    assert(state.active);
    assert(value == 3U);

    /* first value tallied */
    ram[2] = 12;
    value = lboard_evaluate(lboard, &state, &memory);
    assert(state.active);
    assert(value == 4U);

    /* cancelled, no tally */
    ram[0] = 2;
    value = lboard_evaluate(lboard, &state, &memory);
    assert(!state.active);
    assert(value == 0U);

    /* second evalute required before we can reactivate */
    lboard_evaluate(lboard, &state, &memory);

    /* reactivated, tally should be reset, and first still true */
    ram[0] = 0;
    value = lboard_evaluate(lboard, &state, &memory);
    assert(state.active);
    assert(value == 1U);
  }

  {
    /*------------------------------------------------------------------------
    TestUnparsableStringWillNotStart
    We'll test for errors in the memaddr field instead
    ------------------------------------------------------------------------*/

    lboard_check("STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::PRO:0xH04::VAL:0xH02::GARBAGE", RC_INVALID_LBOARD_FIELD);
    lboard_check("CAN:0xH00=2::SUB:0xH00=3::PRO:0xH04::VAL:0xH02", RC_MISSING_START);
    lboard_check("STA:0xH00=0::SUB:0xH00=3::PRO:0xH04::VAL:0xH02", RC_MISSING_CANCEL);
    lboard_check("STA:0xH00=0::CAN:0xH00=2::PRO:0xH04::VAL:0xH02", RC_MISSING_SUBMIT);
    lboard_check("STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::PRO:0xH04", RC_MISSING_VALUE);
    lboard_check("STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::PRO:0xH04::VAL:0xH02::STA:0=0", RC_DUPLICATED_START);
    lboard_check("STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::PRO:0xH04::VAL:0xH02::CAN:0=0", RC_DUPLICATED_CANCEL);
    lboard_check("STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::PRO:0xH04::VAL:0xH02::SUB:0=0", RC_DUPLICATED_SUBMIT);
    lboard_check("STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::PRO:0xH04::VAL:0xH02::VAL:0", RC_DUPLICATED_VALUE);
    lboard_check("STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::PRO:0xH04::VAL:0xH02::PRO:0", RC_DUPLICATED_PROGRESS);
    lboard_check("STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::VAL:M:0xH01=1_M:0xH01=2", RC_MULTIPLE_MEASURED);
    lboard_check("STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::VAL:M:0xH01=1_P:0xH01=2", RC_INVALID_VALUE_FLAG);
    lboard_check("STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::VAL:R:0xH01=1_0xH01=2", RC_INVALID_VALUE_FLAG);
    lboard_check("STA:0xH00=0::CAN:0xH00=2::SUB:0xH00=3::VAL:R:0xH01=1", RC_MISSING_VALUE_MEASURED);
  }
}

static rc_richpresence_t* parse_richpresence(const char* script, void* buffer) {
  int ret;
  rc_richpresence_t* self;

  ret = rc_richpresence_size(script);
  assert(ret >= 0);
  memset(buffer, 0xEE, ret + 128);

  self = rc_parse_richpresence(buffer, script, NULL, 0);
  assert(self != NULL);
  assert(*((int*)((char*)buffer + ret)) == 0xEEEEEEEE);

  return self;
}

static void test_richpresence(void) {
  char buffer[2048];
  char output[128];

  {
    /*------------------------------------------------------------------------
    TestStaticDisplayString
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Display:\nHello, world!", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "Hello, world!") == 0);
    assert(result == 13);
  }

  {
    /*------------------------------------------------------------------------
    TestEscapedComment
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Display:\nWhat \\// Where", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "What // Where") == 0);
    assert(result == 13);
  }

  {
    /*------------------------------------------------------------------------
    TestEscapedBackslash
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Display:\nWhat \\\\ Where", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "What \\ Where") == 0);
    assert(result == 12);
  }

  {
    /*------------------------------------------------------------------------
    TestPartiallyEscapedComment
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Display:\nWhat \\/// Where", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "What /") == 0);
    assert(result == 6);
  }

  {
    /*------------------------------------------------------------------------
    TestTrailingBackslash
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Display:\nWhat \\", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "What ") == 0);
    assert(result == 5);
  }

  {
    /*------------------------------------------------------------------------
    TestConditionalDisplay
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Display:\n?0xH0000=0?Zero\n?0xH0000=1?One\nOther", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "Zero") == 0);
    assert(result == 4);

    ram[0] = 1;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "One") == 0);
    assert(result == 3);

    ram[0] = 2;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "Other") == 0);
    assert(result == 5);
  }

  {
    /*------------------------------------------------------------------------
    TestConditionalDisplayOutOfOrder
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Display:\nOther\n?0xH0000=0?Zero\n?0xH0000=1?One", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "Other") == 0);
    assert(result == 5);
  }

  {
    /*------------------------------------------------------------------------
    TestConditionalDisplayNoDefault
    ------------------------------------------------------------------------*/
    int result = rc_richpresence_size("Display:\n?0xH0000=0?Zero");
    assert(result == RC_MISSING_DISPLAY_STRING);
  }

  {
    /*------------------------------------------------------------------------
    TestConditionalDisplayCommonPrefix
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Display:\n?0xH0000=0_0xH0001=18?First\n?0xH0000=0?Second\nThird", buffer);
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "First") == 0);

    ram[1] = 1;
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "Second") == 0);

    ram[0] = 1;
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "Third") == 0);

    ram[0] = 0;
    ram[1] = 18;
    richpresence = parse_richpresence("Display:\n?0xH0000=0?First\n?0xH0000=0_0xH0001=18?Second\nThird", buffer);
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "First") == 0);

    ram[1] = 1;
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "First") == 0);

    ram[0] = 1;
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "Third") == 0);
  }

  {
    /*------------------------------------------------------------------------
    TestConditionalDisplayDuplicatedCondition
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Display:\n?0xH0000=0?First\n?0xH0000=0?Second\nThird", buffer);
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "First") == 0);

    ram[0] = 1;
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "Third") == 0);
  }

  {
    /*------------------------------------------------------------------------
    TestConditionalDisplayInvalidCondition
    ------------------------------------------------------------------------*/
    int result = rc_richpresence_size("Display:\n?BANANA?First\nOther");
    assert(result == RC_INVALID_MEMORY_OPERAND);
  }

  {
    /*------------------------------------------------------------------------
    TestConditionalDisplayNoText
    ------------------------------------------------------------------------*/
    int result = rc_richpresence_size("Display:\n?0xH0000=0?\nOther");
    assert(result == RC_MISSING_DISPLAY_STRING);
  }

  {
    /*------------------------------------------------------------------------
    TestConditionalDisplayWhitespaceText
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Display:\n?0xH0000=0? \n?0xH0000=1?One\nOther", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, " ") == 0);
    assert(result == 1);

    ram[0] = 1;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "One") == 0);
    assert(result == 3);

    ram[0] = 2;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "Other") == 0);
    assert(result == 5);
  }

  {
    /*------------------------------------------------------------------------
    TestEmpty
    ------------------------------------------------------------------------*/
    int result = rc_richpresence_size("");
    assert(result == RC_MISSING_DISPLAY_STRING);
  }

  {
    /*------------------------------------------------------------------------
    TestValueMacro
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(0x 0001) Points", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "13330 Points") == 0);
    assert(result == 12);

    ram[1] = 20;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "13332 Points") == 0);
    assert(result == 12);

    richpresence = parse_richpresence("Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(0x 0001_V-10000) Points", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "3332 Points") == 0);
    assert(result == 11);

    ram[2] = 7;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "-8188 Points") == 0);
    assert(result == 12);
  }

  {
    /*------------------------------------------------------------------------
    TestFramesMacro
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Format:Frames\nFormatType=FRAMES\n\nDisplay:\n@Frames(0x 0001)", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "3:42.16") == 0);
    assert(result == 7);

    ram[1] = 20;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "3:42.20") == 0);
    assert(result == 7);
  }

  {
    /*------------------------------------------------------------------------
    TestValueMacroFormula
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(0xH0001*100_0xH0002) Points", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "1852 Points") == 0);
    assert(result == 11);

    ram[1] = 0x20;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "3252 Points") == 0);
    assert(result == 11);
  }

  {
    /*------------------------------------------------------------------------
    TestValueMacroFromHits
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Format:Hits\nFormatType=VALUE\n\nDisplay:\n@Hits(M:0xH01=1) Hits", buffer);
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "0 Hits") == 0);

    ram[1] = 1;
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "1 Hits") == 0);

    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "2 Hits") == 0);

    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "3 Hits") == 0);
  }

  {
    /*------------------------------------------------------------------------
    TestValueMacroFromIndirect
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Format:Value\nFormatType=VALUE\n\nDisplay:\nPointing at @Value(I:0xH00_M:0xH01)", buffer);
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "Pointing at 18") == 0);

    ram[1] = 99; /* pointed at data changes */
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "Pointing at 99") == 0);

    ram[0] = 1; /* pointer changes */
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "Pointing at 52") == 0);
  }

  {
    /*------------------------------------------------------------------------
    TestUndefinedMacro
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Display:\n@Points(0x 0001) Points", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "[Unknown macro]Points(0x 0001) Points") == 0);
    assert(result == 37);
  }

  {
    /*------------------------------------------------------------------------
    TestUndefinedMacroAtEndOfLine
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Display:\n@Points(0x 0001)", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "[Unknown macro]Points(0x 0001)") == 0);
    assert(result == 30);  }

  {
    /*------------------------------------------------------------------------
    TestMacroWithNoParameter
    ------------------------------------------------------------------------*/
    int result;

    result = rc_richpresence_size("Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points Points");
    assert(result == RC_MISSING_VALUE);
  }

  {
    /*------------------------------------------------------------------------
    TestConditionalDisplayMacroWithNoParameter
    ------------------------------------------------------------------------*/
    int result;

    result = rc_richpresence_size("Format:Points\nFormatType=VALUE\n\nDisplay:\n?0x0h0001=1?@Points Points\nDefault");
    assert(result == RC_MISSING_VALUE);
  }

  {
    /*------------------------------------------------------------------------
    TestEscapedMacro
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Format:Points\nFormatType=VALUE\n\nDisplay:\n\\@Points(0x 0001) \\@@Points(0x 0001) Points", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "@Points(0x 0001) @13330 Points") == 0);
    assert(result == 30);
  }

  {
    /*------------------------------------------------------------------------
    TestLookup
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Lookup:Location\n0=Zero\n1=One\n\nDisplay:\nAt @Location(0xH0000)", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Zero") == 0);
    assert(result == 7);

    ram[0] = 1;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At One") == 0);
    assert(result == 6);

    ram[0] = 2; /* no entry */
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At ") == 0);
    assert(result == 3);
  }

  {
    /*------------------------------------------------------------------------
    TestLookupFormula
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Lookup:Location\n0=Zero\n1=One\n\nDisplay:\nAt @Location(0xH0000*0.5)", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Zero") == 0);
    assert(result == 7);

    ram[0] = 1;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Zero") == 0);
    assert(result == 7);

    ram[0] = 2; /* no entry */
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At One") == 0);
    assert(result == 6);
  }

  {
    /*------------------------------------------------------------------------
    TestLookupRepeated
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Lookup:Location\n0=Zero\n1=One\n\nDisplay:\nAt @Location(0xH0000), Near @Location(0xH0001)", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Zero, Near ") == 0);
    assert(result == 14);

    ram[1] = 1;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Zero, Near One") == 0);
    assert(result == 17);

    ram[0] = 1;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At One, Near One") == 0);
    assert(result == 16);
  }

  {
    /*------------------------------------------------------------------------
    TestLookupMultiple
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Lookup:Location\n0=Zero\n1=One\n\nLookup:Location2\n0=zero\n1=one\n\nDisplay:\nAt @Location(0xH0000), Near @Location2(0xH0001)", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Zero, Near ") == 0);
    assert(result == 14);

    ram[1] = 1;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Zero, Near one") == 0);
    assert(result == 17);

    ram[0] = 1;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At One, Near one") == 0);
    assert(result == 16);
  }

  {
    /*------------------------------------------------------------------------
    TestLookupAndValue
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Lookup:Location\n0=Zero\n1=One\n\nFormat:Location2\nFormatType=VALUE\n\nDisplay:\nAt @Location(0xH0000), Near @Location2(0xH0001)", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Zero, Near 18") == 0);
    assert(result == 16);

    ram[1] = 1;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Zero, Near 1") == 0);
    assert(result == 15);

    ram[0] = 1;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At One, Near 1") == 0);
    assert(result == 14);
  }

  {
    /*------------------------------------------------------------------------
    TestLookupHexKeys
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Lookup:Location\n0x00=Zero\n0x01=One\n\nDisplay:\nAt @Location(0xH0000)", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Zero") == 0);
    assert(result == 7);

    ram[0] = 1;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At One") == 0);
    assert(result == 6);

    ram[0] = 2; /* no entry */
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At ") == 0);
    assert(result == 3);
  }

  {
    /*------------------------------------------------------------------------
    TestLookupDefault
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Lookup:Location\n0=Zero\n1=One\n*=Star\n\nDisplay:\nAt @Location(0xH0000)", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Zero") == 0);
    assert(result == 7);

    ram[0] = 1;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At One") == 0);
    assert(result == 6);

    ram[0] = 2; /* no entry */
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Star") == 0);
    assert(result == 7);
  }

  {
    /*------------------------------------------------------------------------
    TestLookupCRLF
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Lookup:Location\r\n0=Zero\r\n1=One\r\n\r\nDisplay:\r\nAt @Location(0xH0000)\r\n", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Zero") == 0);
    assert(result == 7);

    ram[0] = 1;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At One") == 0);
    assert(result == 6);

    ram[0] = 2; /* no entry */
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At ") == 0);
    assert(result == 3);
  }

  {
    /*------------------------------------------------------------------------
    TestLookupAfterDisplay
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Display:\nAt @Location(0xH0000)\n\nLookup:Location\n0=Zero\n1=One", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Zero") == 0);
    assert(result == 7);

    ram[0] = 1;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At One") == 0);
    assert(result == 6);

    ram[0] = 2; /* no entry */
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At ") == 0);
    assert(result == 3);
  }

  {
    /*------------------------------------------------------------------------
    TestLookupWhitespace
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;
    int result;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Lookup:Location\n0= Zero \n1= One \n\nDisplay:\nAt '@Location(0xH0000)' ", buffer);
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At ' Zero ' ") == 0);
    assert(result == 12);

    ram[0] = 1;
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At ' One ' ") == 0);
    assert(result == 11);

    ram[0] = 2; /* no entry */
    result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At '' ") == 0);
    assert(result == 6);
  }

  {
    /*------------------------------------------------------------------------
    TestLookupInvalid
    ------------------------------------------------------------------------*/
    int result;

    result = rc_richpresence_size("Lookup:Location\nOx0=Zero\n1=One\n\nDisplay:\nAt @Location(0xH0000)");
    assert(result == RC_INVALID_CONST_OPERAND);

    result = rc_richpresence_size("Lookup:Location\n0xO=Zero\n1=One\n\nDisplay:\nAt @Location(0xH0000)");
    assert(result == RC_INVALID_CONST_OPERAND);

    result = rc_richpresence_size("Lookup:Location\nZero=Zero\n1=One\n\nDisplay:\nAt @Location(0xH0000)");
    assert(result == RC_INVALID_CONST_OPERAND);

    result = rc_richpresence_size("Lookup:Location\n0=Zero\n1=One\n\nDisplay:\nAt @Location");
    assert(result == RC_MISSING_VALUE);

    result = rc_richpresence_size("Lookup:Location\n0=Zero\n1=One\n\nDisplay:\nAt @Location()");
    assert(result == RC_INVALID_MEMORY_OPERAND);

    result = rc_richpresence_size("Lookup:Location\n0=Zero\n1=One\n\nDisplay:\nAt @Location(Zero)");
    assert(result == RC_INVALID_MEMORY_OPERAND);
  }

  {
    /*------------------------------------------------------------------------
    TestRandomTextBetweenSections
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;

    memory.ram = ram;
    memory.size = sizeof(ram);

    /* Anything that doesn't begin with "Format:" "Lookup:" or "Display:" is ignored. People sometimes
       use this logic to add comments to the Rich Presence script - particularly author comments */
    richpresence = parse_richpresence("Locations are fun!\nLookup:Location\n0=Zero\n1=One\n\nDisplay goes here\nDisplay:\nAt @Location(0xH0000)\n\nWritten by User3", buffer);
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Zero") == 0);

    ram[0] = 1;
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At One") == 0);

    ram[0] = 2; /* no entry */
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At ") == 0);
  }

  {
    /*------------------------------------------------------------------------
    TestComments
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("// Locations are fun!\nLookup:Location // lookup\n0=Zero // 0\n1=One // 1\n\n//Display goes here\nDisplay: // display\nAt @Location(0xH0000) // text\n\n//Written by User3", buffer);
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Zero") == 0);

    ram[0] = 1;
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At One") == 0);

    ram[0] = 2; /* no entry */
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At ") == 0);
  }

  {
    /*------------------------------------------------------------------------
    TestConditionalDisplaySharedLookup
    ------------------------------------------------------------------------*/
    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_richpresence_t* richpresence;

    memory.ram = ram;
    memory.size = sizeof(ram);

    richpresence = parse_richpresence("Lookup:Location\n0x00=Zero\n0x01=One\n\nDisplay:\n?0xH0001=18?At @Location(0xH0000)\nNear @Location(0xH0000)", buffer);
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At Zero") == 0);

    ram[0] = 1;
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "At One") == 0);

    ram[1] = 17;
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "Near One") == 0);

    ram[0] = 0;
    rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, &memory, NULL);
    assert(strcmp(output, "Near Zero") == 0);
  }
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

TEST_FRAMEWORK_DECLARATIONS()

int main(void) {
  TEST_FRAMEWORK_INIT();

  test_memref();
  test_operand();
  test_condition();
  test_condset();
  test_trigger();
  test_value();

  test_lboard();
  test_richpresence();
  test_runtime();
  test_lua();

  TEST_FRAMEWORK_SHUTDOWN();

  return TEST_FRAMEWORK_PASSED() ? 0 : 1;
}
