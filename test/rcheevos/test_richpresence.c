#include "internal.h"

#include "../test_framework.h"
#include "mock_memory.h"

static void assert_parse_richpresence(rc_richpresence_t** richpresence, void* buffer, const char* script) {
  int size;
  unsigned* overflow;

  size = rc_richpresence_size(script);
  ASSERT_NUM_GREATER(size, 0);

  overflow = (unsigned*)(((char*)buffer) + size);
  *overflow = 0xCDCDCDCD;

  *richpresence = rc_parse_richpresence(buffer, script, NULL, 0);
  ASSERT_PTR_NOT_NULL(*richpresence);

  if (*overflow != 0xCDCDCDCD) {
      ASSERT_FAIL("write past end of buffer");
  }
}

static void assert_richpresence_output(rc_richpresence_t* richpresence, memory_t* memory, const char* expected_display_string) {
  char output[256];
  int result;

  result = rc_evaluate_richpresence(richpresence, output, sizeof(output), peek, memory, NULL);
  ASSERT_STR_EQUALS(output, expected_display_string);
  ASSERT_NUM_EQUALS(result, strlen(expected_display_string));
}

static void test_empty_script() {
  int result = rc_richpresence_size("");
  ASSERT_NUM_EQUALS(result, RC_MISSING_DISPLAY_STRING);
}

static void test_simple_richpresence(const char* script, const char* expected_display_string) {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, script);
  assert_richpresence_output(richpresence, &memory, expected_display_string);
}

static void test_conditional_display_simple() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Display:\n?0xH0000=0?Zero\n?0xH0000=1?One\nOther");
  assert_richpresence_output(richpresence, &memory, "Zero");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "One");

  ram[0] = 2;
  assert_richpresence_output(richpresence, &memory, "Other");
}

static void test_conditional_display_after_default() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Display:\nOther\n?0xH0000=0?Zero\n?0xH0000=1?One");
  assert_richpresence_output(richpresence, &memory, "Other");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "Other");
}

static void test_conditional_display_no_default() {
  int result = rc_richpresence_size("Display:\n?0xH0000=0?Zero");
  ASSERT_NUM_EQUALS(result, RC_MISSING_DISPLAY_STRING);
}

static void test_conditional_display_common_condition() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* condition for Second is a sub-clause of First */
  assert_parse_richpresence(&richpresence, buffer, "Display:\n?0xH0000=0_0xH0001=18?First\n?0xH0000=0?Second\nThird");
  assert_richpresence_output(richpresence, &memory, "First");

  /* secondary part of first condition is false, will match second condition */
  ram[1] = 1;
  assert_richpresence_output(richpresence, &memory, "Second");

  /* common condition is false, will use default */
  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "Third");

  /* ================================================================ */
  /* == reverse the conditions so the First is a sub-clause of Second */
  assert_parse_richpresence(&richpresence, buffer, "Display:\n?0xH0000=0?First\n?0xH0000=0_0xH0001=18?Second\nThird");

  /* reset the memory so it matches the first test, First clause will be matched before even looking at Second */
  ram[0] = 0;
  ram[1] = 18;
  assert_richpresence_output(richpresence, &memory, "First");

  /* secondary part of second condition is false, will still match first condition */
  ram[1] = 1;
  assert_richpresence_output(richpresence, &memory, "First");

  /* common condition is false, will use default */
  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "Third");
}

static void test_conditional_display_duplicated_condition() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Display:\n?0xH0000=0?First\n?0xH0000=0?Second\nThird");
  assert_richpresence_output(richpresence, &memory, "First");

  /* cannot activate Second */

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "Third");
}

static void test_conditional_display_invalid_condition_logic() {
  int result = rc_richpresence_size("Display:\n?BANANA?Zero\nDefault");
  ASSERT_NUM_EQUALS(result, RC_INVALID_MEMORY_OPERAND);
}

static void test_conditional_display_whitespace_text() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Display:\n?0xH0000=0?  \n?0xH0000=1?One\nOther");
  assert_richpresence_output(richpresence, &memory, "  ");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "One");

  ram[0] = 2;
  assert_richpresence_output(richpresence, &memory, "Other");
}

static void test_macro_value() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(0x 0001) Points");
  assert_richpresence_output(richpresence, &memory, "13330 Points");

  ram[1] = 20;
  assert_richpresence_output(richpresence, &memory, "13332 Points");
}

static void test_macro_value_adjusted_negative() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(0x 0001_V-10000) Points");
  assert_richpresence_output(richpresence, &memory, "3330 Points");

  ram[2] = 7;
  assert_richpresence_output(richpresence, &memory, "-8190 Points");
}

