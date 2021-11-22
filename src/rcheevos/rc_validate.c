#include "rc_validate.h"

#include "rc_compat.h"
#include "rc_internal.h"

#include <stddef.h>
#include <stdlib.h>

static unsigned rc_max_value(const rc_operand_t* operand)
{
  if (operand->type == RC_OPERAND_CONST)
    return operand->value.num;

  if (!rc_operand_is_memref(operand))
    return 0xFFFFFFFF;

  switch (operand->size) {
    case RC_MEMSIZE_BIT_0:
    case RC_MEMSIZE_BIT_1:
    case RC_MEMSIZE_BIT_2:
    case RC_MEMSIZE_BIT_3:
    case RC_MEMSIZE_BIT_4:
    case RC_MEMSIZE_BIT_5:
    case RC_MEMSIZE_BIT_6:
    case RC_MEMSIZE_BIT_7:
      return 1;

    case RC_MEMSIZE_LOW:
    case RC_MEMSIZE_HIGH:
      return 0xF;

    case RC_MEMSIZE_BITCOUNT:
      return 8;

    case RC_MEMSIZE_8_BITS:
      return 0xFF;

    case RC_MEMSIZE_16_BITS:
    case RC_MEMSIZE_16_BITS_BE:
      return 0xFFFF;

    case RC_MEMSIZE_24_BITS:
    case RC_MEMSIZE_24_BITS_BE:
      return 0xFFFFFF;

    default:
      return 0xFFFFFFFF;
  }
}

static int rc_validate_range(unsigned min_val, unsigned max_val, char oper, unsigned max, char result[], const size_t result_size) {
  switch (oper) {
    case RC_OPERATOR_AND:
      if (min_val > max) {
        snprintf(result, result_size, "Mask has more bits than source");
        return 0;
      }
      else if (min_val == 0 && max_val == 0) {
        snprintf(result, result_size, "Result of mask always 0");
        return 0;
      }
      break;

    case RC_OPERATOR_EQ:
      if (min_val > max) {
        snprintf(result, result_size, "Comparison is never true");
        return 0;
      }
      break;

    case RC_OPERATOR_NE:
      if (min_val > max) {
        snprintf(result, result_size, "Comparison is always true");
        return 0;
      }
      break;

    case RC_OPERATOR_GE:
      if (min_val > max) {
        snprintf(result, result_size, "Comparison is never true");
        return 0;
      }
      if (max_val == 0) {
        snprintf(result, result_size, "Comparison is always true");
        return 0;
      }
      break;

    case RC_OPERATOR_GT:
      if (min_val >= max) {
        snprintf(result, result_size, "Comparison is never true");
        return 0;
      }
      break;

    case RC_OPERATOR_LE:
      if (min_val >= max) {
        snprintf(result, result_size, "Comparison is always true");
        return 0;
      }
      break;

    case RC_OPERATOR_LT:
      if (min_val > max) {
        snprintf(result, result_size, "Comparison is always true");
        return 0;
      }
      if (max_val == 0) {
        snprintf(result, result_size, "Comparison is never true");
        return 0;
      }
      break;
  }

  return 1;
}

