#include "internal.h"

#include "../test_framework.h"
#include "../rhash/md5.h"
#include "mock_memory.h"

static void _assert_activate_achievement(rc_runtime_t* runtime, unsigned int id, const char* memaddr)
{
  int result = rc_runtime_activate_achievement(runtime, id, memaddr, NULL, 0);
  ASSERT_NUM_EQUALS(result, RC_OK);
}
#define assert_activate_achievement(runtime, id, memaddr) ASSERT_HELPER(_assert_activate_achievement(runtime, id, memaddr), "assert_activate_achievement")

static void event_handler(const rc_runtime_event_t* e)
{
}

static void assert_do_frame(rc_runtime_t* runtime, memory_t* memory)
{
  rc_runtime_do_frame(runtime, event_handler, peek, memory, NULL);
}

static void _assert_serialize(rc_runtime_t* runtime, unsigned char* buffer, unsigned buffer_size)
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
#define assert_serialize(runtime, buffer, buffer_size) ASSERT_HELPER(_assert_serialize(runtime, buffer, buffer_size), "assert_serialize")

static void _assert_deserialize(rc_runtime_t* runtime, unsigned char* buffer)
{
  int result = rc_runtime_deserialize_progress(runtime, buffer, NULL);
  ASSERT_NUM_EQUALS(result, RC_OK);
}
#define assert_deserialize(runtime, buffer) ASSERT_HELPER(_assert_deserialize(runtime, buffer), "assert_deserialize")

