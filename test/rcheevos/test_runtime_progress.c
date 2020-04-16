#include "internal.h"

#include "../test_framework.h"
#include "../rhash/md5.h"
#include "mock_memory.h"

static void assert_activate_achievement(rc_runtime_t* runtime, unsigned int id, const char* memaddr)
{
  int result = rc_runtime_activate_achievement(runtime, id, memaddr, NULL, 0);
  ASSERT_NUM_EQUALS(result, RC_OK);
}

static void event_handler(const rc_runtime_event_t* e)
{
}

static void assert_do_frame(rc_runtime_t* runtime, memory_t* memory)
{
  rc_runtime_do_frame(runtime, event_handler, peek, memory, NULL);
}

static void assert_serialize(rc_runtime_t* runtime, unsigned char* buffer, unsigned buffer_size)
{
  int result;
  unsigned* overflow;

  unsigned size = rc_runtime_progress_size(runtime, NULL);
  ASSERT_NUM_LESS(size, buffer_size);

  overflow = (unsigned*)(((char*)buffer) + size);
  *overflow = 0xCDCDCDCD;

  result = rc_runtime_serialize_progress(buffer, runtime, NULL);
  ASSERT_NUM_EQUALS(result, RC_OK);

  if (*overflow != 0xCDCDCDCD) {
    ASSERT_FAIL("write past end of buffer");
  }
}

static void assert_deserialize(rc_runtime_t* runtime, unsigned char* buffer)
{
  int result = rc_runtime_deserialize_progress(runtime, buffer, NULL);
  ASSERT_NUM_EQUALS(result, RC_OK);
}

static void assert_memref(rc_runtime_t* runtime, unsigned address, unsigned value, unsigned prev, unsigned prior)
{
  rc_memref_value_t* memref = runtime->memrefs;
  while (memref)
  {
    if (memref->memref.address == address)
    {
      ASSERT_NUM_EQUALS(memref->value, value);
      ASSERT_NUM_EQUALS(memref->previous, prev);
      ASSERT_NUM_EQUALS(memref->prior, prior);
      return;
    }

    memref = memref->next;
  }

  ASSERT_FAIL("could not find memref for address %u", address);
}

static rc_condset_t* find_condset(rc_runtime_t* runtime, unsigned ach_id, unsigned group_idx)
{
  unsigned i;
  for (i = 0; i < runtime->trigger_count; ++i)
  {
    if (runtime->triggers[i].id == ach_id)
    {
      rc_trigger_t* trigger = runtime->triggers[i].trigger;
      if (trigger)
      {
        rc_condset_t* condset = trigger->requirement;
        if (group_idx > 0)
        {
          condset = trigger->alternative;
          while (condset && --group_idx != 0)
            condset = condset->next;
        }

        return condset;
      }
    }
  }

  return NULL;
}

static void assert_hitcount(rc_runtime_t* runtime, unsigned ach_id, unsigned group_idx, unsigned cond_idx, unsigned expected_hits)
{
  rc_condition_t* cond;

  rc_condset_t* condset = find_condset(runtime, ach_id, group_idx);
  ASSERT_PTR_NOT_NULL(condset);

  cond = condset->conditions;
  while (cond && cond_idx > 0)
  {
    --cond_idx;
    cond = cond->next;
  }
  ASSERT_PTR_NOT_NULL(cond);

  ASSERT_NUM_EQUALS(cond->current_hits, expected_hits);
}

static void update_md5(unsigned char* buffer)
{
  md5_state_t state;

  unsigned char* ptr = buffer;
  while (ptr[0] != 'D' || ptr[1] != 'O' || ptr[2] != 'N' || ptr[3] != 'E')
    ++ptr;

  ptr += 8;

  md5_init(&state);
  md5_append(&state, buffer, ptr - buffer);
  md5_finish(&state, ptr);
}

static void reset_runtime(rc_runtime_t* runtime)
{
  rc_memref_value_t* memref;
  rc_trigger_t* trigger;
  rc_condition_t* cond;
  rc_condset_t* condset;
  unsigned i;

  memref = runtime->memrefs;
  while (memref)
  {
    memref->value = 0xFF;
    memref->previous = 0xFF;
    memref->prior = 0xFF;

    memref = memref->next;
  }

  for (i = 0; i < runtime->trigger_count; ++i)
  {
    trigger = runtime->triggers[i].trigger;
    if (trigger)
    {
      trigger->measured_value = 0xFF;
      trigger->measured_target = 0xFF;

      if (trigger->requirement)
      {
        cond = trigger->requirement->conditions;
        while (cond)
        {
          cond->current_hits = 0xFF;
          cond = cond->next;
        }
      }

      condset = trigger->alternative;
      while (condset)
      {
        cond = condset->conditions;
        while (cond)
        {
          cond->current_hits = 0xFF;
          cond = cond->next;
        }

        condset = condset->next;
      }
    }
  }
}

static void test_empty()
{
  unsigned char ram[] = { 2, 3, 6 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);

  ASSERT_PTR_NULL(runtime.memrefs);
  ASSERT_NUM_EQUALS(runtime.trigger_count, 0);
  ASSERT_NUM_EQUALS(runtime.lboard_count, 0);

  rc_runtime_destroy(&runtime);
}

static void test_single_achievement()
{
  unsigned char ram[] = { 2, 3, 6 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "0xH0001=4_0xH0002=5");
  assert_do_frame(&runtime, &memory);
  ram[1] = 4;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  ram[1] = 5;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);

  assert_memref(&runtime, 1, 5, 5, 4);
  assert_memref(&runtime, 2, 6, 6, 0);
  assert_hitcount(&runtime, 1, 0, 0, 3);
  assert_hitcount(&runtime, 1, 0, 1, 0);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);

  assert_memref(&runtime, 1, 5, 5, 4);
  assert_memref(&runtime, 2, 6, 6, 0);
  assert_hitcount(&runtime, 1, 0, 0, 3);
  assert_hitcount(&runtime, 1, 0, 1, 0);

  rc_runtime_destroy(&runtime);
}

