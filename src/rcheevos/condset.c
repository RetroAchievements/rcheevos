#include "rc_internal.h"

#include <string.h> /* memcpy */

enum {
  RC_CONDITION_CLASSIFICATION_COMBINING,
  RC_CONDITION_CLASSIFICATION_PAUSE,
  RC_CONDITION_CLASSIFICATION_RESET,
  RC_CONDITION_CLASSIFICATION_HITTARGET,
  RC_CONDITION_CLASSIFICATION_MEASURED,
  RC_CONDITION_CLASSIFICATION_OTHER,
  RC_CONDITION_CLASSIFICATION_INDIRECT
};

static int rc_classify_condition(const rc_condition_t* cond) {
  switch (cond->type) {
    case RC_CONDITION_PAUSE_IF:
      return RC_CONDITION_CLASSIFICATION_PAUSE;

    case RC_CONDITION_RESET_IF:
      return RC_CONDITION_CLASSIFICATION_RESET;

    case RC_CONDITION_ADD_ADDRESS:
      return RC_CONDITION_CLASSIFICATION_INDIRECT;

    case RC_CONDITION_ADD_HITS:
    case RC_CONDITION_ADD_SOURCE:
    case RC_CONDITION_AND_NEXT:
    case RC_CONDITION_OR_NEXT:
    case RC_CONDITION_REMEMBER:
    case RC_CONDITION_RESET_NEXT_IF:
    case RC_CONDITION_SUB_HITS:
    case RC_CONDITION_SUB_SOURCE:
      return RC_CONDITION_CLASSIFICATION_COMBINING;

    case RC_CONDITION_MEASURED:
    case RC_CONDITION_MEASURED_IF:
      /* even if not measuring a hit target, we still want to evaluate it every frame */
      return RC_CONDITION_CLASSIFICATION_MEASURED;

    default:
      if (cond->required_hits != 0)
        return RC_CONDITION_CLASSIFICATION_HITTARGET;

      return RC_CONDITION_CLASSIFICATION_OTHER;
  }
}

static int32_t rc_classify_conditions(rc_condset_t* self, const char* memaddr) {
  rc_parse_state_t parse;
  rc_memref_t* memrefs;
  rc_condition_t condition;
  int classification;
  uint32_t index = 0;
  uint32_t chain_length = 1;
  uint32_t add_address_count = 0;

  rc_init_parse_state(&parse, NULL, NULL, 0);
  rc_init_parse_state_memrefs(&parse, &memrefs);

  do {
    rc_parse_condition_internal(&condition, &memaddr, &parse);

    if (parse.offset < 0)
      return parse.offset;

    ++index;

    classification = rc_classify_condition(&condition);
    switch (classification) {
      case RC_CONDITION_CLASSIFICATION_COMBINING:
        ++chain_length;
        continue;

      case RC_CONDITION_CLASSIFICATION_INDIRECT:
        ++self->num_indirect_conditions;
        continue;

      case RC_CONDITION_CLASSIFICATION_PAUSE:
        self->num_pause_conditions += chain_length;
        break;

      case RC_CONDITION_CLASSIFICATION_RESET:
        self->num_reset_conditions += chain_length;
        break;

      case RC_CONDITION_CLASSIFICATION_HITTARGET:
        self->num_hittarget_conditions += chain_length;
        break;

      case RC_CONDITION_CLASSIFICATION_MEASURED:
        self->num_measured_conditions += chain_length;
        break;

      default:
        self->num_other_conditions += chain_length;
        break;
    }

    chain_length = 1;
  } while (*memaddr++ == '_');

  return index;
}

