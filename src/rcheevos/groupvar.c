#include "rc_internal.h"

#include <stdlib.h> /* malloc/realloc */
#include <string.h> /* memcpy */


rc_groupvar_t* rc_alloc_groupvar(rc_parse_state_t* parse, uint32_t index, uint8_t type) {
  rc_groupvar_t** next_local;
  rc_groupvar_t* local;

  local = 0;

  /* there ought to be a better place to put this */
  if (!parse->first_groupvar) {
    parse->first_groupvar = (rc_groupvar_t**)malloc(sizeof(rc_groupvar_t*)); /* TODO: free'd in rc_destroy_parse_state in, be we should re-visit to do this better.*/
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

  return local;
}

void rc_groupvar_update(rc_groupvar_t* self, rc_typed_value_t* value) {
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