static void test_macro_value_from_formula() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(0xH0001*100_0xH0002) Points");
  assert_richpresence_output(richpresence, &memory, "1852 Points");

  ram[1] = 32;
  assert_richpresence_output(richpresence, &memory, "3252 Points");
}

static void test_macro_value_from_hits() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Format:Hits\nFormatType=VALUE\n\nDisplay:\n@Hits(M:0xH01=1) Hits");
  assert_richpresence_output(richpresence, &memory, "0 Hits");

  ram[1] = 1;
  assert_richpresence_output(richpresence, &memory, "1 Hits");
  assert_richpresence_output(richpresence, &memory, "2 Hits");
  assert_richpresence_output(richpresence, &memory, "3 Hits");
}

static void test_macro_value_from_indirect() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Format:Value\nFormatType=VALUE\n\nDisplay:\nPointing at @Value(I:0xH00_M:0xH01)");
  assert_richpresence_output(richpresence, &memory, "Pointing at 18");

  /* pointed at data changes */
  ram[1] = 99;
  assert_richpresence_output(richpresence, &memory, "Pointing at 99");

  /* pointer changes */
  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "Pointing at 52");
}

static void test_macro_frames() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Format:Frames\nFormatType=FRAMES\n\nDisplay:\n@Frames(0x 0001)");
  assert_richpresence_output(richpresence, &memory, "3:42.16");

  ram[1] = 20;
  assert_richpresence_output(richpresence, &memory, "3:42.20");
}

static void test_macro_lookup_simple() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Lookup:Location\n0=Zero\n1=One\n\nDisplay:\nAt @Location(0xH0000)");
  assert_richpresence_output(richpresence, &memory, "At Zero");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "At One");

  /* no entry - default to empty string */
  ram[0] = 2;
  assert_richpresence_output(richpresence, &memory, "At ");
}

static void test_macro_lookup_hex_keys() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Lookup:Location\n0x00=Zero\n0x01=One\n\nDisplay:\nAt @Location(0xH0000)");
  assert_richpresence_output(richpresence, &memory, "At Zero");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "At One");

  /* no entry - default to empty string */
  ram[0] = 2;
  assert_richpresence_output(richpresence, &memory, "At ");
}

static void test_macro_lookup_default() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Lookup:Location\n0=Zero\n1=One\n*=Star\n\nDisplay:\nAt @Location(0xH0000)");
  assert_richpresence_output(richpresence, &memory, "At Zero");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "At One");

  /* no entry - default to empty string */
  ram[0] = 2;
  assert_richpresence_output(richpresence, &memory, "At Star");
}

static void test_macro_lookup_crlf() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Lookup:Location\r\n0=Zero\r\n1=One\r\n\r\nDisplay:\r\nAt @Location(0xH0000)");
  assert_richpresence_output(richpresence, &memory, "At Zero");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "At One");

  /* no entry - default to empty string */
  ram[0] = 2;
  assert_richpresence_output(richpresence, &memory, "At ");
}

static void test_macro_lookup_after_display() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Display:\nAt @Location(0xH0000)\n\nLookup:Location\n0=Zero\n1=One");
  assert_richpresence_output(richpresence, &memory, "At Zero");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "At One");

  /* no entry - default to empty string */
  ram[0] = 2;
  assert_richpresence_output(richpresence, &memory, "At ");
}

static void test_macro_lookup_from_formula() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Lookup:Location\n0=Zero\n1=One\n\nDisplay:\nAt @Location(0xH0000*0.5)");
  assert_richpresence_output(richpresence, &memory, "At Zero");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "At Zero");

  ram[0] = 2;
  assert_richpresence_output(richpresence, &memory, "At One");
}

static void test_macro_lookup_repeated() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* same lookup can be used for the same address */
  assert_parse_richpresence(&richpresence, buffer, "Lookup:Location\n0=Zero\n1=One\n\nDisplay:\nAt @Location(0xH0000), Near @Location(0xH0000)");
  assert_richpresence_output(richpresence, &memory, "At Zero, Near Zero");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "At One, Near One");

  /* no entry - default to empty string */
  ram[0] = 2;
  assert_richpresence_output(richpresence, &memory, "At , Near ");
}

static void test_macro_lookup_shared() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* same lookup can be used for multiple addresses */
  assert_parse_richpresence(&richpresence, buffer, "Lookup:Location\n0=Zero\n1=One\n\nDisplay:\nAt @Location(0xH0000), Near @Location(0xH0001)");
  assert_richpresence_output(richpresence, &memory, "At Zero, Near ");

  ram[1] = 1;
  assert_richpresence_output(richpresence, &memory, "At Zero, Near One");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "At One, Near One");
}