static int rc_find_next_classification(const char* memaddr) {
  rc_parse_state_t parse;
  rc_memref_t* memrefs;
  rc_condition_t condition;
  int classification;

  rc_init_parse_state(&parse, NULL, NULL, 0);
  rc_init_parse_state_memrefs(&parse, &memrefs);

  do {
    rc_parse_condition_internal(&condition, &memaddr, &parse);

    classification = rc_classify_condition(&condition);
    switch (classification) {
      case RC_CONDITION_CLASSIFICATION_COMBINING:
      case RC_CONDITION_CLASSIFICATION_INDIRECT:
        break;

      default:
        return classification;
    }
  } while (*memaddr++ == '_');

  return RC_CONDITION_CLASSIFICATION_OTHER;
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

static void rc_update_condition_pause_remember(rc_condset_t* self) {
  rc_operand_t* pause_remember = NULL;
  rc_condition_t* condition;
  rc_condition_t* pause_conditions;
  const rc_condition_t* end_pause_condition;

  /* ASSERT: pause conditions are first conditions */
  pause_conditions = rc_condset_get_conditions(self);
  end_pause_condition = pause_conditions + self->num_pause_conditions;

  for (condition = pause_conditions; condition < end_pause_condition; ++condition) {
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
    for (condition = self->conditions; condition; condition = condition->next) {
      if (condition >= end_pause_condition) {
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
  rc_condition_t condition;
  rc_condition_t* conditions;
  rc_condition_t** next;
  rc_condition_t* pause_conditions = NULL;
  rc_condition_t* reset_conditions = NULL;
  rc_condition_t* hittarget_conditions = NULL;
  rc_condition_t* measured_conditions = NULL;
  rc_condition_t* other_conditions = NULL;
  rc_condition_t* indirect_conditions = NULL;
  int classification, combining_classification = RC_CONDITION_CLASSIFICATION_COMBINING;
  uint32_t measured_target = 0;
  int32_t result;

  self = RC_ALLOC(rc_condset_t, parse);
  memset(self, 0, sizeof(*self));

  if (**memaddr == 'S' || **memaddr == 's' || !**memaddr) {
    /* empty group - editor allows it, so we have to support it */
    return self;
  }

  result = rc_classify_conditions(self, *memaddr);
  if (result < 0) {
    parse->offset = result;
    return NULL;
  }

  conditions = rc_alloc(parse->buffer, &parse->offset, result * sizeof(rc_condition_t), RC_ALIGNOF(rc_condition_t), NULL, 0);
  if (parse->offset < 0)
    return NULL;

  if (parse->buffer) {
    pause_conditions = conditions;
    conditions += self->num_pause_conditions;

    reset_conditions = conditions;
    conditions += self->num_reset_conditions;

    hittarget_conditions = conditions;
    conditions += self->num_hittarget_conditions;

    measured_conditions = conditions;
    conditions += self->num_measured_conditions;

    other_conditions = conditions;
    conditions += self->num_other_conditions;

    indirect_conditions = conditions;
  }

  next = &self->conditions;


  /* each condition set has a functionally new recall accumulator */
  parse->remember.type = RC_OPERAND_NONE;

  for (;;) {
    rc_parse_condition_internal(&condition, memaddr, parse);

    if (parse->offset < 0)
      return NULL;

    if (condition.oper == RC_OPERATOR_NONE) {
      switch (condition.type) {
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
          return NULL;
      }
    }

    switch (condition.type) {
      case RC_CONDITION_MEASURED:
        if (measured_target != 0) {
          /* multiple Measured flags cannot exist in the same group */
          parse->offset = RC_MULTIPLE_MEASURED;
          return 0;
        }
        else if (parse->is_value) {
          measured_target = (uint32_t)-1;
          if (!rc_operator_is_modifying(condition.oper)) {
            /* measuring comparison in a value results in a tally (hit count). set target to MAX_INT */
            condition.required_hits = measured_target;
          }
        }
        else if (condition.required_hits != 0) {
          measured_target = condition.required_hits;
        }
        else if (condition.operand2.type == RC_OPERAND_CONST) {
          measured_target = condition.operand2.value.num;
        }
        else if (condition.operand2.type == RC_OPERAND_FP) {
          measured_target = (unsigned)condition.operand2.value.dbl;
        }
        else {
          parse->offset = RC_INVALID_MEASURED_TARGET;
          return NULL;
        }

        if (parse->measured_target && measured_target != parse->measured_target) {
          /* multiple Measured flags in separate groups must have the same target */
          parse->offset = RC_MULTIPLE_MEASURED;
          return NULL;
        }

        parse->measured_target = measured_target;
        break;

      case RC_CONDITION_STANDARD:
      case RC_CONDITION_TRIGGER:
        /* these flags are not allowed in value expressions */
        if (parse->is_value) {
          parse->offset = RC_INVALID_VALUE_FLAG;
          return NULL;
        }
        break;
    }

    rc_condition_update_parse_state(&condition, parse);

    if (parse->buffer) {
      classification = rc_classify_condition(&condition);
      if (classification == RC_CONDITION_CLASSIFICATION_COMBINING) {
        if (combining_classification == RC_CONDITION_CLASSIFICATION_COMBINING)
          combining_classification = rc_find_next_classification(&(*memaddr)[1]); /* skip over '_' */

        classification = combining_classification;
      }
      else {
        combining_classification = RC_CONDITION_CLASSIFICATION_COMBINING;
      }

      switch (classification) {
        case RC_CONDITION_CLASSIFICATION_PAUSE:
          memcpy(pause_conditions, &condition, sizeof(condition));
          *next = pause_conditions++;
          break;

        case RC_CONDITION_CLASSIFICATION_RESET:
          memcpy(reset_conditions, &condition, sizeof(condition));
          *next = reset_conditions++;
          break;

        case RC_CONDITION_CLASSIFICATION_HITTARGET:
          memcpy(hittarget_conditions, &condition, sizeof(condition));
          *next = hittarget_conditions++;
          break;

        case RC_CONDITION_CLASSIFICATION_MEASURED:
          memcpy(measured_conditions, &condition, sizeof(condition));
          *next = measured_conditions++;
          break;

        case RC_CONDITION_CLASSIFICATION_INDIRECT:
          memcpy(indirect_conditions, &condition, sizeof(condition));
          *next = indirect_conditions++;
          break;

        default:
          memcpy(other_conditions, &condition, sizeof(condition));
          *next = other_conditions++;
          break;
      }

      next = &(*next)->next;
    }

    if (**memaddr != '_')
      break;

    (*memaddr)++;
  }

  *next = NULL;

  self->has_pause = self->num_pause_conditions > 0;
  if (self->has_pause && parse->buffer && parse->remember.type != RC_OPERAND_NONE)
    rc_update_condition_pause_remember(self);

  return self;
}

static int rc_test_condset_internal(rc_condition_t* condition, uint32_t num_conditions,
                                    rc_eval_state_t* eval_state, int can_short_circuit) {
  const rc_condition_t* condition_end = condition + num_conditions;
  int set_valid, cond_valid, and_next, or_next, reset_next, measured_from_hits, can_measure;
  rc_typed_value_t measured_value;
  uint32_t total_hits;

  measured_value.type = RC_VALUE_TYPE_NONE;
  measured_from_hits = 0;
  can_measure = 1;
  total_hits = 0;

  set_valid = 1;
  and_next = 1;
  or_next = 0;
  reset_next = 0;

  for (; condition < condition_end; ++condition) {
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

          if (can_short_circuit)
            return 0;

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
        if (!cond_valid && can_short_circuit)
          return 0;

        /* update truthiness of set, but do not update truthiness of primed state */
        set_valid &= cond_valid;
        continue;

      default:
        break;
    }

    /* STEP 5: update overall truthiness of set and primed state */
    eval_state->primed &= cond_valid;
    set_valid &= cond_valid;

    if (!cond_valid && can_short_circuit)
      return 0;
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

rc_condition_t* rc_condset_get_conditions(rc_condset_t* self) {
  if (self->conditions) {
    /* raw_pointer will point at the data immediately following the condset, but when the conditions are
     * allocated by rc_alloc, it may adjust that pointer to be aligned by the RC_ALIGNOF(rc_condition_t).
     * this logic attempts to find the first element of the array by finding an element in the array
     * and finding the closest aligned element to the raw_pointer. */
    const uint8_t* raw_pointer = (uint8_t*)self + RC_ALIGN(sizeof(*self));
    const size_t first_condition_index = ((uint8_t*)self->conditions - raw_pointer) / sizeof(rc_condition_t);
    rc_condition_t* aligned_pointer = self->conditions - first_condition_index;
    return aligned_pointer;
  }

  return NULL;
}

int rc_test_condset(rc_condset_t* self, rc_eval_state_t* eval_state) {
  rc_condition_t* conditions;
  int result = 1; /* true until proven otherwise */

  eval_state->primed = 1;
  eval_state->add_hits = 0;

  conditions = rc_condset_get_conditions(self);

  if (self->num_pause_conditions) {
    /* one or more Pause conditions exists. if any of them are true, stop processing this group */
    self->is_paused = (char)rc_test_condset_internal(conditions, self->num_pause_conditions, eval_state, 0);

    if (self->is_paused) {
      /* condset is paused. stop processing immediately. */
      eval_state->primed = 0;
      return 0;
    }

    conditions += self->num_pause_conditions;
  }

  if (self->num_reset_conditions) {
    /* one or more Reset conditions exists. if any of them are true, rc_test_condset_internal
     * will return false. clear hits and stop processing this group */
    result &= rc_test_condset_internal(conditions, self->num_reset_conditions, eval_state, eval_state->can_short_curcuit);
    conditions += self->num_reset_conditions;
  }

  if (self->num_hittarget_conditions) {
    /* one or more hit target conditions exists. these must be processed every frame, unless their hit count is going to be reset */
    if (!eval_state->was_reset)
      result &= rc_test_condset_internal(conditions, self->num_hittarget_conditions, eval_state, 0);

    conditions += self->num_hittarget_conditions;
  }

  if (self->num_measured_conditions) {
    /* measured value must be calculated every frame, even if hit counts will be reset */
    result &= rc_test_condset_internal(conditions, self->num_measured_conditions, eval_state, 0);
    conditions += self->num_measured_conditions;
  }

  if (self->num_other_conditions) {
    /* remaining conditions only need to be evaluated if the rest of the condset is true */
    if (result)
      result &= rc_test_condset_internal(conditions, self->num_other_conditions, eval_state, eval_state->can_short_curcuit);
    /* something else is false. if we can't short circuit, and there wasn't a reset, we still need to evaluate these */
    else if (!eval_state->can_short_curcuit && !eval_state->was_reset)
      result &= rc_test_condset_internal(conditions, self->num_other_conditions, eval_state, eval_state->can_short_curcuit);
  }

  return result;
}

void rc_reset_condset(rc_condset_t* self) {
  rc_condition_t* condition;

  for (condition = self->conditions; condition != 0; condition = condition->next) {
    condition->current_hits = 0;
  }
}
