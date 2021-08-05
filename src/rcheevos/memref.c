#include "rc_internal.h"

#include <stdlib.h> /* malloc/realloc */
#include <string.h> /* memcpy */

#define MEMREF_PLACEHOLDER_ADDRESS 0xFFFFFFFF

rc_memref_t* rc_alloc_memref(rc_parse_state_t* parse, unsigned address, char size, char is_indirect) {
  rc_memref_t** next_memref;
  rc_memref_t* memref;

  if (!is_indirect) {
    /* attempt to find an existing memref that can be shared */
    next_memref = parse->first_memref;
    while (*next_memref) {
      memref = *next_memref;
      if (!memref->value.is_indirect && memref->address == address && memref->value.size == size)
        return memref;

      next_memref = &memref->next;
    }

    /* no match found, create a new entry */
    memref = RC_ALLOC_SCRATCH(rc_memref_t, parse);
    *next_memref = memref;
  }
  else {
    /* indirect references always create a new entry because we can't guarantee that the 
     * indirection amount will be the same between references. because they aren't shared,
     * don't bother putting them in the chain.
     */
    memref = RC_ALLOC(rc_memref_t, parse);
  }

  memset(memref, 0, sizeof(*memref));
  memref->address = address;
  memref->value.size = size;
  memref->value.is_indirect = is_indirect;

  return memref;
}

int rc_parse_memref(const char** memaddr, char* size, unsigned* address) {
  const char* aux = *memaddr;
  char* end;
  unsigned long value;

  if (*aux++ != '0')
    return RC_INVALID_MEMORY_OPERAND;

  if (*aux != 'x' && *aux != 'X')
    return RC_INVALID_MEMORY_OPERAND;
  aux++;

  switch (*aux++) {
    /* ordered by estimated frequency in case compiler doesn't build a jump table */
    case 'h': case 'H': *size = RC_MEMSIZE_8_BITS; break;
    case ' ':           *size = RC_MEMSIZE_16_BITS; break;
    case 'x': case 'X': *size = RC_MEMSIZE_32_BITS; break;

    case 'm': case 'M': *size = RC_MEMSIZE_BIT_0; break;
    case 'n': case 'N': *size = RC_MEMSIZE_BIT_1; break;
    case 'o': case 'O': *size = RC_MEMSIZE_BIT_2; break;
    case 'p': case 'P': *size = RC_MEMSIZE_BIT_3; break;
    case 'q': case 'Q': *size = RC_MEMSIZE_BIT_4; break;
    case 'r': case 'R': *size = RC_MEMSIZE_BIT_5; break;
    case 's': case 'S': *size = RC_MEMSIZE_BIT_6; break;
    case 't': case 'T': *size = RC_MEMSIZE_BIT_7; break;
    case 'l': case 'L': *size = RC_MEMSIZE_LOW; break;
    case 'u': case 'U': *size = RC_MEMSIZE_HIGH; break;
    case 'k': case 'K': *size = RC_MEMSIZE_BITCOUNT; break;
    case 'w': case 'W': *size = RC_MEMSIZE_24_BITS; break;
    case 'g': case 'G': *size = RC_MEMSIZE_32_BITS_BE; break;
    case 'i': case 'I': *size = RC_MEMSIZE_16_BITS_BE; break;

    /* case 'j': case 'J': */
    /* case 'v': case 'V': */
    /* case 'y': case 'Y': 64 bit? */
    /* case 'z': case 'Z': 128 bit? */

    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
      /* legacy support - addresses without a size prefix are assumed to be 16-bit */
      aux--;
      *size = RC_MEMSIZE_16_BITS;
      break;

    default:
      return RC_INVALID_MEMORY_OPERAND;
  }

  value = strtoul(aux, &end, 16);

  if (end == aux)
    return RC_INVALID_MEMORY_OPERAND;

  if (value > 0xffffffffU)
    value = 0xffffffffU;

  *address = (unsigned)value;
  *memaddr = end;
  return RC_OK;
}

static const unsigned char rc_bits_set[16] = { 0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4 };

unsigned rc_transform_memref_value(unsigned value, char size)
{
  switch (size)
  {
    case RC_MEMSIZE_BIT_0:
      value = (value >> 0) & 1;
      break;

    case RC_MEMSIZE_BIT_1:
      value = (value >> 1) & 1;
      break;

    case RC_MEMSIZE_BIT_2:
      value = (value >> 2) & 1;
      break;

    case RC_MEMSIZE_BIT_3:
      value = (value >> 3) & 1;
      break;

    case RC_MEMSIZE_BIT_4:
      value = (value >> 4) & 1;
      break;

    case RC_MEMSIZE_BIT_5:
      value = (value >> 5) & 1;
      break;

    case RC_MEMSIZE_BIT_6:
      value = (value >> 6) & 1;
      break;

    case RC_MEMSIZE_BIT_7:
      value = (value >> 7) & 1;
      break;

    case RC_MEMSIZE_LOW:
      value = value & 0x0f;
      break;

    case RC_MEMSIZE_HIGH:
      value = (value >> 4) & 0x0f;
      break;

    case RC_MEMSIZE_BITCOUNT:
      value = rc_bits_set[(value & 0x0F)]
            + rc_bits_set[((value >> 4) & 0x0F)];
      break;

    default:
      break;
  }

  return value;
}

