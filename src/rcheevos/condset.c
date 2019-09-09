#include "internal.h"

static void rc_update_condition_pause(rc_condition_t* condition, int* in_pause) {
  if (condition->next != 0) {
    rc_update_condition_pause(condition->next, in_pause);
  }

  switch (condition->type) {
    case RC_CONDITION_PAUSE_IF:
      *in_pause = condition->pause = 1;
      break;
    
    case RC_CONDITION_ADD_SOURCE:
    case RC_CONDITION_SUB_SOURCE:
    case RC_CONDITION_ADD_HITS:
    case RC_CONDITION_AND_NEXT:
    case RC_CONDITION_ADD_ADDRESS:
      condition->pause = *in_pause;
      break;
    
    default:
      *in_pause = condition->pause = 0;
      break;
  }
}

rc_condset_t* rc_parse_condset(const char** memaddr, rc_parse_state_t* parse) {
  rc_condset_t* self;
  rc_condition_t** next;
  int in_pause;
  int in_add_address;

  self = RC_ALLOC(rc_condset_t, parse);
  self->has_pause = 0;
  next = &self->conditions;

  if (**memaddr == 'S' || **memaddr == 's' || !**memaddr) {
    /* empty group - editor allows it, so we have to support it */
    *next = 0;
    return self;
  }

  in_add_address = 0;
  for (;;) {
    *next = rc_parse_condition(memaddr, parse, in_add_address);

    if (parse->offset < 0) {
      return 0;
    }

    if ((*next)->oper == RC_CONDITION_NONE) {
      switch ((*next)->type) {
        case RC_CONDITION_ADD_ADDRESS:
        case RC_CONDITION_ADD_HITS:
        case RC_CONDITION_ADD_SOURCE:
        case RC_CONDITION_SUB_SOURCE:
        case RC_CONDITION_AND_NEXT:
          break;

        default:
          parse->offset = RC_INVALID_OPERATOR;
          return 0;
      }
    }

    self->has_pause |= (*next)->type == RC_CONDITION_PAUSE_IF;
    in_add_address = (*next)->type == RC_CONDITION_ADD_ADDRESS;

    next = &(*next)->next;

    if (**memaddr != '_') {
      break;
    }

    (*memaddr)++;
  }

  *next = 0;


  if (parse->buffer != 0) {
    in_pause = 0;
    rc_update_condition_pause(self->conditions, &in_pause);
  }

  return self;
}

static int rc_test_condset_internal(rc_condset_t* self, int processing_pause, int* reset, rc_peek_t peek, void* ud, lua_State* L) {
  rc_condition_t* condition;
  int set_valid, cond_valid, prev_cond;
  unsigned add_value, add_hits, add_address;

  set_valid = 1;
  prev_cond = 1;
  add_value = add_hits = add_address = 0;

  for (condition = self->conditions; condition != 0; condition = condition->next) {
    if (condition->pause != processing_pause) {
      continue;
    }

    switch (condition->type) {
      case RC_CONDITION_ADD_SOURCE:
        add_value += rc_evaluate_operand(&condition->operand1, add_address, peek, ud, L);
        add_address = 0;
        continue;
      
      case RC_CONDITION_SUB_SOURCE:
        add_value -= rc_evaluate_operand(&condition->operand1, add_address, peek, ud, L);
        add_address = 0;
        continue;
      
      case RC_CONDITION_ADD_HITS:
        /* always evaluate the condition to ensure everything is updated correctly */
        cond_valid = rc_test_condition(condition, add_value, add_address, peek, ud, L);

        /* merge AndNext value and reset it for the next condition */
        cond_valid &= prev_cond;
        prev_cond = 1;

        /* if the condition is true, tally it */
        if (cond_valid) {
          if (condition->required_hits == 0 || condition->current_hits < condition->required_hits) {
            condition->current_hits++;
          }
        }

        add_value = 0;
        add_address = 0;
        add_hits += condition->current_hits;
        continue;

      case RC_CONDITION_AND_NEXT:
        prev_cond &= rc_test_condition(condition, add_value, add_address, peek, ud, L);
        add_value = 0;
        add_address = 0;
        continue;

      case RC_CONDITION_ADD_ADDRESS:
        add_address = rc_evaluate_operand(&condition->operand1, add_address, peek, ud, L);
        continue;
    }

    /* always evaluate the condition to ensure everything is updated correctly */
    cond_valid = rc_test_condition(condition, add_value, add_address, peek, ud, L);

    /* merge AndNext value and reset it for the next condition */
    cond_valid &= prev_cond;
    prev_cond = 1;

    /* if the condition has a target hit count that has already been met, it's automatically true, even if not currently true. */
    if (condition->required_hits != 0 && (condition->current_hits + add_hits) >= condition->required_hits) {
      cond_valid = 1;
    }
    else if (cond_valid) {
      condition->current_hits++;

      if (condition->required_hits == 0) {
        /* not a hit-based requirement: ignore any additional logic! */
      }
      else if ((condition->current_hits + add_hits) < condition->required_hits) {
        /* HitCount target has not yet been met, condition is not yet valid */
        cond_valid = 0;
      }
    }

    /* reset AddHits and AddSource/SubSource values */
    add_value = add_hits = add_address = 0;

    switch (condition->type) {
      case RC_CONDITION_PAUSE_IF:
        /* as soon as we find a PauseIf that evaluates to true, stop processing the rest of the group */
        if (cond_valid) {
          return 1;
        }

        /* if we make it to the end of the function, make sure we indicate that nothing matched. if we do find
           a later PauseIf match, it'll automatically return true via the previous condition. */
        set_valid = 0;

        if (condition->required_hits == 0) {
          /* PauseIf didn't evaluate true, and doesn't have a HitCount, reset the HitCount to indicate the condition didn't match */
          condition->current_hits = 0;
        }
        else {
          /* PauseIf has a HitCount that hasn't been met, ignore it for now. */
        }

        break;
      
      case RC_CONDITION_RESET_IF:
        if (cond_valid) {
          *reset = 1; /* let caller know to reset all hit counts */
          set_valid = 0; /* cannot be valid if we've hit a reset condition */
        }

        break;

      default:
        set_valid &= cond_valid;
        break;
    }
  }

  return set_valid;
}

int rc_test_condset(rc_condset_t* self, int* reset, rc_peek_t peek, void* ud, lua_State* L) {
  if (self->conditions == 0) {
    /* important: empty group must evaluate true */
    return 1;
  }

  if (self->has_pause && rc_test_condset_internal(self, 1, reset, peek, ud, L)) {
    /* one or more Pause conditions exists, if any of them are true, stop processing this group */
    return 0;
  }

  return rc_test_condset_internal(self, 0, reset, peek, ud, L);
}

void rc_reset_condset(rc_condset_t* self) {
  rc_condition_t* condition;

  for (condition = self->conditions; condition != 0; condition = condition->next) {
    condition->current_hits = 0;
  }
}
