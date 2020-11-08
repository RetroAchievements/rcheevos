#include "internal.h"

#include <stdlib.h> /* malloc/realloc */
#include <string.h> /* memcpy */

#define MEMREF_PLACEHOLDER_ADDRESS 0xFFFFFFFF

rc_memref_t* rc_alloc_memref(rc_parse_state_t* parse, unsigned address, char size, char is_indirect) {
  rc_memref_t** next_memref;
  rc_memref_t* memref;
  rc_memref_t* indirect_memref;

  if (!is_indirect) {
    /* attempt to find an existing rc_memref_value_t */
    next_memref = parse->first_memref;
    while (*next_memref) {
      memref = *next_memref;
      if (!memref->value.is_indirect && memref->address == address && memref->value.size == size)
        return memref;

      next_memref = &memref->next;
    }
  }
  else {
    /* indirect address always creates two new entries - one for the original address, and one for
       the indirect dereference */
    if (!parse->buffer) {
      /* in sizing mode, only allocate space for the two entries, but don't add them to the chain */
      memref = RC_ALLOC(rc_memref_t, parse);
      indirect_memref = RC_ALLOC(rc_memref_t, parse);
      return memref;
    }

    /* non-sizing mode - just skip ahead to the end of the list so we can append the new entries */
    next_memref = parse->first_memref;
    while (*next_memref)
      next_memref = &(*next_memref)->next;
  }

  /* no match found, create a new entry */
  memref = RC_ALLOC_SCRATCH(rc_memref_t, parse);
  memset(memref, 0, sizeof(*memref));
  memref->address = address;
  memref->value.size = size;
  memref->value.is_indirect = is_indirect;

  *next_memref = memref;

  /* also create the indirect deference entry for indirect references */
  if (is_indirect) {
    indirect_memref = RC_ALLOC(rc_memref_t, parse);
    memset(indirect_memref, 0, sizeof(*indirect_memref));
    indirect_memref->address = MEMREF_PLACEHOLDER_ADDRESS;
    indirect_memref->value.size = size;
    indirect_memref->value.is_indirect = 1;

    memref->next = indirect_memref;
  }

  return memref;
}

static unsigned rc_memref_get_value(rc_memref_t* self, rc_peek_t peek, void* ud) {
  unsigned value;

  if (!peek)
    return 0;

  switch (self->value.size)
  {
    case RC_MEMSIZE_BIT_0:
      value = (peek(self->address, 1, ud) >> 0) & 1;
      break;

    case RC_MEMSIZE_BIT_1:
      value = (peek(self->address, 1, ud) >> 1) & 1;
      break;

    case RC_MEMSIZE_BIT_2:
      value = (peek(self->address, 1, ud) >> 2) & 1;
      break;

    case RC_MEMSIZE_BIT_3:
      value = (peek(self->address, 1, ud) >> 3) & 1;
      break;

    case RC_MEMSIZE_BIT_4:
      value = (peek(self->address, 1, ud) >> 4) & 1;
      break;

    case RC_MEMSIZE_BIT_5:
      value = (peek(self->address, 1, ud) >> 5) & 1;
      break;

    case RC_MEMSIZE_BIT_6:
      value = (peek(self->address, 1, ud) >> 6) & 1;
      break;

    case RC_MEMSIZE_BIT_7:
      value = (peek(self->address, 1, ud) >> 7) & 1;
      break;

    case RC_MEMSIZE_LOW:
      value = peek(self->address, 1, ud) & 0x0f;
      break;

    case RC_MEMSIZE_HIGH:
      value = (peek(self->address, 1, ud) >> 4) & 0x0f;
      break;

    case RC_MEMSIZE_8_BITS:
      value = peek(self->address, 1, ud);
      break;

    case RC_MEMSIZE_16_BITS:
      value = peek(self->address, 2, ud);
      break;

    case RC_MEMSIZE_24_BITS:
      /* peek 4 bytes - don't expect the caller to understand 24-bit numbers */
      value = peek(self->address, 4, ud) & 0x00FFFFFF;
      break;

    case RC_MEMSIZE_32_BITS:
      value = peek(self->address, 4, ud);
      break;

    default:
      value = 0;
      break;
  }

  return value;
}

void rc_update_memref_value(rc_memref_t* memref, rc_peek_t peek, void* ud) {
  memref->value.previous = memref->value.value;
  memref->value.value = rc_memref_get_value(memref, peek, ud);
  if (memref->value.value != memref->value.previous)
    memref->value.prior = memref->value.previous;
}

void rc_update_memref_values(rc_memref_t* memref, rc_peek_t peek, void* ud) {
  while (memref) {
    if (memref->address != MEMREF_PLACEHOLDER_ADDRESS)
      rc_update_memref_value(memref, peek, ud);
    memref = memref->next;
  }
}

void rc_init_parse_state_memrefs(rc_parse_state_t* parse, rc_memref_t** memrefs) {
  parse->first_memref = memrefs;
  *memrefs = 0;
}

rc_memref_value_t* rc_get_indirect_memref(rc_memref_t* memref, rc_eval_state_t* eval_state) {
  unsigned new_address;

  if (eval_state->add_address == 0)
    return &memref->value;

  if (!memref->value.is_indirect)
    return &memref->value;

  new_address = memref->address + eval_state->add_address;

  /* an extra rc_memref_value_t is allocated for offset calculations */
  memref = memref->next;

  /* if the adjusted address has changed, update the record */
  if (memref->address != new_address) {
    memref->address = new_address;
    rc_update_memref_value(memref, eval_state->peek, eval_state->peek_userdata);
  }

  return &memref->value;
}
