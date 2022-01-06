#include "rc_validate.h"

#include "rc_compat.h"

#include "../test_framework.h"

#include <stdlib.h>
#include <string.h>

int validate_trigger(const char* trigger, char result[], const size_t result_size, unsigned max_address) {
  char* buffer;
  rc_trigger_t* compiled;
  int success = 0;

  int ret = rc_trigger_size(trigger);
  if (ret < 0) {
    snprintf(result, result_size, "%s", rc_error_str(ret));
    return 0;
  }

  buffer = (char*)malloc(ret + 4);
  memset(buffer + ret, 0xCD, 4);
  compiled = rc_parse_trigger(buffer, trigger, NULL, 0);
  if (compiled == NULL) {
    snprintf(result, result_size, "parse failed");
  }
  else if (*(unsigned*)&buffer[ret] != 0xCDCDCDCD) {
    snprintf(result, result_size, "write past end of buffer");
  }
  else if (rc_validate_trigger(compiled, result, result_size, max_address)) {
    success = 1;
  }

  free(buffer);
  return success;
}

static void test_validate_trigger_max_address(const char* trigger, const char* expected_error, unsigned max_address) {
  char buffer[512];
  int valid = validate_trigger(trigger, buffer, sizeof(buffer), max_address);

  if (*expected_error) {
    ASSERT_STR_EQUALS(buffer, expected_error);
    ASSERT_NUM_EQUALS(valid, 0);
  }
  else {
    ASSERT_STR_EQUALS(buffer, "");
    ASSERT_NUM_EQUALS(valid, 1);
  }
}

static void test_validate_trigger(const char* trigger, const char* expected_error) {
  test_validate_trigger_max_address(trigger, expected_error, 0xFFFFFFFF);
}

static void test_validate_trigger_64k(const char* trigger, const char* expected_error) {
  test_validate_trigger_max_address(trigger, expected_error, 0xFFFF);
}

static void test_validate_trigger_128k(const char* trigger, const char* expected_error) {
  test_validate_trigger_max_address(trigger, expected_error, 0x1FFFF);
}

static void test_combining_conditions_at_end_of_definition() {
  TEST_PARAMS2(test_validate_trigger, "0xH1234=1_A:0xH2345=2", "Final condition type expects another condition to follow");
  TEST_PARAMS2(test_validate_trigger, "0xH1234=1_B:0xH2345=2", "Final condition type expects another condition to follow");
  TEST_PARAMS2(test_validate_trigger, "0xH1234=1_C:0xH2345=2", "Final condition type expects another condition to follow");
  TEST_PARAMS2(test_validate_trigger, "0xH1234=1_D:0xH2345=2", "Final condition type expects another condition to follow");
  TEST_PARAMS2(test_validate_trigger, "0xH1234=1_N:0xH2345=2", "Final condition type expects another condition to follow");
  TEST_PARAMS2(test_validate_trigger, "0xH1234=1_O:0xH2345=2", "Final condition type expects another condition to follow");
  TEST_PARAMS2(test_validate_trigger, "0xH1234=1_Z:0xH2345=2", "Final condition type expects another condition to follow");

  TEST_PARAMS2(test_validate_trigger, "0xH1234=1_A:0xH2345=2S0x3456=1", "Core Final condition type expects another condition to follow");
  TEST_PARAMS2(test_validate_trigger, "0x3456=1S0xH1234=1_A:0xH2345=2", "Alt1 Final condition type expects another condition to follow");

  /* combining conditions not at end of definition */
  TEST_PARAMS2(test_validate_trigger, "A:0xH1234=1_0xH2345=2", "");
  TEST_PARAMS2(test_validate_trigger, "B:0xH1234=1_0xH2345=2", "");
  TEST_PARAMS2(test_validate_trigger, "N:0xH1234=1_0xH2345=2", "");
  TEST_PARAMS2(test_validate_trigger, "O:0xH1234=1_0xH2345=2", "");
  TEST_PARAMS2(test_validate_trigger, "Z:0xH1234=1_0xH2345=2", "");
}

static void test_addhits_chain_without_target() {
  TEST_PARAMS2(test_validate_trigger, "C:0xH1234=1_0xH2345=2", "Condition 2: Final condition in AddHits chain must have a hit target");
  TEST_PARAMS2(test_validate_trigger, "D:0xH1234=1_0xH2345=2", "Condition 2: Final condition in AddHits chain must have a hit target");
  TEST_PARAMS2(test_validate_trigger, "C:0xH1234=1_0xH2345=2.1.", "");
  TEST_PARAMS2(test_validate_trigger, "D:0xH1234=1_0xH2345=2.1.", "");
}

