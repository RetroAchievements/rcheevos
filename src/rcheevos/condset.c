#include "rc_internal.h"

#include <string.h> /* memcpy */

static void rc_update_condition_pause(rc_condition_t* condition) {
  rc_condition_t* subclause = condition;

  while (condition) {
    if (condition->type == RC_CONDITION_PAUSE_IF) {
      while (subclause != condition) {
        subclause->pause = 1;
        subclause = subclause->next;
      }
      condition->pause = 1;
    }
    else {
      condition->pause = 0;
    }

    if (!rc_condition_is_combining(condition))
      subclause = condition->next;

    condition = condition->next;
  }
}

static void rc_condition_update_recall_operand(rc_operand_t* operand, const rc_operand_t* remember)
{
  if (operand->type == RC_OPERAND_RECALL) {
    if (rc_operand_type_is_memref(operand->memref_access_type) && operand->value.memref == NULL) {
      memcpy(operand, remember, sizeof(*remember));
      operand->memref_access_type = operand->type;
      operand->type = RC_OPERAND_RECALL;
    }
  }
  else if (rc_operand_is_memref(operand) && operand->value.memref->value.memref_type == RC_MEMREF_TYPE_MODIFIED_MEMREF) {
    rc_modified_memref_t* modified_memref = (rc_modified_memref_t*)operand->value.memref;
    rc_condition_update_recall_operand(&modified_memref->parent, remember);
    rc_condition_update_recall_operand(&modified_memref->modifier, remember);
  }
}

static void rc_update_condition_pause_remember(rc_condition_t* conditions, rc_memref_t* memrefs) {
  rc_operand_t* pause_remember = NULL;
  rc_condition_t* condition;

  for (condition = conditions; condition; condition = condition->next) {
    if (!condition->pause)
      continue;

    if (condition->type == RC_CONDITION_REMEMBER) {
      pause_remember = &condition->operand1;
    }
    else if (pause_remember == NULL) {
      /* if we picked up a non-pause remember, discard it */
      if (condition->operand1.type == RC_OPERAND_RECALL &&
          rc_operand_type_is_memref(condition->operand1.memref_access_type)) {
        condition->operand1.value.memref = NULL;
      }

      if (condition->operand2.type == RC_OPERAND_RECALL &&
          rc_operand_type_is_memref(condition->operand2.memref_access_type)) {
        condition->operand2.value.memref = NULL;
      }
    }
  }

  if (pause_remember) {
    for (condition = conditions; condition; condition = condition->next) {
      if (!condition->pause) {
        /* if we didn't find a remember for a non-pause condition, use the last pause remember */
        rc_condition_update_recall_operand(&condition->operand1, pause_remember);
        rc_condition_update_recall_operand(&condition->operand2, pause_remember);
      }

      /* Anything after this point will have already been handled */
      if (condition->type == RC_CONDITION_REMEMBER)
        break;
    }
  }
}