static void test_macro_lookup_multiple() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* multiple lookups can be used for same address */
  assert_parse_richpresence(&richpresence, buffer, "Lookup:Location\n0=Zero\n1=One\n\nLookup:Location2\n0=zero\n1=one\n\nDisplay:\nAt @Location(0xH0000), Near @Location2(0xH0000)");
  assert_richpresence_output(richpresence, &memory, "At Zero, Near zero");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "At One, Near one");

  ram[0] = 2;
  assert_richpresence_output(richpresence, &memory, "At , Near ");
}

static void test_macro_lookup_and_value() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Lookup:Location\n0=Zero\n1=One\n\nFormat:Location2\nFormatType=VALUE\n\nDisplay:\nAt @Location(0xH0000), Near @Location2(0xH0001)");
  assert_richpresence_output(richpresence, &memory, "At Zero, Near 18");

  ram[1] = 1;
  assert_richpresence_output(richpresence, &memory, "At Zero, Near 1");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "At One, Near 1");
}

static void test_macro_lookup_value_with_whitespace() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Lookup:Location\n0= Zero \n1= One \n\nDisplay:\nAt '@Location(0xH0000)' ");
  assert_richpresence_output(richpresence, &memory, "At ' Zero ' ");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "At ' One ' ");

  /* no entry - default to empty string */
  ram[0] = 2;
  assert_richpresence_output(richpresence, &memory, "At '' ");
}

static void test_macro_lookup_invalid() {
  int result;
    
  /* lookup value starts with Ox instead of 0x */
  result = rc_richpresence_size("Lookup:Location\nOx0=Zero\n1=One\n\nDisplay:\nAt @Location(0xH0000)");
  ASSERT_NUM_EQUALS(result, RC_INVALID_CONST_OPERAND);

  /* lookup value contains invalid hex character */
  result = rc_richpresence_size("Lookup:Location\n0xO=Zero\n1=One\n\nDisplay:\nAt @Location(0xH0000)");
  ASSERT_NUM_EQUALS(result, RC_INVALID_CONST_OPERAND);

  /* lookup value is not numeric */
  result = rc_richpresence_size("Lookup:Location\nZero=Zero\n1=One\n\nDisplay:\nAt @Location(0xH0000)");
  ASSERT_NUM_EQUALS(result, RC_INVALID_CONST_OPERAND);
}

static void test_macro_escaped() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* ensures @ can be used in the display string by escaping it */
  assert_parse_richpresence(&richpresence, buffer, "Format:Points\nFormatType=VALUE\n\nDisplay:\n\\@Points(0x 0001) \\@@Points(0x 0001) Points");
  assert_richpresence_output(richpresence, &memory, "@Points(0x 0001) @13330 Points");
}

static void test_macro_undefined() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Display:\n@Points(0x 0001) Points");
  assert_richpresence_output(richpresence, &memory, "[Unknown macro]Points(0x 0001) Points");
}

static void test_macro_undefined_at_end_of_line() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  /* adding [Unknown macro] to the output effectively makes the script larger than it started.
   * since we don't detect unknown macros in `rc_richpresence_size`, this was causing a 
   * write-past-end-of-buffer memory corruption error. this test recreated that error. */
  assert_parse_richpresence(&richpresence, buffer, "Display:\n@Points(0x 0001)");
  assert_richpresence_output(richpresence, &memory, "[Unknown macro]Points(0x 0001)");
}

static void test_macro_without_parameter() {
  int result;

  result = rc_richpresence_size("Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points Points");
  ASSERT_NUM_EQUALS(result, RC_MISSING_VALUE);

  result = rc_richpresence_size("Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points() Points");
  ASSERT_NUM_EQUALS(result, RC_INVALID_MEMORY_OPERAND);
}

static void test_macro_without_parameter_conditional_display() {
  int result;
      
  result = rc_richpresence_size("Format:Points\nFormatType=VALUE\n\nDisplay:\n?0x0h0001=1?@Points Points\nDefault");
  ASSERT_NUM_EQUALS(result, RC_MISSING_VALUE);

  result = rc_richpresence_size("Format:Points\nFormatType=VALUE\n\nDisplay:\n?0x0h0001=1?@Points() Points\nDefault");
  ASSERT_NUM_EQUALS(result, RC_INVALID_MEMORY_OPERAND);
}

