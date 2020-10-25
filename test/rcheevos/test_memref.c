#include "internal.h"

#include "../test_framework.h"
#include "mock_memory.h"

static int get_memref_count(rc_parse_state_t* parse) {
  int count = 0;
  rc_memref_value_t *memref = *parse->first_memref;
  while (memref) {
    ++count;
    memref = memref->next;
  }

  return count;
}

static void test_allocate_shared_address() {
  rc_parse_state_t parse;
  rc_memref_value_t* memrefs;
  rc_init_parse_state(&parse, NULL, 0, 0);
  rc_init_parse_state_memrefs(&parse, &memrefs);

  rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_8_BITS, 0);
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 1);

  rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_16_BITS, 0); /* differing size will not match */
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 2);

  rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_LOW, 0); /* differing size will not match */
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 3);

  rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_BIT_2, 0); /* differing size will not match */
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 4);

  rc_alloc_memref_value(&parse, 2, RC_MEMSIZE_8_BITS, 0); /* differing address will not match */
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 5);

  rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_8_BITS, 0); /* match */
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 5);

  rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_16_BITS, 0); /* match */
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 5);

  rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_BIT_2, 0); /* match */
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 5);

  rc_alloc_memref_value(&parse, 2, RC_MEMSIZE_8_BITS, 0); /* match */
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 5);

  rc_destroy_parse_state(&parse);
}

static void test_allocate_shared_address2() {
  rc_parse_state_t parse;
  rc_memref_value_t* memrefs;
  rc_memref_value_t* memref1;
  rc_memref_value_t* memref2;
  rc_memref_value_t* memref3;
  rc_memref_value_t* memref4;
  rc_memref_value_t* memref5;
  rc_memref_value_t* memrefX;
  rc_init_parse_state(&parse, NULL, 0, 0);
  rc_init_parse_state_memrefs(&parse, &memrefs);

  memref1 = rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_8_BITS, 0);
  ASSERT_NUM_EQUALS(memref1->memref.address, 1);
  ASSERT_NUM_EQUALS(memref1->memref.size, RC_MEMSIZE_8_BITS);
  ASSERT_NUM_EQUALS(memref1->memref.is_indirect, 0);
  ASSERT_NUM_EQUALS(memref1->value, 0);
  ASSERT_NUM_EQUALS(memref1->previous, 0);
  ASSERT_NUM_EQUALS(memref1->prior, 0);
  ASSERT_PTR_EQUALS(memref1->next, 0);

  memref2 = rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_16_BITS, 0); /* differing size will not match */
  memref3 = rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_LOW, 0); /* differing size will not match */
  memref4 = rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_BIT_2, 0); /* differing size will not match */
  memref5 = rc_alloc_memref_value(&parse, 2, RC_MEMSIZE_8_BITS, 0); /* differing address will not match */

  memrefX = rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_8_BITS, 0); /* match */
  ASSERT_PTR_EQUALS(memrefX, memref1);

  memrefX = rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_16_BITS, 0); /* match */
  ASSERT_PTR_EQUALS(memrefX, memref2);

  memrefX = rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_LOW, 0); /* match */
  ASSERT_PTR_EQUALS(memrefX, memref3);

  memrefX = rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_BIT_2, 0); /* match */
  ASSERT_PTR_EQUALS(memrefX, memref4);

  memrefX = rc_alloc_memref_value(&parse, 2, RC_MEMSIZE_8_BITS, 0); /* match */
  ASSERT_PTR_EQUALS(memrefX, memref5);

  rc_destroy_parse_state(&parse);
}

