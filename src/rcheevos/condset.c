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
    case RC_CONDITION_OR_NEXT:
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
  self->has_pause = self->is_paused = 0;
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
        case RC_CONDITION_OR_NEXT:
            break;

        default:
          parse->offset = RC_INVALID_OPERATOR;
          return 0;
      }
    }

    self->has_pause |= (*next)->type == RC_CONDITION_PAUSE_IF;
    in_add_address = (*next)->type == RC_CONDITION_ADD_ADDRESS;

    if ((*next)->type == RC_CONDITION_MEASURED) {
      unsigned measured_target = 0;
      if ((*next)->required_hits == 0) {
        if ((*next)->operand2.type != RC_OPERAND_CONST) {
           parse->offset = RC_INVALID_MEASURED_TARGET;
           return 0;
        }

        measured_target = (*next)->operand2.value.num;
      }
      else {
        measured_target = (*next)->required_hits;
      }

      if (parse->measured_target && measured_target != parse->measured_target) {
        parse->offset = RC_MULTIPLE_MEASURED;
        return 0;
      }

      parse->measured_target = measured_target;
    }

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

static int rc_test_condset_internal(rc_condset_t* self, int processing_pause, rc_eval_state_t* eval_state) {
  rc_condition_t* condition;
  int set_valid, cond_valid, and_next, or_next;
  unsigned measured_value, total_hits;

  set_valid = 1;
  and_next = 1;
  or_next = 0;
  eval_state->add_value = eval_state->add_hits = eval_state->add_address = 0;

  for (condition = self->conditions; condition != 0; condition = condition->next) {
    if (condition->pause != processing_pause) {
      continue;
    }

    /* process modifier conditions */
    switch (condition->type) {
      case RC_CONDITION_ADD_SOURCE:
        eval_state->add_value += rc_evaluate_operand(&condition->operand1, eval_state);
        eval_state->add_address = 0;
        continue;
      
      case RC_CONDITION_SUB_SOURCE:
        eval_state->add_value -= rc_evaluate_operand(&condition->operand1, eval_state);
        eval_state->add_address = 0;
        continue;

      case RC_CONDITION_ADD_ADDRESS:
        eval_state->add_address = rc_evaluate_operand(&condition->operand1, eval_state);
        continue;

      case RC_CONDITION_MEASURED:
        if (condition->required_hits == 0) {
          measured_value = rc_evaluate_operand(&condition->operand1, eval_state) + eval_state->add_value;
        }
        break;

      default:
        break;
    }

    /* evaluate the current condition */
    condition->is_true = rc_test_condition(condition, eval_state);
    eval_state->add_value = 0;
    eval_state->add_address = 0;

    /* apply logic flags and reset them for the next condition */
    cond_valid = condition->is_true;
    cond_valid &= and_next;
    cond_valid |= or_next;
    and_next = 1;
    or_next = 0;

    /* true conditions should update hit count */
    if (cond_valid) {
      eval_state->has_hits = 1;

      if (condition->required_hits == 0 ||
          (condition->current_hits + eval_state->add_hits) < condition->required_hits) {
        ++condition->current_hits;
      }
    }
    else if (condition->current_hits > 0) {
      eval_state->has_hits = 1;
    }

    /* handle logic flags */
    switch (condition->type) {
      case RC_CONDITION_ADD_HITS:
        eval_state->add_hits += condition->current_hits;
        continue;

      case RC_CONDITION_AND_NEXT:
        /* don't count hits on non-final clause of compound condition */
        condition->current_hits = 0;
        and_next = cond_valid;
        continue;

      case RC_CONDITION_OR_NEXT:
        /* don't count hits on non-final clause of compound condition */
        condition->current_hits = 0;
        or_next = cond_valid;
        continue;

      default:
        break;
    }

    /* calculate the total hit count and reset the AddHits counter */
    total_hits = (condition->current_hits + eval_state->add_hits);
    eval_state->add_hits = 0;

    /* if the condition has a target hit count, the overall truthiness of the condition (cond_valid)
       is determined by that, not the individual truthiness (condition->is_true). */
    if (condition->required_hits != 0) {
      cond_valid = (total_hits >= condition->required_hits);
      measured_value = total_hits;
    }

    /* handle special flags */
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

        continue;
      
      case RC_CONDITION_RESET_IF:
        if (cond_valid) {
          eval_state->was_reset = 1; /* let caller know to reset all hit counts */
          set_valid = 0; /* cannot be valid if we've hit a reset condition */
        }
        continue;

      case RC_CONDITION_MEASURED:
        if (measured_value > eval_state->measured_value) {
          eval_state->measured_value = measured_value;
        }
        break;

      default:
        break;
    }

    /* update overall truthiness of set */
    set_valid &= cond_valid;
  }

  return set_valid;
}

int rc_test_condset(rc_condset_t* self, rc_eval_state_t* eval_state) {
  if (self->conditions == 0) {
    /* important: empty group must evaluate true */
    return 1;
  }

  if (self->has_pause) {
    if ((self->is_paused = rc_test_condset_internal(self, 1, eval_state))) {
      /* one or more Pause conditions exists, if any of them are true, stop processing this group */
      return 0;
    }
  }

  return rc_test_condset_internal(self, 0, eval_state);
}

void rc_reset_condset(rc_condset_t* self) {
  rc_condition_t* condition;

  for (condition = self->conditions; condition != 0; condition = condition->next) {
    condition->current_hits = 0;
  }
}
