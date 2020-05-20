#include "internal.h"

#include "../test_framework.h"
#include "mock_memory.h"

static void _assert_parse_condset(rc_condset_t** condset, rc_memref_value_t** memrefs, void* buffer, const char* memaddr)
{
  rc_parse_state_t parse;
  int size;

  rc_init_parse_state(&parse, buffer, 0, 0);
  rc_init_parse_state_memrefs(&parse, memrefs);

  *condset = rc_parse_condset(&memaddr, &parse);
  size = parse.offset;
  rc_destroy_parse_state(&parse);

  ASSERT_NUM_GREATER(size, 0);
  ASSERT_PTR_NOT_NULL(*condset);
}
#define assert_parse_condset(condset, memrefs_out, buffer, memaddr) ASSERT_HELPER(_assert_parse_condset(condset, memrefs_out, buffer, memaddr), "assert_parse_condset")

static void _assert_evaluate_condset(rc_condset_t* condset, rc_memref_value_t* memrefs, memory_t* memory, int expected_result) {
  int result;
  rc_eval_state_t eval_state;

  rc_update_memref_values(memrefs, peek, memory);

  memset(&eval_state, 0, sizeof(eval_state));
  eval_state.peek = peek;
  eval_state.peek_userdata = memory;

  result = rc_test_condset(condset, &eval_state);

  /* NOTE: reset normally handled by trigger since it's not group specific */
  if (eval_state.was_reset)
    rc_reset_condset(condset);

  ASSERT_NUM_EQUALS(result, expected_result);
}
#define assert_evaluate_condset(condset, memrefs, memory, expected_result) ASSERT_HELPER(_assert_evaluate_condset(condset, memrefs, memory, expected_result), "assert_evaluate_condset")

static rc_condition_t* condset_get_cond(rc_condset_t* condset, int cond_index) {
  rc_condition_t* cond = condset->conditions;

  while (cond_index-- != 0) {
    if (cond == NULL)
      break;

    cond = cond->next;
  }

  return cond;
}

static void _assert_hit_count(rc_condset_t* condset, int cond_index, unsigned expected_hit_count) {
  rc_condition_t* cond = condset_get_cond(condset, cond_index);
  ASSERT_PTR_NOT_NULL(cond);

  ASSERT_NUM_EQUALS(cond->current_hits, expected_hit_count);
}
#define assert_hit_count(condset, cond_index, expected_hit_count) ASSERT_HELPER(_assert_hit_count(condset, cond_index, expected_hit_count), "assert_hit_count")


static void test_hitcount_increment_when_true() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, "0xH0001=18"); /* one condition, true */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1U);
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2U);
}

static void test_hitcount_does_not_increment_when_false() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, "0xH0001!=18"); /* one condition, false */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0U);
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0U);
}

static void test_hitcount_target() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, "0xH0001=20(2)_0xH0002=52");
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  ram[1] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 2);

  /* hit target met, overall is true */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 3);

  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2); /* hit target met, not incremented */
  assert_hit_count(condset, 1, 4);

  /* first condition no longer true, but hit count was met so it acts true */
  ram[1] = 18;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 5);
}

static void test_hitcount_two_conditions(const char* memaddr, unsigned expected_result, unsigned expected_hitcount1, unsigned expected_hitcount2) {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, memaddr);
  assert_evaluate_condset(condset, memrefs, &memory, expected_result);
  assert_hit_count(condset, 0, expected_hitcount1);
  assert_hit_count(condset, 1, expected_hitcount2);
}

static void test_hitcount_three_conditions(const char* memaddr, unsigned expected_result, unsigned expected_hitcount1, 
    unsigned expected_hitcount2, unsigned expected_hitcount3) {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, memaddr);
  assert_evaluate_condset(condset, memrefs, &memory, expected_result);
  assert_hit_count(condset, 0, expected_hitcount1);
  assert_hit_count(condset, 1, expected_hitcount2);
  assert_hit_count(condset, 2, expected_hitcount3);
}

static void test_pauseif() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, "0xH0001=18_P:0xH0002=52_P:0xL0x0004=6");

  /* first condition true, but ignored because both pause conditions are true */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0); /* Also true, but processing stops on first PauseIf */

  /* first pause condition no longer true, but second still is */
  ram[2] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0); /* PauseIf goes to 0 when false */
  assert_hit_count(condset, 2, 1);

  /* both pause conditions not true, set will trigger */
  ram[4] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
}

static void test_pauseif_hitcount_one() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, "0xH0001=18_P:0xH0002=52.1.");

  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* pause condition no longer true, but hitcount prevents trigger */
  ram[2] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
}

static void test_pauseif_hitcount_two() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, "0xH0001=18_P:0xH0002=52.2.");

  /* pause hit target has not been met, group is true */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);

  /* pause hit target has been met, group is false */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 2);

  /* pause condition is no longer true, but hitcount prevents trigger */
  ram[2] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 2);
}

