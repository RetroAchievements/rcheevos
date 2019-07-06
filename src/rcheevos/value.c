#include "internal.h"

static void rc_parse_cond_value(rc_value_t* self, const char** memaddr, rc_parse_state_t* parse) {
  rc_condition_t** next;
  int has_measured;

  has_measured = 0;
  self->expressions = 0;

  /* this largely duplicates rc_parse_condset, but we cannot call it directly, as we need to check the 
   * type of each condition as we go */
  self->conditions = RC_ALLOC(rc_condset_t, parse);
  self->conditions->next = 0;
  self->conditions->has_pause = 0;

  next = &self->conditions->conditions;
  for (;;) {
    *next = rc_parse_condition(memaddr, parse);

    if (parse->offset < 0) {
      return;
    }

    switch ((*next)->type) {
      case RC_CONDITION_ADD_HITS:
      case RC_CONDITION_ADD_SOURCE:
      case RC_CONDITION_SUB_SOURCE:
      case RC_CONDITION_AND_NEXT:
        /* combining flags are allowed */
        break;

      case RC_CONDITION_RESET_IF:
        /* ResetIf is allowed (primarily for rich presense - leaderboard will typically cancel instead of resetting) */
        break;

      case RC_CONDITION_MEASURED:
        if (has_measured) {
          parse->offset = RC_DUPLICATED_VALUE_MEASURED;
          return;
        }
        has_measured = 1;
        break;

      default:
        /* non-combinding flags and PauseIf are not allowed */
        parse->offset = RC_INVALID_VALUE_FLAG;
        return;
    }

    (*next)->pause = 0;
    next = &(*next)->next;

    if (**memaddr != '_') {
      break;
    }

    (*memaddr)++;
  }

  *next = 0;

  if (!has_measured) {
    parse->offset = RC_MISSING_VALUE_MEASURED;
  }
}

void rc_parse_value_internal(rc_value_t* self, const char** memaddr, rc_parse_state_t* parse) {
  rc_expression_t** next;

  /* if it starts with a condition flag (M: A: B: C:), parse the conditions */
  if ((*memaddr)[1] == ':') {
    rc_parse_cond_value(self, memaddr, parse);
    return;
  }

  self->conditions = 0;
  next = &self->expressions;

  for (;;) {
    *next = rc_parse_expression(memaddr, parse);

    if (parse->offset < 0) {
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

int rc_value_size(const char* memaddr) {
  rc_value_t* self;
  rc_parse_state_t parse;
  rc_init_parse_state(&parse, 0, 0, 0);

  self = RC_ALLOC(rc_value_t, &parse);
  rc_parse_value_internal(self, &memaddr, &parse);

  rc_destroy_parse_state(&parse);
  return parse.offset;
}

rc_value_t* rc_parse_value(void* buffer, const char* memaddr, lua_State* L, int funcs_ndx) {
  rc_value_t* self;
  rc_parse_state_t parse;
  rc_init_parse_state(&parse, buffer, L, funcs_ndx);
  
  self = RC_ALLOC(rc_value_t, &parse);
  rc_init_parse_state_memrefs(&parse, &self->memrefs);

  rc_parse_value_internal(self, &memaddr, &parse);

  rc_destroy_parse_state(&parse);
  return parse.offset >= 0 ? self : 0;
}

static unsigned rc_evaluate_cond_value(rc_value_t* self, rc_peek_t peek, void* ud, lua_State* L) {
  rc_condition_t* condition;
  int reset;

  rc_test_condset(self->conditions, &reset, peek, ud, L);

  for (condition = self->conditions->conditions; condition != 0; condition = condition->next) {
    if (condition->type == RC_CONDITION_MEASURED)
      return rc_total_hit_count(self->conditions->conditions, condition);
  }

  return 0;
}

static unsigned rc_evaluate_expr_value(rc_value_t* self, rc_peek_t peek, void* ud, lua_State* L) {
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

unsigned rc_evaluate_value(rc_value_t* self, rc_peek_t peek, void* ud, lua_State* L) {
  rc_update_memref_values(self->memrefs, peek, ud);

  if (self->expressions) {
    return rc_evaluate_expr_value(self, peek, ud, L);
  }

  return rc_evaluate_cond_value(self, peek, ud, L);
}
