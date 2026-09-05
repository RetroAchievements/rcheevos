#ifndef MOCK_MEMORY_H
#define MOCK_MEMORY_H

#include <stdint.h>
#include <string.h>

typedef struct {
  uint8_t* ram;
  uint32_t size;
}
memory_t;

static uint32_t read_memory(uint32_t address, uint8_t* buffer, uint32_t num_bytes, void* ud) {
  memory_t* memory = (memory_t*)ud;

  if (address >= memory->size)
    return 0;

  uint32_t available = memory->size - address;
  if (available < num_bytes)
    num_bytes = available;

  memcpy(buffer, &memory->ram[address], num_bytes);
  return num_bytes;
}

#endif /* MOCK_MEMORY_H */