char rc_memref_shared_size(char size)
{
  switch (size) {
    case RC_MEMSIZE_BIT_0:
    case RC_MEMSIZE_BIT_1:
    case RC_MEMSIZE_BIT_2:
    case RC_MEMSIZE_BIT_3:
    case RC_MEMSIZE_BIT_4:
    case RC_MEMSIZE_BIT_5:
    case RC_MEMSIZE_BIT_6:
    case RC_MEMSIZE_BIT_7:
    case RC_MEMSIZE_LOW:
    case RC_MEMSIZE_HIGH:
    case RC_MEMSIZE_BITCOUNT:
      /* these can all share an 8-bit memref and just mask off the appropriate data in rc_transform_memref_value */
      return RC_MEMSIZE_8_BITS;

    default:
      return size;
  }
}

static unsigned rc_peek_value(unsigned address, char size, rc_peek_t peek, void* ud) {
  unsigned value;

  if (!peek)
    return 0;

  switch (size)
  {
    case RC_MEMSIZE_8_BITS:
      value = peek(address, 1, ud);
      break;

    case RC_MEMSIZE_16_BITS:
      value = peek(address, 2, ud);
      break;

    case RC_MEMSIZE_24_BITS:
      /* peek 4 bytes - don't expect the caller to understand 24-bit numbers */
      value = peek(address, 4, ud) & 0x00FFFFFF;
      break;

    case RC_MEMSIZE_32_BITS:
      value = peek(address, 4, ud);
      break;

    case RC_MEMSIZE_16_BITS_BE:
      value = peek(address, 2, ud);
      value = (value >> 8) | ((value & 0xFF) << 8);
      break;

    case RC_MEMSIZE_32_BITS_BE:
      value = peek(address, 4, ud);
      value = ((value & 0xFF000000) >> 24) |
              ((value & 0x00FF0000) >> 8) |
              ((value & 0x0000FF00) << 8) |
              ((value & 0x000000FF) << 24);
      break;

    default:
      if (rc_memref_shared_size(size) == RC_MEMSIZE_8_BITS)
      {
        value = peek(address, 1, ud);
        value = rc_transform_memref_value(value, size);
      }
      else
      {
        value = 0;
      }
      break;
  }

  return value;
}

void rc_update_memref_value(rc_memref_value_t* memref, unsigned new_value) {
  if (memref->value == new_value) {
    memref->changed = 0;
  }
  else {
    memref->prior = memref->value;
    memref->value = new_value;
    memref->changed = 1;
  }
}

void rc_update_memref_values(rc_memref_t* memref, rc_peek_t peek, void* ud) {
  while (memref) {
    /* indirect memory references are not shared and will be updated in rc_get_memref_value */
    if (!memref->value.is_indirect)
      rc_update_memref_value(&memref->value, rc_peek_value(memref->address, memref->value.size, peek, ud));

    memref = memref->next;
  }
}

void rc_init_parse_state_memrefs(rc_parse_state_t* parse, rc_memref_t** memrefs) {
  parse->first_memref = memrefs;
  *memrefs = 0;
}

unsigned rc_get_memref_value(rc_memref_t* memref, int operand_type, rc_eval_state_t* eval_state) {
  /* if this is an indirect reference, handle the indirection. */
  if (memref->value.is_indirect) {
    const unsigned new_address = memref->address + eval_state->add_address;
    rc_update_memref_value(&memref->value, rc_peek_value(new_address, memref->value.size, eval_state->peek, eval_state->peek_userdata));
  }

  return rc_get_memref_value_value(&memref->value, operand_type);
}

unsigned rc_get_memref_value_value(rc_memref_value_t* memref, int operand_type) {
  switch (operand_type)
  {
    /* most common case explicitly first, even though it could be handled by default case.
     * this helps the compiler to optimize if it turns the switch into a series of if/elses */
    case RC_OPERAND_ADDRESS:
      return memref->value;

    case RC_OPERAND_DELTA:
      if (!memref->changed) {
        /* fallthrough */
    default:
        return memref->value;
      }
      /* fallthrough */
    case RC_OPERAND_PRIOR:
      return memref->prior;
  }
}
