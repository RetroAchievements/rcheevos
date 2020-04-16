#include "internal.h"

#include "../test_framework.h"
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

  ASSERT_NUM_EQUALS(runtime.memrefs->value, 5);
  ASSERT_NUM_EQUALS(runtime.memrefs->previous, 5);
  ASSERT_NUM_EQUALS(runtime.memrefs->prior, 4);
  ASSERT_NUM_EQUALS(runtime.memrefs->next->value, 6);
  ASSERT_NUM_EQUALS(runtime.memrefs->next->previous, 6);
  ASSERT_NUM_EQUALS(runtime.memrefs->next->prior, 0);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->requirement->conditions->current_hits, 3);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->requirement->conditions->next->current_hits, 0);

  assert_serialize(&runtime, buffer, sizeof(buffer));

  reset_runtime(&runtime);
  assert_deserialize(&runtime, buffer);

  ASSERT_NUM_EQUALS(runtime.memrefs->value, 5);
  ASSERT_NUM_EQUALS(runtime.memrefs->previous, 5);
  ASSERT_NUM_EQUALS(runtime.memrefs->prior, 4);
  ASSERT_NUM_EQUALS(runtime.memrefs->next->value, 6);
  ASSERT_NUM_EQUALS(runtime.memrefs->next->previous, 6);
  ASSERT_NUM_EQUALS(runtime.memrefs->next->prior, 0);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->requirement->conditions->current_hits, 3);
  ASSERT_NUM_EQUALS(runtime.triggers[0].trigger->requirement->conditions->next->current_hits, 0);

  rc_runtime_destroy(&runtime);
}

/* ======================================================== */

void test_runtime_progress(void) {
  TEST_SUITE_BEGIN();

  TEST(test_single_achievement);

  TEST_SUITE_END();
}