static void _assert_sized_memref(rc_runtime_t* runtime, unsigned address, char size, unsigned value, unsigned prev, unsigned prior)
{
  rc_memref_value_t* memref = runtime->memrefs;
  while (memref)
  {
    if (memref->memref.address == address && memref->memref.size == size)
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
#define assert_sized_memref(runtime, address, size, value, prev, prior) ASSERT_HELPER(_assert_sized_memref(runtime, address, size, value, prev, prior), "assert_sized_memref")
#define assert_memref(runtime, address, value, prev, prior) ASSERT_HELPER(_assert_sized_memref(runtime, address, RC_MEMSIZE_8_BITS, value, prev, prior), "assert_memref")

static rc_trigger_t* find_trigger(rc_runtime_t* runtime, unsigned ach_id)
{
  unsigned i;
  for (i = 0; i < runtime->trigger_count; ++i) {
    if (runtime->triggers[i].id == ach_id && runtime->triggers[i].trigger)
      return runtime->triggers[i].trigger;
  }

  return NULL;
}

static rc_condset_t* find_condset(rc_runtime_t* runtime, unsigned ach_id, unsigned group_idx)
{
  rc_trigger_t* trigger = find_trigger(runtime, ach_id);
  if (!trigger) {
    ASSERT_MESSAGE("could not find trigger for achievement %u", ach_id);
    return NULL;
  }

  rc_condset_t* condset = trigger->requirement;
  if (group_idx > 0) {
    condset = trigger->alternative;
    while (condset && --group_idx != 0)
      condset = condset->next;
  }

  return condset;
}

static void _assert_hitcount(rc_runtime_t* runtime, unsigned ach_id, unsigned group_idx, unsigned cond_idx, unsigned expected_hits)
{
  rc_condition_t* cond;

  rc_condset_t* condset = find_condset(runtime, ach_id, group_idx);
  ASSERT_PTR_NOT_NULL(condset);

  cond = condset->conditions;
  while (cond && cond_idx > 0) {
    --cond_idx;
    cond = cond->next;
  }
  ASSERT_PTR_NOT_NULL(cond);

  ASSERT_NUM_EQUALS(cond->current_hits, expected_hits);
}
#define assert_hitcount(runtime, ach_id, group_idx, cond_idx, expected_hits) ASSERT_HELPER(_assert_hitcount(runtime, ach_id, group_idx, cond_idx, expected_hits), "assert_hitcount")

static void _assert_achievement_state(rc_runtime_t* runtime, unsigned ach_id, int state)
{
  rc_trigger_t* trigger = find_trigger(runtime, ach_id);
  ASSERT_PTR_NOT_NULL(trigger);

  ASSERT_NUM_EQUALS(trigger->state, state);
}
#define assert_achievement_state(runtime, ach_id, state) ASSERT_HELPER(_assert_achievement_state(runtime, ach_id, state), "assert_achievement_state")

static void update_md5(unsigned char* buffer)
{
  md5_state_t state;

  unsigned char* ptr = buffer;
  while (ptr[0] != 'D' || ptr[1] != 'O' || ptr[2] != 'N' || ptr[3] != 'E')
    ++ptr;

  ptr += 8;

  md5_init(&state);
  md5_append(&state, buffer, (int)(ptr - buffer));
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
  unsigned char buffer[2048];
  rc_runtime_t runtime;

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

static void test_single_achievement_deactivated()
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

  /* disabled achievement */
  rc_runtime_deactivate_achievement(&runtime, 1);
  assert_deserialize(&runtime, buffer);

  assert_memref(&runtime, 1, 5, 5, 4);
  assert_memref(&runtime, 2, 6, 6, 0);

  /* reactivate */
  assert_activate_achievement(&runtime, 1, "0xH0001=4_0xH0002=5");
  assert_achievement_state(&runtime, 1, RC_TRIGGER_STATE_WAITING);
  assert_hitcount(&runtime, 1, 0, 0, 0);
  assert_hitcount(&runtime, 1, 0, 1, 0);

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);
  assert_memref(&runtime, 1, 5, 5, 4);
  assert_memref(&runtime, 2, 6, 6, 0);
  assert_hitcount(&runtime, 1, 0, 0, 3);
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

static void setup_multiple_achievements(rc_runtime_t* runtime, memory_t* memory)
{
  rc_runtime_init(runtime);

  assert_activate_achievement(runtime, 1, "0xH0001=4_0xH0000=1");
  assert_activate_achievement(runtime, 2, "0xH0002=7_0xH0000=2");
  assert_activate_achievement(runtime, 3, "0xH0003=9_0xH0000=3");
  assert_activate_achievement(runtime, 4, "0xH0004=1_0xH0000=4");
  assert_do_frame(runtime, memory);
  memory->ram[1] = 4;
  assert_do_frame(runtime, memory);
  memory->ram[2] = 7;
  assert_do_frame(runtime, memory);
  memory->ram[3] = 9;
  assert_do_frame(runtime, memory);
  memory->ram[4] = 1;
  assert_do_frame(runtime, memory);

  assert_memref(runtime, 0, 0, 0, 0);
  assert_memref(runtime, 1, 4, 4, 1);
  assert_memref(runtime, 2, 7, 7, 2);
  assert_memref(runtime, 3, 9, 9, 3);
  assert_memref(runtime, 4, 1, 4, 4);
  assert_hitcount(runtime, 1, 0, 0, 4);
  assert_hitcount(runtime, 2, 0, 0, 3);
  assert_hitcount(runtime, 3, 0, 0, 2);
  assert_hitcount(runtime, 4, 0, 0, 1);
}

static void test_no_core_group()
{
  unsigned char ram[] = { 2, 3, 6 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "S0xH0001=4_0xH0002=5");
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
  assert_hitcount(&runtime, 1, 1, 0, 3);
  assert_hitcount(&runtime, 1, 1, 1, 0);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);

  assert_memref(&runtime, 1, 5, 5, 4);
  assert_memref(&runtime, 2, 6, 6, 0);
  assert_hitcount(&runtime, 1, 1, 0, 3);
  assert_hitcount(&runtime, 1, 1, 1, 0);

  rc_runtime_destroy(&runtime);
}

static void test_memref_shared_address()
{
  unsigned char ram[] = { 2, 3, 0, 0, 0 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);

  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "0xH0001=4_0x 0001=5_0xX0001=6");
  assert_do_frame(&runtime, &memory);
  ram[1] = 4;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  ram[1] = 5;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  ram[1] = 6;
  assert_do_frame(&runtime, &memory);

  assert_sized_memref(&runtime, 1, RC_MEMSIZE_8_BITS, 6, 5, 5);
  assert_sized_memref(&runtime, 1, RC_MEMSIZE_16_BITS, 6, 5, 5);
  assert_sized_memref(&runtime, 1, RC_MEMSIZE_32_BITS, 6, 5, 5);
  assert_hitcount(&runtime, 1, 0, 0, 3);
  assert_hitcount(&runtime, 1, 0, 1, 2);
  assert_hitcount(&runtime, 1, 0, 2, 1);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);

  assert_sized_memref(&runtime, 1, RC_MEMSIZE_8_BITS, 6, 5, 5);
  assert_sized_memref(&runtime, 1, RC_MEMSIZE_16_BITS, 6, 5, 5);
  assert_sized_memref(&runtime, 1, RC_MEMSIZE_32_BITS, 6, 5, 5);
  assert_hitcount(&runtime, 1, 0, 0, 3);
  assert_hitcount(&runtime, 1, 0, 1, 2);
  assert_hitcount(&runtime, 1, 0, 2, 1);

  rc_runtime_destroy(&runtime);
}

