#include "rc_internal.h"

#include "../test_framework.h"
#include "mock_memory.h"

static void test_evaluate_value(const char* memaddr, int expected_value) {
  rc_value_t* self;
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  char buffer[2048];
  unsigned* overflow;
  int ret;

  memory.ram = ram;
  memory.size = sizeof(ram);

  ret = rc_value_size(memaddr);
  ASSERT_NUM_GREATER_EQUALS(ret, 0);

  overflow = (unsigned*)(((char*)buffer) + ret);
  *overflow = 0xCDCDCDCD;

  self = rc_parse_value(buffer, memaddr, NULL, 0);
  ASSERT_PTR_NOT_NULL(self);
  if (*overflow != 0xCDCDCDCD) {
    ASSERT_FAIL("write past end of buffer");
  }

  ret = rc_evaluate_value(self, peek, &memory, NULL);
  ASSERT_NUM_EQUALS(ret, expected_value);
}

static void test_invalid_value(const char* memaddr, int expected_error) {
  int ret = rc_value_size(memaddr);
  ASSERT_NUM_EQUALS(ret, expected_error);
}

static void test_measured_value_target(const char* memaddr, int expected_target) {
  rc_value_t* self;
  char buffer[2048];
  unsigned* overflow;
  int ret;

  ret = rc_value_size(memaddr);
  ASSERT_NUM_GREATER_EQUALS(ret, 0);

  overflow = (unsigned*)(((char*)buffer) + ret);
  *overflow = 0xCDCDCDCD;

  self = rc_parse_value(buffer, memaddr, NULL, 0);
  ASSERT_PTR_NOT_NULL(self);
  if (*overflow != 0xCDCDCDCD) {
    ASSERT_FAIL("write past end of buffer");
  }

  ASSERT_NUM_EQUALS(self->conditions->conditions->required_hits, expected_target);
}

static void test_evaluate_measured_value_with_pause() {
  rc_value_t* self;
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  char buffer[2048];
  const char* memaddr = "P:0xH0003=hAB_M:0xH0002!=d0xH0002";
  int ret;

  memory.ram = ram;
  memory.size = sizeof(ram);

  ret = rc_value_size(memaddr);
  ASSERT_NUM_GREATER_EQUALS(ret, 0);

  self = rc_parse_value(buffer, memaddr, NULL, 0);
  ASSERT_PTR_NOT_NULL(self);

  /* should initially be paused, no hits captured */
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);

  /* pause should prevent hitcount */
  ram[2]++;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);

  /* unpause should not report the change that occurred while paused */
  ram[3] = 0;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);

  /* hitcount should be captured */
  ram[2]++;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 1);

  /* pause should return current hitcount */
  ram[3] = 0xAB;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 1);

  /* pause should prevent hitcount */
  ram[2]++;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 1);

  /* unpause should not report the change that occurred while paused */
  ram[3] = 0;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 1);

  /* additional hitcount should be captured */
  ram[2]++;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 2);
}

static void test_evaluate_measured_value_with_reset() {
  rc_value_t* self;
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  char buffer[2048];
  const char* memaddr = "R:0xH0003=hAB_M:0xH0002!=d0xH0002";
  int ret;

  memory.ram = ram;
  memory.size = sizeof(ram);

  ret = rc_value_size(memaddr);
  ASSERT_NUM_GREATER_EQUALS(ret, 0);

  self = rc_parse_value(buffer, memaddr, NULL, 0);
  ASSERT_PTR_NOT_NULL(self);

  /* reset should initially be true, no hits captured */
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);

  /* reset should prevent hitcount */
  ram[2]++;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);

  /* reset no longer true, change while reset shouldn't be captured */
  ram[3] = 0;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);

  /* additional hitcount should be captured */
  ram[2]++;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 1);

  /* reset should clear hit count */
  ram[3] = 0xAB;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);

  /* reset should prevent hitcount */
  ram[2]++;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);

  /* reset no longer true, change while reset shouldn't be captured */
  ram[3] = 0;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 0);

  /* additional hitcount should be captured */
  ram[2]++;
  ASSERT_NUM_EQUALS(rc_evaluate_value(self, peek, &memory, NULL), 1);
}

