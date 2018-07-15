#include "internal.h"

#include <stddef.h>

void rc_parse_trigger_internal(rc_trigger_t* self, int* ret, void* buffer, const char** memaddr, lua_State* L, int funcs_ndx) {
  rc_condset_t** next;
  const char* aux;

  aux = *memaddr;
  next = &self->alternative;

  if (*aux == 's' || *aux == 'S') {
    self->requirement = 0;
  }
  else {
    self->requirement = rc_parse_condset(ret, buffer, &aux, L, funcs_ndx);

    if (*ret < 0) {
      return;
    }
  }

  while (*aux == 's' || *aux == 'S') {
    aux++;
    *next = rc_parse_condset(ret, buffer, &aux, L, funcs_ndx);

    if (*ret < 0) {
      return;
    }

    next = &(*next)->next;
  }
  
  *next = 0;
  *memaddr = aux;
}

rc_trigger_t* rc_parse_trigger(int* ret, void* buffer, const char* memaddr, lua_State* L, int funcs_ndx) {
  rc_trigger_t dummy;
  rc_trigger_t* self;
  
  self = (rc_trigger_t*)rc_alloc(buffer, ret, sizeof(rc_trigger_t), &dummy);
  rc_parse_trigger_internal(self, ret, buffer, &memaddr, L, funcs_ndx);

  return self;
}

int rc_test_trigger(rc_trigger_t* self, int* dirty, int* reset, rc_peek_t peek, void* ud, lua_State* L) {
  int ret;
  rc_condset_t* condset;

  *dirty = *reset = 0;
  ret = self->requirement != 0 ? rc_test_condset(self->requirement, dirty, reset, peek, ud, L) : 1;
  condset = self->alternative;

  if (condset) {
    int sub = 0;

    do {
      sub |= rc_test_condset(condset, dirty, reset, peek, ud, L);
      condset = condset->next;
    }
    while (condset != 0);

    ret &= sub && !*reset;
  }

  if (*reset) {
    rc_reset_trigger(self, dirty);
  }

  return ret;
}

void rc_reset_trigger(rc_trigger_t* self, int* dirty) {
  rc_condset_t* condset;

  if (self->requirement != 0) {
    *dirty |= rc_reset_condset(self->requirement);
  }

  condset = self->alternative;

  while (condset != 0) {
    *dirty |= rc_reset_condset(condset);
    condset = condset->next;
  }
}