static void test_multiple_achievements()
{
  unsigned char ram[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);
  setup_multiple_achievements(&runtime, &memory);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);

  assert_memref(&runtime, 0, 0, 0, 0);
  assert_memref(&runtime, 1, 4, 4, 1);
  assert_memref(&runtime, 2, 7, 7, 2);
  assert_memref(&runtime, 3, 9, 9, 3);
  assert_memref(&runtime, 4, 1, 4, 4);
  assert_hitcount(&runtime, 1, 0, 0, 4);
  assert_hitcount(&runtime, 1, 0, 1, 0);
  assert_hitcount(&runtime, 2, 0, 0, 3);
  assert_hitcount(&runtime, 2, 0, 1, 0);
  assert_hitcount(&runtime, 3, 0, 0, 2);
  assert_hitcount(&runtime, 3, 0, 1, 0);
  assert_hitcount(&runtime, 4, 0, 0, 1);
  assert_hitcount(&runtime, 4, 0, 1, 0);

  rc_runtime_destroy(&runtime);
}

static void test_multiple_achievements_ignore_triggered_and_inactive()
{
  unsigned char ram[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);
  setup_multiple_achievements(&runtime, &memory);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  /* trigger achievement 3 */
  ram[0] = 3;
  assert_do_frame(&runtime, &memory);
  assert_achievement_state(&runtime, 3, RC_TRIGGER_STATE_TRIGGERED);

  /* reset achievement 2 to inactive */
  find_trigger(&runtime, 2)->state = RC_TRIGGER_STATE_INACTIVE;

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);

  assert_memref(&runtime, 0, 0, 0, 0);
  assert_memref(&runtime, 1, 4, 4, 1);
  assert_memref(&runtime, 2, 7, 7, 2);
  assert_memref(&runtime, 3, 9, 9, 3);
  assert_memref(&runtime, 4, 1, 4, 4);
  assert_achievement_state(&runtime, 1, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 2, RC_TRIGGER_STATE_INACTIVE);
  assert_achievement_state(&runtime, 3, RC_TRIGGER_STATE_TRIGGERED);
  assert_achievement_state(&runtime, 4, RC_TRIGGER_STATE_ACTIVE);
  assert_hitcount(&runtime, 1, 0, 0, 4);
  assert_hitcount(&runtime, 1, 0, 1, 0);
  assert_hitcount(&runtime, 2, 0, 0, 0xFF); /* inactive achievement should be ignored */
  assert_hitcount(&runtime, 2, 0, 1, 0xFF);
  assert_hitcount(&runtime, 3, 0, 0, 0xFF); /* triggered achievement should be ignored */
  assert_hitcount(&runtime, 3, 0, 1, 0xFF);
  assert_hitcount(&runtime, 4, 0, 0, 1);
  assert_hitcount(&runtime, 4, 0, 1, 0);

  rc_runtime_destroy(&runtime);
}