static void test_pauseif_hitcount_with_reset() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, "0xH0001=18_P:0xH0002=52.1._R:0xH0003=1");

  /* pauseif triggered, non-pauseif conditions ignored */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);

  /* pause condition is no longer true, but hitcount prevents trigger */
  ram[2] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);

  /* pause has precedence over reset, reset in group is ignored */
  ram[3] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);
}

static void test_pauseif_does_not_increment_hits() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, "0xH0001=18(2)_0xH0002=52_P:0xL0004=4");

  /* both conditions true */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);

  /* pause condition is true, other conditions should not tally hits */
  ram[4] = 0x54;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 1);

  /* pause condition not true, other conditions should tally hits */
  ram[4] = 0x56;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 0);

  /* pause condition is true, other conditions should not tally hits */
  ram[4] = 0x54;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 1);

  /* pause condition not true, other conditions should tally hits */
  ram[4] = 0x56;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 3);
  assert_hit_count(condset, 2, 0);
}

static void test_pauseif_delta_updated() {
  unsigned char ram[] = {0x00, 0x00, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, "P:0xH0001=1_d0xH0002=60");

  /* upaused, delta = 0, current = 52 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);

  /* paused, delta = 52, current = 44 */
  ram[1] = 1;
  ram[2] = 44;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);

  /* paused, delta = 44, current = 60 */
  ram[2] = 60;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 0);

  /* unpaused, delta = 60, current = 97 */
  ram[1] = 0;
  ram[2] = 97;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
}

static void test_resetif() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, "0xH0001=18_R:0xH0002=50_R:0xL0x0004=4");

  /* first condition true, neither reset true */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);

  /* first reset true */
  ram[2] = 50;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0); /* hitcount reset */

  /* both resets true */
  ram[4] = 0x54;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);

  /* only second reset is true */
  ram[2] = 52;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);

  /* neither reset true */
  ram[4] = 0x56;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
}

static void test_resetif_cond_with_hittarget() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, "0xH0001=18(2)_0xH0002=52_R:0xL0004=4");

  /* both conditions true, reset not true */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);

  /* hit target met */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 0);

  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 3);
  assert_hit_count(condset, 2, 0);

  /* reset */
  ram[4] = 0x54;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* reset no longer true, hit target not met */
  ram[4] = 0x56;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);

  /* hit target met */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 0);
}

static void test_resetif_hitcount() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, "0xH0001=18(2)_0xH0002=52_R:0xL0004=4.2.");

  /* hitcounts on conditions 1 and 2 are incremented */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* hitcounts on conditions 1 and 2 are incremented. cond 1 now true, so entire set is true */
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* hitcount on condition 2 is incremented, cond 1 already at its target */
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 3);
  assert_hit_count(condset, 2, 0);

  /* reset condition is true, but its hitcount is not met */
  ram[4] = 0x54;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 4);
  assert_hit_count(condset, 2, 1);

  /* second hit on reset condition should reset everything */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
}

static void test_resetif_hitcount_one() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, "0xH0001=18(2)_0xH0002=52_R:0xL0004=4.1.");

  /* hitcounts on conditions 1 and 2 are incremented */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* hitcounts on conditions 1 and 2 are incremented. cond 1 now true, so entire set is true */
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* hitcount on condition 2 is incremented, cond 1 already at its target */
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 3);
  assert_hit_count(condset, 2, 0);

  /* reset condition is true, its hitcount is met, so all hitcounts (including the resetif) should be reset */
  ram[4] = 0x54;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
}

static void test_resetif_hitcount_addhits() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* never(repeated(3, byte(1) == 18 || low(4) == 6)) */
  assert_parse_condset(&condset, &memrefs, buffer, "C:0xH0001=18_R:0xL0004=6(3)");

  /* result is true, no non-reset conditions */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);

  /* total hitcount is met (2 for each condition, need 3 total) , everything resets */
  ram[4] = 0x54;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
}

static void test_pauseif_resetif_hitcounts() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_condset(&condset, &memrefs, buffer, "0xH0001=18(2)_R:0xH0002=50_P:0xL0004=4");

  /* first condition is true, pauseif and resetif are not, so it gets a hit */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);

  /* pause is true, hit not incremented or reset */
  ram[4] = 0x54;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);

  /* reset if true, but set is still paused */
  ram[2] = 50;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);

  /* set no longer paused, reset clears hitcount */
  ram[4] = 0x56;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);

  /* reset no longer true, hits increment again */
  ram[2] = 52;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);

  /* hitcount met, set is true */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
}

static void test_addsource() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(1) + byte(2) == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "A:0xH0001=0_0xH0002=22");

  /* sum is not correct */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);

  /* sum is correct */
  ram[2] = 4;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1); /* hit only tallied on final condition */

  /* first condition is true, but not sum */
  ram[1] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* first condition is true, sum is correct */
  ram[2] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 2);
}

static void test_addsource_overflow() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* adding two bytes will result in a value larger than 256, don't truncate to a byte */
  assert_parse_condset(&condset, &memrefs, buffer, "A:0xH0001=0_0xH0002=22");

  /* sum is 0x102 (0x12 + 0xF0) */
  ram[2] = 0xF0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* sum is 0x122 (0x32 + 0xF0) */
  ram[1] = 0x32;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
}

