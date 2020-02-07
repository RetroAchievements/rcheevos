#include "internal.h"
#include "rurl.h"

#include "smw_snes.h"
#include "galaga_nes.h"

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

static void parse_operand(rc_operand_t* self, const char** memaddr) {
  rc_parse_state_t parse;
  char buffer[256];
  rc_memref_value_t* memrefs;
  int ret;

  rc_init_parse_state(&parse, buffer, 0, 0);   
  rc_init_parse_state_memrefs(&parse, &memrefs);
  ret = rc_parse_operand(self, memaddr, 1, 0, &parse);
  rc_destroy_parse_state(&parse);

  assert(ret >= 0);
  assert(**memaddr == 0);
}

static void comp_operand(rc_operand_t* self, char expected_type, char expected_size, unsigned expected_address) {
  assert(expected_type == self->type);
  switch (expected_type) {
    case RC_OPERAND_ADDRESS:
    case RC_OPERAND_DELTA:
    case RC_OPERAND_PRIOR:
      assert(expected_size == self->size);
      assert(expected_address == self->value.memref->memref.address);
      break;

    case RC_OPERAND_CONST:
      assert(expected_address == self->value.num);
      break;
  }
}

static void parse_comp_operand(const char* memaddr, char expected_type, char expected_size, unsigned expected_value) {
  rc_operand_t self;
  parse_operand(&self, &memaddr);
  comp_operand(&self, expected_type, expected_size, expected_value);
}

static void parse_error_operand(const char* memaddr, int valid_chars) {
  rc_operand_t self;
  rc_parse_state_t parse;
  int ret;
  const char* begin = memaddr;
  rc_memref_value_t* memrefs;

  rc_init_parse_state(&parse, 0, 0, 0);
  rc_init_parse_state_memrefs(&parse, &memrefs);
  ret = rc_parse_operand(&self, &memaddr, 1, 0, &parse);
  rc_destroy_parse_state(&parse);

  assert(ret < 0);
  assert(memaddr - begin == valid_chars);
}

static unsigned evaluate_operand(rc_operand_t* op, memory_t* memory, rc_memref_value_t* memrefs)
{
  rc_eval_state_t eval_state;

  memset(&eval_state, 0, sizeof(eval_state));
  eval_state.peek = peek;
  eval_state.peek_userdata = memory;

  rc_update_memref_values(memrefs, peek, memory);
  return rc_evaluate_operand(op, &eval_state);
}

static void parse_comp_operand_value(const char* memaddr, memory_t* memory, unsigned expected_value) {
  rc_operand_t self;
  rc_parse_state_t parse;
  rc_memref_value_t* memrefs;
  char buffer[512];
  unsigned value;

  rc_init_parse_state(&parse, buffer, 0, 0);
  rc_init_parse_state_memrefs(&parse, &memrefs);
  rc_parse_operand(&self, &memaddr, 1, 0, &parse);
  rc_destroy_parse_state(&parse);

  value = evaluate_operand(&self, memory, memrefs);
  assert(value == expected_value);
}

static void test_memref(void) {
  char buffer[512];
  {
    /*------------------------------------------------------------------------
    TestAllocMemrefValueDuplicatesSizing
    ------------------------------------------------------------------------*/
    rc_parse_state_t parse;
    rc_init_parse_state(&parse, buffer, 0, 0);

    rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_8_BITS, 0);
    assert(parse.scratch.memref_count == 1);

    rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_16_BITS, 0); /* differing size will not match */
    assert(parse.scratch.memref_count == 2);

    rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_LOW, 0); /* differing size will not match */
    assert(parse.scratch.memref_count == 3);

    rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_BIT_2, 0); /* differing size will not match */
    assert(parse.scratch.memref_count == 4);

    rc_alloc_memref_value(&parse, 2, RC_MEMSIZE_8_BITS, 0); /* differing address will not match */
    assert(parse.scratch.memref_count == 5);

    rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_8_BITS, 0); /* match */
    assert(parse.scratch.memref_count == 5);

    rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_16_BITS, 0); /* match */
    assert(parse.scratch.memref_count == 5);

    rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_BIT_2, 0); /* match */
    assert(parse.scratch.memref_count == 5);

    rc_alloc_memref_value(&parse, 2, RC_MEMSIZE_8_BITS, 0); /* match */
    assert(parse.scratch.memref_count == 5);

    rc_destroy_parse_state(&parse);
  }

  {
    /*------------------------------------------------------------------------
    TestAllocMemrefValueGrowthSizing
    ------------------------------------------------------------------------*/
    int i;
    rc_parse_state_t parse;
    rc_init_parse_state(&parse, buffer, 0, 0);

    for (i = 0; i < 100; i++) {
      rc_alloc_memref_value(&parse, i, RC_MEMSIZE_8_BITS, 0);
    }
    assert(parse.scratch.memref_count == 100);

    rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_8_BITS, 0);
    assert(parse.scratch.memref_count == 100);

    rc_alloc_memref_value(&parse, 25, RC_MEMSIZE_8_BITS, 0);
    assert(parse.scratch.memref_count == 100);

    rc_alloc_memref_value(&parse, 50, RC_MEMSIZE_8_BITS, 0);
    assert(parse.scratch.memref_count == 100);

    rc_alloc_memref_value(&parse, 75, RC_MEMSIZE_8_BITS, 0);
    assert(parse.scratch.memref_count == 100);

    rc_alloc_memref_value(&parse, 99, RC_MEMSIZE_8_BITS, 0);
    assert(parse.scratch.memref_count == 100);

    rc_destroy_parse_state(&parse);
  }

  {
    /*------------------------------------------------------------------------
    TestAllocMemrefValueDuplicates
    ------------------------------------------------------------------------*/
    rc_parse_state_t parse;
    rc_memref_value_t* memrefs;
    rc_memref_value_t* memref1;
    rc_memref_value_t* memref2;
    rc_memref_value_t* memref3;
    rc_memref_value_t* memref4;
    rc_memref_value_t* memref5;
    rc_memref_value_t* memrefX;
    rc_init_parse_state(&parse, buffer, 0, 0);
    rc_init_parse_state_memrefs(&parse, &memrefs);

    memref1 = rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_8_BITS, 0);
    assert(memref1->memref.address == 1);
    assert(memref1->memref.size == RC_MEMSIZE_8_BITS);
    assert(memref1->memref.is_indirect == 0);
    assert(memref1->value == 0);
    assert(memref1->previous == 0);
    assert(memref1->prior == 0);
    assert(memref1->next == 0);

    memref2 = rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_16_BITS, 0); /* differing size will not match */
    memref3 = rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_LOW, 0); /* differing size will not match */
    memref4 = rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_BIT_2, 0); /* differing size will not match */
    memref5 = rc_alloc_memref_value(&parse, 2, RC_MEMSIZE_8_BITS, 0); /* differing address will not match */

    memrefX = rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_8_BITS, 0); /* match */
    assert(memrefX == memref1);

    memrefX = rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_16_BITS, 0); /* match */
    assert(memrefX == memref2);

    memrefX = rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_LOW, 0); /* match */
    assert(memrefX == memref3);

    memrefX = rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_BIT_2, 0); /* match */
    assert(memrefX == memref4);

    memrefX = rc_alloc_memref_value(&parse, 2, RC_MEMSIZE_8_BITS, 0); /* match */
    assert(memrefX == memref5);

    rc_destroy_parse_state(&parse);
  }

  {
    /*------------------------------------------------------------------------
    TestUpdateMemrefValues
    ------------------------------------------------------------------------*/
    rc_parse_state_t parse;
    rc_memref_value_t* memrefs;
    rc_memref_value_t* memref1;
    rc_memref_value_t* memref2;

    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    memory.ram = ram;
    memory.size = sizeof(ram);

    rc_init_parse_state(&parse, buffer, 0, 0);
    rc_init_parse_state_memrefs(&parse, &memrefs);

    memref1 = rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_8_BITS, 0);
    memref2 = rc_alloc_memref_value(&parse, 2, RC_MEMSIZE_8_BITS, 0);

    rc_update_memref_values(memrefs, peek, &memory);

    assert(memref1->value == 0x12);
    assert(memref1->previous == 0);
    assert(memref1->prior == 0);
    assert(memref2->value == 0x34);
    assert(memref2->previous == 0);
    assert(memref2->prior == 0);

    ram[1] = 3;
    rc_update_memref_values(memrefs, peek, &memory);

    assert(memref1->value == 3);
    assert(memref1->previous == 0x12);
    assert(memref1->prior == 0x12);
    assert(memref2->value == 0x34);
    assert(memref2->previous == 0x34);
    assert(memref2->prior == 0);

    ram[1] = 5;
    rc_update_memref_values(memrefs, peek, &memory);

    assert(memref1->value == 5);
    assert(memref1->previous == 3);
    assert(memref1->prior == 3);
    assert(memref2->value == 0x34);
    assert(memref2->previous == 0x34);
    assert(memref2->prior == 0);

    ram[2] = 7;
    rc_update_memref_values(memrefs, peek, &memory);

    assert(memref1->value == 5);
    assert(memref1->previous == 5);
    assert(memref1->prior == 3);
    assert(memref2->value == 7);
    assert(memref2->previous == 0x34);
    assert(memref2->prior == 0x34);

    rc_destroy_parse_state(&parse);
  }
}