static void test_multiple_achievements_overwrite_waiting()
{
  unsigned char ram[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);
  setup_multiple_achievements(&runtime, &memory);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  /* reset achievement 2 to waiting */
  rc_reset_trigger(find_trigger(&runtime, 2));
  assert_achievement_state(&runtime, 2, RC_TRIGGER_STATE_WAITING);

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);

  assert_achievement_state(&runtime, 1, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 2, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 3, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 4, RC_TRIGGER_STATE_ACTIVE);
  assert_hitcount(&runtime, 1, 0, 0, 4);
  assert_hitcount(&runtime, 1, 0, 1, 0);
  assert_hitcount(&runtime, 2, 0, 0, 3); /* waiting achievement should be set back to active */
  assert_hitcount(&runtime, 2, 0, 1, 0);
  assert_hitcount(&runtime, 3, 0, 0, 2);
  assert_hitcount(&runtime, 3, 0, 1, 0);
  assert_hitcount(&runtime, 4, 0, 0, 1);
  assert_hitcount(&runtime, 4, 0, 1, 0);

  rc_runtime_destroy(&runtime);
}

static void test_multiple_achievements_reactivate_waiting()
{
  unsigned char ram[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);
  setup_multiple_achievements(&runtime, &memory);

  /* reset achievement 2 to waiting */
  rc_reset_trigger(find_trigger(&runtime, 2));
  assert_achievement_state(&runtime, 2, RC_TRIGGER_STATE_WAITING);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  /* reactivate achievement 2 */
  assert_do_frame(&runtime, &memory);
  assert_achievement_state(&runtime, 2, RC_TRIGGER_STATE_ACTIVE);

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);

  assert_achievement_state(&runtime, 1, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 2, RC_TRIGGER_STATE_WAITING);
  assert_achievement_state(&runtime, 3, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 4, RC_TRIGGER_STATE_ACTIVE);
  assert_hitcount(&runtime, 1, 0, 0, 4);
  assert_hitcount(&runtime, 1, 0, 1, 0);
  assert_hitcount(&runtime, 2, 0, 0, 0); /* active achievement should be set back to waiting */
  assert_hitcount(&runtime, 2, 0, 1, 0);
  assert_hitcount(&runtime, 3, 0, 0, 2);
  assert_hitcount(&runtime, 3, 0, 1, 0);
  assert_hitcount(&runtime, 4, 0, 0, 1);
  assert_hitcount(&runtime, 4, 0, 1, 0);

  rc_runtime_destroy(&runtime);
}

static void test_multiple_achievements_paused_and_primed()
{
  unsigned char ram[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
  unsigned char buffer[2048];
  unsigned char buffer2[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);
  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "0xH0001=4_0xH0000=1");
  assert_activate_achievement(&runtime, 2, "0xH0002=7_0xH0000=2_P:0xH0005=4");
  assert_activate_achievement(&runtime, 3, "0xH0003=9_0xH0000=3");
  assert_activate_achievement(&runtime, 4, "0xH0004=1_T:0xH0000=4");

  assert_do_frame(&runtime, &memory);
  ram[1] = 4;
  ram[2] = 7;
  ram[3] = 9;
  ram[4] = 1;
  ram[5] = 4;
  assert_do_frame(&runtime, &memory);
  assert_achievement_state(&runtime, 1, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 2, RC_TRIGGER_STATE_PAUSED);
  assert_achievement_state(&runtime, 3, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 4, RC_TRIGGER_STATE_PRIMED);
  ASSERT_TRUE(find_condset(&runtime, 2, 0)->is_paused);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  /* unpause achievement 2 and unprime achievement 4 */
  ram[5] = 2;
  ram[4] = 2;
  assert_do_frame(&runtime, &memory);
  assert_achievement_state(&runtime, 1, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 2, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 3, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 4, RC_TRIGGER_STATE_ACTIVE);
  ASSERT_FALSE(find_condset(&runtime, 2, 0)->is_paused);

  assert_serialize(&runtime, buffer2, sizeof(buffer2));

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);

  assert_achievement_state(&runtime, 1, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 2, RC_TRIGGER_STATE_PAUSED);
  assert_achievement_state(&runtime, 3, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 4, RC_TRIGGER_STATE_PRIMED);
  ASSERT_TRUE(find_condset(&runtime, 2, 0)->is_paused);

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer2);

  assert_achievement_state(&runtime, 1, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 2, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 3, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 4, RC_TRIGGER_STATE_ACTIVE);
  ASSERT_FALSE(find_condset(&runtime, 2, 0)->is_paused);

  rc_runtime_destroy(&runtime);
}

