#include "internal.h"

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

static void test_format_value(int format, int value, const char* expected) {
  char buffer[64];
  int result;

  result = rc_format_value(buffer, sizeof(buffer), value, format);
  ASSERT_STR_EQUALS(expected, buffer);
  ASSERT_NUM_EQUALS(result, strlen(expected));
}

static void test_parse_format(const char* format, int expected) {
  ASSERT_NUM_EQUALS(rc_parse_format(format), expected);
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
  TEST_PARAMS2(test_evaluate_value, "0xH01*0xH02", 0x12 * 0x34); /* multiply by second address */
  TEST_PARAMS2(test_evaluate_value, "0xH01*0xT02", 0); /* multiply by bit */
  TEST_PARAMS2(test_evaluate_value, "0xH01*~0xT02", 0x12); /* multiply by inverse bit */
  TEST_PARAMS2(test_evaluate_value, "0xH01*~0xH02", 0x12 * (0x34 ^ 0xff)); /* multiply by inverse byte */

  TEST_PARAMS2(test_evaluate_value, "B0xH01", 12);
  TEST_PARAMS2(test_evaluate_value, "B0x00001", 3412);
  TEST_PARAMS2(test_evaluate_value, "B0xH03", 111); /* 0xAB not really BCD */

  /* measured format - supports hit counts, AddSource, SubSource, and AddAddress */
  TEST_PARAMS2(test_evaluate_value, "A:0xH0001_M:0xH0002", 0x12 + 0x34);
  TEST_PARAMS2(test_evaluate_value, "I:0xH0000_M:0xH0002", 0x34);

  /* delta should initially be 0, so a hit will be tallied */
  TEST_PARAMS2(test_evaluate_value, "M:0xH0002!=0xd0xH0002", 1);

  /* overflow - 145406052 * 86 = 125049208332 -> 0x1D1D837E0C, leading 0x1D is truncated off */
  TEST_PARAMS2(test_evaluate_value, "0xX0001*0xH0004", 0x1D837E0C);


  /* rc_format_value */
  TEST_PARAMS3(test_format_value, RC_FORMAT_VALUE, 12345, "12345");
  TEST_PARAMS3(test_format_value, RC_FORMAT_VALUE, -12345, "-12345");
  TEST_PARAMS3(test_format_value, RC_FORMAT_VALUE, 0xFFFFFFFF, "-1");
  TEST_PARAMS3(test_format_value, RC_FORMAT_SCORE, 12345, "012345");
  TEST_PARAMS3(test_format_value, RC_FORMAT_SECONDS, 45, "0:45");
  TEST_PARAMS3(test_format_value, RC_FORMAT_SECONDS, 345, "5:45");
  TEST_PARAMS3(test_format_value, RC_FORMAT_SECONDS, 12345, "3h25:45");
  TEST_PARAMS3(test_format_value, RC_FORMAT_CENTISECS, 345, "0:03.45");
  TEST_PARAMS3(test_format_value, RC_FORMAT_CENTISECS, 12345, "2:03.45");
  TEST_PARAMS3(test_format_value, RC_FORMAT_CENTISECS, 1234567, "3h25:45.67");
  TEST_PARAMS3(test_format_value, RC_FORMAT_SECONDS_AS_MINUTES, 45, "0h00");
  TEST_PARAMS3(test_format_value, RC_FORMAT_SECONDS_AS_MINUTES, 345, "0h05");
  TEST_PARAMS3(test_format_value, RC_FORMAT_SECONDS_AS_MINUTES, 12345, "3h25");
  TEST_PARAMS3(test_format_value, RC_FORMAT_MINUTES, 45, "0h45");
  TEST_PARAMS3(test_format_value, RC_FORMAT_MINUTES, 345, "5h45");
  TEST_PARAMS3(test_format_value, RC_FORMAT_MINUTES, 12345, "205h45");
  TEST_PARAMS3(test_format_value, RC_FORMAT_FRAMES, 345, "0:05.75");
  TEST_PARAMS3(test_format_value, RC_FORMAT_FRAMES, 12345, "3:25.75");
  TEST_PARAMS3(test_format_value, RC_FORMAT_FRAMES, 1234567, "5h42:56.11");

  /* rc_parse_format */
  TEST_PARAMS2(test_parse_format, "VALUE", RC_FORMAT_VALUE);
  TEST_PARAMS2(test_parse_format, "SECS", RC_FORMAT_SECONDS);
  TEST_PARAMS2(test_parse_format, "TIMESECS", RC_FORMAT_SECONDS);
  TEST_PARAMS2(test_parse_format, "TIME", RC_FORMAT_FRAMES);
  TEST_PARAMS2(test_parse_format, "MINUTES", RC_FORMAT_MINUTES);
  TEST_PARAMS2(test_parse_format, "SECS_AS_MINS", RC_FORMAT_SECONDS_AS_MINUTES);
  TEST_PARAMS2(test_parse_format, "FRAMES", RC_FORMAT_FRAMES);
  TEST_PARAMS2(test_parse_format, "SCORE", RC_FORMAT_SCORE);
  TEST_PARAMS2(test_parse_format, "POINTS", RC_FORMAT_SCORE);
  TEST_PARAMS2(test_parse_format, "MILLISECS", RC_FORMAT_CENTISECS);
  TEST_PARAMS2(test_parse_format, "OTHER", RC_FORMAT_SCORE);
  TEST_PARAMS2(test_parse_format, "INVALID", RC_FORMAT_VALUE);

  TEST_SUITE_END();
}