static void test_operand(void) {
  {
    /*------------------------------------------------------------------------
    TestParseVariableAddress
    ------------------------------------------------------------------------*/

    /* sizes */
    parse_comp_operand("0xH1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U);
    parse_comp_operand("0x 1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_16_BITS, 0x1234U);
    parse_comp_operand("0x1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_16_BITS, 0x1234U);
    parse_comp_operand("0xW1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_24_BITS, 0x1234U);
    parse_comp_operand("0xX1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_32_BITS, 0x1234U);
    parse_comp_operand("0xL1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_LOW, 0x1234U);
    parse_comp_operand("0xU1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_HIGH, 0x1234U);
    parse_comp_operand("0xM1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BIT_0, 0x1234U);
    parse_comp_operand("0xN1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BIT_1, 0x1234U);
    parse_comp_operand("0xO1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BIT_2, 0x1234U);
    parse_comp_operand("0xP1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BIT_3, 0x1234U);
    parse_comp_operand("0xQ1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BIT_4, 0x1234U);
    parse_comp_operand("0xR1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BIT_5, 0x1234U);
    parse_comp_operand("0xS1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BIT_6, 0x1234U);
    parse_comp_operand("0xT1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BIT_7, 0x1234U);

    /* sizes (ignore case) */
    parse_comp_operand("0Xh1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U);
    parse_comp_operand("0xx1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_32_BITS, 0x1234U);
    parse_comp_operand("0xl1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_LOW, 0x1234U);
    parse_comp_operand("0xu1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_HIGH, 0x1234U);
    parse_comp_operand("0xm1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BIT_0, 0x1234U);
    parse_comp_operand("0xn1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BIT_1, 0x1234U);
    parse_comp_operand("0xo1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BIT_2, 0x1234U);
    parse_comp_operand("0xp1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BIT_3, 0x1234U);
    parse_comp_operand("0xq1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BIT_4, 0x1234U);
    parse_comp_operand("0xr1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BIT_5, 0x1234U);
    parse_comp_operand("0xs1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BIT_6, 0x1234U);
    parse_comp_operand("0xt1234", RC_OPERAND_ADDRESS, RC_MEMSIZE_BIT_7, 0x1234U);

    /* addresses */
    parse_comp_operand("0xH0000", RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x0000U);
    parse_comp_operand("0xH12345678", RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x12345678U);
    parse_comp_operand("0xHABCD", RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0xABCDU);
    parse_comp_operand("0xhabcd", RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0xABCDU);
  }

  {
    /*------------------------------------------------------------------------
    TestParseVariableDeltaMem
    ------------------------------------------------------------------------*/

    /* sizes */
    parse_comp_operand("d0xH1234", RC_OPERAND_DELTA, RC_MEMSIZE_8_BITS, 0x1234U);
    parse_comp_operand("d0x 1234", RC_OPERAND_DELTA, RC_MEMSIZE_16_BITS, 0x1234U);
    parse_comp_operand("d0x1234", RC_OPERAND_DELTA, RC_MEMSIZE_16_BITS, 0x1234U);
    parse_comp_operand("d0xW1234", RC_OPERAND_DELTA, RC_MEMSIZE_24_BITS, 0x1234U);
    parse_comp_operand("d0xX1234", RC_OPERAND_DELTA, RC_MEMSIZE_32_BITS, 0x1234U);
    parse_comp_operand("d0xL1234", RC_OPERAND_DELTA, RC_MEMSIZE_LOW, 0x1234U);
    parse_comp_operand("d0xU1234", RC_OPERAND_DELTA, RC_MEMSIZE_HIGH, 0x1234U);
    parse_comp_operand("d0xM1234", RC_OPERAND_DELTA, RC_MEMSIZE_BIT_0, 0x1234U);
    parse_comp_operand("d0xN1234", RC_OPERAND_DELTA, RC_MEMSIZE_BIT_1, 0x1234U);
    parse_comp_operand("d0xO1234", RC_OPERAND_DELTA, RC_MEMSIZE_BIT_2, 0x1234U);
    parse_comp_operand("d0xP1234", RC_OPERAND_DELTA, RC_MEMSIZE_BIT_3, 0x1234U);
    parse_comp_operand("d0xQ1234", RC_OPERAND_DELTA, RC_MEMSIZE_BIT_4, 0x1234U);
    parse_comp_operand("d0xR1234", RC_OPERAND_DELTA, RC_MEMSIZE_BIT_5, 0x1234U);
    parse_comp_operand("d0xS1234", RC_OPERAND_DELTA, RC_MEMSIZE_BIT_6, 0x1234U);
    parse_comp_operand("d0xT1234", RC_OPERAND_DELTA, RC_MEMSIZE_BIT_7, 0x1234U);

    /* ignores case */
    parse_comp_operand("D0Xh1234", RC_OPERAND_DELTA, RC_MEMSIZE_8_BITS, 0x1234U);

    /* addresses */
    parse_comp_operand("d0xH0000", RC_OPERAND_DELTA, RC_MEMSIZE_8_BITS, 0x0000U);
    parse_comp_operand("d0xH12345678", RC_OPERAND_DELTA, RC_MEMSIZE_8_BITS, 0x12345678U);
    parse_comp_operand("d0xHABCD", RC_OPERAND_DELTA, RC_MEMSIZE_8_BITS, 0xABCDU);
    parse_comp_operand("d0xhabcd", RC_OPERAND_DELTA, RC_MEMSIZE_8_BITS, 0xABCDU);
  }

  {
    /*------------------------------------------------------------------------
    TestParseVariablePriorMem
    ------------------------------------------------------------------------*/

    /* sizes */
    parse_comp_operand("p0xH1234", RC_OPERAND_PRIOR, RC_MEMSIZE_8_BITS, 0x1234U);
    parse_comp_operand("p0x 1234", RC_OPERAND_PRIOR, RC_MEMSIZE_16_BITS, 0x1234U);
    parse_comp_operand("p0x1234", RC_OPERAND_PRIOR, RC_MEMSIZE_16_BITS, 0x1234U);
    parse_comp_operand("p0xW1234", RC_OPERAND_PRIOR, RC_MEMSIZE_24_BITS, 0x1234U);
    parse_comp_operand("p0xX1234", RC_OPERAND_PRIOR, RC_MEMSIZE_32_BITS, 0x1234U);
    parse_comp_operand("p0xL1234", RC_OPERAND_PRIOR, RC_MEMSIZE_LOW, 0x1234U);
    parse_comp_operand("p0xU1234", RC_OPERAND_PRIOR, RC_MEMSIZE_HIGH, 0x1234U);
    parse_comp_operand("p0xM1234", RC_OPERAND_PRIOR, RC_MEMSIZE_BIT_0, 0x1234U);
    parse_comp_operand("p0xN1234", RC_OPERAND_PRIOR, RC_MEMSIZE_BIT_1, 0x1234U);
    parse_comp_operand("p0xO1234", RC_OPERAND_PRIOR, RC_MEMSIZE_BIT_2, 0x1234U);
    parse_comp_operand("p0xP1234", RC_OPERAND_PRIOR, RC_MEMSIZE_BIT_3, 0x1234U);
    parse_comp_operand("p0xQ1234", RC_OPERAND_PRIOR, RC_MEMSIZE_BIT_4, 0x1234U);
    parse_comp_operand("p0xR1234", RC_OPERAND_PRIOR, RC_MEMSIZE_BIT_5, 0x1234U);
    parse_comp_operand("p0xS1234", RC_OPERAND_PRIOR, RC_MEMSIZE_BIT_6, 0x1234U);
    parse_comp_operand("p0xT1234", RC_OPERAND_PRIOR, RC_MEMSIZE_BIT_7, 0x1234U);

    /* ignores case */
    parse_comp_operand("P0Xh1234", RC_OPERAND_PRIOR, RC_MEMSIZE_8_BITS, 0x1234U);

    /* addresses */
    parse_comp_operand("p0xH0000", RC_OPERAND_PRIOR, RC_MEMSIZE_8_BITS, 0x0000U);
    parse_comp_operand("p0xH12345678", RC_OPERAND_PRIOR, RC_MEMSIZE_8_BITS, 0x12345678U);
    parse_comp_operand("p0xHABCD", RC_OPERAND_PRIOR, RC_MEMSIZE_8_BITS, 0xABCDU);
    parse_comp_operand("p0xhabcd", RC_OPERAND_PRIOR, RC_MEMSIZE_8_BITS, 0xABCDU);
  }

  {
    /*------------------------------------------------------------------------
    TestParseVariableValue
    ------------------------------------------------------------------------*/

    /* decimal - values don't actually have size, default is RC_MEMSIZE_8_BITS */
    parse_comp_operand("123", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 123U);
    parse_comp_operand("123456", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 123456U);
    parse_comp_operand("0", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 0U);
    parse_comp_operand("0000000000", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 0U);
    parse_comp_operand("4294967295", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 4294967295U);

    /* hex - 'H' prefix, not '0x'! */
    parse_comp_operand("H123", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 0x123U);
    parse_comp_operand("HABCD", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 0xABCDU);
    parse_comp_operand("h123", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 0x123U);
    parse_comp_operand("habcd", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 0xABCDU);
    parse_comp_operand("HFFFFFFFF", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 4294967295U);

    /* '0x' is an address */
    parse_comp_operand("0x123", RC_OPERAND_ADDRESS, RC_MEMSIZE_16_BITS, 0x123U);

    /* hex without prefix */
    parse_error_operand("ABCD", 0);

    /* more than 32-bits (error), will be constrained to 32-bits */
    parse_comp_operand("4294967296", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 4294967295U);

    /* negative value (error), will be "wrapped around": -1 = 0x100000000 - 1 = 0xFFFFFFFF = 4294967295 */
    parse_comp_operand("-1", RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 4294967295U);
  }

  {
    /*------------------------------------------------------------------------
    TestVariableGetValue
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    memory.ram = ram;
    memory.size = sizeof(ram);

    /* value */
    parse_comp_operand_value("0", &memory, 0x00U);

    /* eight-bit */
    parse_comp_operand_value("0xh0", &memory, 0x00U);
    parse_comp_operand_value("0xh1", &memory, 0x12U);
    parse_comp_operand_value("0xh4", &memory, 0x56U);
    parse_comp_operand_value("0xh5", &memory, 0x00U); /* out of range */

    /* sixteen-bit */
    parse_comp_operand_value("0x 0", &memory, 0x1200U);
    parse_comp_operand_value("0x 3", &memory, 0x56ABU);
    parse_comp_operand_value("0x 4", &memory, 0x0056U); /* out of range */

    /* twenty-four-bit */
    parse_comp_operand_value("0xw0", &memory, 0x341200U);
    parse_comp_operand_value("0xw2", &memory, 0x56AB34U);
    parse_comp_operand_value("0xw3", &memory, 0x0056ABU); /* out of range */

    /* thirty-two-bit */
    parse_comp_operand_value("0xx0", &memory, 0xAB341200U);
    parse_comp_operand_value("0xx1", &memory, 0x56AB3412U);
    parse_comp_operand_value("0xx3", &memory, 0x000056ABU); /* out of range */

    /* nibbles */
    parse_comp_operand_value("0xu0", &memory, 0x0U);
    parse_comp_operand_value("0xu1", &memory, 0x1U);
    parse_comp_operand_value("0xu4", &memory, 0x5U);
    parse_comp_operand_value("0xu5", &memory, 0x0U); /* out of range */

    parse_comp_operand_value("0xl0", &memory, 0x0U);
    parse_comp_operand_value("0xl1", &memory, 0x2U);
    parse_comp_operand_value("0xl4", &memory, 0x6U);
    parse_comp_operand_value("0xl5", &memory, 0x0U); /* out of range */

    /* bits */
    parse_comp_operand_value("0xm0", &memory, 0x0U);
    parse_comp_operand_value("0xm3", &memory, 0x1U);
    parse_comp_operand_value("0xn3", &memory, 0x1U);
    parse_comp_operand_value("0xo3", &memory, 0x0U);
    parse_comp_operand_value("0xp3", &memory, 0x1U);
    parse_comp_operand_value("0xq3", &memory, 0x0U);
    parse_comp_operand_value("0xr3", &memory, 0x1U);
    parse_comp_operand_value("0xs3", &memory, 0x0U);
    parse_comp_operand_value("0xt3", &memory, 0x1U);
    parse_comp_operand_value("0xm5", &memory, 0x0U); /* out of range */

    /* bit count */
    parse_comp_operand_value("0xc0", &memory, 0x0U); /* 0 bits in 0x00 */
    parse_comp_operand_value("0xc1", &memory, 0x2U); /* 2 bits in 0x12 */
    parse_comp_operand_value("0xc2", &memory, 0x3U); /* 3 bits in 0x34 */
    parse_comp_operand_value("0xc3", &memory, 0x5U); /* 5 bits in 0xAB */
    parse_comp_operand_value("0xc4", &memory, 0x4U); /* 4 bits in 0x56 */

    /* BCD */
    parse_comp_operand_value("b0xh3", &memory, 111U); /* 0xAB not technically valid in BCD */

    ram[3] = 0x56; /* 0xAB not valid in BCD */
    parse_comp_operand_value("b0xh0", &memory, 00U);
    parse_comp_operand_value("b0xh1", &memory, 12U);
    parse_comp_operand_value("b0x 1", &memory, 3412U);
    parse_comp_operand_value("b0xw1", &memory, 563412U);
    parse_comp_operand_value("b0xx1", &memory, 56563412U);
  }

  {
    /*------------------------------------------------------------------------
    TestVariableGetValueDelta
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_operand_t op;
    const char* memaddr;
    rc_parse_state_t parse;
    char buffer[256];
    rc_memref_value_t* memrefs;

    memory.ram = ram;
    memory.size = sizeof(ram);

    memaddr = "d0xh1";
    rc_init_parse_state(&parse, buffer, 0, 0);
    rc_init_parse_state_memrefs(&parse, &memrefs);
    rc_parse_operand(&op, &memaddr, 1, 0, &parse);
    rc_destroy_parse_state(&parse);

    assert(evaluate_operand(&op, &memory, memrefs) == 0x00); /* first call gets uninitialized value */
    assert(evaluate_operand(&op, &memory, memrefs) == 0x12); /* second gets current value */

    /* RC_OPERAND_DELTA is always one frame behind */
    ram[1] = 0x13;
    assert(evaluate_operand(&op, &memory, memrefs) == 0x12U);

    ram[1] = 0x14;
    assert(evaluate_operand(&op, &memory, memrefs) == 0x13U);

    ram[1] = 0x15;
    assert(evaluate_operand(&op, &memory, memrefs) == 0x14U);

    ram[1] = 0x16;
    assert(evaluate_operand(&op, &memory, memrefs) == 0x15U);

    assert(evaluate_operand(&op, &memory, memrefs) == 0x16U);
    assert(evaluate_operand(&op, &memory, memrefs) == 0x16U);
  }

  {
    /*------------------------------------------------------------------------
    TestVariableGetValueDelta
    ------------------------------------------------------------------------*/

    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_operand_t op;
    const char* memaddr;
    rc_parse_state_t parse;
    char buffer[256];
    rc_memref_value_t* memrefs;

    memory.ram = ram;
    memory.size = sizeof(ram);

    memaddr = "p0xh1";
    rc_init_parse_state(&parse, buffer, 0, 0);
    rc_init_parse_state_memrefs(&parse, &memrefs);
    rc_parse_operand(&op, &memaddr, 1, 0, &parse);
    rc_destroy_parse_state(&parse);

    /* RC_OPERAND_PRIOR only updates when the memory value changes */
    assert(evaluate_operand(&op, &memory, memrefs) == 0x00); /* first call gets uninitialized value */
    assert(evaluate_operand(&op, &memory, memrefs) == 0x00); /* value only changes when memory changes */

    ram[1] = 0x13;
    assert(evaluate_operand(&op, &memory, memrefs) == 0x12U);
    assert(evaluate_operand(&op, &memory, memrefs) == 0x12U);
    assert(evaluate_operand(&op, &memory, memrefs) == 0x12U);
    assert(evaluate_operand(&op, &memory, memrefs) == 0x12U);

    ram[1] = 0x14;
    assert(evaluate_operand(&op, &memory, memrefs) == 0x13U);

    ram[1] = 0x15;
    assert(evaluate_operand(&op, &memory, memrefs) == 0x14U);

    ram[1] = 0x16;
    assert(evaluate_operand(&op, &memory, memrefs) == 0x15U);
    assert(evaluate_operand(&op, &memory, memrefs) == 0x15U);
    assert(evaluate_operand(&op, &memory, memrefs) == 0x15U);
  }
}

static void parse_comp_condition(
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

  assert(self->type == expected_type);
  comp_operand(&self->operand1, expected_left_type, expected_left_size, expected_left_value);
  assert(self->oper == expected_operator);
  comp_operand(&self->operand2, expected_right_type, expected_right_size, expected_right_value);
  assert(self->required_hits == expected_required_hits);
}

static int evaluate_condition(rc_condition_t* cond, memory_t* memory, rc_memref_value_t* memrefs) {
  rc_eval_state_t eval_state;

  memset(&eval_state, 0, sizeof(eval_state));
  eval_state.peek = peek;
  eval_state.peek_userdata = memory;
  
  rc_update_memref_values(memrefs, peek, memory);
  return rc_test_condition(cond, &eval_state);
}

static void parse_test_condition(const char* memaddr, memory_t* memory, int value) {
  rc_condition_t* self;
  rc_parse_state_t parse;
  char buffer[512];
  rc_memref_value_t* memrefs;
  int ret;

  rc_init_parse_state(&parse, buffer, 0, 0);
  rc_init_parse_state_memrefs(&parse, &memrefs);
  self = rc_parse_condition(&memaddr, &parse, 0);
  rc_destroy_parse_state(&parse);

  assert(parse.offset >= 0);
  assert(*memaddr == 0);

  ret = evaluate_condition(self, memory, memrefs);

  assert((ret && value) || (!ret && !value));
}

static void test_condition(void) {
  {
    /*------------------------------------------------------------------------
    TestParseConditionMemoryComparisonValue
    ------------------------------------------------------------------------*/

    /* different comparisons */
    parse_comp_condition(
      "0xH1234=8",
      RC_CONDITION_STANDARD,
      RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U,
      RC_CONDITION_EQ,
      RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 8U,
      0
    );

    parse_comp_condition(
      "0xH1234==8",
      RC_CONDITION_STANDARD,
      RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U,
      RC_CONDITION_EQ,
      RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 8U,
      0
    );

    parse_comp_condition(
      "0xH1234!=8",
      RC_CONDITION_STANDARD,
      RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U,
      RC_CONDITION_NE,
      RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 8U,
      0
    );

    parse_comp_condition(
      "0xH1234<8",
      RC_CONDITION_STANDARD,
      RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U,
      RC_CONDITION_LT,
      RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 8U,
      0
    );

    parse_comp_condition(
      "0xH1234<=8",
      RC_CONDITION_STANDARD,
      RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U,
      RC_CONDITION_LE,
      RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 8U,
      0
    );

    parse_comp_condition(
      "0xH1234>8",
      RC_CONDITION_STANDARD,
      RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U,
      RC_CONDITION_GT,
      RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 8U,
      0
    );

    parse_comp_condition(
      "0xH1234>=8",
      RC_CONDITION_STANDARD,
      RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U,
      RC_CONDITION_GE,
      RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 8U,
      0
    );

    /* delta */
    parse_comp_condition(
      "d0xH1234=8",
      RC_CONDITION_STANDARD,
      RC_OPERAND_DELTA, RC_MEMSIZE_8_BITS, 0x1234U,
      RC_CONDITION_EQ,
      RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 8U,
      0
    );

    /* flags */
    parse_comp_condition(
      "R:0xH1234=8",
      RC_CONDITION_RESET_IF,
      RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U,
      RC_CONDITION_EQ,
      RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 8U,
      0
    );

    parse_comp_condition(
      "P:0xH1234=8",
      RC_CONDITION_PAUSE_IF,
      RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U,
      RC_CONDITION_EQ,
      RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 8U,
      0
    );

    parse_comp_condition(
      "A:0xH1234=8",
      RC_CONDITION_ADD_SOURCE,
      RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U,
      RC_CONDITION_EQ,
      RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 8U,
      0
    );

    parse_comp_condition(
      "B:0xH1234=8",
      RC_CONDITION_SUB_SOURCE,
      RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U,
      RC_CONDITION_EQ,
      RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 8U,
      0
    );

    parse_comp_condition(
      "C:0xH1234=8",
      RC_CONDITION_ADD_HITS,
      RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U,
      RC_CONDITION_EQ,
      RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 8U,
      0
    );

    parse_comp_condition(
      "M:0xH1234=8",
      RC_CONDITION_MEASURED,
      RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U,
      RC_CONDITION_EQ,
      RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 8U,
      0
    );

    parse_comp_condition(
      "I:0xH1234=8",
      RC_CONDITION_ADD_ADDRESS,
      RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U,
      RC_CONDITION_EQ,
      RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 8U,
      0
    );

    /* hit count */
    parse_comp_condition(
      "0xH1234=8(1)",
      RC_CONDITION_STANDARD,
      RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U,
      RC_CONDITION_EQ,
      RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 8U,
      1
    );

    parse_comp_condition(
      "0xH1234=8.1.", /* legacy format */
      RC_CONDITION_STANDARD,
      RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U,
      RC_CONDITION_EQ,
      RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 8U,
      1
    );

    parse_comp_condition(
      "0xH1234=8(100)",
      RC_CONDITION_STANDARD,
      RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U,
      RC_CONDITION_EQ,
      RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 8U,
      100
    );
  }

  {
    /*------------------------------------------------------------------------
    TestParseConditionMemoryComparisonHexValue
    ------------------------------------------------------------------------*/

    /* hex value is interpreted as a 16-bit memory reference */
    parse_comp_condition(
      "0xH1234=0x80",
      RC_CONDITION_STANDARD,
      RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U,
      RC_CONDITION_EQ,
      RC_OPERAND_ADDRESS, RC_MEMSIZE_16_BITS, 0x80U,
      0
    );
  }

  {
    /*------------------------------------------------------------------------
    TestParseConditionMemoryComparisonMemory
    ------------------------------------------------------------------------*/

    parse_comp_condition(
      "0xL1234!=0xU3456",
      RC_CONDITION_STANDARD,
      RC_OPERAND_ADDRESS, RC_MEMSIZE_LOW, 0x1234U,
      RC_CONDITION_NE,
      RC_OPERAND_ADDRESS, RC_MEMSIZE_HIGH, 0x3456U,
      0
    );
  }

  {
    /*------------------------------------------------------------------------
    TestParseConditionShorthand
    ------------------------------------------------------------------------*/

    parse_comp_condition(
      "A:0xH1234",
      RC_CONDITION_ADD_SOURCE,
      RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U,
      RC_CONDITION_NONE,
      RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 1U,
      0
    );

    parse_comp_condition(
      "B:0xH1234",
      RC_CONDITION_SUB_SOURCE,
      RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U,
      RC_CONDITION_NONE,
      RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 1U,
      0
    );

    parse_comp_condition(
      "C:0xH1234",
      RC_CONDITION_ADD_HITS,
      RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U,
      RC_CONDITION_NONE,
      RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 1U,
      0
    );

    parse_comp_condition(
      "N:0xH1234",
      RC_CONDITION_AND_NEXT,
      RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U,
      RC_CONDITION_NONE,
      RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 1U,
      0
    );

    parse_comp_condition(
      "I:0xH1234",
      RC_CONDITION_ADD_ADDRESS,
      RC_OPERAND_ADDRESS, RC_MEMSIZE_8_BITS, 0x1234U,
      RC_CONDITION_NONE,
      RC_OPERAND_CONST, RC_MEMSIZE_8_BITS, 1U,
      0
    );
  }

  {
    /*------------------------------------------------------------------------
    TestParseCondition
    ------------------------------------------------------------------------*/
    assert(rc_trigger_size("0xH1234==0") > 0);
    assert(rc_trigger_size("H0x1234==0") == RC_INVALID_CONST_OPERAND);
    assert(rc_trigger_size("0x1234") == RC_INVALID_OPERATOR);
    assert(rc_trigger_size("P:0x1234") == RC_INVALID_OPERATOR);
    assert(rc_trigger_size("R:0x1234") == RC_INVALID_OPERATOR);
    assert(rc_trigger_size("M:0x1234") == RC_INVALID_OPERATOR);
    assert(rc_trigger_size("Z:0x1234") == RC_INVALID_CONDITION_TYPE);
    assert(rc_trigger_size("0x1234=1.2") == RC_INVALID_REQUIRED_HITS);
  }

  {
    /*------------------------------------------------------------------------
    TestConditionCompare
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;

    memory.ram = ram;
    memory.size = sizeof(ram);

    /* values */
    parse_test_condition("0xH0001=18", &memory, 1);
    parse_test_condition("0xH0001!=18", &memory, 0);
    parse_test_condition("0xH0001<=18", &memory, 1);
    parse_test_condition("0xH0001>=18", &memory, 1);
    parse_test_condition("0xH0001<18", &memory, 0);
    parse_test_condition("0xH0001>18", &memory, 0);
    parse_test_condition("0xH0001>0", &memory, 1);
    parse_test_condition("0xH0001!=0", &memory, 1);

    /* memory */
    parse_test_condition("0xH0001<0xH0002", &memory, 1);
    parse_test_condition("0xH0001>0xH0002", &memory, 0);
    parse_test_condition("0xH0001=0xH0001", &memory, 1);
    parse_test_condition("0xH0001!=0xH0002", &memory, 1);
  }

  {
    /*------------------------------------------------------------------------
    TestConditionCompareDelta
    ------------------------------------------------------------------------*/

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

    assert(parse.offset >= 0);
    assert(*cond_str == 0);
    memory.ram = ram;
    memory.size = sizeof(ram);

    /* initial delta value is 0, 0x12 > 0 */
    assert(evaluate_condition(cond, &memory, memrefs) == 1);

    /* delta value is now 0x12, 0x12 = 0x12 */
    assert(evaluate_condition(cond, &memory, memrefs) == 0);

    /* delta value is now 0x12, 0x11 < 0x12 */
    ram[1] = 0x11;
    assert(evaluate_condition(cond, &memory, memrefs) == 0);

    /* delta value is now 0x13, 0x12 > 0x11 */
    ram[1] = 0x12;
    assert(evaluate_condition(cond, &memory, memrefs) == 1);
  }
}

static void parse_trigger(rc_trigger_t** self, void* buffer, const char* memaddr) {
  int ret = rc_trigger_size(memaddr);
  assert(ret >= 0);
  memset(buffer, 0xEE, ret + 128);

  *self = rc_parse_trigger(buffer, memaddr, NULL, 0);
  assert(*self != NULL);
  assert(*((int*)((char*)buffer + ret)) == 0xEEEEEEEE);
}

static void comp_trigger(rc_trigger_t* self, memory_t* memory, int expected_result) {
  int ret = rc_test_trigger(self, peek, memory, NULL);
  assert(expected_result == ret);
}

static int evaluate_trigger(rc_trigger_t* self, memory_t* memory) {
  return rc_evaluate_trigger(self, peek, memory, NULL);
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

static unsigned condset_get_total_hitcount(rc_condset_t* condset, int ndx) {
  unsigned total;
  rc_condition_t* first = condset->conditions;

  total = 0;
  for (; first != 0; first = first->next) {
    total += first->current_hits;
    if (ndx == 0)
      return total;
    ndx--;

    switch (first->type) {
      case RC_CONDITION_ADD_HITS:
      case RC_CONDITION_ADD_SOURCE:
      case RC_CONDITION_SUB_SOURCE:
      case RC_CONDITION_AND_NEXT:
      case RC_CONDITION_ADD_ADDRESS:
        /* combining flag, don't reset total */
        break;

      default:
        /* non-combining flag, reset total */
        total = 0;
        break;
    }
  }

  /* condition not found */
  return 0;
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

static void test_trigger(void) {
  char buffer[2048];
    
  {
    /*------------------------------------------------------------------------
    TestSimpleSets
    Only standard conditions, no alt groups
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0001=18"); /* one condition, true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);

    parse_trigger(&trigger, buffer, "0xH0001!=18"); /* one condition, false */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);

    parse_trigger(&trigger, buffer, "0xH0001=18_0xH0002=52"); /* two conditions, true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);

    parse_trigger(&trigger, buffer, "0xH0001=18_0xH0002>52"); /* two conditions, false */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);

    parse_trigger(&trigger, buffer, "0xH0001=18_0xH0002=52_0xL0004=6"); /* three conditions, true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 1U);

    parse_trigger(&trigger, buffer, "0xH0001=16_0xH0002=52_0xL0004=6"); /* three conditions, first false */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 1U);

    parse_trigger(&trigger, buffer, "0xH0001=18_0xH0002=50_0xL0004=6"); /* three conditions, first false */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 1U);

    parse_trigger(&trigger, buffer, "0xH0001=18_0xH0002=52_0xL0004=4"); /* three conditions, first false */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);

    parse_trigger(&trigger, buffer, "0xH0001=16_0xH0002=50_0xL0004=4"); /* three conditions, all false */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
  }

  {
    /*------------------------------------------------------------------------
    TestPauseIf
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0001=18_P:0xH0002=52_P:0xL0x0004=6");
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U); /* Also true, but processing stops on first PauseIf */

    ram[2] = 0;
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U); /* PauseIf goes to 0 when false */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 1U); /* PauseIf stays at 1 when false */

    ram[4] = 0;
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U); /* PauseIf goes to 0 when false */
  }

  {
    /*------------------------------------------------------------------------
    TestPauseIfHitCountOne
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0001=18_P:0xH0002=52.1.");
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);

    ram[2] = 0;
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U); /* PauseIf with HitCount doesn't automatically go back to 0 */
  }

  {
    /*------------------------------------------------------------------------
    TestPauseIfHitCountTwo
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0001=18_P:0xH0002=52.2.");
    comp_trigger(trigger, &memory, 1); /* PauseIf counter hasn't reached HitCount target, non-PauseIf condition still true */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);

    comp_trigger(trigger, &memory, 0); /* PauseIf counter has reached HitCount target, non-PauseIf conditions ignored */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U);

    ram[2] = 0;
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U); /* PauseIf with HitCount doesn't automatically go back to 0 */
  }

  {
    /*------------------------------------------------------------------------
    TestPauseIfHitReset
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0001=18_P:0xH0002=52.1._R:0xH0003=1SR:0xH0003=2");
    comp_trigger(trigger, &memory, 0); /* Trigger PauseIf, non-PauseIf conditions ignored */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 0U);

    ram[2] = 0;
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U); /* PauseIf with HitCount doesn't automatically go back to 0 */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 0U);

    ram[3] = 1;
    comp_trigger(trigger, &memory, 0); /* ResetIf in Paused group is ignored */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 0U);

    ram[3] = 2;
    comp_trigger(trigger, &memory, 0); /* ResetIf in alternate group is honored, PauseIf does not retrigger and non-PauseIf condition is true */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U); /* ResetIf causes entire achievement to fail */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 0U);

    ram[3] = 3;
    comp_trigger(trigger, &memory, 1); /* ResetIf no longer true, achievement allowed to trigger */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 0U);
  }

  {
    /*------------------------------------------------------------------------
    TestResetIf
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0001=18_R:0xH0002=50_R:0xL0x0004=4");
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);

    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);

    ram[2] = 50;
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U); /* True, but ResetIf also resets true marker */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);

    ram[4] = 0x54;
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U); /* True, but ResetIf also resets true marker */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U); /* Also true, but processing stop on first ResetIf */

    ram[2] = 52;
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U); /* True, but ResetIf also resets true marker */

    ram[4] = 0x56;
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
  }

  {
    /*------------------------------------------------------------------------
    TestHitCount
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0001=20(2)_0xH0002=52");
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);

    ram[1] = 20;
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U);

    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 3U);

    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U); /* hits stop increment once count it reached */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 4U);
  }

  {
    /*------------------------------------------------------------------------
    TestHitCountResetIf
    Verifies that ResetIf resets HitCounts
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0001=18(2)_0xH0002=52_R:0xL0004=4");
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);

    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U);

    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 3U);

    ram[4] = 0x54;
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);

    ram[4] = 0x56;
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);

    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U);
  }

  {
    /*------------------------------------------------------------------------
    TestHitCountResetIfHitCount
    Verifies that ResetIf with HitCount target only resets HitCounts when target is met
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0001=18(2)_0xH0002=52_R:0xL0004=4.2.");
    comp_trigger(trigger, &memory, 0); /* HitCounts on conditions 1 and 2 are incremented */
    comp_trigger(trigger, &memory, 1); /* HitCounts on conditions 1 and 2 are incremented, cond 1 is now true so entire achievement is true */
    comp_trigger(trigger, &memory, 1); /* HitCount on condition 2 is incremented, cond 1 already met its target HitCount */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 3U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U); /* ResetIf HitCount should still be 0 */

    ram[4] = 0x54;

    /* first hit on ResetIf should not reset anything */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U); /* condition 1 stopped at it's HitCount target */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 4U); /* condition 2 continues to increment */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 1U); /* ResetIf HitCount should be 1 */

    /* second hit on ResetIf should reset everything */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U); /* ResetIf HitCount should also be reset */
  }

  {
    /*------------------------------------------------------------------------
    TestAddHitsResetIf
    Verifies that ResetIf works with AddHits
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "C:0xH0001=18_R:0xL0004=6(3)"); /* never(repeated(3, byte(1) == 18 || low(4) == 6)) */
    comp_trigger(trigger, &memory, 1); /* result is true, no non-reset conditions */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);
    assert(condset_get_total_hitcount(trigger_get_set(trigger, 0), 1) == 2U);

    comp_trigger(trigger, &memory, 0); /* total hits met (2 for each condition, only needed 3 total) (2 hits on condition 2 is not enough), result is always false if reset */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_total_hitcount(trigger_get_set(trigger, 0), 1) == 0U);
  }

  {
    /*------------------------------------------------------------------------
    TestHitCountResetIfHitCountOne
    Verifies that ResetIf HitCount(1) behaves like ResetIf without a HitCount
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0001=18(2)_0xH0002=52_R:0xL0004=4.1.");
    comp_trigger(trigger, &memory, 0); /* HitCounts on conditions 1 and 2 are incremented */
    comp_trigger(trigger, &memory, 1); /* HitCounts on conditions 1 and 2 are incremented, cond 1 is now true so entire achievement is true */
    comp_trigger(trigger, &memory, 1); /* HitCount on condition 2 is incremented, cond 1 already met its target HitCount */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 3U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U); /* ResetIf HitCount should still be 0 */

    ram[4] = 0x54;

    /* ResetIf HitCount(1) should behave just like ResetIf without a HitCount - all items, including ResetIf should be reset. */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U); /* ResetIf HitCount should also be reset */
  }

  {
    /*------------------------------------------------------------------------
    TestHitCountPauseIf
    Verifies that PauseIf stops HitCount processing
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0001=18(2)_0xH0002=52_P:0xL0004=4");
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);

    ram[4] = 0x54;
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);

    ram[4] = 0x56;
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U);

    ram[4] = 0x54;
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U);

    ram[4] = 0x56;
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 3U);
  }

  {
    /*------------------------------------------------------------------------
    TestAddHitsNoHitCount
    Odd use case: AddHits a=1
                          b=1
    Since b=1 doesn't have a hitcount, it ignores the hits tallied by a=1
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "C:0xH0001=18_0xH0000=1");
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_total_hitcount(trigger_get_set(trigger, 0), 1) == 1U);

    ram[0] = 1;
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);
    assert(condset_get_total_hitcount(trigger_get_set(trigger, 0), 1) == 3U);
  }

  {
    /*------------------------------------------------------------------------
    TestHitCountPauseIfResetIf
    Verifies that PauseIf prevents ResetIf processing
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0001=18(2)_R:0xH0002=50_P:0xL0004=4");
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);

    ram[4] = 0x54; /* pause */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);

    ram[2] = 50; /* reset (but still paused) */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);

    ram[4] = 0x56; /* unpause (still reset) */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);

    ram[2] = 52; /* unreset */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);

    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
  }

  {
    /*------------------------------------------------------------------------
    TestAddSource
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "A:0xH0001=0_0xH0002=22");
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);

    ram[2] = 4; /* sum is correct */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U); /* AddSource condition does not have hit tracking */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);

    ram[1] = 0; /* first condition is true, but not sum */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U); /* AddSource condition does not have hit tracking */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);

    ram[2] = 22; /* first condition is true, sum is correct */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U); /* AddSource condition does not have hit tracking */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U);
  }

  {
    /*------------------------------------------------------------------------
    TestSubSource
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "B:0xH0002=0_0xH0001=14"); /* NOTE: SubSource subtracts the first value from the second! */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);

    ram[2] = 4; /* difference is correct */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U); /* SubSource condition does not have hit tracking */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);

    ram[1] = 0; /* first condition is true, but not difference */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U); /* SubSource condition does not have hit tracking */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);

    ram[2] = 14; /* first condition is true, value is negative inverse of expected value */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U); /* SubSource condition does not have hit tracking */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);

    ram[1] = 28; /* difference is correct again */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U); /* SubSource condition does not have hit tracking */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U);
  }

  {
    /*------------------------------------------------------------------------
    TestAddSubSource
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "A:0xH0001=0_B:0xL0002=0_0xL0004=14"); /* byte(1) - low(2) + low(4) == 14 */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);

    ram[1] = 12; /* total is correct */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U); /* AddSource condition does not have hit tracking */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U); /* SubSource condition does not have hit tracking */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 1U);

    ram[1] = 0; /* first condition is true, but not total */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U); /* AddSource condition does not have hit tracking */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U); /* SubSource condition does not have hit tracking */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 1U);

    ram[4] = 18; /* byte(4) would make total true, but not low(4) */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U); /* AddSource condition does not have hit tracking */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U); /* SubSource condition does not have hit tracking */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 1U);

    ram[2] = 1;
    ram[4] = 15; /* difference is correct again */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U); /* AddSource condition does not have hit tracking */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U); /* SubSource condition does not have hit tracking */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 2U);
  }

  {
    /*------------------------------------------------------------------------
    TestAddHits
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    rc_condset_t* condset;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "C:0xH0001=18(2)_0xL0004=6(4)"); /* repeated(4, byte(1) == 18 || low(4) == 6) */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);
    assert(condset_get_total_hitcount(trigger_get_set(trigger, 0), 1) == 2U);

    comp_trigger(trigger, &memory, 1); /* total hits met (2 for each condition) */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U);
    assert(condset_get_total_hitcount(trigger_get_set(trigger, 0), 1) == 4U);

    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U); /* threshold met, stop incrementing */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U); /* total met prevents incrementing even though individual tally has not reached total */
    assert(condset_get_total_hitcount(trigger_get_set(trigger, 0), 1) == 4U);

    rc_reset_condset(trigger->requirement);

    for (condset = trigger->alternative; condset != NULL; condset = condset->next) {
      rc_reset_condset(condset);
    }

    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);
    assert(condset_get_total_hitcount(trigger_get_set(trigger, 0), 1) == 2U);

    ram[1] = 16;
    comp_trigger(trigger, &memory, 0); /* 1 + 2 < 4, not met */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U);
    assert(condset_get_total_hitcount(trigger_get_set(trigger, 0), 1) == 3U);

    comp_trigger(trigger, &memory, 1); /* 1 + 3 = 4, met */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 3U);
    assert(condset_get_total_hitcount(trigger_get_set(trigger, 0), 1) == 4U);
  }

  {
    /*------------------------------------------------------------------------
    TestAddSourceAddHits
    ------------------------------------------------------------------------*/

    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "A:0xH0001_C:0xH0002=70_0xH0000=0(2)"); /* repeated(2, (byte(1) + byte(2) == 70) || byte(0) == 0) */
    comp_trigger(trigger, &memory, 1); /* both conditions are true - addhits should match required 2 hits */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U); /* 0x12+0x34 = 0x46 - true! */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 1U); /* 0 = 0 - true! */
    assert(condset_get_total_hitcount(trigger_get_set(trigger, 0), 2) == 2U);

    parse_trigger(&trigger, buffer, "C:0xH0000=0_A:0xH0001=0_0xH0002=70(2)"); /* repeated(2, byte(0) == 0 || (byte(1) + byte(2) == 70)) */
    comp_trigger(trigger, &memory, 1); /* both conditions are true - addhits should match required 2 hits */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U); /* 0 = 0 - true! */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 1U); /* 0x12+0x34 = 0x46 - true! */
    assert(condset_get_total_hitcount(trigger_get_set(trigger, 0), 2) == 2U);
  }

  {
    /*------------------------------------------------------------------------
    TestAndNext
    ------------------------------------------------------------------------*/

    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    /* once(byte(0x0001) == 20 && byte(0x0002) == 20 && byte(0x0003) == 20) */
    parse_trigger(&trigger, buffer, "N:0xH0001=20_N:0xH0002=20_0xH0003=20.3.");
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);

    ram[3] = 20; /* final condition is not enough to reset */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);

    ram[2] = 20; /* two conditions is not enough to reset */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);

    ram[1] = 20; /* all three conditions true, only count hit on final */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 1U);

    ram[2] = 30; /* middle condition not true */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 1U);

    ram[2] = 20; /* all three conditions true */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 2U);

    ram[3] = 30; /* third condition not true */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 2U);

    ram[3] = 20; /* all three conditions true, HitCount reached */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 3U);

    comp_trigger(trigger, &memory, 1); /* HitCount reached */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 3U);
  }

  {
    /*------------------------------------------------------------------------
    TestAndNextBoundaries
    ------------------------------------------------------------------------*/

    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_trigger_t* trigger;
    char buffer[2048];

    memory.ram = ram;
    memory.size = sizeof(ram);

    /* byte(0x0000) == 0 && once(byte(0x0001) == 20 && byte(0x0002) == 20 && byte(0x0003) == 20) && byte(0x0000) == 0 */
    parse_trigger(&trigger, buffer, "0xH0000=0_N:0xH0001=20_N:0xH0002=20_0xH0003=20.1._0xH0000=0");
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 3)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 4)->current_hits == 1U);

    ram[3] = 20; /* final condition is not enough to reset */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 3)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 4)->current_hits == 2U);

    ram[2] = 20; /* two conditions is not enough to reset */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 3U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 3)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 4)->current_hits == 3U);

    ram[1] = 20; /* all three conditions true, whole trigger is true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 4U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 3)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 4)->current_hits == 4U);
  }

  {
    /*------------------------------------------------------------------------
    TestAndNextReset
    ------------------------------------------------------------------------*/

    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    /* byte(0x0000) == 0 && never(byte(0x0001) == 20 && byte(0x0002) == 20 && byte(0x0003) == 20) */
    parse_trigger(&trigger, buffer, "0xH0000=0_N:0xH0001=20_N:0xH0002=20_R:0xH0003=20");
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 3)->current_hits == 0U);

    ram[3] = 20; /* final condition is not enough to reset */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 3)->current_hits == 0U);

    ram[2] = 20; /* two conditions is not enough to reset */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 3U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 3)->current_hits == 0U);

    ram[1] = 20; /* all three conditions true, reset */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 3)->current_hits == 0U);

    ram[2] = 30; /* middle condition not true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);

    ram[2] = 20; /* all three conditions true */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);

    ram[3] = 30; /* third condition not true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);

    ram[3] = 20; /* all three conditions true */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
  }

  {
    /*------------------------------------------------------------------------
    TestAndNextPause
    ------------------------------------------------------------------------*/

    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    /* byte(0x0000) == 0 && never(byte(0x0001) == 20 && byte(0x0002) == 20 && byte(0x0003) == 20) */
    parse_trigger(&trigger, buffer, "0xH0000=0_N:0xH0001=20_N:0xH0002=20_P:0xH0003=20");
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 3)->current_hits == 0U);

    ram[3] = 20; /* final condition is not enough to pause */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 3)->current_hits == 0U);

    ram[2] = 20; /* two conditions is not enough to pause */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 3U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 3)->current_hits == 0U);

    ram[1] = 20; /* all three conditions true, pause */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 3U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 3)->current_hits == 1U);

    ram[2] = 30; /* middle condition not true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 4U);

    ram[2] = 20; /* all three conditions true */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 4U);

    ram[3] = 30; /* third condition not true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 5U);

    ram[3] = 20; /* all three conditions true */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 5U);
  }

  {
    /*------------------------------------------------------------------------
    TestAndNextAddSource
    ------------------------------------------------------------------------*/

    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    /* once(byte(0x0001) + byte(0x0002) == 20 && byte(0x0003) == 20) */
    parse_trigger(&trigger, buffer, "A:0xH0001=0_N:0xH0002=20_0xH0003=20.1.");
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);

    ram[3] = 20; /* final condition is true */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);

    ram[2] = 20; /* AddSource condition would be true if AddSource ignored */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);

    ram[2] = 10;
    ram[1] = 10; /* AddSource condition true only via addition, whole trigger is true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 1U);
  }

  {
    /*------------------------------------------------------------------------
    TestAndNextChangesTo
    ------------------------------------------------------------------------*/

    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    /* byte(0x0001) ~> 18 */
    parse_trigger(&trigger, buffer, "N:0xH0001=18_d0xH0001!=18");

    /* value already 18, initial delta value is 0, so considered changed */
    comp_trigger(trigger, &memory, 1);

    /* value already 18 */
    comp_trigger(trigger, &memory, 0);

    /* value no longer 18 */
    ram[1] = 20;
    comp_trigger(trigger, &memory, 0);

    /* value changes to 18 */
    ram[1] = 18;
    comp_trigger(trigger, &memory, 1);

    /* value already 18 */
    comp_trigger(trigger, &memory, 0);
  }

  {
    /*------------------------------------------------------------------------
    TestAndNextAddHits
    ------------------------------------------------------------------------*/

    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    /* repeated(5, (byte(0) == 1 && byte(0x0001) > prev(byte(0x0001))) || byte(0) == 2 || 0 == 1) */
    parse_trigger(&trigger, buffer, "N:0xH00=1_C:0xH01>d0xH01_N:0=1_0=1.2.");

    /* initialize delta */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 3)->current_hits == 0U);

    /* first part of AndNext not true */
    ++ram[1];
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 3)->current_hits == 0U);

    /* AndNext should be true */
    ram[0] = 1;
    ++ram[1];
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 3)->current_hits == 0U);

    /* AndNext should be true, hit count should be sufficient to trigger */
    ++ram[1];
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 3)->current_hits == 0U);
  }

  {
    /*------------------------------------------------------------------------
    TestAndNextWithHits
    ------------------------------------------------------------------------*/

    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    /* AndNext has higher priority than AddHits
     *
     *   AddHits byte(0x0001) == 20 (2)
     *   AndNext byte(0x0002) == 20 (2)  <-- hit count will be ignored so line 1's hits can affect line 3
     *           byte(0x0003) == 20 (4)
     *
     * The AndNext on line 2 will combine with line 3, not line 1, so the overall interpretation is:
     *
     *   repeated(4, repeated(2, byte(0x0001) == 20) || (byte(0x0002) == 20 && byte(0x0003) == 20)))
     */
    parse_trigger(&trigger, buffer, "C:0xH0001=20.2._N:0xH0002=20.2._0xH0003=20.4.");
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);

    ram[3] = 20; /* final condition is not enough to trigger */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);

    ram[2] = 20; /* two conditions will trigger the secondary subclause - line 2 will not tally */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 1U);

    ram[3] = 0; /* first conditions true, but not second, only first will increment */
    ram[1] = 20;
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 1U);

    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 1U);

    ram[3] = 20; /* all three conditions true, first already hit target hit count, third will, so overall will be true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 2U);

    /* HitCounts reached, nothing will be tallied. overall still true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 2U);
  }

  {
    /*------------------------------------------------------------------------
    TestOrNext
    ------------------------------------------------------------------------*/

    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    /* repeated(5, byte(0x0001) == 20 || byte(0x0002) == 20 || byte(0x0003) == 20) */
    parse_trigger(&trigger, buffer, "O:0xH0001=20_O:0xH0002=20_0xH0003=20.6.");
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 0U);

    ram[1] = 20; /* first condition is true, only captured on last */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 1U);

    ram[2] = 20; /* two conditions are true, should only increment last once */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 2U);

    ram[3] = 20; /* all three conditions true, only increment last */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 3U);

    ram[1] = ram[3] = 30; /* only middle is true */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 4U);

    ram[2] = 30; ram[3] = 20; /* only last is true */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 5U);

    ram[3] = 30; /* none are true */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 5U);

    ram[1] = 20; /* first is true, hitcount met, trigger */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 6U);

    comp_trigger(trigger, &memory, 1); /* HitCount reached */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->current_hits == 6U);
  }

  {
    /*------------------------------------------------------------------------
    TestAndNextOrNextInteraction
    ------------------------------------------------------------------------*/

    unsigned char ram[] = { 0, 0, 0, 0, 0 };
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    /* AndNext and OrNext are evaluated at each step: (((1 || 2) && 3) || 4) */
    parse_trigger(&trigger, buffer, "O:0xH0001=1_N:0xH0002=1_O:0xH0003=1_0xH0004=1");

                comp_trigger(trigger, &memory, 0); /* (((0 || 0) && 0) || 0) = 0 */
    ram[4] = 1; comp_trigger(trigger, &memory, 1); /* (((0 || 0) && 0) || 1) = 1 */
    ram[3] = 1; comp_trigger(trigger, &memory, 1); /* (((0 || 0) && 1) || 1) = 1 */
    ram[4] = 0; comp_trigger(trigger, &memory, 0); /* (((0 || 0) && 1) || 0) = 0 */
    ram[2] = 1; comp_trigger(trigger, &memory, 1); /* (((0 || 1) && 1) || 0) = 1 */
    ram[1] = 1; comp_trigger(trigger, &memory, 1); /* (((1 || 1) && 1) || 0) = 1 */
    ram[2] = 0; comp_trigger(trigger, &memory, 1); /* (((1 || 0) && 1) || 0) = 1 */
    ram[3] = 0; comp_trigger(trigger, &memory, 0); /* (((1 || 0) && 0) || 0) = 0 */
    ram[4] = 1; comp_trigger(trigger, &memory, 1); /* (((1 || 0) && 0) || 1) = 1 */
    ram[3] = 1; comp_trigger(trigger, &memory, 1); /* (((1 || 0) && 1) || 1) = 1 */
  }

  {
    /*------------------------------------------------------------------------
    TestAddAddressDirectPointer
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x01, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "I:0xH0000=0_0xH0000=22");
    comp_trigger(trigger, &memory, 0);

    ram[1] = 22; /* value is correct */
    comp_trigger(trigger, &memory, 1);

    ram[0] = 2; /* point to new value */
    comp_trigger(trigger, &memory, 0);

    ram[2] = 22; /* new value is correct */
    comp_trigger(trigger, &memory, 1);

    ram[0] = 1; /* point to original value */
    comp_trigger(trigger, &memory, 1);

    ram[1] = 11; /* original value is not correct */
    comp_trigger(trigger, &memory, 0);
  }

  {
    /*------------------------------------------------------------------------
    TestAddAddressIndirectPointer
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x01, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "I:0xH0000=0_0xH0002=22");
    comp_trigger(trigger, &memory, 0);

    ram[1] = 22; /* non-offset value is correct */
    comp_trigger(trigger, &memory, 0);

    ram[3] = 22; /* offset value is correct */
    comp_trigger(trigger, &memory, 1);

    ram[0] = 2; /* point to new value */
    comp_trigger(trigger, &memory, 0);

    ram[4] = 22; /* new value is correct */
    comp_trigger(trigger, &memory, 1);

    ram[0] = 0; /* point to new value */
    comp_trigger(trigger, &memory, 0);

    ram[2] = 22; /* new value is correct */
    comp_trigger(trigger, &memory, 1);
  }

  {
    /*------------------------------------------------------------------------
    TestAddAddressIndirectPointerDataSizeDiffersFromPointerSize
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x01, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "I:0xH0000=0_0x 0002=22");
    comp_trigger(trigger, &memory, 0);

    ram[3] = 22; /* 8-bit offset value is correct */
    comp_trigger(trigger, &memory, 0);

    ram[4] = 0; /* 16-bit offset value is correct */
    comp_trigger(trigger, &memory, 1);

    ram[0] = 0; /* point to new value */
    comp_trigger(trigger, &memory, 0);

    ram[3] = 0; /* new value only partially correct */
    comp_trigger(trigger, &memory, 0);

    ram[2] = 22; /* new value is correct */
    comp_trigger(trigger, &memory, 1);
  }

  {
    /*------------------------------------------------------------------------
    TestAddAddressIndirectPointerOutOfRange
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x01, 0x12, 0x34, 0xAB, 0x56, 0x16};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = 5; /* 6th byte is valid, but out of range */

    parse_trigger(&trigger, buffer, "I:0xH0000=0_0xH0002=22");
    comp_trigger(trigger, &memory, 0);

    ram[3] = 22; /* offset value is correct */
    comp_trigger(trigger, &memory, 1);

    ram[0] = 100; /* way out of bounds */
    comp_trigger(trigger, &memory, 0);

    ram[0] = 3; /* boundary condition - value is correct, but should be unreachable. 
                   note: address validation must be handled by registered 'peek' callback */
    comp_trigger(trigger, &memory, 0);
  }

  {
    /*------------------------------------------------------------------------
    TestAddAddressDoubleIndirection
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x01, 0x02, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "I:0xH0000=0_I:0xH0000=0_0xH0000=22"); /* $($($0000)) == 22 */
    comp_trigger(trigger, &memory, 0);

    ram[2] = 22; /* value is correct */
    comp_trigger(trigger, &memory, 1);

    ram[1] = 3; /* second pointer in chain causes final pointer to point at address 3 */
    comp_trigger(trigger, &memory, 0);

    ram[3] = 22; /* new value is correct */
    comp_trigger(trigger, &memory, 1);

    ram[0] = 2; /* first pointer points at address 2, which is 22, so out-of-bounds */
    comp_trigger(trigger, &memory, 0);

    ram[2] = 3; /* second pointer points at address 3, which is correct */
    comp_trigger(trigger, &memory, 1);
  }

  {
    /*------------------------------------------------------------------------
    TestAddAddressBothSidesDiffer
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x02, 0x11, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "I:0xH0000=0_0xH0000=0xH0001"); /* $($0) == $($0 + 1) */
    comp_trigger(trigger, &memory, 0);

    ram[2] = ram[3]; /* value is correct */
    comp_trigger(trigger, &memory, 1);

    ram[0] = 1; /* adjust pointer */
    comp_trigger(trigger, &memory, 0);

    ram[1] = ram[2]; /* new value is correct */
    comp_trigger(trigger, &memory, 1);
  }

  {
    /*------------------------------------------------------------------------
    TestAddAddressBothSidesSame
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x02, 0x11, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "I:0xH0000=0_0xH0000>d0xH0000"); /* $($0) > delta $($0) */
    comp_trigger(trigger, &memory, 1); /* initial delta will be 0, so non-zero is greater */

    comp_trigger(trigger, &memory, 0); /* delta should be same as current */

    ram[2]++; /* value increased */
    comp_trigger(trigger, &memory, 1);

    ram[2]--; /* value decreased */
    comp_trigger(trigger, &memory, 0);

    /* this is a small hiccup in the AddAddress behavior, we can't reasonably know the delta 
     * of a value when the pointer changes as we didn't evaluate the new address on the last frame */
    ram[0] = 3; /* point at new value, which is greater */
    comp_trigger(trigger, &memory, 1);
  }
  
  {
    /*------------------------------------------------------------------------
    TestAddAddressIndirectPointerMultiple
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x01, 0x02, 0x03, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    /* the expectation is that the AddAddress lines will share rc_memref_value_t's, but the following lines
       will generate their own rc_memref_value_t's for indirection. none of this is actually verified. */
    parse_trigger(&trigger, buffer, 
        "I:0xH0000=0_0xH0002=22_I:0xH0000=0_0xH0003=23_I:0xH0001=0_0xH0003=24");
    /*   $(0002 + $0000) == 22 && $(0003 + $0000) == 23 && $(0003 + $0001) == 24 */
    /*   $0003 (0x34)    == 22 && $0004 (0xAB)    == 23 && $0005 (0x56)    == 24 */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 3)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 5)->current_hits == 0U);

    ram[3] = 22; /* first condition true */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 3)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 5)->current_hits == 0U);

    ram[4] = 23; /* second condition true */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 3)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 5)->current_hits == 0U);

    ram[5] = 24; /* third condition true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 3U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 3)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 5)->current_hits == 1U);
  }

  {
    /*------------------------------------------------------------------------
    TestMeasured
    ------------------------------------------------------------------------*/

    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "M:0xH0002=52(3)"); /* measured(3, byte(2) == 52) */
    comp_trigger(trigger, &memory, 0); /* condition is true - hit count should be incremented */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(trigger->measured_value == 1U);
    assert(trigger->measured_target == 3U);

    comp_trigger(trigger, &memory, 0); /* condition is true - hit count should be incremented */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(trigger->measured_value == 2U);

    comp_trigger(trigger, &memory, 1); /* condition is true - hit count should be incremented and target met */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 3U);
    assert(trigger->measured_value == 3U);

    comp_trigger(trigger, &memory, 1); /* conditions is true, target met - hit count should stop incrementing */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 3U);
    assert(trigger->measured_value == 3U);
    assert(trigger->measured_target == 3U);
  }
  
  {
    /*------------------------------------------------------------------------
    TestMeasuredAddHits
    ------------------------------------------------------------------------*/

    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "C:0xH0001=10_M:0xH0002=10(5)"); /* measured(5, byte(1) == 10 || byte(2) == 10) */
    comp_trigger(trigger, &memory, 0); /* neither is true - hit count should not be incremented */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);
    ram[2] = 10;
    comp_trigger(trigger, &memory, 0); /* second is true - hit count should be incremented */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);
    assert(trigger->measured_value == 1U);
    assert(trigger->measured_target == 5U);
    ram[1] = 10;
    comp_trigger(trigger, &memory, 0); /* both are true - hit count should be incremented */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U);
    assert(trigger->measured_value == 3U);
    ram[2] = 0;
    comp_trigger(trigger, &memory, 0); /* first is true - hit count should be incremented */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U);
    assert(trigger->measured_value == 4U);
    ram[1] = 0;
    comp_trigger(trigger, &memory, 0); /* neither is true - hit count should not be incremented */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U);
    assert(trigger->measured_value == 4U);
    ram[1] = 10;
    comp_trigger(trigger, &memory, 1); /* first is true - hit count should be incremented and target met */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 3U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U);
    assert(trigger->measured_value == 5U);
  }

  {
    /*------------------------------------------------------------------------
    TestMeasuredMultiple
    ------------------------------------------------------------------------*/

    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    /* multiple measured conditions are only okay if they all have the same target, in which
     * case, the maximum of all the measured values is returned */
    parse_trigger(&trigger, buffer, "SM:0xH0002=52(3)SM:0xH0003=17(3)"); /* measured(3, byte(2) == 52) || measured(3, byte(3) == 17) */
    comp_trigger(trigger, &memory, 0); /* first condition is true - hit count should be incremented */
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 1U);
    assert(trigger->measured_value == 1U);
    assert(trigger->measured_target == 3U);

    /* second condition is true - hit count should be incremented - both values should be the same */
    ram[2] = 9;
    ram[3] = 17;
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 1U);
    assert(trigger->measured_value == 1U);

    comp_trigger(trigger, &memory, 0); /* second condition should become prominent */
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 2U);
    assert(trigger->measured_value == 2U);
    assert(trigger->measured_target == 3U);

    /* switch back to first condition */
    ram[2] = 52;
    ram[3] = 8;
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 2U);
    assert(trigger->measured_value == 2U);

    comp_trigger(trigger, &memory, 1); /* hit count should be incremented and target met */
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 3U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 2U);
    assert(trigger->measured_value == 3U);

    /* both conditions true - first should stop incrementing as target was hit */
    ram[3] = 17;
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 3U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 3U);
    assert(trigger->measured_value == 3U);

    comp_trigger(trigger, &memory, 1); /* both conditions target met - hit count should stop incrementing */
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 3U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 3U);
    assert(trigger->measured_value == 3U);
    assert(trigger->measured_target == 3U);
  }

  {
    /*------------------------------------------------------------------------
    TestMeasuredWhilePaused
    ------------------------------------------------------------------------*/

    unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "M:0xH0002=52(3)_P:0xH0001=1"); /* measured(3, byte(2) == 52) && unless(byte(1) == 1) */
    comp_trigger(trigger, &memory, 0); /* condition is true - hit count should be incremented */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(trigger->measured_value == 1U);
    assert(trigger->measured_target == 3U);

    comp_trigger(trigger, &memory, 0); /* condition is true - hit count should be incremented */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(trigger->measured_value == 2U);

    memory.ram[1] = 1; /* paused, hit count should not be incremented */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(trigger->measured_value == 2U);

    memory.ram[1] = 2; /* unpaused, hit count should be incremented */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 3U);
    assert(trigger->measured_value == 3U);
  }

  {
    /*------------------------------------------------------------------------
    TestMeasuredWhilePausedAlts
    ------------------------------------------------------------------------*/

    unsigned char ram[] = { 0x00, 0x00, 0x34, 0xAB, 0x56 };
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    /* (measured(6, byte(2) == 52) && unless(bit0(1) == 1)) || (measured(6, byte(0) == 0) && unless(bit1(1) == 1)) */
    parse_trigger(&trigger, buffer, "SM:0xH0002=52(6)_P:0xM0001=1SM:0xH0000=0(6)_P:0xN0001=1");
    comp_trigger(trigger, &memory, 0); /* both alts should be incremented */
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 1U);
    assert(trigger->measured_value == 1U);
    assert(trigger->measured_target == 6U);

    memory.ram[1] = 1; /* first alt is paused, second should update */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 2U);
    assert(trigger->measured_value == 2U);

    comp_trigger(trigger, &memory, 0); /* first alt still paused, second should update again */
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 3U);
    assert(trigger->measured_value == 3U);

    memory.ram[1] = 3; /* both alts paused, neither should update, last measured value is kept */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 3U);
    assert(trigger->measured_value == 3U);

    memory.ram[1] = 2; /* first alt unpaused, it should update, measured will use unpaused value */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 3U);
    assert(trigger->measured_value == 2U);

    memory.ram[1] = 0; /* both alts unpaused, both should update, measured will use higher value */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 3U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 4U);
    assert(trigger->measured_value == 4U);
  }

  {
    /*------------------------------------------------------------------------
    TestAltGroups
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0001=16S0xH0002=52S0xL0004=6");

    /* core not true, both alts are */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 1U);

    ram[1] = 16; /* core and both alts true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 2U);

    ram[4] = 0; /* core and first alt true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 3U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 2U);

    ram[2] = 0; /* core true, but neither alt is */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 3U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 3U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 2U);

    ram[4] = 6; /* core and second alt true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 4U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 3U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 3U);
  }

  {
    /*------------------------------------------------------------------------
    TestEmptyCore
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;
    char buffer[2048];

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "S0xH0002=2S0xL0004=4");

    /* core implicitly true, neither alt true */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 0U);

    ram[2] = 2; /* core and first alt true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 0U);

    ram[4] = 4; /* core and both alts true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 1U);

    ram[2] = 0; /* core and second alt true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 2U);
  }

  {
    /*------------------------------------------------------------------------
    TestEmptyAlt
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;
    char buffer[2048];

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0002=2SS0xL0004=4");

    /* core false, first alt implicitly true */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 0U);

    ram[2] = 2; /* core and first alt true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 0U);

    ram[4] = 4; /* core and both alts true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 1U);
  }

  {
    /*------------------------------------------------------------------------
    TestEmptyLastAlt
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;
    char buffer[2048];

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0002=2S0xL0004=4S");

    /* core false, second alt implicitly true */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 0U);

    ram[2] = 2; /* core and second alt true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 0U);

    ram[4] = 4; /* core and both alts true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 1U);
  }

  {
    /*------------------------------------------------------------------------
    TestEmptyAllAlts
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;
    char buffer[2048];

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0002=2SS");

    /* core false, all alts implicitly true */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);

    ram[2] = 2; /* core and all alts true */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
  }

  {
    /*------------------------------------------------------------------------
    TestResetIfInAltGroup
    Verifies that a ResetIf resets everything regardless of where it is
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0001=18(1)_R:0xH0000=1S0xH0002=52(1)S0xL0004=6(1)_R:0xH0000=2");
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 1U);

    ram[0] = 1; /* reset in core group resets everything */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 0U);

    ram[0] = 0;
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 1U);

    ram[0] = 2; /* reset in alt group resets everything */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 0U);
  }

  {
    /*------------------------------------------------------------------------
    TestPauseIfInAltGroup
    Verifies that PauseIf only pauses the group it's in
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0001=18_P:0xH0000=1S0xH0002=52S0xL0004=6_P:0xH0000=2");
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 1U);

    ram[0] = 1; /* pause in core group only pauses core group */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 2U);

    ram[0] = 0;
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 3U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 3U);

    ram[0] = 2; /* pause in alt group only pauses alt group */
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 3U);
    assert(condset_get_cond(trigger_get_set(trigger, 1), 0)->current_hits == 4U);
    assert(condset_get_cond(trigger_get_set(trigger, 2), 0)->current_hits == 3U);
  }

  {
    /*------------------------------------------------------------------------
    TestPauseIfResetIfAltGroup
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0000=0.1._0xH0000=2SP:0xH0001=18_R:0xH0002=52");
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);

    ram[0] = 1; /* move off HitCount */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);

    ram[1] = 16; /* unpause alt group, HitCount should be reset */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);

    ram[0] = 0;
    ram[1] = 18; /* repause alt group, reset hitcount target, hitcount should be set */
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);

    ram[0] = 2; /* trigger win condition. alt group has no normal conditions, it should be considered false */
    comp_trigger(trigger, &memory, 0);
  }

  {
    /*------------------------------------------------------------------------
    TestDeltaUpdatedInPauseIf
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "P:0xH0001=18_d0xH0002=52");
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U); /* PauseIf true */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U); /* delta = 0, not true */

    ram[2] = 52;
    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 2U); /* PauseIf true */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U); /* delta = 0, but paused */

    comp_trigger(trigger, &memory, 0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 3U); /* PauseIf true */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U); /* delta = 52, but paused */

    ram[1] = 0;
    comp_trigger(trigger, &memory, 1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U); /* PauseIf false */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U); /* delta = 52, count it */
  }

  {
    /*------------------------------------------------------------------------
    TestEvaluateTriggerTransistions
    Verifies return codes of rc_evaluate_trigger
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0001=18_0xH0002<=52_R:0xL0004=4");

    /* ==== INACTIVE ==== */
    trigger->state = RC_TRIGGER_STATE_INACTIVE;

    /* Inactive is a permanent state - trigger is initially true */
    assert(evaluate_trigger(trigger, &memory) == RC_TRIGGER_STATE_INACTIVE);
    assert(evaluate_trigger(trigger, &memory) == RC_TRIGGER_STATE_INACTIVE);
    ram[2] = 24;
    assert(evaluate_trigger(trigger, &memory) == RC_TRIGGER_STATE_INACTIVE);

    /* Trigger no longer true, still inactive */
    ram[1] = 1;
    assert(evaluate_trigger(trigger, &memory) == RC_TRIGGER_STATE_INACTIVE);

    /* hits should not be tallied when inactive */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 0U);

    /* memrefs should be updated while inactive */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->operand1.value.memref->value == 24U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->operand1.value.memref->previous == 24U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->operand1.value.memref->prior == 52U);

    /* reset should be ignored while inactive */
    ram[4] = 4;
    condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits = 1U;
    assert(evaluate_trigger(trigger, &memory) == RC_TRIGGER_STATE_INACTIVE);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);

    /* ==== WAITING ==== */
    trigger->state = RC_TRIGGER_STATE_WAITING;

    /* set trigger state = true, ResetIf false */
    ram[1] = 18;
    ram[4] = 9;

    /* state doesn't change as long as it's waiting - prevents triggers from uninitialized memory */
    assert(evaluate_trigger(trigger, &memory) == RC_TRIGGER_STATE_WAITING);
    assert(evaluate_trigger(trigger, &memory) == RC_TRIGGER_STATE_WAITING);
    ram[2] = 16;
    assert(evaluate_trigger(trigger, &memory) == RC_TRIGGER_STATE_WAITING);

    /* waiting trigger should not tally hits */
    assert(!trigger->has_hits);

    /* ResetIf makes the trigger state false, so the trigger should become active */
    ram[4] = 4;
    assert(evaluate_trigger(trigger, &memory) == RC_TRIGGER_STATE_ACTIVE);

    /* reset to previous state */
    trigger->state = RC_TRIGGER_STATE_WAITING;
    ram[4] = 9;
    assert(evaluate_trigger(trigger, &memory) == RC_TRIGGER_STATE_WAITING);
    assert(!trigger->has_hits);

    /* trigger is no longer true, proceed to active state */
    ram[1] = 5;
    assert(evaluate_trigger(trigger, &memory) == RC_TRIGGER_STATE_ACTIVE);
    assert(trigger->has_hits);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 1U);
    
    /* ==== RESET ==== */
    assert(trigger->state == RC_TRIGGER_STATE_ACTIVE);

    /* reset when has_hits returns RESET notification, but doesn't change state */
    ram[4] = 4;
    assert(evaluate_trigger(trigger, &memory) == RC_TRIGGER_STATE_RESET);
    assert(trigger->state == RC_TRIGGER_STATE_ACTIVE);
    assert(!trigger->has_hits);

    /* reset when !has_hits does not return RESET notification */
    assert(evaluate_trigger(trigger, &memory) == RC_TRIGGER_STATE_ACTIVE);
    assert(trigger->state == RC_TRIGGER_STATE_ACTIVE);
    assert(!trigger->has_hits);

    /* ==== TRIGGERED ==== */
    ram[4] = 9;
    assert(evaluate_trigger(trigger, &memory) == RC_TRIGGER_STATE_ACTIVE);

    ram[1] = 18;
    assert(evaluate_trigger(trigger, &memory) == RC_TRIGGER_STATE_TRIGGERED);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U);

    /* triggered trigger remains triggered but does not increment hit counts */
    assert(evaluate_trigger(trigger, &memory) == RC_TRIGGER_STATE_INACTIVE);
    assert(trigger->state == RC_TRIGGER_STATE_TRIGGERED);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U);

    /* triggered trigger remains triggered when no longer true */
    ram[1] = 5;
    assert(evaluate_trigger(trigger, &memory) == RC_TRIGGER_STATE_INACTIVE);
    assert(trigger->state == RC_TRIGGER_STATE_TRIGGERED);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->current_hits == 2U);

    /* triggered trigger does not update deltas */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->operand1.value.memref->value == 18U);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->operand1.value.memref->previous == 5U);
  }

  {
    /*------------------------------------------------------------------------
    TestEvaluateTriggerTransistionsPause
    Verifies return codes of rc_evaluate_trigger
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    rc_trigger_t* trigger;

    memory.ram = ram;
    memory.size = sizeof(ram);

    parse_trigger(&trigger, buffer, "0xH0001=18_0xH0003=171_P:0xH0002=1SR:0xH0004=4");

    /* Inactive is a permanent state - trigger is initially true */
    trigger->state = RC_TRIGGER_STATE_INACTIVE;
    assert(evaluate_trigger(trigger, &memory) == RC_TRIGGER_STATE_INACTIVE);

    /* PauseIf is ignored while inactive */
    ram[2] = 1;
    assert(evaluate_trigger(trigger, &memory) == RC_TRIGGER_STATE_INACTIVE);

    /* switch state to waiting, ready to trigger */
    ram[2] = 2;
    trigger->state = RC_TRIGGER_STATE_WAITING;
    assert(evaluate_trigger(trigger, &memory) == RC_TRIGGER_STATE_WAITING);

    /* PauseIf makes the evaluation false, so state should advance to active, but paused */
    ram[2] = 1;
    assert(evaluate_trigger(trigger, &memory) == RC_TRIGGER_STATE_PAUSED);
    assert(trigger->has_hits); /* the pauseif has a hit */
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);

    /* hitcounts should update when unpaused - adjust memory so trigger no longer true */
    ram[2] = 2;
    ram[3] = 99;
    assert(evaluate_trigger(trigger, &memory) == RC_TRIGGER_STATE_ACTIVE);
    assert(trigger->has_hits);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);

    /* hitcounts should remain when paused */
    ram[2] = 1;
    assert(evaluate_trigger(trigger, &memory) == RC_TRIGGER_STATE_PAUSED);
    assert(trigger->has_hits);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 1U);

    /* Reset while paused should notify, but not change state */
    ram[4] = 4;
    assert(evaluate_trigger(trigger, &memory) == RC_TRIGGER_STATE_RESET);
    assert(trigger->state == RC_TRIGGER_STATE_PAUSED);
    assert(!trigger->has_hits);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->current_hits == 0U);

    /* Reset without hitcounts should return current state */
    assert(evaluate_trigger(trigger, &memory) == RC_TRIGGER_STATE_PAUSED);

    /* trigger while paused is ignored */
    ram[4] = 0;
    ram[3] = 171;
    assert(evaluate_trigger(trigger, &memory) == RC_TRIGGER_STATE_PAUSED);

    /* trigger should fire when unpaused */
    ram[2] = 2;
    assert(evaluate_trigger(trigger, &memory) == RC_TRIGGER_STATE_TRIGGERED);

    /* triggered trigger ignores pause */
    ram[2] = 1;
    assert(evaluate_trigger(trigger, &memory) == RC_TRIGGER_STATE_INACTIVE);
    assert(trigger->state == RC_TRIGGER_STATE_TRIGGERED);
  }

  {
    /*------------------------------------------------------------------------
    TestBitLookupsShareMemRef
    Verifies a single memref is used for multiple bit references to the same byte
    ------------------------------------------------------------------------*/

    rc_trigger_t* trigger;

    parse_trigger(&trigger, buffer, "0xM0001=1_0xN0x0001=0_0xO0x0001=1");

    assert(trigger->memrefs->memref.address == 1);
    assert(trigger->memrefs->memref.size == RC_MEMSIZE_8_BITS);
    assert(trigger->memrefs->next == NULL);

    assert(condset_get_cond(trigger_get_set(trigger, 0), 0)->operand1.size == RC_MEMSIZE_BIT_0);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 1)->operand1.size == RC_MEMSIZE_BIT_1);
    assert(condset_get_cond(trigger_get_set(trigger, 0), 2)->operand1.size == RC_MEMSIZE_BIT_2);
  }

  {
    /*------------------------------------------------------------------------
    TestLargeMemRefNotShared
    Verifies a single memref is used for multiple bit references to the same byte
    ------------------------------------------------------------------------*/

    rc_trigger_t* trigger;

    parse_trigger(&trigger, buffer, "0xH1234=1_0xX1234>d0xX1234");

    assert(trigger->memrefs->memref.address == 0x1234);
    assert(trigger->memrefs->memref.size == RC_MEMSIZE_8_BITS);
    assert(trigger->memrefs->next != NULL);
    assert(trigger->memrefs->next->memref.address == 0x1234);
    assert(trigger->memrefs->next->memref.size == RC_MEMSIZE_32_BITS);
    assert(trigger->memrefs->next->next == NULL);
  }
}

