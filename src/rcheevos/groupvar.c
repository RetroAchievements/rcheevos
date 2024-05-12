#include "rc_internal.h"

#include <stdlib.h> /* malloc/realloc */
#include <string.h> /* memcpy */


rc_groupvar_t* rc_alloc_groupvar(rc_parse_state_t* parse, uint32_t index, uint8_t type) {
  rc_groupvar_t** next_local;
  rc_groupvar_t* local;

  local = 0;

  /* there ought to be a better place to put this */
  if (!parse->first_groupvar) {
    parse->first_groupvar = (rc_groupvar_t**)malloc(sizeof(rc_groupvar_t*)); /* free'd in rc_destroy_parse_state in*/
    if (!parse->first_groupvar)
      return local;
    parse->first_groupvar[0] = 0;
  }

  /* attempt to find an existing group var with this index */
  next_local = parse->first_groupvar;
  while (*next_local) {
    local = *next_local;
    if (local->index == index && local->type == type)
      return local;

    next_local = &local->next;
  }

  /* no match found, create a new entry */
  local = RC_ALLOC_SCRATCH(rc_groupvar_t, parse);
  *next_local = local;
 

  memset(local, 0, sizeof(*local));
  local->index = index;
  local->type = type;
  local->num_memrefs = 0;
  local->capacity_memrefs = 0;

  return local;
}

int rc_groupvar_add_memref(rc_groupvar_t* self, rc_memref_t* memref) {
  rc_memref_t** reallocated_memrefs;

  /* consider alternatives to just growing by 5 every allocation. 5 chosen arbitrarily expecting a group variable to be used as an indirection offset at most 5 times.*/
  if (self->capacity_memrefs == 0) {
    self->capacity_memrefs += 5;
    self->memrefs = (rc_memref_t**)malloc(self->capacity_memrefs * sizeof(rc_memref_t*)); /* TODO: this array will need to be free'd. when? */
    if (!self->memrefs) {
      return RC_OUT_OF_MEMORY;
    }
  }
  else if (self->num_memrefs == self->capacity_memrefs) {
    self->capacity_memrefs += 5;
    reallocated_memrefs = realloc(self->memrefs, self->capacity_memrefs * sizeof(rc_memref_t*));
    if (!reallocated_memrefs) {
      return RC_OUT_OF_MEMORY;
    }
    self->memrefs = reallocated_memrefs;
  }

  self->memrefs[self->num_memrefs++] = memref;

  return RC_OK;
}

void rc_groupvar_update(rc_groupvar_t* self, rc_typed_value_t* value) {
  size_t i;

  switch (self->type) {
  case RC_GROUPVAR_TYPE_32_BITS:
    switch (value->type) {
    case RC_VALUE_TYPE_SIGNED:
    case RC_VALUE_TYPE_FLOAT:
      rc_typed_value_convert( value, RC_VALUE_TYPE_UNSIGNED);
      /* fall through */
      break;
    case RC_VALUE_TYPE_UNSIGNED:
      self->u32 = value->value.u32;
      break;
    }

    /* update memrefs that use this as the offset address */
    for (i = 0; i < self->num_memrefs; i++) {
      self->memrefs[i]->address = self->u32;
    }

    break;

  case RC_GROUPVAR_TYPE_FLOAT:
    switch (value->type) {
    case RC_VALUE_TYPE_SIGNED:
    case RC_VALUE_TYPE_UNSIGNED:
      rc_typed_value_convert(value, RC_VALUE_TYPE_FLOAT);
      /* fall through */
    case RC_VALUE_TYPE_FLOAT:
      self->f32 = value->value.f32;
      break;
    }

    break;
  }
}

int rc_parse_groupvar_num(const char** memaddr, uint32_t* varIndex) {
  const char* aux = *memaddr;
  char* end;
  unsigned long value;

  if (aux[0] == '0') {
    if (aux[1] != 'x' && aux[1] != 'X')
      return RC_INVALID_GROUPVAR_OPERAND;
  }
  else {
    return RC_INVALID_GROUPVAR_OPERAND;
  }
  aux += 2;

  value = strtoul(aux, &end, 16);

  if (end == aux)
    return RC_INVALID_GROUPVAR_OPERAND;

  if (value > 0xffffffffU)
    value = 0xffffffffU;

  *varIndex = (uint32_t)value;
  *memaddr = end;
  return RC_OK;
}

int rc_parse_groupvar_offset(const char** memaddr, uint8_t* size, uint32_t* varIndex) {
  const char* aux = *memaddr;
  char* end;
  unsigned long value;

  if (aux[0] == '0') {
    if (aux[1] != 'x' && aux[1] != 'X')
      return RC_INVALID_GROUPVAR_OFFSET;

    aux += 2;
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
    case 'j': case 'J': *size = RC_MEMSIZE_24_BITS_BE; break;

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
      return RC_INVALID_GROUPVAR_OFFSET;
    }
  }
  else if (aux[0] == 'f' || aux[0] == 'F') {
    ++aux;
    switch (*aux++) {
    case 'f': case 'F': *size = RC_MEMSIZE_FLOAT; break;
    case 'b': case 'B': *size = RC_MEMSIZE_FLOAT_BE; break;
    case 'h': case 'H': *size = RC_MEMSIZE_DOUBLE32; break;
    case 'i': case 'I': *size = RC_MEMSIZE_DOUBLE32_BE; break;
    case 'm': case 'M': *size = RC_MEMSIZE_MBF32; break;
    case 'l': case 'L': *size = RC_MEMSIZE_MBF32_LE; break;

    default:
      return RC_INVALID_GROUPVAR_OFFSET;
    }
  }
  else {
    return RC_INVALID_GROUPVAR_OFFSET;
  }

  value = strtoul(aux, &end, 16);

  if (end == aux)
    return RC_INVALID_GROUPVAR_OFFSET;

  if (value > 0xffffffffU)
    value = 0xffffffffU;

  *varIndex = (uint32_t)value;
  *memaddr = end;
  return RC_OK;
}