rc_condset_t* rc_parse_condset(const char** memaddr, rc_parse_state_t* parse) {
  rc_condset_t* self;
  rc_condition_t** next;
  rc_condition_t* condition;
  uint32_t measured_target = 0;

  self = RC_ALLOC(rc_condset_t, parse);
  self->has_pause = self->is_paused = 0;
  next = &self->conditions;

  if (**memaddr == 'S' || **memaddr == 's' || !**memaddr) {
    /* empty group - editor allows it, so we have to support it */
    *next = 0;
    return self;
  }

  /* each condition set has a functionally new recall accumulator */
  parse->remember.type = RC_OPERAND_NONE;

  for (;;) {
    condition = rc_parse_condition(memaddr, parse);
    *next = condition;

    if (parse->offset < 0)
      return 0;

    if (condition->oper == RC_OPERATOR_NONE) {
      switch (condition->type) {
        case RC_CONDITION_ADD_ADDRESS:
        case RC_CONDITION_ADD_SOURCE:
        case RC_CONDITION_SUB_SOURCE:
        case RC_CONDITION_REMEMBER:
          /* these conditions don't require a right hand size (implied *1) */
          break;

        case RC_CONDITION_MEASURED:
          /* right hand side is not required when Measured is used in a value */
          if (parse->is_value)
            break;
          /* fallthrough */ /* to default */

        default:
          parse->offset = RC_INVALID_OPERATOR;
          return 0;
      }
    }

    switch (condition->type) {
      case RC_CONDITION_MEASURED:
        if (measured_target != 0) {
          /* multiple Measured flags cannot exist in the same group */
          parse->offset = RC_MULTIPLE_MEASURED;
          return 0;
        }
        else if (parse->is_value) {
          measured_target = (unsigned)-1;
          switch (condition->oper) {
            case RC_OPERATOR_AND:
            case RC_OPERATOR_XOR:
            case RC_OPERATOR_DIV:
            case RC_OPERATOR_MULT:
            case RC_OPERATOR_MOD:
            case RC_OPERATOR_ADD:
            case RC_OPERATOR_SUB:
            case RC_OPERATOR_NONE:
              /* measuring value. leave required_hits at 0 */
              break;

            default:
              /* comparison operator, measuring hits. set required_hits to MAX_INT */
              condition->required_hits = measured_target;
              break;
          }
        }
        else if (condition->required_hits != 0) {
          measured_target = condition->required_hits;
        }
        else if (condition->operand2.type == RC_OPERAND_CONST) {
          measured_target = condition->operand2.value.num;
        }
        else if (condition->operand2.type == RC_OPERAND_FP) {
          measured_target = (unsigned)condition->operand2.value.dbl;
        }
        else {
          parse->offset = RC_INVALID_MEASURED_TARGET;
          return 0;
        }

        if (parse->measured_target && measured_target != parse->measured_target) {
          /* multiple Measured flags in separate groups must have the same target */
          parse->offset = RC_MULTIPLE_MEASURED;
          return 0;
        }

        parse->measured_target = measured_target;
        break;

      case RC_CONDITION_STANDARD:
      case RC_CONDITION_TRIGGER:
        /* these flags are not allowed in value expressions */
        if (parse->is_value) {
          parse->offset = RC_INVALID_VALUE_FLAG;
          return 0;
        }
        break;
    }

    rc_condition_update_parse_state(condition, parse);

    self->has_pause |= condition->type == RC_CONDITION_PAUSE_IF;

    next = &condition->next;

    if (**memaddr != '_')
      break;

    (*memaddr)++;
  }

  *next = 0;

  if (parse->buffer && self->has_pause) {
    rc_update_condition_pause(self->conditions);

    if (parse->remember.type != RC_OPERATOR_NONE)
      rc_update_condition_pause_remember(self->conditions, *parse->first_memref);
  }

  return self;
}

