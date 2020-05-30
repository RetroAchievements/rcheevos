#include "internal.h"

#include "../test_framework.h"
#include "mock_memory.h"

static void _assert_operand(rc_operand_t* self, char expected_type, char expected_size, unsigned expected_address) {
  ASSERT_NUM_EQUALS(self->type, expected_type);

  switch (expected_type) {
    case RC_OPERAND_ADDRESS:
    case RC_OPERAND_DELTA:
    case RC_OPERAND_PRIOR:
      ASSERT_NUM_EQUALS(self->size, expected_size);
      ASSERT_NUM_EQUALS(self->value.memref->memref.address, expected_address);
      break;

    case RC_OPERAND_CONST:
      ASSERT_NUM_EQUALS(self->value.num, expected_address);
      break;
  }
}
#define assert_operand(operand, expected_type, expected_size, expected_address) ASSERT_HELPER(_assert_operand(operand, expected_type, expected_size, expected_address), "assert_operand")

static void _assert_parse_condition(
    const char* memaddr, char expected_type,
    char expected_left_type, char expected_left_size, unsigned expected_left_value,
    char expected_operator,
    char expected_right_type, char expected_right_size, unsigned expected_right_value,
    unsigned expected_required_hits
) {
    rc_condition_t* self;
    rc_parse_state_t parse;
    rc_memref_value_t* memrefs;
    char buffer[512];

    rc_init_parse_state(&parse, buffer, 0, 0);
    rc_init_parse_state_memrefs(&parse, &memrefs);
    self = rc_parse_condition(&memaddr, &parse, 0);
    rc_destroy_parse_state(&parse);

    ASSERT_NUM_EQUALS(self->type, expected_type);
    assert_operand(&self->operand1, expected_left_type, expected_left_size, expected_left_value);
    ASSERT_NUM_EQUALS(self->oper, expected_operator);
    assert_operand(&self->operand2, expected_right_type, expected_right_size, expected_right_value);
    ASSERT_NUM_EQUALS(self->required_hits, expected_required_hits);
}
#define assert_parse_condition(memaddr, expected_type, expected_left_type, expected_left_size, expected_left_value, \
                               expected_operator, expected_right_type, expected_right_size, expected_right_value, expected_required_hits) \
    ASSERT_HELPER(_assert_parse_condition(memaddr, expected_type, expected_left_type, expected_left_size, expected_left_value, \
                                          expected_operator, expected_right_type, expected_right_size, expected_right_value, expected_required_hits), "assert_parse_condition")

static void test_parse_condition(const char* memaddr, int expected_type, int expected_left_type,
    int expected_operator, int expected_required_hits) {
  if (expected_operator == RC_OPERATOR_NONE) {
    assert_parse_condition(memaddr, expected_type,
      expected_left_type, RC_MEMSIZE_8_BITS, 0x1234U,
      expected_operator,
      RC_INVALID_CONST_OPERAND, RC_MEMSIZE_8_BITS, 0U,
      expected_required_hits
    );
  }
  else {
    assert_parse_condition(memaddr, expected_type,
      expected_left_type, RC_MEMSIZE_8_BITS, 0x1234U,
      expected_operator,
      RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 8U,
      expected_required_hits
    );
  }
}

static void test_parse_operands(const char* memaddr,
    int expected_left_type, int expected_left_size, unsigned expected_left_value,
    int expected_right_type, int expected_right_size, unsigned expected_right_value) {
  assert_parse_condition(memaddr, RC_CONDITION_STANDARD,
    expected_left_type, expected_left_size, expected_left_value,
    RC_OPERATOR_EQ,
    expected_right_type, expected_right_size, expected_right_value,
    0
  );
}

static void test_parse_modifier(const char* memaddr, int expected_operator, int expected_operand, double expected_multiplier) {
  assert_parse_condition(memaddr, RC_CONDITION_ADD_SOURCE,
    RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U,
    expected_operator,
    expected_operand, RC_MEMSIZE_8_BITS, (int)expected_multiplier,
    0
  );
}

static void test_parse_modifier_shorthand(const char* memaddr, int expected_type) {
  assert_parse_condition(memaddr, expected_type,
    RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U,
    RC_OPERATOR_NONE,
    RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 1U,
    0
  );
}

static void test_parse_condition_error(const char* memaddr, int expected_error) {
  if (expected_error == RC_OK) {
    ASSERT_NUM_GREATER(rc_trigger_size(memaddr), 0);
  } else {
    ASSERT_NUM_EQUALS(rc_trigger_size(memaddr), expected_error);
  }
}