static void test_subsource() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* NOTE: SubSource subtracts the first item from the second! */
  /* byte(1) - byte(2) == 14 */
  assert_parse_condset(&condset, &memrefs, buffer, "B:0xH0002=0_0xH0001=14");

  /* difference is not correct */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);

  /* difference is correct */
  ram[2] = 4;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1); /* hit only tallied on final condition */

  /* first condition is true, but not difference */
  ram[1] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* first condition is true, difference is negative inverse of expected value */
  ram[2] = 14;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* difference is correct again */
  ram[1] = 28;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 2);
}

static void test_subsource_overflow() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* subtracting two bytes will result in a very large positive number, don't truncate to a byte */
  assert_parse_condset(&condset, &memrefs, buffer, "B:0xH0002=0_0xH0001=14");

  /* difference is -10 (8 - 18) */
  ram[2] = 8;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* difference is 0xFFFFFF0E (8 - 0xFA) */
  ram[1] = 0xFA;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
}

static void test_addsource_subsource() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(1) - low(2) + low(4) == 14 */
  assert_parse_condset(&condset, &memrefs, buffer, "A:0xH0001=0_B:0xL0002=0_0xL0004=14");

  /* sum is not correct */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* sum is correct */
  ram[1] = 12;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 1);

  /* first condition is true, but not sum */
  ram[1] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 1);

  /* byte(4) would make sum true, but not low(4) */
  ram[4] = 0x12;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 1);

  /* difference is correct again */
  ram[2] = 1;
  ram[4] = 15;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 2);
}

static void test_addsource_multiply() {
  unsigned char ram[] = {0x00, 0x06, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(1) * 3 + byte(2) == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "A:0xH0001*3_0xH0002=22");

  /* sum is not correct */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);

  /* sum is correct */
  ram[2] = 4;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* sum is not correct */
  ram[1] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* sum is correct */
  ram[2] = 19;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 2);
}

static void test_subsource_multiply() {
  unsigned char ram[] = {0x00, 0x06, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(2) - byte(1) * 3 == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "B:0xH0001*3_0xH0002=14");

  /* difference is not correct */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);

  /* difference is correct */
  ram[2] = 32;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* difference is not correct */
  ram[1] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* difference is correct */
  ram[2] = 17;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 2);
}

static void test_addsource_multiply_fraction() {
  unsigned char ram[] = {0x00, 0x08, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(1) * 0.75 + byte(2) == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "A:0xH0001*f0.75_0xH0002=22");

  /* sum is not correct */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);

  /* sum is correct */
  ram[2] = 16;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* sum is not correct */
  ram[1] = 15;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* sum is correct */
  ram[2] = 11;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 2);
}

static void test_addsource_divide() {
  unsigned char ram[] = {0x00, 0x06, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(1) / 3 + byte(2) == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "A:0xH0001/3_0xH0002=22");

  /* sum is not correct */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);

  /* sum is correct */
  ram[2] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* sum is not correct */
  ram[1] = 14;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* sum is correct */
  ram[2] = 18;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 2);
}

static void test_subsource_divide() {
  unsigned char ram[] = {0x00, 0x06, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(2) - byte(1) / 3 == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "B:0xH0001/3_0xH0002=14");

  /* difference is not correct */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);

  /* difference is correct */
  ram[2] = 16;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* difference is not correct */
  ram[1] = 14;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* difference is correct */
  ram[2] = 18;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 2);
}

static void test_addsource_mask() {
  unsigned char ram[] = {0x00, 0x06, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(1) & 0x07 + byte(2) == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "A:0xH0001&h7_0xH0002=22");

  /* sum is not correct */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);

  /* sum is correct */
  ram[2] = 16;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* sum is not correct */
  ram[1] = 0x74;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* sum is correct */
  ram[2] = 18;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 2);
}

static void test_subsource_mask() {
  unsigned char ram[] = {0x00, 0x6C, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(2) - byte(1) & 0x06 == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "B:0xH0001&6_0xH0002=14");

  /* difference is not correct */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);

  /* difference is correct */
  ram[2] = 18;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* difference is not correct */
  ram[1] = 10;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);

  /* difference is correct */
  ram[2] = 16;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 2);
}

static void test_addhits() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /*  repeated(4, byte(1) == 18 || low(4) == 6) */
  assert_parse_condset(&condset, &memrefs, buffer, "C:0xH0001=18(2)_0xL0004=6(4)");

  /* both conditions true, total not met */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);

  /* total hits met (two for each condition) */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 2);

  /* target met for first, it stops incrementing, second continues */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 3);

  /* reset hit counts */
  rc_reset_condset(condset);

  /* both conditions true, total not met */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);

  /* first condition not true, total not met*/
  ram[1] = 16;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 2);

  /* total met */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 3);
}