static int rc_test_condset_internal(rc_condset_t* self, int processing_pause, rc_eval_state_t* eval_state) {
  rc_condition_t* condition;
  int set_valid, cond_valid, and_next, or_next, reset_next, measured_from_hits, can_measure;
  rc_typed_value_t measured_value;
  uint32_t total_hits;

  measured_value.type = RC_VALUE_TYPE_NONE;
  measured_from_hits = 0;
  can_measure = 1;
  total_hits = 0;

  eval_state->primed = 1;
  set_valid = 1;
  and_next = 1;
  or_next = 0;
  reset_next = 0;
  eval_state->add_hits = 0;

  for (condition = self->conditions; condition != 0; condition = condition->next) {
    if (condition->pause != processing_pause)
      continue;

    /* STEP 1: process modifier conditions */
    switch (condition->type) {
      case RC_CONDITION_ADD_SOURCE:
      case RC_CONDITION_SUB_SOURCE:
      case RC_CONDITION_ADD_ADDRESS:
      case RC_CONDITION_REMEMBER:
        /* these are all managed by rc_modified_memref_t now */
        continue;

      case RC_CONDITION_MEASURED:
        if (condition->required_hits == 0 && can_measure) {
          /* Measured condition without a hit target measures the value of the left operand */
          rc_evaluate_operand(&measured_value, &condition->operand1, eval_state);
        }
        break;

      default:
        break;
    }

    /* STEP 2: evaluate the current condition */
    condition->is_true = (uint8_t)rc_test_condition(condition, eval_state);

    /* apply logic flags and reset them for the next condition */
    cond_valid = condition->is_true;
    cond_valid &= and_next;
    cond_valid |= or_next;
    and_next = 1;
    or_next = 0;

    if (reset_next) {
      /* previous ResetNextIf resets the hit count on this condition and prevents it from being true */
      if (condition->current_hits)
        eval_state->was_cond_reset = 1;

      condition->current_hits = 0;
      cond_valid = 0;
    }
    else if (cond_valid) {
      /* true conditions should update hit count */
      eval_state->has_hits = 1;

      if (condition->required_hits == 0) {
        /* no target hit count, just keep tallying */
        ++condition->current_hits;
      }
      else if (condition->current_hits < condition->required_hits) {
        /* target hit count hasn't been met, tally and revalidate - only true if hit count becomes met */
        ++condition->current_hits;
        cond_valid = (condition->current_hits == condition->required_hits);
      }
      else {
        /* target hit count has been met, do nothing */
      }
    }
    else if (condition->current_hits > 0) {
      /* target has been true in the past, if the hit target is met, consider it true now */
      eval_state->has_hits = 1;
      cond_valid = (condition->current_hits == condition->required_hits);
    }

    /* STEP 3: handle logic flags */
    switch (condition->type) {
      case RC_CONDITION_ADD_HITS:
        eval_state->add_hits += condition->current_hits;
        reset_next = 0; /* ResetNextIf was applied to this AddHits condition; don't apply it to future conditions */
        continue;

      case RC_CONDITION_SUB_HITS:
        eval_state->add_hits -= condition->current_hits;
        reset_next = 0; /* ResetNextIf was applied to this AddHits condition; don't apply it to future conditions */
        continue;

      case RC_CONDITION_RESET_NEXT_IF:
        reset_next = cond_valid;
        continue;

      case RC_CONDITION_AND_NEXT:
        and_next = cond_valid;
        continue;

      case RC_CONDITION_OR_NEXT:
        or_next = cond_valid;
        continue;

      default:
        break;
    }

    /* reset logic flags for next condition */
    reset_next = 0;

    /* STEP 4: calculate total hits */
    total_hits = condition->current_hits;

    if (eval_state->add_hits) {
      if (condition->required_hits != 0) {
        /* if the condition has a target hit count, we have to recalculate cond_valid including the AddHits counter */
        const int signed_hits = (int)condition->current_hits + eval_state->add_hits;
        total_hits = (signed_hits >= 0) ? (unsigned)signed_hits : 0;
        cond_valid = (total_hits >= condition->required_hits);
      }
      else {
        /* no target hit count. we can't tell if the add_hits value is from this frame or not, so ignore it.
           complex condition will only be true if the current condition is true */
      }

      eval_state->add_hits = 0;
    }

    /* STEP 5: handle special flags */
    switch (condition->type) {
      case RC_CONDITION_PAUSE_IF:
        /* as soon as we find a PauseIf that evaluates to true, stop processing the rest of the group */
        if (cond_valid)
          return 1;

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
        if (condition->required_hits != 0) {
          /* if there's a hit target, capture the current hits for recording Measured value later */
          measured_from_hits = 1;
          if (can_measure) {
            measured_value.value.u32 = total_hits;
            measured_value.type = RC_VALUE_TYPE_UNSIGNED;
          }
        }
        break;

      case RC_CONDITION_MEASURED_IF:
        if (!cond_valid) {
          measured_value.value.u32 = 0;
          measured_value.type = RC_VALUE_TYPE_UNSIGNED;
          can_measure = 0;
        }
        break;

      case RC_CONDITION_TRIGGER:
        /* update truthiness of set, but do not update truthiness of primed state */
        set_valid &= cond_valid;
        continue;

      default:
        break;
    }

    /* STEP 5: update overall truthiness of set and primed state */
    eval_state->primed &= cond_valid;
    set_valid &= cond_valid;
  }

  if (measured_value.type != RC_VALUE_TYPE_NONE) {
    /* if no previous Measured value was captured, or the new one is greater, keep the new one */
    if (eval_state->measured_value.type == RC_VALUE_TYPE_NONE ||
        rc_typed_value_compare(&measured_value, &eval_state->measured_value, RC_OPERATOR_GT)) {
      memcpy(&eval_state->measured_value, &measured_value, sizeof(measured_value));
      eval_state->measured_from_hits = (uint8_t)measured_from_hits;
    }
  }

  return set_valid;
}

int rc_test_condset(rc_condset_t* self, rc_eval_state_t* eval_state) {
  if (self->conditions == 0) {
    /* important: empty group must evaluate true */
    return 1;
  }

  if (self->has_pause) {
    /* one or more Pause conditions exists, if any of them are true, stop processing this group */
    self->is_paused = (char)rc_test_condset_internal(self, 1, eval_state);
    if (self->is_paused) {
      eval_state->primed = 0;
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