static void parse_comp_term(const char* memaddr, char expected_var_size, unsigned expected_address, int is_const) {
  rc_term_t* self;
  rc_parse_state_t parse;
  rc_memref_value_t* memrefs;
  char buffer[512];

  rc_init_parse_state(&parse, buffer, 0, 0);
  rc_init_parse_state_memrefs(&parse, &memrefs);
  self = rc_parse_term(&memaddr, 0, &parse);
  rc_destroy_parse_state(&parse);

  assert(parse.offset >= 0);
  assert(*memaddr == 0);

  if (is_const) {
    assert(self->operand1.type == RC_OPERAND_CONST);
  }
  else {
    assert(self->operand1.size == expected_var_size);
    assert(self->operand1.value.memref->memref.address == expected_address);
  }
  assert(self->invert == 0);
  assert(self->operand2.type == RC_OPERAND_CONST);
  assert(self->operand2.value.num == 1);
}

static void parse_comp_term_fp(const char* memaddr, char expected_var_size, unsigned expected_address, double fp) {
  rc_term_t* self;
  rc_parse_state_t parse;
  rc_memref_value_t* memrefs;
  char buffer[512];

  rc_init_parse_state(&parse, buffer, 0, 0);
  rc_init_parse_state_memrefs(&parse, &memrefs);
  self = rc_parse_term(&memaddr, 0, &parse);
  rc_destroy_parse_state(&parse);

  assert(parse.offset >= 0);
  assert(*memaddr == 0);

  assert(self->operand1.value.memref->memref.size == expected_var_size);
  assert(self->operand1.value.memref->memref.address == expected_address);
  if (self->operand2.type == RC_OPERAND_CONST) {
    assert(self->operand2.type == RC_OPERAND_CONST);
    assert(self->operand2.value.num == (unsigned)fp);
  }
  else {
    assert(self->operand2.type == RC_OPERAND_FP);
    assert(self->operand2.value.dbl == fp);
  }
}