static void test_range_comparisons() {
  TEST_PARAMS2(test_validate_trigger, "0xH1234>1", "");

  TEST_PARAMS2(test_validate_trigger, "0xH1234=255", "");
  TEST_PARAMS2(test_validate_trigger, "0xH1234!=255", "");
  TEST_PARAMS2(test_validate_trigger, "0xH1234>255", "Condition 1: Comparison is never true");
  TEST_PARAMS2(test_validate_trigger, "0xH1234>=255", "");
  TEST_PARAMS2(test_validate_trigger, "0xH1234<255", "");
  TEST_PARAMS2(test_validate_trigger, "0xH1234<=255", "Condition 1: Comparison is always true");

  TEST_PARAMS2(test_validate_trigger, "R:0xH1234<255", "");
  TEST_PARAMS2(test_validate_trigger, "R:0xH1234<=255", "Condition 1: Comparison is always true");

  TEST_PARAMS2(test_validate_trigger, "0xH1234=256", "Condition 1: Comparison is never true");
  TEST_PARAMS2(test_validate_trigger, "0xH1234!=256", "Condition 1: Comparison is always true");
  TEST_PARAMS2(test_validate_trigger, "0xH1234>256", "Condition 1: Comparison is never true");
  TEST_PARAMS2(test_validate_trigger, "0xH1234>=256", "Condition 1: Comparison is never true");
  TEST_PARAMS2(test_validate_trigger, "0xH1234<256", "Condition 1: Comparison is always true");
  TEST_PARAMS2(test_validate_trigger, "0xH1234<=256", "Condition 1: Comparison is always true");

  TEST_PARAMS2(test_validate_trigger, "0x 1234>=65535", "");
  TEST_PARAMS2(test_validate_trigger, "0x 1234>=65536", "Condition 1: Comparison is never true");

  TEST_PARAMS2(test_validate_trigger, "0xW1234>=16777215", "");
  TEST_PARAMS2(test_validate_trigger, "0xW1234>=16777216", "Condition 1: Comparison is never true");

  TEST_PARAMS2(test_validate_trigger, "0xX1234>=4294967295", "");
  TEST_PARAMS2(test_validate_trigger, "0xX1234>4294967295", "Condition 1: Comparison is never true");

  TEST_PARAMS2(test_validate_trigger, "0xT1234>=1", "");
  TEST_PARAMS2(test_validate_trigger, "0xT1234>1", "Condition 1: Comparison is never true");

  /* max for AddSource is the sum of all parts (255+255=510) */
  TEST_PARAMS2(test_validate_trigger, "A:0xH1234_0<255", "");
  TEST_PARAMS2(test_validate_trigger, "A:0xH1234_0<=255", "Condition 2: Comparison is always true");
  TEST_PARAMS2(test_validate_trigger, "A:0xH1234_0xH1235<510", "");
  TEST_PARAMS2(test_validate_trigger, "A:0xH1234_0xH1235<=510", "Condition 2: Comparison is always true");
  TEST_PARAMS2(test_validate_trigger, "A:0xH1234*10_0xH1235>=2805", "");
  TEST_PARAMS2(test_validate_trigger, "A:0xH1234*10_0xH1235>2805", "Condition 2: Comparison is never true");
  TEST_PARAMS2(test_validate_trigger, "A:0xH1234/10_0xH1235>=280", "");
  TEST_PARAMS2(test_validate_trigger, "A:0xH1234/10_0xH1235>280", "Condition 2: Comparison is never true");
  TEST_PARAMS2(test_validate_trigger, "A:0xH1234&10_0xH1235>=265", "");
  TEST_PARAMS2(test_validate_trigger, "A:0xH1234&10_0xH1235>265", "Condition 2: Comparison is never true");

  /* max for SubSource is always 0xFFFFFFFF */
  TEST_PARAMS2(test_validate_trigger, "B:0xH1234_0xH1235<510", "");
  TEST_PARAMS2(test_validate_trigger, "B:0xH1234_0xH1235<=510", "");
}

void test_size_comparisons() {
  TEST_PARAMS2(test_validate_trigger, "0xH1234>0xH1235", "");
  TEST_PARAMS2(test_validate_trigger, "0xH1234>0x 1235", "Condition 1: Comparing different memory sizes");

  /* AddSource chain may compare different sizes without warning as the chain changes the
   * size of the final result. */
  TEST_PARAMS2(test_validate_trigger, "A:0xH1234_0xH1235=0xH2345", "");
  TEST_PARAMS2(test_validate_trigger, "A:0xH1234_0xH1235=0x 2345", "");
}