static void test_addhits_no_target() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* AddHits is not a substitution for OrNext */
  /* since the second condition doesn't have a target hit count, the hits tallied by the first condition are ignored */
  assert_parse_condset(&condset, &memrefs, buffer, "C:0xH0001=18_0xH0000=1");

  /* first condition true, but ignored */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);

  /* second condition true, overall is true */
  ram[0] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 1);

  /* second condition no longer true, overall is not true */
  ram[0] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 3);
  assert_hit_count(condset, 1, 1);
}

static void test_addhits_with_addsource() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* repeated(2, (byte(1) + byte(2) == 70) || byte(0) == 0) */
  assert_parse_condset(&condset, &memrefs, buffer, "A:0xH0001_C:0xH0002=70_0xH0000=0(2)");

  /* addsource (conditions 1 and 2) is true, condition 3 is true, total of two hits, overall is true */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 1);

  /* repeated(2, byte(0) == 0 || (byte(1) + byte(2) == 70)) */
  assert_parse_condset(&condset, &memrefs, buffer, "C:0xH0000=0_A:0xH0001=0_0xH0002=70(2)");

  /* condition 1 is true, addsource (conditions 2 and 3) is true, total of two hits, overall is true */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 1);
}

static void test_andnext() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* repeated(3, byte(0x0001) == 20 && byte(0x0002) == 20 && byte(0x0003) == 20) */
  assert_parse_condset(&condset, &memrefs, buffer, "N:0xH0001=20_N:0xH0002=20_0xH0003=20.3.");

  /* all conditions are false */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* final condition is not enough */
  ram[3] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* first two are true, still not enough */
  ram[2] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* all three are true, tally hits. hits are tallied for each true statement starting with the first */
  ram[1] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 1);

  /* middle condition not true, only first tallies a hit */
  ram[2] = 30;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 1);

  /* all three conditions are true */
  ram[2] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 3);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 2);

  /* third condition not true, first two tally hits */
  ram[3] = 30;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 4);
  assert_hit_count(condset, 1, 3);
  assert_hit_count(condset, 2, 2);

  /* all three conditions are true, hit target reached */
  ram[3] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 5);
  assert_hit_count(condset, 1, 4);
  assert_hit_count(condset, 2, 3);

  /* hit target previously reached */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 6);
  assert_hit_count(condset, 1, 5);
  assert_hit_count(condset, 2, 3);

  /* second condition no longer true, only first condition tallied, hit target was previously met */
  ram[2] = 30;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 7);
  assert_hit_count(condset, 1, 5);
  assert_hit_count(condset, 2, 3);
}

static void test_andnext_boundaries() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(0x0000) == 0 && once(byte(0x0001) == 20 && byte(0x0002) == 20 && byte(0x0003) == 20) && byte(0x0000) == 0 */
  assert_parse_condset(&condset, &memrefs, buffer, "0xH0000=0_N:0xH0001=20_N:0xH0002=20_0xH0003=20.1._0xH0000=0");

  /* first and last condition are true */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);
  assert_hit_count(condset, 4, 1);

  /* final condition of AndNext chain is not enough */
  ram[3] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);
  assert_hit_count(condset, 4, 2);

  /* two conditions of AndNext chain are true, still not enough */
  ram[2] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 3);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);
  assert_hit_count(condset, 4, 3);

  /* whole AndNext chain is true */
  ram[1] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 4);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 1);
  assert_hit_count(condset, 3, 1);
  assert_hit_count(condset, 4, 4);
}

static void test_andnext_resetif() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(0x0000) == 0 && never(byte(0x0001) == 20 && byte(0x0002) == 20 && byte(0x0003) == 20) */
  assert_parse_condset(&condset, &memrefs, buffer, "0xH0000=0_N:0xH0001=20_N:0xH0002=20_R:0xH0003=20");

  /* tally a hit */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);

  /* final condition of AndNext chain is not enough */
  ram[3] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);

  /* two conditions of AndNext chain are true, still not enough */
  ram[2] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 3);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);

  /* whole AndNext chain is true */
  ram[1] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);

  /* middle condition not true */
  ram[2] = 30;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);

  /* whole AndNext chain is true */
  ram[2] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);

  /* third condition not true */
  ram[3] = 30;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 1);
  assert_hit_count(condset, 3, 0);

  /* whole AndNext chain is true */
  ram[3] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);
}

static void test_andnext_pauseif() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(0x0000) == 0 && unless(byte(0x0001) == 20 && byte(0x0002) == 20 && byte(0x0003) == 20) */
  assert_parse_condset(&condset, &memrefs, buffer, "0xH0000=0_N:0xH0001=20_N:0xH0002=20_P:0xH0003=20");

  /* tally a hit */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);

  /* final condition of AndNext chain is not enough */
  ram[3] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);

  /* two conditions of AndNext chain are true, still not enough */
  ram[2] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 3);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);

  /* whole AndNext chain is true */
  ram[1] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 3);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 1);
  assert_hit_count(condset, 3, 1);

  /* middle condition not true */
  ram[2] = 30;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 4);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 1);
  assert_hit_count(condset, 3, 0); /* pauseif goes to 0 when not true */

  /* whole AndNext chain is true */
  ram[2] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 4);
  assert_hit_count(condset, 1, 3);
  assert_hit_count(condset, 2, 2);
  assert_hit_count(condset, 3, 1);

  /* third condition not true */
  ram[3] = 30;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 5);
  assert_hit_count(condset, 1, 4);
  assert_hit_count(condset, 2, 3);
  assert_hit_count(condset, 3, 0); /* pauseif goes to 0 when not true */

  /* whole AndNext chain is true */
  ram[3] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 5);
  assert_hit_count(condset, 1, 5);
  assert_hit_count(condset, 2, 4);
  assert_hit_count(condset, 3, 1);
}