static void test_sizing_mode_grow_buffer() {
  int i;
  rc_parse_state_t parse;
  rc_memref_value_t* memrefs;
  rc_init_parse_state(&parse, NULL, 0, 0);
  rc_init_parse_state_memrefs(&parse, &memrefs);

  /* memrefs are allocated 16 at a time */
  for (i = 0; i < 100; i++) {
      rc_alloc_memref_value(&parse, i, RC_MEMSIZE_8_BITS, 0);
  }
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 100);

  /* 100 have been allocated, make sure we can still access items at various addresses without allocating more */
  rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_8_BITS, 0);
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 100);

  rc_alloc_memref_value(&parse, 25, RC_MEMSIZE_8_BITS, 0);
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 100);

  rc_alloc_memref_value(&parse, 50, RC_MEMSIZE_8_BITS, 0);
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 100);

  rc_alloc_memref_value(&parse, 75, RC_MEMSIZE_8_BITS, 0);
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 100);

  rc_alloc_memref_value(&parse, 99, RC_MEMSIZE_8_BITS, 0);
  ASSERT_NUM_EQUALS(get_memref_count(&parse), 100);

  rc_destroy_parse_state(&parse);
}

static void test_update_memref_values() {
  rc_parse_state_t parse;
  rc_memref_value_t* memrefs;
  rc_memref_value_t* memref1;
  rc_memref_value_t* memref2;

  unsigned char ram[] = { 0x00, 0x12, 0x34, 0xAB, 0x56 };
  memory_t memory;
  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_init_parse_state(&parse, NULL, 0, 0);
  rc_init_parse_state_memrefs(&parse, &memrefs);

  memref1 = rc_alloc_memref_value(&parse, 1, RC_MEMSIZE_8_BITS, 0);
  memref2 = rc_alloc_memref_value(&parse, 2, RC_MEMSIZE_8_BITS, 0);

  rc_update_memref_values(memrefs, peek, &memory);

  ASSERT_NUM_EQUALS(memref1->value, 0x12);
  ASSERT_NUM_EQUALS(memref1->previous, 0);
  ASSERT_NUM_EQUALS(memref1->prior, 0);
  ASSERT_NUM_EQUALS(memref2->value, 0x34);
  ASSERT_NUM_EQUALS(memref2->previous, 0);
  ASSERT_NUM_EQUALS(memref2->prior, 0);

  ram[1] = 3;
  rc_update_memref_values(memrefs, peek, &memory);

  ASSERT_NUM_EQUALS(memref1->value, 3);
  ASSERT_NUM_EQUALS(memref1->previous, 0x12);
  ASSERT_NUM_EQUALS(memref1->prior, 0x12);
  ASSERT_NUM_EQUALS(memref2->value, 0x34);
  ASSERT_NUM_EQUALS(memref2->previous, 0x34);
  ASSERT_NUM_EQUALS(memref2->prior, 0);

  ram[1] = 5;
  rc_update_memref_values(memrefs, peek, &memory);

  ASSERT_NUM_EQUALS(memref1->value, 5);
  ASSERT_NUM_EQUALS(memref1->previous, 3);
  ASSERT_NUM_EQUALS(memref1->prior, 3);
  ASSERT_NUM_EQUALS(memref2->value, 0x34);
  ASSERT_NUM_EQUALS(memref2->previous, 0x34);
  ASSERT_NUM_EQUALS(memref2->prior, 0);

  ram[2] = 7;
  rc_update_memref_values(memrefs, peek, &memory);

  ASSERT_NUM_EQUALS(memref1->value, 5);
  ASSERT_NUM_EQUALS(memref1->previous, 5);
  ASSERT_NUM_EQUALS(memref1->prior, 3);
  ASSERT_NUM_EQUALS(memref2->value, 7);
  ASSERT_NUM_EQUALS(memref2->previous, 0x34);
  ASSERT_NUM_EQUALS(memref2->prior, 0x34);

  rc_destroy_parse_state(&parse);
}

void test_memref(void) {
  TEST_SUITE_BEGIN();

  TEST(test_allocate_shared_address);
  TEST(test_allocate_shared_address2);
  TEST(test_sizing_mode_grow_buffer);
  TEST(test_update_memref_values);

  TEST_SUITE_END();
}
