#include "internal.h"

void rc_parse_value(rc_value_t* self, int* ret, void* buffer, const char** memaddr, lua_State* L, int funcs_ndx) {
  rc_expression_t** next;

  next = &self->expressions;

  for (;;) {
    *next = rc_parse_expression(ret, buffer, memaddr, L, funcs_ndx);

    if (*ret < 0) {
      return;
    }

    next = &(*next)->next;

    if (**memaddr != '$') {
      break;
    }

    (*memaddr)++;
  }

  *next = 0;
}

unsigned rc_evaluate_value(rc_value_t* self, rc_peek_t peek, void* ud, lua_State* L) {
  rc_expression_t* exp;
  unsigned value, max;

  exp = self->expressions;
  max = rc_evaluate_expression(exp, peek, ud, L);

  for (exp = exp->next; exp != 0; exp = exp->next) {
    value = rc_evaluate_expression(exp, peek, ud, L);

    if (value > max) {
      max = value;
    }
  }

  return max;
}