static void test_andnext_addsource() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* once(byte(0x0001) + byte(0x0002) == 20 && byte(0x0003) == 20) */
  assert_parse_condset(&condset, &memrefs, buffer, "A:0xH0001=0_N:0xH0002=20_0xH0003=20.1.");

  /* nothing true */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* final condition true */
  ram[3] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* conditions 2 and 3 true, but AddSource in condition 1 makes condition 2 not true */
  ram[2] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* AddSource condition true via sum, whole set is true */
  ram[1] = ram[2] = 10;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 1);
}

static void test_andnext_addhits() {
  unsigned char ram[] = {0x00, 0x00, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* repeated(5, (byte(0) == 1 && byte(0x0001) > prev(byte(0x0001))) || byte(0) == 2 || 0 == 1) */
  assert_parse_condset(&condset, &memrefs, buffer, "N:0xH00=1_C:0xH01>d0xH01_N:0=1_0=1.2.");

  /* initialize delta */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);

  /* second part of AndNext is true, but first is still false */
  ram[1] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);

  /* both parts of AndNext are true */
  ram[0] = 1;
  ram[1] = 2;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);

  /* And Next true again, hit count should match target */
  ram[1] = 3;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 0);
  assert_hit_count(condset, 3, 0);
}

static void test_andnext_between_addhits() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* AndNext has higher priority than AddHits
  *
  *   AddHits byte(0x0001) == 20 (2)
  *   AndNext byte(0x0002) == 20 (2)  <-- hit count only applies to line 2, AddHits on line 1 modifies line 3
  *           byte(0x0003) == 20 (4)
  *
  * The AndNext on line 2 will combine with line 3, not line 1, so the overall interpretation is:
  *
  *   repeated(4, repeated(2, byte(0x0001) == 20) || (byte(0x0002) == 20 && byte(0x0003) == 20)))
  */
  assert_parse_condset(&condset, &memrefs, buffer, "C:0xH0001=20.2._N:0xH0002=20.2._0xH0003=20.4.");

  /* nothing true */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* final condition is not enough to trigger */
  ram[3] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* second condition is true, but only has one hit, so won't increment third */
  ram[2] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);

  /* first condition true, but not second, only first will increment */
  /* hits from first condition should not cause second condition to act true */
  ram[2] = 0;
  ram[1] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);

  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);

  /* all three conditions are true. the first has already hit its target hit count, the
   * second and third will increment. the total of the first and third is only 3, so no trigger */
  ram[2] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 1);

  /* third clause will tally again and set will be true */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 2);
}

static void test_andnext_with_hits_chain() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* AndNext has higher priority than AddHits
  *
  *   AndNext byte(0x0001) == 20 (1)
  *   AndNext byte(0x0002) == 20 (1)
  *           byte(0x0003) == 20 (1)
  *
  * Line 1 must be true before line 2 can be true, which has to be true before line 3
  *
  *   a = once(byte(0x0001) == 20)
  *   b = once(a && byte(0x0002) == 20)
  *   c = once(b && byte(0x0003) == 20)
  */
  assert_parse_condset(&condset, &memrefs, buffer, "N:0xH0001=20.1._N:0xH0002=20.1._0xH0003=20.1.");

  /* nothing true */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* final condition is not enough to trigger */
  ram[3] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* second condition is true, cut can't tally until the first is true */
  ram[2] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 0);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* first condition is true, but not second, only first will increment */
  ram[2] = 0;
  ram[1] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* final condition cannot tally without the previous items in the chain */
  ram[3] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 0);
  assert_hit_count(condset, 2, 0);

  /* only second condition true. first is historically true, so second can tally */
  ram[3] = ram[1] = 0;
  ram[2] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 0);

  /* only final condition true, first two historically true, so can tally */
  ram[3] = 20;
  ram[2] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 1);

  /* nothing true, but all historically true, overall still true */
  ram[3] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 1);
}

static void test_andnext_changes_to() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(0x0001) ~> 18 */
  assert_parse_condset(&condset, &memrefs, buffer, "N:0xH0001=18_d0xH0001!=18");

  /* value already 18, initial delta value is 0, so its considered changed */
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* value already 18 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* value no longer 18 */
  ram[1] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* value changes to 18 */
  ram[1] = 18;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* value already 18 */
  assert_evaluate_condset(condset, memrefs, &memory, 0);
}

