#include "internal.h"

static void rc_parse_cond_value(rc_value_t* self, const char** memaddr, rc_parse_state_t* parse) {
  rc_condition_t** next;
  int has_measured;
  int in_add_address;

  has_measured = 0;
  in_add_address = 0;
  self->expressions = 0;

  /* this largely duplicates rc_parse_condset, but we cannot call it directly, as we need to check the 
   * type of each condition as we go */
  self->conditions = RC_ALLOC(rc_condset_t, parse);
  self->conditions->next = 0;
  self->conditions->has_pause = 0;

  next = &self->conditions->conditions;
  for (;;) {
    *next = rc_parse_condition(memaddr, parse, in_add_address);

    if (parse->offset < 0) {
      return;
    }

    in_add_address = (*next)->type == RC_CONDITION_ADD_ADDRESS;

    switch ((*next)->type) {
      case RC_CONDITION_ADD_HITS:
      case RC_CONDITION_ADD_SOURCE:
      case RC_CONDITION_SUB_SOURCE:
      case RC_CONDITION_AND_NEXT:
      case RC_CONDITION_ADD_ADDRESS:
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

static unsigned rc_total_value(rc_condition_t* first, rc_condition_t* condition, rc_peek_t peek, void* ud, lua_State* L) {
  unsigned add_value, add_address;

  add_value = add_address = 0;

  for (; first != 0; first = first->next) {
    switch (first->type) {
      case RC_CONDITION_ADD_SOURCE:
        add_value += rc_evaluate_operand(&first->operand1, add_address, peek, ud, L);
        add_address = 0;
        continue;

      case RC_CONDITION_SUB_SOURCE:
        add_value -= rc_evaluate_operand(&first->operand1, add_address, peek, ud, L);
        add_address = 0;
        continue;

      case RC_CONDITION_ADD_ADDRESS:
        add_address = rc_evaluate_operand(&first->operand1, add_address, peek, ud, L);
        continue;

      case RC_CONDITION_MEASURED:
        if (first == condition)
          return rc_evaluate_operand(&first->operand1, add_address, peek, ud, L) + add_value;
        break;

    }

    add_value = add_address = 0;
  }

  return 0;
}

static int rc_evaluate_cond_value(rc_value_t* self, rc_peek_t peek, void* ud, lua_State* L) {
  rc_condition_t* condition;
  int reset;

  for (condition = self->conditions->conditions; condition != 0; condition = condition->next) {
    if (condition->type == RC_CONDITION_MEASURED) {
      if (condition->oper == RC_CONDITION_NONE) {
        return rc_total_value(self->conditions->conditions, condition, peek, ud, L);
      }
      else {
        rc_test_condset(self->conditions, &reset, peek, ud, L);
        return rc_total_hit_count(self->conditions->conditions, condition);
      }
    }
  }

  return 0;
}

static int rc_evaluate_expr_value(rc_value_t* self, rc_peek_t peek, void* ud, lua_State* L) {
  rc_expression_t* exp;
  int value, max;

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

int rc_evaluate_value(rc_value_t* self, rc_peek_t peek, void* ud, lua_State* L) {
  rc_update_memref_values(self->memrefs, peek, ud);

  if (self->expressions) {
    return rc_evaluate_expr_value(self, peek, ud, L);
  }

  return rc_evaluate_cond_value(self, peek, ud, L);
}
