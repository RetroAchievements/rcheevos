#include "rc_internal.h"

#include <stdlib.h>
#include <string.h>

unsigned rc_djb2(const char* input)
{
  unsigned result = 5381;
  char c;

  while ((c = *input++) != '\0')
    result = ((result << 5) + result) + c; /* result = result * 33 + c */

  return result;
}

const char* rc_error_str(int ret)
{
  switch (ret) {
    case RC_OK: return "OK";
    case RC_INVALID_LUA_OPERAND: return "Invalid Lua operand";
    case RC_INVALID_MEMORY_OPERAND: return "Invalid memory operand";
    case RC_INVALID_CONST_OPERAND: return "Invalid constant operand";
    case RC_INVALID_FP_OPERAND: return "Invalid floating-point operand";
    case RC_INVALID_CONDITION_TYPE: return "Invalid condition type";
    case RC_INVALID_OPERATOR: return "Invalid operator";
    case RC_INVALID_REQUIRED_HITS: return "Invalid required hits";
    case RC_DUPLICATED_START: return "Duplicated start condition";
    case RC_DUPLICATED_CANCEL: return "Duplicated cancel condition";
    case RC_DUPLICATED_SUBMIT: return "Duplicated submit condition";
    case RC_DUPLICATED_VALUE: return "Duplicated value expression";
    case RC_DUPLICATED_PROGRESS: return "Duplicated progress expression";
    case RC_MISSING_START: return "Missing start condition";
    case RC_MISSING_CANCEL: return "Missing cancel condition";
    case RC_MISSING_SUBMIT: return "Missing submit condition";
    case RC_MISSING_VALUE: return "Missing value expression";
    case RC_INVALID_LBOARD_FIELD: return "Invalid field in leaderboard";
    case RC_MISSING_DISPLAY_STRING: return "Missing display string";
    case RC_OUT_OF_MEMORY: return "Out of memory";
    case RC_INVALID_VALUE_FLAG: return "Invalid flag in value expression";
    case RC_MISSING_VALUE_MEASURED: return "Missing measured flag in value expression";
    case RC_MULTIPLE_MEASURED: return "Multiple measured targets";
    case RC_INVALID_MEASURED_TARGET: return "Invalid measured target";
    case RC_INVALID_COMPARISON: return "Invalid comparison";
    case RC_INVALID_STATE: return "Invalid state";
    case RC_INVALID_JSON: return "Invalid JSON";
    case RC_API_FAILURE: return "API call failed";
    case RC_LOGIN_REQUIRED: return "Login required";
    case RC_NO_GAME_LOADED: return "No game loaded";
    case RC_HARDCORE_DISABLED: return "Hardcore disabled";
    case RC_ABORTED: return "Aborted";
    case RC_NO_RESPONSE: return "No response";
    case RC_ACCESS_DENIED: return "Access denied";
    case RC_INVALID_CREDENTIALS: return "Invalid credentials";
    case RC_EXPIRED_TOKEN: return "Expired token";
    default: return "Unknown error";
  }
}