static void test_ornext() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* repeated(5, byte(0x0001) == 20 || byte(0x0002) == 20 || byte(0x0003) == 20) */
  assert_parse_condset(&condset, &memrefs, buffer, "O:0xH0001=20_O:0xH0002=20_0xH0003=20.6.");

  /* first condition is true, which chains to make the second and third conditions true */
  ram[1] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 1);
  assert_hit_count(condset, 1, 1);
  assert_hit_count(condset, 2, 1);

  /* first and second are true, all but third should update, but only 1 hit each */
  ram[2] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 2);
  assert_hit_count(condset, 1, 2);
  assert_hit_count(condset, 2, 2);

  /* all three true, only increment each once */
  ram[2] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 3);
  assert_hit_count(condset, 1, 3);
  assert_hit_count(condset, 2, 3);

  /* only middle is true, first won't be incremented */
  ram[1] = ram[3] = 30;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 3);
  assert_hit_count(condset, 1, 4);
  assert_hit_count(condset, 2, 4);

  /* only last is true, only it will be incremented */
  ram[2] = 30; 
  ram[3] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 3);
  assert_hit_count(condset, 1, 4);
  assert_hit_count(condset, 2, 5);

  /* none are true */
  ram[3] = 30;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
  assert_hit_count(condset, 0, 3);
  assert_hit_count(condset, 1, 4);
  assert_hit_count(condset, 2, 5);

  /* first is true, hit target met, set is true */
  ram[1] = 20;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 4);
  assert_hit_count(condset, 1, 5);
  assert_hit_count(condset, 2, 6);

  /* hit target met */
  assert_evaluate_condset(condset, memrefs, &memory, 1);
  assert_hit_count(condset, 0, 5);
  assert_hit_count(condset, 1, 6);
  assert_hit_count(condset, 2, 6);
}

static void test_andnext_ornext_interaction() {
  unsigned char ram[] = {0, 0, 0, 0, 0};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* AndNext and OrNext are evaluated at each step: (((1 || 2) && 3) || 4) */
  assert_parse_condset(&condset, &memrefs, buffer, "O:0xH0001=1_N:0xH0002=1_O:0xH0003=1_0xH0004=1");

  ram[3] = 0; assert_evaluate_condset(condset, memrefs, &memory, 0); /* (((0 || 0) && 0) || 0) = 0 */
  ram[4] = 1; assert_evaluate_condset(condset, memrefs, &memory, 1); /* (((0 || 0) && 0) || 1) = 1 */
  ram[3] = 1; assert_evaluate_condset(condset, memrefs, &memory, 1); /* (((0 || 0) && 1) || 1) = 1 */
  ram[4] = 0; assert_evaluate_condset(condset, memrefs, &memory, 0); /* (((0 || 0) && 1) || 0) = 0 */
  ram[2] = 1; assert_evaluate_condset(condset, memrefs, &memory, 1); /* (((0 || 0) && 1) || 0) = 1 */
  ram[1] = 1; assert_evaluate_condset(condset, memrefs, &memory, 1); /* (((1 || 0) && 1) || 0) = 1 */
  ram[2] = 0; assert_evaluate_condset(condset, memrefs, &memory, 1); /* (((1 || 0) && 1) || 0) = 1 */
  ram[3] = 0; assert_evaluate_condset(condset, memrefs, &memory, 0); /* (((1 || 0) && 0) || 0) = 0 */
  ram[4] = 1; assert_evaluate_condset(condset, memrefs, &memory, 1); /* (((1 || 0) && 0) || 1) = 1 */
  ram[3] = 1; assert_evaluate_condset(condset, memrefs, &memory, 1); /* (((1 || 0) && 1) || 1) = 1 */
}

static void test_addaddress_direct_pointer() {
  unsigned char ram[] = {0x01, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(0x0000 + byte(0xh0000)) == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "I:0xH0000=0_0xH0000=22");

  /* initially, byte(0x0000 + 1) == 22, false */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* pointed-at value is correct */
  ram[1] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* point to new value */
  ram[0] = 2;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* new pointed-at value is correct */
  ram[2] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* point to original value, still correct */
  ram[0] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* original value no longer correct */
  ram[1] = 11;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
}

static void test_addaddress_indirect_pointer() {
  unsigned char ram[] = {0x01, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(0x0002 + byte(0xh0000)) == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "I:0xH0000_0xH0002=22");

  /* initially, byte(0x0002 + 1) == 22, false */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* non-offset value is correct */
  ram[1] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* pointed-at value is correct */
  ram[3] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* point to new value */
  ram[0] = 2;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* new pointed-at value is correct */
  ram[4] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* point to new value */
  ram[0] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* new pointed-at value is correct */
  ram[2] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
}

static void test_addaddress_indirect_pointer_out_of_range() {
  unsigned char ram[] = {0x01, 0x12, 0x34, 0xAB, 0x56, 0x16};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram) - 1; /* purposely hide ram[5] */

  /* byte(0x0002 + byte(0xh0000)) == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "I:0xH0000_0xH0002=22");

  /* pointed-at value is correct */
  ram[3] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* way out of bounds */
  ram[0] = 100;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* boundary condition - ram[5] value is correct, but should be unreachable */
  /* NOTE: address validation must be handled by the registered 'peek' callback */
  ram[0] = 3;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
}