static int evaluate_condition(rc_condition_t* cond, memory_t* memory, rc_memref_value_t* memrefs) {
  rc_eval_state_t eval_state;

  memset(&eval_state, 0, sizeof(eval_state));
  eval_state.peek = peek;
  eval_state.peek_userdata = memory;

  rc_update_memref_values(memrefs, peek, memory);
  return rc_test_condition(cond, &eval_state);
}

static void test_evaluate_condition(const char* memaddr, int expected_result) {
  rc_condition_t* self;
  rc_parse_state_t parse;
  char buffer[512];
  rc_memref_value_t* memrefs;
  int ret;
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_init_parse_state(&parse, buffer, 0, 0);
  rc_init_parse_state_memrefs(&parse, &memrefs);
  self = rc_parse_condition(&memaddr, &parse, 0);
  rc_destroy_parse_state(&parse);

  ASSERT_NUM_GREATER(parse.offset, 0);
  ASSERT_NUM_EQUALS(*memaddr, 0);

  ret = evaluate_condition(self, &memory, memrefs);

  if (expected_result) {
    ASSERT_NUM_EQUALS(ret, 1);
  } else {
    ASSERT_NUM_EQUALS(ret, 0);
  }
}

static void test_condition_compare_delta() {
  unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
  memory_t memory;
  rc_condition_t* cond;
  rc_parse_state_t parse;
  char buffer[512];
  rc_memref_value_t* memrefs;

  const char* cond_str = "0xH0001>d0xH0001";
  rc_init_parse_state(&parse, buffer, 0, 0);
  rc_init_parse_state_memrefs(&parse, &memrefs);
  cond = rc_parse_condition(&cond_str, &parse, 0);
  rc_destroy_parse_state(&parse);

  ASSERT_NUM_GREATER(parse.offset, 0);
  ASSERT_NUM_EQUALS(*cond_str, 0);
  memory.ram = ram;
  memory.size = sizeof(ram);

  /* initial delta value is 0, 0x12 > 0 */
  ASSERT_NUM_EQUALS(evaluate_condition(cond, &memory, memrefs), 1);

  /* delta value is now 0x12, 0x12 = 0x12 */
  ASSERT_NUM_EQUALS(evaluate_condition(cond, &memory, memrefs), 0);

  /* delta value is now 0x12, 0x11 < 0x12 */
  ram[1] = 0x11;
  ASSERT_NUM_EQUALS(evaluate_condition(cond, &memory, memrefs), 0);

  /* delta value is now 0x13, 0x12 > 0x11 */
  ram[1] = 0x12;
  ASSERT_NUM_EQUALS(evaluate_condition(cond, &memory, memrefs), 1);
}