static void test_macro_non_numeric_parameter() {
  int result = rc_richpresence_size("Format:Points\nFormatType=VALUE\n\nDisplay:\n@Points(Zero) Points");
  ASSERT_NUM_EQUALS(result, RC_INVALID_MEMORY_OPERAND);
}

static void test_random_text_between_sections() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "Locations are fun!\nLookup:Location\n0=Zero\n1=One\n\nDisplay goes here\nDisplay:\nAt @Location(0xH0000)\n\nWritten by User3");
  assert_richpresence_output(richpresence, &memory, "At Zero");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "At One");

  /* no entry - default to empty string */
  ram[0] = 2;
  assert_richpresence_output(richpresence, &memory, "At ");
}

static void test_comments() {
  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  rc_richpresence_t* richpresence;
  char buffer[1024];

  memory.ram = ram;
  memory.size = sizeof(ram);

  assert_parse_richpresence(&richpresence, buffer, "// Locations are fun!\nLookup:Location // lookup\n0=Zero // 0\n1=One // 1\n\n//Display goes here\nDisplay: // display\nAt @Location(0xH0000) // text\n\n//Written by User3");
  assert_richpresence_output(richpresence, &memory, "At Zero");

  ram[0] = 1;
  assert_richpresence_output(richpresence, &memory, "At One");

  /* no entry - default to empty string */
  ram[0] = 2;
  assert_richpresence_output(richpresence, &memory, "At ");
}


void test_richpresence(void) {
  TEST_SUITE_BEGIN();

  TEST(test_empty_script);

  /* static display string */
  TEST_PARAMS2(test_simple_richpresence, "Display:\nHello, world!", "Hello, world!");

  /* static display string with trailing whitespace */
  TEST_PARAMS2(test_simple_richpresence, "Display:\nWhat ", "What ");

  /* static display string whitespace only*/
  TEST_PARAMS2(test_simple_richpresence, "Display:\n    ", "    ");

  /* static display string with comment (trailing whitespace will be trimmed) */
  TEST_PARAMS2(test_simple_richpresence, "Display:\nWhat // Where", "What");

  /* static display string with escaped comment */
  TEST_PARAMS2(test_simple_richpresence, "Display:\nWhat \\// Where", "What // Where");

  /* static display string with escaped backslash */
  TEST_PARAMS2(test_simple_richpresence, "Display:\nWhat \\\\ Where", "What \\ Where");

  /* static display string with partially escaped comment */
  TEST_PARAMS2(test_simple_richpresence, "Display:\nWhat \\/// Where", "What /");

  /* static display string with trailing backslash (backslash will be ignored) */
  TEST_PARAMS2(test_simple_richpresence, "Display:\nWhat \\", "What ");

  /* static display string with trailing text */
  TEST_PARAMS2(test_simple_richpresence, "Display:\nWhat\n\nWhere", "What");

  /* condition display */
  TEST(test_conditional_display_simple);
  TEST(test_conditional_display_after_default);
  TEST(test_conditional_display_no_default);
  TEST(test_conditional_display_common_condition);
  TEST(test_conditional_display_duplicated_condition);
  TEST(test_conditional_display_invalid_condition_logic);
  TEST(test_conditional_display_whitespace_text);

  /* value macros */
  TEST(test_macro_value);
  TEST(test_macro_value_adjusted_negative);
  TEST(test_macro_value_from_formula);
  TEST(test_macro_value_from_hits);
  TEST(test_macro_value_from_indirect);

  /* frames macros */
  TEST(test_macro_frames);

  /* lookup macros */
  TEST(test_macro_lookup_simple);
  TEST(test_macro_lookup_hex_keys);
  TEST(test_macro_lookup_default);
  TEST(test_macro_lookup_crlf);
  TEST(test_macro_lookup_after_display);
  TEST(test_macro_lookup_from_formula);
  TEST(test_macro_lookup_repeated);
  TEST(test_macro_lookup_shared);
  TEST(test_macro_lookup_multiple);
  TEST(test_macro_lookup_and_value);
  TEST(test_macro_lookup_value_with_whitespace);
  TEST(test_macro_lookup_invalid);

  /* escaped macro */
  TEST(test_macro_escaped);

  /* macro errors */
  TEST(test_macro_undefined);
  TEST(test_macro_undefined_at_end_of_line);
  TEST(test_macro_without_parameter);
  TEST(test_macro_without_parameter_conditional_display);
  TEST(test_macro_non_numeric_parameter);

  /* comments */
  TEST(test_random_text_between_sections); /* before official comments extra text was ignored, so was occassionally used to comment */
  TEST(test_comments);

  TEST_SUITE_END();
}