static void parse_comp_term_mem(const char* memaddr, char expected_size_1, unsigned expected_address_1, char expected_size_2, unsigned expected_address_2) {
  rc_term_t* self;
  rc_parse_state_t parse;
  rc_memref_value_t* memrefs;
  char buffer[512];

  rc_init_parse_state(&parse, buffer, 0, 0);
  rc_init_parse_state_memrefs(&parse, &memrefs);
  self = rc_parse_term(&memaddr, 0, &parse);
  rc_destroy_parse_state(&parse);

  assert(parse.offset >= 0);
  assert(*memaddr == 0);

  assert(self->operand1.size == expected_size_1);
  assert(self->operand1.value.memref->memref.address == expected_address_1);
  assert(self->operand2.size == expected_size_2);
  assert(self->operand2.value.memref->memref.address == expected_address_2);
}

static void parse_comp_term_value(const char* memaddr, memory_t* memory, int value) {
  rc_term_t* self;
  rc_parse_state_t parse;
  rc_memref_value_t* memrefs;
  rc_eval_state_t eval_state;
  char buffer[512];

  rc_init_parse_state(&parse, buffer, 0, 0);
  rc_init_parse_state_memrefs(&parse, &memrefs);
  self = rc_parse_term(&memaddr, 0, &parse);
  rc_destroy_parse_state(&parse);

  assert(parse.offset >= 0);
  assert(*memaddr == 0);

  memset(&eval_state, 0, sizeof(eval_state));
  eval_state.peek = peek;
  eval_state.peek_userdata = memory;

  rc_update_memref_values(memrefs, peek, memory);
  assert(rc_evaluate_term(self, &eval_state) == value);
}

