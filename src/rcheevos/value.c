#include "internal.h"

#include <string.h> /* memset */
#include <ctype.h> /* isdigit */

static void rc_parse_cond_value(rc_value_t* self, const char** memaddr, rc_parse_state_t* parse) {
  self->conditions = rc_parse_condset(memaddr, parse, 1);
  if (parse->offset < 0) {
    return;
  }

  if (**memaddr == 'S' || **memaddr == 's') {
    /* alt groups not supported */
    parse->offset = RC_INVALID_VALUE_FLAG;
  }
  else if (parse->measured_target == 0) {
    parse->offset = RC_MISSING_VALUE_MEASURED;
  }
  else {
    self->conditions->next = 0;
  }
}

void rc_parse_legacy_value(rc_value_t* self, const char** memaddr, rc_parse_state_t* parse) {
  rc_condition_t** next;
  rc_condset_t** next_clause;
  rc_condition_t* cond;
  char buffer[64] = "A:";
  const char* buffer_ptr;
  char* ptr;
  int end_of_clause;

  /* convert legacy format into condset */
  self->conditions = RC_ALLOC(rc_condset_t, parse);
  self->conditions->has_pause = 0;
  self->conditions->is_paused = 0;
  self->measured_value = 0;

  next = &self->conditions->conditions;
  next_clause = &self->conditions->next;

  for (;;) {
    ptr = &buffer[2];
    end_of_clause = 0;

    do {
      switch (**memaddr) {
        case '_': /* add next */
        case '$': /* maximum of */
        case '\0': /* end of string */
        case ':': /* end of leaderboard clause */
        case ')': /* end of rich presence macro */
          end_of_clause = 1;
          *ptr = '\0';
          break;

        case '*':
          *ptr++ = '*';

          buffer_ptr = *memaddr + 1;
          if (*buffer_ptr == '-') {
            /* negative value automatically needs prefix, 'f' handles both float and digits, so use it */
            *ptr++ = 'f';
          }
          else {
            /* if it looks like a floating point number, add the 'f' prefix */
            while (isdigit(*buffer_ptr))
              ++buffer_ptr;
            if (*buffer_ptr == '.')
              *ptr++ = 'f';
          }
          break;

        default:
          *ptr++ = **memaddr;
          break;
      }

      ++(*memaddr);
    } while (!end_of_clause);

    buffer_ptr = buffer;
    cond = rc_parse_condition(&buffer_ptr, parse, 0);
    if (parse->offset < 0) {
      return;
    }

    switch (cond->oper) {
      case RC_OPERATOR_MULT:
      case RC_OPERATOR_DIV:
      case RC_OPERATOR_AND:
      case RC_OPERATOR_NONE:
        break;

      default:
        parse->offset = RC_INVALID_OPERATOR;
        return;
    }

    cond->pause = 0;
    *next = cond;

    switch ((*memaddr)[-1]) {
      case '_': /* add next */
        next = &cond->next;
        break;

      case '$': /* max of */
        cond->type = RC_CONDITION_MEASURED;
        cond->next = 0;
        *next_clause = RC_ALLOC(rc_condset_t, parse);
        (*next_clause)->has_pause = 0;
        (*next_clause)->is_paused = 0;
        next = &(*next_clause)->conditions;
        next_clause = &(*next_clause)->next;
        break;

      default: /* end of valid string */
        --(*memaddr); /* undo the increment we performed when copying the string */
        cond->type = RC_CONDITION_MEASURED;
        cond->next = 0;
        *next_clause = 0;
        return;
    }
  }
}

void rc_parse_value_internal(rc_value_t* self, const char** memaddr, rc_parse_state_t* parse) {
  /* if it starts with a condition flag (M: A: B: C:), parse the conditions */
  if ((*memaddr)[1] == ':') {
    rc_parse_cond_value(self, memaddr, parse);
  }
  else {
    rc_parse_legacy_value(self, memaddr, parse);
  }

  self->measured_value = 0;
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

int rc_evaluate_value(rc_value_t* self, rc_peek_t peek, void* ud, lua_State* L) {
  rc_eval_state_t eval_state;
  rc_condset_t* condset;
  int result = 0;
  int paused = 1;

  rc_update_memref_values(self->memrefs, peek, ud);

  for (condset = self->conditions; condset != NULL; condset = condset->next) {
    memset(&eval_state, 0, sizeof(eval_state));
    eval_state.peek = peek;
    eval_state.peek_userdata = ud;
    eval_state.L = L;

    rc_test_condset(condset, &eval_state);

    if (condset->is_paused)
      continue;

    if (eval_state.was_reset) {
      /* if any ResetIf condition was true, reset the hit counts 
       * NOTE: ResetIf only affects the current condset when used in values!
       */
      rc_reset_condset(condset);

      /* if the measured value came from a hit count, reset it too */
      if (eval_state.measured_from_hits)
        eval_state.measured_value = 0;
    }

    if (paused) {
      /* capture the first valid measurement */
      result = (int)eval_state.measured_value;
      paused = 0;
    }
    else {
      /* multiple condsets are currently only used for the MAX_OF operation.
       * only keep the condset's value if it's higher than the current highest value.
       */
      if ((int)eval_state.measured_value > result)
        result = (int)eval_state.measured_value;
    }
  }

  if (!paused) {
    /* if not paused, store the value so that it's available when paused. */
    self->measured_value = result;
  }
  else {
    /* when paused, the Measured value will not be captured, use the last captured value. */
    result = self->measured_value;
  }

  return result;
}

void rc_reset_value(rc_value_t* self)
{
  rc_condset_t* condset = self->conditions;
  while (condset != NULL) {
    rc_reset_condset(condset);
    condset = condset->next;
  }

  self->measured_value = 0;
}