void test_value(void) {
  TEST_SUITE_BEGIN();

  /* ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56}; */

  /* classic format - supports multipliers, max, inversion */
  TEST_PARAMS2(test_evaluate_value, "V6", 6);
  TEST_PARAMS2(test_evaluate_value, "V6*2", 12);
  TEST_PARAMS2(test_evaluate_value, "V6*0.5", 3);
  TEST_PARAMS2(test_evaluate_value, "V-6", -6);
  TEST_PARAMS2(test_evaluate_value, "V-6*2", -12);

  TEST_PARAMS2(test_evaluate_value, "0xH0001_0xH0002", 0x12 + 0x34);
  TEST_PARAMS2(test_evaluate_value, "0xH0001*100_0xH0002*0.5_0xL0003", 0x12 * 100 + 0x34 / 2 + 0x0B);
  TEST_PARAMS2(test_evaluate_value, "0xH0001$0xH0002", 0x34);
  TEST_PARAMS2(test_evaluate_value, "0xH0001_0xH0004*3$0xH0002*0xL0003", 0x34 * 0x0B);
  TEST_PARAMS2(test_evaluate_value, "0xH001_V-20", 0x12 - 20);
  TEST_PARAMS2(test_evaluate_value, "0xH0001_H10", 0x12 + 0x10);
  TEST_PARAMS2(test_evaluate_value, "0xh0000*-1_99_0xh0001*-100_5900", 4199);

  TEST_PARAMS2(test_evaluate_value, "0xH01*4", 0x12 * 4); /* multiply by constant */
  TEST_PARAMS2(test_evaluate_value, "0xH01*0.5", 0x12 / 2); /* multiply by fraction */
  TEST_PARAMS2(test_evaluate_value, "0xH01/2", 0x12 / 2); /* divide by constant */
  TEST_PARAMS2(test_evaluate_value, "0xH01*0xH02", 0x12 * 0x34); /* multiply by second address */
  TEST_PARAMS2(test_evaluate_value, "0xH01*0xT02", 0); /* multiply by bit */
  TEST_PARAMS2(test_evaluate_value, "0xH01*~0xT02", 0x12); /* multiply by inverse bit */
  TEST_PARAMS2(test_evaluate_value, "0xH01*~0xH02", 0x12 * (0x34 ^ 0xff)); /* multiply by inverse byte */

  TEST_PARAMS2(test_evaluate_value, "B0xH01", 12);
  TEST_PARAMS2(test_evaluate_value, "B0x00001", 3412);
  TEST_PARAMS2(test_evaluate_value, "B0xH03", 111); /* 0xAB not really BCD */

  /* non-comparison measured values just return the value at the address and have no target */
  TEST_PARAMS2(test_measured_value_target, "M:0xH0002", 0);

  /* hitcount based measured values always have unbounded targets, even if one is specified */
  TEST_PARAMS2(test_measured_value_target, "M:0xH0002!=d0xH0002", (unsigned)-1);
  TEST_PARAMS2(test_measured_value_target, "M:0xH0002!=d0xH0002.99.", (unsigned)-1);
  /* measured values always assumed to be hitcount based - they do not stop/trigger when the condition is met */
  TEST_PARAMS2(test_measured_value_target, "M:0xH0002<100", (unsigned)-1);

  /* measured format - supports hit counts and combining flags
   * (AddSource, SubSource, AddHits, SubHits, AndNext, OrNext, and AddAddress) */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0001_M:0xH0002", 0x12 + 0x34);
  TEST_PARAMS2(test_evaluate_value, "B:0xH0001_M:0xH0002", 0x34 - 0x12);
  TEST_PARAMS2(test_evaluate_value, "C:0xH0000=0_M:0xH0002=52", 2);
  TEST_PARAMS2(test_evaluate_value, "C:0xH0000=0_D:0xH0001=18_M:0xH0002=52", 1);
  TEST_PARAMS2(test_evaluate_value, "N:0xH0000=0_M:0xH0002=52", 1);
  TEST_PARAMS2(test_evaluate_value, "O:0xH0000=0_M:0xH0002=0", 1);
  TEST_PARAMS2(test_evaluate_value, "I:0xH0000_M:0xH0002", 0x34);

  /* measured format does not support alt groups */
  TEST_PARAMS2(test_invalid_value, "M:0xH0002=6SM:0xH0003=6", RC_INVALID_VALUE_FLAG);
  /* does not start with X:, so legacy parser says it's an invalid memory accessor */
  TEST_PARAMS2(test_invalid_value, "SM:0xH0002=6SM:0xH0003=6", RC_INVALID_MEMORY_OPERAND);

  /* measured format does not support trigger flag */
  TEST_PARAMS2(test_invalid_value, "T:0xH0002=6", RC_INVALID_VALUE_FLAG);

  /* measured format requires a measured condition */
  TEST_PARAMS2(test_invalid_value, "A:0xH0002_0xH0003>10.99.", RC_INVALID_VALUE_FLAG); /* no flag on condition 2 */
  TEST_PARAMS2(test_invalid_value, "A:0xH0002_A:0xH0003", RC_MISSING_VALUE_MEASURED);

  /* delta should initially be 0, so a hit will be tallied */
  TEST_PARAMS2(test_evaluate_value, "M:0xH0002!=d0xH0002", 1);

  /* pause and reset affect hit count */
  TEST(test_evaluate_measured_value_with_pause);
  TEST(test_evaluate_measured_value_with_reset);

  /* overflow - 145406052 * 86 = 125049208332 -> 0x1D1D837E0C, leading 0x1D is truncated off */
  TEST_PARAMS2(test_evaluate_value, "0xX0001*0xH0004", 0x1D837E0C);

  TEST_SUITE_END();
}