static void test_term(void) {
  {
    /*------------------------------------------------------------------------
    TestClauseParseFromString
    ------------------------------------------------------------------------*/

    /* sizes */
    parse_comp_term("0xH1234", RC_MEMSIZE_8_BITS, 0x1234U, 0);
    parse_comp_term("0x 1234", RC_MEMSIZE_16_BITS, 0x1234U, 0);
    parse_comp_term("0x1234", RC_MEMSIZE_16_BITS, 0x1234U, 0);
    parse_comp_term("0xW1234", RC_MEMSIZE_24_BITS, 0x1234U, 0);
    parse_comp_term("0xX1234", RC_MEMSIZE_32_BITS, 0x1234U, 0);
    parse_comp_term("0xL1234", RC_MEMSIZE_LOW, 0x1234U, 0);
    parse_comp_term("0xU1234", RC_MEMSIZE_HIGH, 0x1234U, 0);
    parse_comp_term("0xM1234", RC_MEMSIZE_BIT_0, 0x1234U, 0);
    parse_comp_term("0xN1234", RC_MEMSIZE_BIT_1, 0x1234U, 0);
    parse_comp_term("0xO1234", RC_MEMSIZE_BIT_2, 0x1234U, 0);
    parse_comp_term("0xP1234", RC_MEMSIZE_BIT_3, 0x1234U, 0);
    parse_comp_term("0xQ1234", RC_MEMSIZE_BIT_4, 0x1234U, 0);
    parse_comp_term("0xR1234", RC_MEMSIZE_BIT_5, 0x1234U, 0);
    parse_comp_term("0xS1234", RC_MEMSIZE_BIT_6, 0x1234U, 0);
    parse_comp_term("0xT1234", RC_MEMSIZE_BIT_7, 0x1234U, 0);

    /* BCD */
    parse_comp_term("B0xH1234", RC_MEMSIZE_8_BITS_BCD, 0x1234U, 0);
    parse_comp_term("B0xX1234", RC_MEMSIZE_32_BITS_BCD, 0x1234U, 0);
    parse_comp_term("b0xH1234", RC_MEMSIZE_8_BITS_BCD, 0x1234U, 0);

    /* bit count */
    parse_comp_term("0xC1234", RC_MEMSIZE_8_BITS_BITCOUNT, 0x1234U, 0);

    /* Value */
    parse_comp_term("V1234", 0, 1234, 1);
    parse_comp_term("V+1", 0, 1, 1);
    parse_comp_term("V-1", 0, 0xFFFFFFFFU, 1);
    parse_comp_term("V-2", 0, 0xFFFFFFFEU, 1); /* twos compliment still works for addition */
  }

  {
    /*------------------------------------------------------------------------
    TestClauseParseFromStringMultiply
    ------------------------------------------------------------------------*/

    parse_comp_term_fp("0xH1234", RC_MEMSIZE_8_BITS, 0x1234U, 1.0);
    parse_comp_term_fp("0xH1234*1", RC_MEMSIZE_8_BITS, 0x1234U, 1.0);
    parse_comp_term_fp("0xH1234*3", RC_MEMSIZE_8_BITS, 0x1234U, 3.0);
    parse_comp_term_fp("0xH1234*0.5", RC_MEMSIZE_8_BITS, 0x1234U, 0.5);
    parse_comp_term_fp("0xH1234*.5", RC_MEMSIZE_8_BITS, 0x1234U, 0.5);
    parse_comp_term_fp("0xH1234*-1", RC_MEMSIZE_8_BITS, 0x1234U, -1.0);
  }

  {
    /*------------------------------------------------------------------------
    TestClauseParseFromStringMultiplyAddress
    ------------------------------------------------------------------------*/

    parse_comp_term_mem("0xH1234*0xH3456", RC_MEMSIZE_8_BITS, 0x1234U, RC_MEMSIZE_8_BITS, 0x3456U);
    parse_comp_term_mem("0xH1234*0xL2222", RC_MEMSIZE_8_BITS, 0x1234U, RC_MEMSIZE_LOW, 0x2222U);
    parse_comp_term_mem("0xH1234*0x1111", RC_MEMSIZE_8_BITS, 0x1234U, RC_MEMSIZE_16_BITS, 0x1111U);
  }

  {
    /*------------------------------------------------------------------------
    TestClauseGetValue
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;

    memory.ram = ram;
    memory.size = sizeof(ram);

    /* value */
    parse_comp_term_value("V6", &memory, 6);
    parse_comp_term_value("V6*2", &memory, 12);
    parse_comp_term_value("V6*0.5", &memory, 3);
    parse_comp_term_value("V-6", &memory, -6);
    parse_comp_term_value("V-6*2", &memory, -12);

    /* memory */
    parse_comp_term_value("0xH01", &memory, 0x12);
    parse_comp_term_value("0x0001", &memory, 0x3412);

    /* BCD encoding */
    parse_comp_term_value("B0xH01", &memory, 12);
    parse_comp_term_value("B0x0001", &memory, 3412);

    /* multiplication */
    parse_comp_term_value("0xH01*4", &memory, 0x12 * 4); /* multiply by constant */
    parse_comp_term_value("0xH01*0.5", &memory, 0x12 / 2); /* multiply by fraction */
    parse_comp_term_value("0xH01*0xH02", &memory, 0x12 * 0x34); /* multiply by second address */
    parse_comp_term_value("0xH01*0xT02", &memory, 0); /* multiply by bit */
    parse_comp_term_value("0xH01*~0xT02", &memory, 0x12); /* multiply by inverse bit */
    parse_comp_term_value("0xH01*~0xH02", &memory, 0x12 * (0x34 ^ 0xff)); /* multiply by inverse byte */
  }
}