static void test_addaddress_indirect_pointer_multiple() {
    unsigned char ram[] = {0x01, 0x02, 0x03, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_condset_t* condset;
    rc_memref_value_t* memrefs = NULL;
    char buffer[2048];

    memory.ram = ram;
    memory.size = sizeof(ram);

    /* the expectation is that the AddAddress lines will share rc_memref_value_t's, but the following lines
       will generate their own rc_memref_value_t's for indirection. none of that is actually verified. */
    assert_parse_condset(&condset, &memrefs, buffer, 
        "I:0xH0000=0_0xH0002=22_I:0xH0000=0_0xH0003=23_I:0xH0001=0_0xH0003=24");
    /*   $(0002 + $0000) == 22 && $(0003 + $0000) == 23 && $(0003 + $0001) == 24 */
    /*   $0003 (0x34)    == 22 && $0004 (0xAB)    == 23 && $0005 (0x56)    == 24 */

    assert_evaluate_condset(condset, memrefs, &memory, 0);
    assert_hit_count(condset, 1, 0);
    assert_hit_count(condset, 3, 0);
    assert_hit_count(condset, 5, 0);

    /* first condition is true */
    ram[3] = 22;
    assert_evaluate_condset(condset, memrefs, &memory, 0);
    assert_hit_count(condset, 1, 1);
    assert_hit_count(condset, 3, 0);
    assert_hit_count(condset, 5, 0);

    /* second condition is true */
    ram[4] = 23;
    assert_evaluate_condset(condset, memrefs, &memory, 0);
    assert_hit_count(condset, 1, 2);
    assert_hit_count(condset, 3, 1);
    assert_hit_count(condset, 5, 0);

    /* third condition is true */
    ram[5] = 24;
    assert_evaluate_condset(condset, memrefs, &memory, 1);
    assert_hit_count(condset, 1, 3);
    assert_hit_count(condset, 3, 2);
    assert_hit_count(condset, 5, 1);
}

static void test_addaddress_pointer_data_size_differs_from_pointer_size() {
  unsigned char ram[] = {0x01, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(0x0002 + word(0xh0000)) == 22 */
  assert_parse_condset(&condset, &memrefs, buffer, "I:0xH0000_0x 0002=22");

  /* 8-bit pointed-at value is correct */
  ram[3] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* 16-bit pointed-at value is correct */
  ram[4] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* point to new value */
  ram[0] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* new pointed-at value is only partially correct */
  ram[3] = 0;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* new pointed-at value is correct */
  ram[2] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
}

static void test_addaddress_double_indirection() {
  unsigned char ram[] = {0x01, 0x02, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* byte(0x0000 + byte(0x0000 + byte(0x0000))) == 22 | $($($0000))) == 22*/
  assert_parse_condset(&condset, &memrefs, buffer, "I:0xH0000=0_I:0xH0000=0_0xH0000=22");

  /* value is correct: $0000=1, $0001=2, $0002 = 22 */
  ram[2] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* second pointer in chain causes final pointer to point at address 3 */
  ram[1] = 3;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* new pointed-at value is correct */
  ram[3] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* first pointer points at address 2, which is 22, so out-of-bounds */
  ram[0] = 2;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* second pointer points at address 3, which is correct */
  ram[2] = 3;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* first pointer is out of range, so returns 0 for the second pointer, $0 contains the correct value */
  ram[0] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
}

static void test_addaddress_adjust_both_sides() {
  unsigned char ram[] = {0x02, 0x11, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* $($0) > delta $($0) */
  assert_parse_condset(&condset, &memrefs, buffer, "I:0xH0000=0_0xH0000>d0xH0000");

  /* initial delta will be 0, so 2 will be greater */
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* delta should be the same as current */
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* value increased */
  ram[2]++;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* value decreased */
  ram[2]--;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* this is a small hiccup in the AddAddress behavior. when the pointer changes, we 
   * can't reasonably know the previous value, so delta will be 0 for the first frame. 
   * 52 is greater than 0 (even though it didn't change), so set will be true. */
  ram[0] = 3;
  assert_evaluate_condset(condset, memrefs, &memory, 1);
}

static void test_addaddress_adjust_both_sides_different_bases() {
  unsigned char ram[] = {0x02, 0x11, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* $($0) == $($0 + 1) */
  assert_parse_condset(&condset, &memrefs, buffer, "I:0xH0000=0_0xH0000=0xH0001");
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* values are the same */
  ram[2] = ram[3];
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* adjust pointer */
  ram[0] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* values are the same */
  ram[1] = ram[2];
  assert_evaluate_condset(condset, memrefs, &memory, 1);
}

static void test_addaddress_scaled() {
  unsigned char ram[] = {0x01, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condset_t* condset;
  rc_memref_value_t* memrefs = NULL;
  char buffer[2048];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* $($0 * 2) */
  assert_parse_condset(&condset, &memrefs, buffer, "I:0xH0000*2_0xH0000=22");
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* value is correct */
  ram[2] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* adjust pointer */
  ram[0] = 2;
  assert_evaluate_condset(condset, memrefs, &memory, 0);

  /* new value is correct */
  ram[4] = 22;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* point to original value */
  ram[0] = 1;
  assert_evaluate_condset(condset, memrefs, &memory, 1);

  /* original value no longer correct */
  ram[2] = 11;
  assert_evaluate_condset(condset, memrefs, &memory, 0);
}

void test_condset(void) {
  TEST_SUITE_BEGIN();

  /* hit counts */
  TEST(test_hitcount_increment_when_true);
  TEST(test_hitcount_does_not_increment_when_false);
  TEST(test_hitcount_target);

  /* two conditions */
  TEST_PARAMS4(test_hitcount_two_conditions, "0xH0001=18_0xH0002=52", 1, 1, 1);
  TEST_PARAMS4(test_hitcount_two_conditions, "0xH0001=18_0xH0002!=52", 0, 1, 0);
  TEST_PARAMS4(test_hitcount_two_conditions, "0xH0001>18_0xH0002=52", 0, 0, 1);
  TEST_PARAMS4(test_hitcount_two_conditions, "0xH0001<18_0xH0002>52", 0, 0, 0);

  /* three conditions */
  TEST_PARAMS5(test_hitcount_three_conditions, "0xH0001=18_0xH0002=52_0xL0004=6", 1, 1, 1, 1);
  TEST_PARAMS5(test_hitcount_three_conditions, "0xH0001=18_0xH0002=52_0xL0004>6", 0, 1, 1, 0);
  TEST_PARAMS5(test_hitcount_three_conditions, "0xH0001=18_0xH0002<52_0xL0004=6", 0, 1, 0, 1);
  TEST_PARAMS5(test_hitcount_three_conditions, "0xH0001=18_0xH0002<52_0xL0004>6", 0, 1, 0, 0);
  TEST_PARAMS5(test_hitcount_three_conditions, "0xH0001>18_0xH0002=52_0xL0004=6", 0, 0, 1, 1);
  TEST_PARAMS5(test_hitcount_three_conditions, "0xH0001>18_0xH0002=52_0xL0004>6", 0, 0, 1, 0);
  TEST_PARAMS5(test_hitcount_three_conditions, "0xH0001>18_0xH0002<52_0xL0004=6", 0, 0, 0, 1);
  TEST_PARAMS5(test_hitcount_three_conditions, "0xH0001>18_0xH0002<52_0xL0004>6", 0, 0, 0, 0);

  /* pauseif */
  TEST(test_pauseif);
  TEST(test_pauseif_hitcount_one);
  TEST(test_pauseif_hitcount_two);
  TEST(test_pauseif_hitcount_with_reset);
  TEST(test_pauseif_does_not_increment_hits);
  TEST(test_pauseif_delta_updated);

  /* resetif */
  TEST(test_resetif);
  TEST(test_resetif_cond_with_hittarget);
  TEST(test_resetif_hitcount);
  TEST(test_resetif_hitcount_one);
  TEST(test_resetif_hitcount_addhits);

  TEST(test_pauseif_resetif_hitcounts);

  /* addsource/subsource */
  TEST(test_addsource);
  TEST(test_addsource_overflow);
  TEST(test_subsource);
  TEST(test_subsource_overflow);
  TEST(test_addsource_subsource);
  TEST(test_addsource_multiply);
  TEST(test_subsource_multiply);
  TEST(test_addsource_multiply_fraction);
  TEST(test_addsource_divide);
  TEST(test_subsource_divide);
  TEST(test_addsource_mask);
  TEST(test_subsource_mask);

  /* addhits */
  TEST(test_addhits);
  TEST(test_addhits_no_target);
  TEST(test_addhits_with_addsource);

  /* andnext */
  TEST(test_andnext);
  TEST(test_andnext_boundaries);
  TEST(test_andnext_resetif);
  TEST(test_andnext_pauseif);
  TEST(test_andnext_addsource);
  TEST(test_andnext_addhits);
  TEST(test_andnext_between_addhits);
  TEST(test_andnext_with_hits_chain);
  TEST(test_andnext_changes_to);

  /* ornext */
  TEST(test_ornext);
  TEST(test_andnext_ornext_interaction);

  /* addaddress */
  TEST(test_addaddress_direct_pointer);
  TEST(test_addaddress_indirect_pointer);
  TEST(test_addaddress_indirect_pointer_out_of_range);
  TEST(test_addaddress_indirect_pointer_multiple);
  TEST(test_addaddress_pointer_data_size_differs_from_pointer_size);
  TEST(test_addaddress_double_indirection);
  TEST(test_addaddress_adjust_both_sides);
  TEST(test_addaddress_adjust_both_sides_different_bases);
  TEST(test_addaddress_scaled);

  TEST_SUITE_END();
}