int rc_validate_condset(const rc_condset_t* condset, char result[], const size_t result_size, unsigned max_address) {
  const rc_condition_t* cond;
  unsigned max_val;
  int index = 1;
  unsigned long long add_source_max = 0;
  int in_add_hits = 0;
  int in_add_address = 0;
  int is_combining = 0;

  if (!condset) {
    *result = '\0';
    return 1;
  }

  for (cond = condset->conditions; cond; cond = cond->next, ++index) {
    unsigned max = rc_max_value(&cond->operand1);
    const int is_memref1 = rc_operand_is_memref(&cond->operand1);
    const int is_memref2 = rc_operand_is_memref(&cond->operand2);

    if (!in_add_address) {
      if (is_memref1 && cond->operand1.value.memref->address > max_address) {
        snprintf(result, result_size, "Condition %d: Address %04X out of range (max %04X)",
            index, cond->operand1.value.memref->address, max_address);
        return 0;
      }
      if (is_memref2 && cond->operand2.value.memref->address > max_address) {
        snprintf(result, result_size, "Condition %d: Address %04X out of range (max %04X)",
            index, cond->operand2.value.memref->address, max_address);
        return 0;
      }
    }
    else {
      in_add_address = 0;
    }

    switch (cond->type) {
      case RC_CONDITION_ADD_SOURCE:
        add_source_max += max;
        is_combining = 1;
        continue;

      case RC_CONDITION_SUB_SOURCE:
        if (add_source_max < max) /* potential underflow - may be expected */
          add_source_max = 0xFFFFFFFF;
        is_combining = 1;
        continue;

      case RC_CONDITION_ADD_ADDRESS:
        if (cond->operand1.type == RC_OPERAND_DELTA || cond->operand1.type == RC_OPERAND_PRIOR) {
          snprintf(result, result_size, "Condition %d: Using pointer from previous frame", index);
          return 0;
        }
        in_add_address = 1;
        is_combining = 1;
        continue;

      case RC_CONDITION_ADD_HITS:
      case RC_CONDITION_SUB_HITS:
        in_add_hits = 1;
        is_combining = 1;
        break;

      case RC_CONDITION_AND_NEXT:
      case RC_CONDITION_OR_NEXT:
      case RC_CONDITION_RESET_NEXT_IF:
        is_combining = 1;
        break;

      default:
        if (in_add_hits) {
          if (cond->required_hits == 0) {
            snprintf(result, result_size, "Condition %d: Final condition in AddHits chain must have a hit target", index);
            return 0;
          }

          in_add_hits = 0;
        }

        is_combining = 0;
        break;
    }

    /* if we're in an add source chain, check for overflow */
    if (add_source_max) {
      const unsigned long long overflow = add_source_max + max;
      if (overflow > 0xFFFFFFFFUL)
        max = 0xFFFFFFFF;
      else
        max += (unsigned)add_source_max;
    }

    /* check for comparing two differently sized memrefs */
    max_val = rc_max_value(&cond->operand2);
    if (max_val != max && add_source_max == 0 && is_memref1 && is_memref2) {
      snprintf(result, result_size, "Condition %d: Comparing different memory sizes", index);
      return 0;
    }

    /* if either side is a memref, or there's a running add source chain, check for impossible comparisons */
    if (is_memref1 || is_memref2 || add_source_max) {
      unsigned min_val;
      switch (cond->operand2.type) {
        case RC_OPERAND_CONST:
          min_val = cond->operand2.value.num;
          break;

        case RC_OPERAND_FP:
          min_val = (int)cond->operand2.value.dbl;

          /* cannot compare an integer memory reference to a non-integral floating point value */
          /* assert: is_memref1 (because operand2==FP means !is_memref2) */
          if (!add_source_max && !rc_operand_is_float_memref(&cond->operand1) &&
              (float)min_val != cond->operand2.value.dbl) {
            snprintf(result, result_size, "Condition %d: Comparison is never true", index);
            return 0;
          }

          break;

        default:
          min_val = 0;

          /* cannot compare an integer memory reference to a non-integral floating point value */
          /* assert: is_memref2 (because operand1==FP means !is_memref1) */
          if (cond->operand1.type == RC_OPERAND_FP && !add_source_max && !rc_operand_is_float_memref(&cond->operand2) &&
              (float)((int)cond->operand1.value.dbl) != cond->operand1.value.dbl) {
            snprintf(result, result_size, "Condition %d: Comparison is never true", index);
            return 0;
          }

          break;
      }

      const size_t prefix_length = snprintf(result, result_size, "Condition %d: ", index);
      if (!rc_validate_range(min_val, max_val, cond->oper, max, result + prefix_length, result_size - prefix_length))
        return 0;
    }

    add_source_max = 0;
  }

  if (is_combining) {
    snprintf(result, result_size, "Final condition type expects another condition to follow");
    return 0;
  }

  *result = '\0';
  return 1;
}

int rc_validate_memrefs(const rc_memref_t* memref, char result[], const size_t result_size, unsigned max_address) {
  if (max_address < 0xFFFFFFFF) {
    while (memref) {
      if (memref->address > max_address) {
        snprintf(result, result_size, "Address %04X out of range (max %04X)", memref->address, max_address);
        return 0;
      }

      memref = memref->next;
    }
  }

  return 1;
}

int rc_validate_trigger(const rc_trigger_t* trigger, char result[], const size_t result_size, unsigned max_address) {
  const rc_condset_t* alt;
  int index;

  if (!trigger->alternative)
    return rc_validate_condset(trigger->requirement, result, result_size, max_address);

  snprintf(result, result_size, "Core ");
  if (!rc_validate_condset(trigger->requirement, result + 5, result_size - 5, max_address))
    return 0;

  index = 1;
  for (alt = trigger->alternative; alt; alt = alt->next, ++index) {
    const size_t prefix_length = snprintf(result, result_size, "Alt%d ", index);
    if (!rc_validate_condset(alt, result + prefix_length, result_size - prefix_length, max_address))
      return 0;
  }

  *result = '\0';
  return 1;
}