static void parse_comp_value(const char* memaddr, memory_t* memory, int expected_value) {
  rc_value_t* self;
  char buffer[2048];
  int ret;

  ret = rc_value_size(memaddr);
  assert(ret >= 0);
  memset(buffer, 0xEE, ret + 128);

  self = rc_parse_value(buffer, memaddr, NULL, 0);
  assert(self != NULL);
  assert(*((int*)((char*)buffer + ret)) == 0xEEEEEEEE);

  ret = rc_evaluate_value(self, peek, memory, NULL);
  assert(ret == expected_value);
}

static void test_format_value(int format, int value, const char* expected) {
    char buffer[64];
    int result;

    result = rc_format_value(buffer, sizeof(buffer), value, format);
    assert(!strcmp(expected, buffer));
    assert(result == (int)strlen(expected));
}

static void test_value(void) {
  {
    /*------------------------------------------------------------------------
    TestValueCalculations
    ------------------------------------------------------------------------*/

    unsigned char ram[] = {0x00, 0x12, 0x34, 0xAB, 0x56};
    memory_t memory;
    
    memory.ram = ram;
    memory.size = sizeof(ram);

    /* classic format - supports multipliers, max, inversion */
    parse_comp_value("0xH0001_0xH0002", &memory, 0x12 + 0x34); /* TestAdditionSimple */
    parse_comp_value("0xH0001*100_0xH0002*0.5_0xL0003", &memory, 0x12 * 100 + 0x34 / 2 + 0x0B);/* TestAdditionComplex */
    parse_comp_value("0xH0001$0xH0002", &memory, 0x34);/* TestMaximumSimple */
    parse_comp_value("0xH0001_0xH0004*3$0xH0002*0xL0003", &memory, 0x34 * 0xB);/* TestMaximumComplex */
    parse_comp_value("0xH0001_V-20", &memory, 0x12 - 20);
    parse_comp_value("0xH0001_H10", &memory, 0x12 + 0x10);

    parse_comp_value("0xh0000*-1_99_0xh0001*-100_5900", &memory, 4199);

    /* "Measured" format - supports hit counts, AddSource, SubSource, and AddAddress */
    parse_comp_value("A:0xH0001_M:0xH0002", &memory, 0x12 + 0x34);
    parse_comp_value("I:0xH0000_M:0xH0002", &memory, 0x34);
    parse_comp_value("M:0xH0002!=d0xH0002", &memory, 1); /* delta should initially be 0, so a hit should be tallied */
  }

  {
    /*------------------------------------------------------------------------
    TestFormatValue
    ------------------------------------------------------------------------*/

    test_format_value(RC_FORMAT_VALUE, 12345, "12345");
    test_format_value(RC_FORMAT_VALUE, -12345, "-12345");
    test_format_value(RC_FORMAT_VALUE, 0xFFFFFFFF, "-1");
    test_format_value(RC_FORMAT_SCORE, 12345, "012345");
    test_format_value(RC_FORMAT_SECONDS, 45, "0:45");
    test_format_value(RC_FORMAT_SECONDS, 345, "5:45");
    test_format_value(RC_FORMAT_SECONDS, 12345, "3h25:45");
    test_format_value(RC_FORMAT_CENTISECS, 345, "0:03.45");
    test_format_value(RC_FORMAT_CENTISECS, 12345, "2:03.45");
    test_format_value(RC_FORMAT_CENTISECS, 1234567, "3h25:45.67");
    test_format_value(RC_FORMAT_SECONDS_AS_MINUTES, 45, "0h00");
    test_format_value(RC_FORMAT_SECONDS_AS_MINUTES, 345, "0h05");
    test_format_value(RC_FORMAT_SECONDS_AS_MINUTES, 12345, "3h25");
    test_format_value(RC_FORMAT_MINUTES, 45, "0h45");
    test_format_value(RC_FORMAT_MINUTES, 345, "5h45");
    test_format_value(RC_FORMAT_MINUTES, 12345, "205h45");
    test_format_value(RC_FORMAT_FRAMES, 345, "0:05.75");
    test_format_value(RC_FORMAT_FRAMES, 12345, "3:25.75");
    test_format_value(RC_FORMAT_FRAMES, 1234567, "5h42:56.11");
  }

  {
    /*------------------------------------------------------------------------
    TestParseMemValueFormat
    ------------------------------------------------------------------------*/

    assert(rc_parse_format("VALUE") == RC_FORMAT_VALUE);
    assert(rc_parse_format("SECS") == RC_FORMAT_SECONDS);
    assert(rc_parse_format("TIMESECS") == RC_FORMAT_SECONDS);
    assert(rc_parse_format("TIME") == RC_FORMAT_FRAMES);
    assert(rc_parse_format("MINUTES") == RC_FORMAT_MINUTES);
    assert(rc_parse_format("SECS_AS_MINS") == RC_FORMAT_SECONDS_AS_MINUTES);
    assert(rc_parse_format("FRAMES") == RC_FORMAT_FRAMES);
    assert(rc_parse_format("SCORE") == RC_FORMAT_SCORE);
    assert(rc_parse_format("POINTS") == RC_FORMAT_SCORE);
    assert(rc_parse_format("MILLISECS") == RC_FORMAT_CENTISECS);
    assert(rc_parse_format("OTHER") == RC_FORMAT_SCORE);
    assert(rc_parse_format("INVALID") == RC_FORMAT_VALUE);
  }
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

int main(void) {
  test_memref();
  test_operand();
  test_condition();
  test_trigger();
  test_term();
  test_value();
  test_lboard();
  test_richpresence();
  test_runtime();
  test_lua();

  return 0;
}