void test_condition(void) {
  TEST_SUITE_BEGIN();

  /* different comparison operators */
  TEST_PARAMS5(test_parse_condition, "0xH1234=8", RC_CONDITION_STANDARD, RC_OPERAND_ADDRESS, RC_OPERATOR_EQ, 0);
  TEST_PARAMS5(test_parse_condition, "0xH1234==8", RC_CONDITION_STANDARD, RC_OPERAND_ADDRESS, RC_OPERATOR_EQ, 0);
  TEST_PARAMS5(test_parse_condition, "0xH1234!=8", RC_CONDITION_STANDARD, RC_OPERAND_ADDRESS, RC_OPERATOR_NE, 0);
  TEST_PARAMS5(test_parse_condition, "0xH1234<8", RC_CONDITION_STANDARD, RC_OPERAND_ADDRESS, RC_OPERATOR_LT, 0);
  TEST_PARAMS5(test_parse_condition, "0xH1234<=8", RC_CONDITION_STANDARD, RC_OPERAND_ADDRESS, RC_OPERATOR_LE, 0);
  TEST_PARAMS5(test_parse_condition, "0xH1234>8", RC_CONDITION_STANDARD, RC_OPERAND_ADDRESS, RC_OPERATOR_GT, 0);
  TEST_PARAMS5(test_parse_condition, "0xH1234>=8", RC_CONDITION_STANDARD, RC_OPERAND_ADDRESS, RC_OPERATOR_GE, 0);
  TEST_PARAMS5(test_parse_condition, "0xH1234<8", RC_CONDITION_STANDARD, RC_OPERAND_ADDRESS, RC_OPERATOR_LT, 0);

  /* special accessors */
  TEST_PARAMS5(test_parse_condition, "d0xH1234=8", RC_CONDITION_STANDARD, RC_OPERAND_DELTA, RC_OPERATOR_EQ, 0);
  TEST_PARAMS5(test_parse_condition, "p0xH1234=8", RC_CONDITION_STANDARD, RC_OPERAND_PRIOR, RC_OPERATOR_EQ, 0);

  /* flags */
  TEST_PARAMS5(test_parse_condition, "R:0xH1234=8", RC_CONDITION_RESET_IF, RC_OPERAND_ADDRESS, RC_OPERATOR_EQ, 0);
  TEST_PARAMS5(test_parse_condition, "P:0xH1234=8", RC_CONDITION_PAUSE_IF, RC_OPERAND_ADDRESS, RC_OPERATOR_EQ, 0);
  TEST_PARAMS5(test_parse_condition, "A:0xH1234=8", RC_CONDITION_ADD_SOURCE, RC_OPERAND_ADDRESS, RC_OPERATOR_NONE, 0);
  TEST_PARAMS5(test_parse_condition, "B:0xH1234=8", RC_CONDITION_SUB_SOURCE, RC_OPERAND_ADDRESS, RC_OPERATOR_NONE, 0);
  TEST_PARAMS5(test_parse_condition, "C:0xH1234=8", RC_CONDITION_ADD_HITS, RC_OPERAND_ADDRESS, RC_OPERATOR_EQ, 0);
  TEST_PARAMS5(test_parse_condition, "M:0xH1234=8", RC_CONDITION_MEASURED, RC_OPERAND_ADDRESS, RC_OPERATOR_EQ, 0);
  TEST_PARAMS5(test_parse_condition, "Q:0xH1234=8", RC_CONDITION_MEASURED_IF, RC_OPERAND_ADDRESS, RC_OPERATOR_EQ, 0);
  TEST_PARAMS5(test_parse_condition, "I:0xH1234=8", RC_CONDITION_ADD_ADDRESS, RC_OPERAND_ADDRESS, RC_OPERATOR_NONE, 0);
  TEST_PARAMS5(test_parse_condition, "T:0xH1234=8", RC_CONDITION_TRIGGER, RC_OPERAND_ADDRESS, RC_OPERATOR_EQ, 0);

  /* modifiers (only valid with some flags, use A:) */
  TEST_PARAMS5(test_parse_condition, "A:0xH1234*8", RC_CONDITION_ADD_SOURCE, RC_OPERAND_ADDRESS, RC_OPERATOR_MULT, 0);
  TEST_PARAMS5(test_parse_condition, "A:0xH1234/8", RC_CONDITION_ADD_SOURCE, RC_OPERAND_ADDRESS, RC_OPERATOR_DIV, 0);
  TEST_PARAMS5(test_parse_condition, "A:0xH1234&8", RC_CONDITION_ADD_SOURCE, RC_OPERAND_ADDRESS, RC_OPERATOR_AND, 0);

  TEST_PARAMS4(test_parse_modifier, "A:0xH1234", RC_OPERATOR_NONE, RC_OPERAND_CONST, 1);
  TEST_PARAMS4(test_parse_modifier, "A:0xH1234*1", RC_OPERATOR_MULT, RC_OPERAND_CONST, 1);
  TEST_PARAMS4(test_parse_modifier, "A:0xH1234*3", RC_OPERATOR_MULT, RC_OPERAND_CONST, 3);
  TEST_PARAMS4(test_parse_modifier, "A:0xH1234*f0.5", RC_OPERATOR_MULT, RC_OPERAND_FP, 0.5);
  TEST_PARAMS4(test_parse_modifier, "A:0xH1234*f.5", RC_OPERATOR_MULT, RC_OPERAND_FP, 0.5);
  TEST_PARAMS4(test_parse_modifier, "A:0xH1234*-1", RC_OPERATOR_MULT, RC_OPERAND_CONST, -1);
  TEST_PARAMS4(test_parse_modifier, "A:0xH1234*0xH3456", RC_OPERATOR_MULT, RC_OPERAND_ADDRESS, 0x3456);

  /* hit counts */
  TEST_PARAMS5(test_parse_condition, "0xH1234=8(1)", RC_CONDITION_STANDARD, RC_OPERAND_ADDRESS, RC_OPERATOR_EQ, 1);
  TEST_PARAMS5(test_parse_condition, "0xH1234=8.1.", RC_CONDITION_STANDARD, RC_OPERAND_ADDRESS, RC_OPERATOR_EQ, 1); /* legacy format */
  TEST_PARAMS5(test_parse_condition, "0xH1234=8(1000)", RC_CONDITION_STANDARD, RC_OPERAND_ADDRESS, RC_OPERATOR_EQ, 1000);

  /* hex value is interpreted as a 16-bit memory reference */
  TEST_PARAMS7(test_parse_operands, "0xH1234=0x80", RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U, RC_OPERAND_ADDRESS, RC_MEMSIZE_16_BITS, 0x80U);

  TEST_PARAMS7(test_parse_operands, "0xL1234=0xU3456", RC_OPERAND_ADDRESS, RC_MEMSIZE_LOW, 0x1234U, RC_OPERAND_ADDRESS, RC_MEMSIZE_HIGH, 0x3456U);

  /* shorthard for modifier conditions */
  TEST_PARAMS2(test_parse_modifier_shorthand, "A:0xH1234", RC_CONDITION_ADD_SOURCE);
  TEST_PARAMS2(test_parse_modifier_shorthand, "B:0xH1234", RC_CONDITION_SUB_SOURCE);
  TEST_PARAMS2(test_parse_modifier_shorthand, "C:0xH1234", RC_CONDITION_ADD_HITS);
  TEST_PARAMS2(test_parse_modifier_shorthand, "N:0xH1234", RC_CONDITION_AND_NEXT);
  TEST_PARAMS2(test_parse_modifier_shorthand, "O:0xH1234", RC_CONDITION_OR_NEXT);
  TEST_PARAMS2(test_parse_modifier_shorthand, "I:0xH1234", RC_CONDITION_ADD_ADDRESS);

  /* parse errors */
  TEST_PARAMS2(test_parse_condition_error, "0xH1234==0", RC_OK);
  TEST_PARAMS2(test_parse_condition_error, "H0x1234==0", RC_INVALID_CONST_OPERAND);
  TEST_PARAMS2(test_parse_condition_error, "0x1234", RC_INVALID_OPERATOR);
  TEST_PARAMS2(test_parse_condition_error, "P:0x1234", RC_INVALID_OPERATOR);
  TEST_PARAMS2(test_parse_condition_error, "R:0x1234", RC_INVALID_OPERATOR);
  TEST_PARAMS2(test_parse_condition_error, "M:0x1234", RC_INVALID_OPERATOR);
  TEST_PARAMS2(test_parse_condition_error, "Z:0x1234", RC_INVALID_CONDITION_TYPE);
  TEST_PARAMS2(test_parse_condition_error, "0x1234=1.2", RC_INVALID_REQUIRED_HITS);
  TEST_PARAMS2(test_parse_condition_error, "0.1234==0", RC_INVALID_OPERATOR); /* period is assumed to be operator */
  TEST_PARAMS2(test_parse_condition_error, "0==0.1234", RC_INVALID_REQUIRED_HITS); /* period is assumed to be start of hit target, no end marker */
  TEST_PARAMS2(test_parse_condition_error, "F0.1234==0", RC_INVALID_COMPARISON); /* floating value only valid on modifiers */
  TEST_PARAMS2(test_parse_condition_error, "0==f0.1234", RC_INVALID_COMPARISON); /* floating value only valid on modifiers */
  TEST_PARAMS2(test_parse_condition_error, "A:F0.1234*2", RC_INVALID_FP_OPERAND); /* floating value only valid on right side of modifiers */
  TEST_PARAMS2(test_parse_condition_error, "A:2*f0.1234", RC_OK); /* floating value only valid on right side of modifiers */

  /* simple evaluations (ram[1] = 18, ram[2] = 52) */
  TEST_PARAMS2(test_evaluate_condition, "0xH0001=18", 1);
  TEST_PARAMS2(test_evaluate_condition, "0xH0001!=18", 0);
  TEST_PARAMS2(test_evaluate_condition, "0xH0001<=18", 1);
  TEST_PARAMS2(test_evaluate_condition, "0xH0001>=18", 1);
  TEST_PARAMS2(test_evaluate_condition, "0xH0001<18", 0);
  TEST_PARAMS2(test_evaluate_condition, "0xH0001>18", 0);
  TEST_PARAMS2(test_evaluate_condition, "0xH0001>0", 1);
  TEST_PARAMS2(test_evaluate_condition, "0xH0001!=0", 1);

  TEST_PARAMS2(test_evaluate_condition, "0xH0001<0xH0002", 1);
  TEST_PARAMS2(test_evaluate_condition, "0xH0001>0xH0002", 0);
  TEST_PARAMS2(test_evaluate_condition, "0xH0001=0xH0001", 1);
  TEST_PARAMS2(test_evaluate_condition, "0xH0001!=0xH0002", 1);

  TEST(test_condition_compare_delta);

  TEST_SUITE_END();
}
