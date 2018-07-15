#include "internal.h"

rc_expression_t* rc_parse_expression(int* ret, void* buffer, const char** memaddr, lua_State* L, int funcs_ndx) {
  rc_expression_t* self, dummy;
  rc_term_t** next;

  self = (rc_expression_t*)rc_alloc(buffer, ret, sizeof(rc_expression_t), &dummy);
  next = &self->terms;

  for (;;) {
    *next = rc_parse_term(ret, buffer, memaddr, L, funcs_ndx);

    if (*ret < 0) {
      return 0;
    }

    next = &(*next)->next;

    if (**memaddr != '_') {
      break;
    }

    (*memaddr)++;
  }

  *next = 0;
  return self;
}

unsigned rc_evaluate_expression(rc_expression_t* self, rc_peek_t peek, void* ud, lua_State* L) {
  rc_term_t* term;
  unsigned value;

  value = 0;

  for (term = self->terms; term != 0; term = term->next) {
    value += rc_evaluate_term(term, peek, ud, L);
  }

  return value;
}