static void test_invalid_marker()
{
  unsigned char ram[] = { 2, 3, 6 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "0xH0001=4_0xH0002=5");
  assert_do_frame(&runtime, &memory);
  ram[1] = 4;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  ram[1] = 5;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);

  assert_memref(&runtime, 1, 5, 5, 4);
  assert_memref(&runtime, 2, 6, 6, 0);
  assert_hitcount(&runtime, 1, 0, 0, 3);
  assert_hitcount(&runtime, 1, 0, 1, 0);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  /* invalid header prevents anything from being deserialized */
  buffer[0] = 0x40;
  update_md5(buffer);

  reset_runtime(&runtime);
  ASSERT_NUM_EQUALS(rc_runtime_deserialize_progress(&runtime, buffer, NULL), RC_INVALID_STATE);

  assert_memref(&runtime, 1, 0xFF, 0xFF, 0xFF);
  assert_memref(&runtime, 2, 0xFF, 0xFF, 0xFF);
  assert_hitcount(&runtime, 1, 0, 0, 0);
  assert_hitcount(&runtime, 1, 0, 1, 0);

  rc_runtime_destroy(&runtime);
}

static void test_invalid_memref_chunk_id()
{
  unsigned char ram[] = { 2, 3, 6 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "0xH0001=4_0xH0002=5");
  assert_do_frame(&runtime, &memory);
  ram[1] = 4;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  ram[1] = 5;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);

  assert_memref(&runtime, 1, 5, 5, 4);
  assert_memref(&runtime, 2, 6, 6, 0);
  assert_hitcount(&runtime, 1, 0, 0, 3);
  assert_hitcount(&runtime, 1, 0, 1, 0);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  /* invalid chunk is ignored, achievement hits will still be read */
  buffer[5] = 0x40;
  update_md5(buffer);

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);

  assert_memref(&runtime, 1, 0xFF, 0xFF, 0xFF);
  assert_memref(&runtime, 2, 0xFF, 0xFF, 0xFF);
  assert_hitcount(&runtime, 1, 0, 0, 3);
  assert_hitcount(&runtime, 1, 0, 1, 0);

  rc_runtime_destroy(&runtime);
}

static void test_modified_data()
{
  unsigned char ram[] = { 2, 3, 6 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "0xH0001=4_0xH0002=5");
  assert_do_frame(&runtime, &memory);
  ram[1] = 4;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  ram[1] = 5;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);

  assert_memref(&runtime, 1, 5, 5, 4);
  assert_memref(&runtime, 2, 6, 6, 0);
  assert_hitcount(&runtime, 1, 0, 0, 3);
  assert_hitcount(&runtime, 1, 0, 1, 0);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  /* this changes the current hits for the test condition to 6, but doesn't update the checksum, so should be ignored */
  ASSERT_NUM_EQUALS(buffer[84], 3);
  buffer[84] = 6;

  reset_runtime(&runtime);
  ASSERT_NUM_EQUALS(rc_runtime_deserialize_progress(&runtime, buffer, NULL), RC_INVALID_STATE);

  /* memrefs will have been processed and cannot be "reset" */
  assert_memref(&runtime, 1, 5, 5, 4);
  assert_memref(&runtime, 2, 6, 6, 0);

  /* deserialization failure causes all hits to be reset */
  assert_hitcount(&runtime, 1, 0, 0, 0);
  assert_hitcount(&runtime, 1, 0, 1, 0);

  rc_runtime_destroy(&runtime);
}

static void test_single_achievement_md5_changed()
{
  unsigned char ram[] = { 2, 3, 6 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "0xH0001=4_0xH0002=5");
  assert_do_frame(&runtime, &memory);
  ram[1] = 4;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  ram[1] = 5;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);

  assert_memref(&runtime, 1, 5, 5, 4);
  assert_memref(&runtime, 2, 6, 6, 0);
  assert_hitcount(&runtime, 1, 0, 0, 3);
  assert_hitcount(&runtime, 1, 0, 1, 0);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  reset_runtime(&runtime);

  /* new achievement definition - rack up a couple hits */
  assert_activate_achievement(&runtime, 1, "0xH0001=4_0xH0002=5.1.");
  ram[1] = 3;
  assert_do_frame(&runtime, &memory);
  ram[1] = 4;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  assert_hitcount(&runtime, 1, 0, 0, 2);
  assert_hitcount(&runtime, 1, 0, 1, 0);
  assert_memref(&runtime, 1, 4, 4, 3);

  assert_deserialize(&runtime, buffer);

  /* memrefs should be restored */
  assert_memref(&runtime, 1, 5, 5, 4);
  assert_memref(&runtime, 2, 6, 6, 0);

  /* achievement definition changed, achievement should be reset */
  assert_hitcount(&runtime, 1, 0, 0, 0);
  assert_hitcount(&runtime, 1, 0, 1, 0);

  rc_runtime_destroy(&runtime);
}

/* ======================================================== */

void test_runtime_progress(void) {
  TEST_SUITE_BEGIN();

  TEST(test_empty);
  TEST(test_single_achievement);
  TEST(test_invalid_marker);
  TEST(test_invalid_memref_chunk_id);
  TEST(test_modified_data);

  TEST(test_single_achievement_md5_changed);

  TEST_SUITE_END();
}
