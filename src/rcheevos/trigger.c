#include "internal.h"

#include <stddef.h>
#include <memory.h>

void rc_parse_trigger_internal(rc_trigger_t* self, const char** memaddr, rc_parse_state_t* parse) {
  rc_condset_t** next;
  const char* aux;

  aux = *memaddr;
  next = &self->alternative;

  if (*aux == 's' || *aux == 'S') {
    self->requirement = 0;
  }
  else {
    self->requirement = rc_parse_condset(&aux, parse);

    if (parse->offset < 0) {
      return;
    }

    self->requirement->next = 0;
  }

  while (*aux == 's' || *aux == 'S') {
    aux++;
    *next = rc_parse_condset(&aux, parse);

    if (parse->offset < 0) {
      return;
    }

    next = &(*next)->next;
  }
  
  *next = 0;
  *memaddr = aux;
}

int rc_trigger_size(const char* memaddr) {
  rc_trigger_t* self;
  rc_parse_state_t parse;
  rc_init_parse_state(&parse, 0, 0, 0);

  self = RC_ALLOC(rc_trigger_t, &parse);
  rc_parse_trigger_internal(self, &memaddr, &parse);

  rc_destroy_parse_state(&parse);
  return parse.offset;
}

rc_trigger_t* rc_parse_trigger(void* buffer, const char* memaddr, lua_State* L, int funcs_ndx) {
  rc_trigger_t* self;
  rc_parse_state_t parse;
  rc_init_parse_state(&parse, buffer, L, funcs_ndx);
  
  self = RC_ALLOC(rc_trigger_t, &parse);
  rc_init_parse_state_memrefs(&parse, &self->memrefs);

  rc_parse_trigger_internal(self, &memaddr, &parse);

  rc_destroy_parse_state(&parse);
  return parse.offset >= 0 ? self : 0;
}

int rc_evaluate_trigger(rc_trigger_t* self, rc_peek_t peek, void* ud, lua_State* L) {
  rc_eval_state_t eval_state;
  int ret, reset;
  rc_condset_t* condset;

  memset(&eval_state, 0, sizeof(eval_state));
  eval_state.peek = peek;
  eval_state.peek_userdata = ud;
  eval_state.L = L;

  rc_update_memref_values(self->memrefs, peek, ud);

  reset = 0;
  ret = self->requirement != 0 ? rc_test_condset(self->requirement, &eval_state) : 1;
  condset = self->alternative;

  if (condset) {
    int sub = 0;

    do {
      sub |= rc_test_condset(condset, &eval_state);
      condset = condset->next;
    }
    while (condset != 0);

    ret &= sub && !eval_state.was_reset;
  }

  self->measured_value = eval_state.measured_value;

  if (eval_state.was_reset) {
    rc_reset_trigger(self);

    if (self->can_reset && self->has_hits) {
      self->has_hits = 0;
      return RC_TRIGGER_STATE_RESET;
    }
  }

  return ret ? RC_TRIGGER_STATE_TRIGGERED : RC_TRIGGER_STATE_ACTIVE;
}

int rc_test_trigger(rc_trigger_t* self, rc_peek_t peek, void* ud, lua_State* L) {
  return (rc_evaluate_trigger(self, peek, ud, L) == RC_TRIGGER_STATE_TRIGGERED);
}

void rc_reset_trigger(rc_trigger_t* self) {
  rc_condset_t* condset;

  if (self->requirement != 0) {
    rc_reset_condset(self->requirement);
  }

  condset = self->alternative;

  while (condset != 0) {
    rc_reset_condset(condset);
    condset = condset->next;
  }
}