static void test_multiple_achievements_deactivated_memrefs()
{
  unsigned char ram[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);
  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "0xH0001=4_0xH0000=1");
  assert_activate_achievement(&runtime, 2, "0xH0001=5_0xH0000=2");
  assert_activate_achievement(&runtime, 3, "0xH0001=6_0xH0000=3");

  ram[1] = 4;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  ram[1] = 5;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  ram[1] = 6;
  assert_do_frame(&runtime, &memory);

  assert_hitcount(&runtime, 1, 0, 0, 3);
  assert_hitcount(&runtime, 2, 0, 0, 2);
  assert_hitcount(&runtime, 3, 0, 0, 1);

  /* deactivate an achievement with memrefs - trigger should be nulled */
  ASSERT_NUM_EQUALS(runtime.trigger_count, 3);
  ASSERT_TRUE(runtime.triggers[0].owns_memrefs);
  rc_runtime_deactivate_achievement(&runtime, 1);
  ASSERT_NUM_EQUALS(runtime.trigger_count, 3);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  /* reactivate achievement 1 */
  assert_activate_achievement(&runtime, 1, "0xH0001=4_0xH0000=2");

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);

  assert_achievement_state(&runtime, 1, RC_TRIGGER_STATE_WAITING);
  assert_achievement_state(&runtime, 2, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 3, RC_TRIGGER_STATE_ACTIVE);
  assert_hitcount(&runtime, 1, 0, 0, 0);
  assert_hitcount(&runtime, 2, 0, 0, 2);
  assert_hitcount(&runtime, 3, 0, 0, 1);

  rc_runtime_destroy(&runtime);
}

static void test_multiple_achievements_deactivated_no_memrefs()
{
  unsigned char ram[] = { 0, 1, 2, 3, 4, 5, 6, 7 };
  unsigned char buffer[2048];
  memory_t memory;
  rc_runtime_t runtime;

  memory.ram = ram;
  memory.size = sizeof(ram);
  rc_runtime_init(&runtime);

  assert_activate_achievement(&runtime, 1, "0xH0001=4_0xH0000=1");
  assert_activate_achievement(&runtime, 2, "0xH0001=5_0xH0000=2");
  assert_activate_achievement(&runtime, 3, "0xH0001=6_0xH0000=3");

  ram[1] = 4;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  ram[1] = 5;
  assert_do_frame(&runtime, &memory);
  assert_do_frame(&runtime, &memory);
  ram[1] = 6;
  assert_do_frame(&runtime, &memory);

  assert_hitcount(&runtime, 1, 0, 0, 3);
  assert_hitcount(&runtime, 2, 0, 0, 2);
  assert_hitcount(&runtime, 3, 0, 0, 1);

  /* deactivate an achievement without memrefs - trigger should be removed */
  ASSERT_NUM_EQUALS(runtime.trigger_count, 3);
  ASSERT_FALSE(runtime.triggers[1].owns_memrefs);
  rc_runtime_deactivate_achievement(&runtime, 2);
  ASSERT_NUM_EQUALS(runtime.trigger_count, 2);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  /* reactivate achievement 2 */
  assert_activate_achievement(&runtime, 2, "0xH0001=5_0xH0000=2");

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);

  assert_achievement_state(&runtime, 1, RC_TRIGGER_STATE_ACTIVE);
  assert_achievement_state(&runtime, 2, RC_TRIGGER_STATE_WAITING);
  assert_achievement_state(&runtime, 3, RC_TRIGGER_STATE_ACTIVE);
  assert_hitcount(&runtime, 1, 0, 0, 3);
  assert_hitcount(&runtime, 2, 0, 0, 0);
  assert_hitcount(&runtime, 3, 0, 0, 1);

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
  TEST(test_single_achievement_deactivated);
  TEST(test_single_achievement_md5_changed);

  TEST(test_no_core_group);
  TEST(test_memref_shared_address);

  TEST(test_multiple_achievements);
  TEST(test_multiple_achievements_ignore_triggered_and_inactive);
  TEST(test_multiple_achievements_overwrite_waiting);
  TEST(test_multiple_achievements_reactivate_waiting);
  TEST(test_multiple_achievements_paused_and_primed);
  TEST(test_multiple_achievements_deactivated_memrefs);
  TEST(test_multiple_achievements_deactivated_no_memrefs);


  TEST_SUITE_END();
}