void test_address_range() {
  /* basic checks for each side */
  TEST_PARAMS2(test_validate_trigger_64k, "0xH1234>0xH1235", "");
  TEST_PARAMS2(test_validate_trigger_64k, "0xH12345>0xH1235", "Condition 1: Address 12345 out of range (max FFFF)");
  TEST_PARAMS2(test_validate_trigger_64k, "0xH1234>0xH12345", "Condition 1: Address 12345 out of range (max FFFF)");
  TEST_PARAMS2(test_validate_trigger_64k, "0xH12345>0xH12345", "Condition 1: Address 12345 out of range (max FFFF)");
  TEST_PARAMS2(test_validate_trigger_64k, "0xX1234>h12345", "");

  /* support for multiple memory blocks and edge addresses */
  TEST_PARAMS2(test_validate_trigger_128k, "0xH1234>0xH1235", "");
  TEST_PARAMS2(test_validate_trigger_128k, "0xH12345>0xH1235", "");
  TEST_PARAMS2(test_validate_trigger_128k, "0xH0000>5", "");
  TEST_PARAMS2(test_validate_trigger_128k, "0xH1FFFF>5", "");
  TEST_PARAMS2(test_validate_trigger_128k, "0xH20000>5", "Condition 1: Address 20000 out of range (max 1FFFF)");

  /* AddAddress can use really big values for negative offsets, don't flag them. */
  TEST_PARAMS2(test_validate_trigger_128k, "I:0xX1234_0xHFFFFFF00>5", "");
  TEST_PARAMS2(test_validate_trigger_128k, "I:0xX1234_0xH1234>5_0xHFFFFFF00>5", "Condition 3: Address FFFFFF00 out of range (max 1FFFF)");
}

void test_delta_pointers() {
  TEST_PARAMS2(test_validate_trigger, "I:0xX1234_0xH0000=1", "");
  TEST_PARAMS2(test_validate_trigger, "I:d0xX1234_0xH0000=1", "Condition 1: Using pointer from previous frame");
  TEST_PARAMS2(test_validate_trigger, "I:p0xX1234_0xH0000=1", "Condition 1: Using pointer from previous frame");
  TEST_PARAMS2(test_validate_trigger, "I:0xX1234_d0xH0000=1", "");
  TEST_PARAMS2(test_validate_trigger, "I:d0xX1234_I:d0xH0010_0xH0000=1", "Condition 1: Using pointer from previous frame");
  TEST_PARAMS2(test_validate_trigger, "I:d0xX1234_I:0xH0010_0xH0000=1", "Condition 1: Using pointer from previous frame");
  TEST_PARAMS2(test_validate_trigger, "I:0xX1234_I:d0xH0010_0xH0000=1", "Condition 2: Using pointer from previous frame");
  TEST_PARAMS2(test_validate_trigger, "I:0xX1234_I:0xH0010_0xH0000=1", "");
}

void test_float_comparisons() {
  TEST_PARAMS2(test_validate_trigger, "fF1234=f2.3", "");
  TEST_PARAMS2(test_validate_trigger, "fM1234=f2.3", "");
  TEST_PARAMS2(test_validate_trigger, "fF1234=2", "");
  TEST_PARAMS2(test_validate_trigger, "0xX1234=2", "");
  TEST_PARAMS2(test_validate_trigger, "0xX1234=f2.3", "Condition 1: Comparison is never true"); /* non integral comparison */
  TEST_PARAMS2(test_validate_trigger, "0xX1234!=f2.3", "Condition 1: Comparison is never true"); /* non integral comparison */
  TEST_PARAMS2(test_validate_trigger, "0xX1234<f2.3", "Condition 1: Comparison is never true"); /* non integral comparison */
  TEST_PARAMS2(test_validate_trigger, "0xX1234<=f2.3", "Condition 1: Comparison is never true"); /* non integral comparison */
  TEST_PARAMS2(test_validate_trigger, "0xX1234>f2.3", "Condition 1: Comparison is never true"); /* non integral comparison */
  TEST_PARAMS2(test_validate_trigger, "0xX1234>=f2.3", "Condition 1: Comparison is never true"); /* non integral comparison */
  TEST_PARAMS2(test_validate_trigger, "0xX1234=f2.0", ""); /* float can be converted to int without loss of data*/
  TEST_PARAMS2(test_validate_trigger, "0xH1234=f2.3", "Condition 1: Comparison is never true");
  TEST_PARAMS2(test_validate_trigger, "0xH1234=f300.0", "Condition 1: Comparison is never true"); /* value out of range */
  TEST_PARAMS2(test_validate_trigger, "f2.3=fF1234", "");
  TEST_PARAMS2(test_validate_trigger, "f2.3=0xX1234", "Condition 1: Comparison is never true"); /* non integral comparison */
  TEST_PARAMS2(test_validate_trigger, "f2.0=0xX1234", "");
  TEST_PARAMS2(test_validate_trigger, "A:Ff2345_fF1234=f2.3", "");
  TEST_PARAMS2(test_validate_trigger, "A:0xX2345_fF1234=f2.3", "");
  TEST_PARAMS2(test_validate_trigger, "A:Ff2345_0x1234=f2.3", "");
}

void test_rc_validate(void) {
  TEST_SUITE_BEGIN();

  /* positive baseline test cases */
  TEST_PARAMS2(test_validate_trigger, "", "");
  TEST_PARAMS2(test_validate_trigger, "0xH1234=1_0xH2345=2S0xH3456=1S0xH3456=2", "");

  test_combining_conditions_at_end_of_definition();
  test_addhits_chain_without_target();
  test_range_comparisons();
  test_size_comparisons();
  test_address_range();
  test_delta_pointers();
  test_float_comparisons();

  TEST_SUITE_END();
}
