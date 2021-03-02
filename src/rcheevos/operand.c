#include "rc_internal.h"

#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#ifndef RC_DISABLE_LUA

#ifdef __cplusplus
extern "C" {
#endif

#include <lua.h>
#include <lauxlib.h>

#ifdef __cplusplus
}
#endif

#endif /* RC_DISABLE_LUA */

static int rc_parse_operand_lua(rc_operand_t* self, const char** memaddr, rc_parse_state_t* parse) {
  const char* aux = *memaddr;
#ifndef RC_DISABLE_LUA
  const char* id;
#endif

  if (*aux++ != '@') {
    return RC_INVALID_LUA_OPERAND;
  }

  if (!isalpha(*aux)) {
    return RC_INVALID_LUA_OPERAND;
  }

#ifndef RC_DISABLE_LUA
  id = aux;
#endif

  while (isalnum(*aux) || *aux == '_') {
    aux++;
  }

#ifndef RC_DISABLE_LUA

  if (parse->L != 0) {
    if (!lua_istable(parse->L, parse->funcs_ndx)) {
      return RC_INVALID_LUA_OPERAND;
    }

    lua_pushlstring(parse->L, id, aux - id);
    lua_gettable(parse->L, parse->funcs_ndx);

    if (!lua_isfunction(parse->L, -1)) {
      lua_pop(parse->L, 1);
      return RC_INVALID_LUA_OPERAND;
    }

    self->value.luafunc = luaL_ref(parse->L, LUA_REGISTRYINDEX);
  }

#endif /* RC_DISABLE_LUA */

  self->type = RC_OPERAND_LUA;
  *memaddr = aux;
  return RC_OK;
}

static int rc_parse_operand_memory(rc_operand_t* self, const char** memaddr, rc_parse_state_t* parse, int is_indirect) {
  const char* aux = *memaddr;
  unsigned address;
  char size;
  int ret;

  switch (*aux) {
    case 'd': case 'D':
      self->type = RC_OPERAND_DELTA;
      ++aux;
      break;

    case 'p': case 'P':
      self->type = RC_OPERAND_PRIOR;
      ++aux;
      break;

    case 'b': case 'B':
      self->type = RC_OPERAND_BCD;
      ++aux;
      break;

    case '~':
      self->type = RC_OPERAND_INVERTED;
      ++aux;
      break;

    default:
      self->type = RC_OPERAND_ADDRESS;
      break;
  }

  ret = rc_parse_memref(&aux, &self->size, &address);
  if (ret != RC_OK)
    return ret;

  switch (self->size) {
    case RC_MEMSIZE_BIT_0:
    case RC_MEMSIZE_BIT_1:
    case RC_MEMSIZE_BIT_2:
    case RC_MEMSIZE_BIT_3:
    case RC_MEMSIZE_BIT_4:
    case RC_MEMSIZE_BIT_5:
    case RC_MEMSIZE_BIT_6:
    case RC_MEMSIZE_BIT_7:
    case RC_MEMSIZE_LOW:
    case RC_MEMSIZE_HIGH:
    case RC_MEMSIZE_BITCOUNT:
      /* these can all share an 8-bit memref and just mask off the appropriate data in rc_evaluate_operand */
      size = RC_MEMSIZE_8_BITS;
      break;

    default:
      size = self->size;
      break;
  }

  self->value.memref = rc_alloc_memref(parse, address, size, is_indirect);
  if (parse->offset < 0)
    return parse->offset;

  *memaddr = aux;
  return RC_OK;
}

int rc_parse_operand(rc_operand_t* self, const char** memaddr, int is_trigger, int is_indirect, rc_parse_state_t* parse) {
  const char* aux = *memaddr;
  char* end;
  int ret;
  unsigned long value;
  int negative;

  self->size = RC_MEMSIZE_32_BITS;

  switch (*aux) {
    case 'h': case 'H': /* hex constant */
      if (aux[2] == 'x' || aux[2] == 'X') {
        /* H0x1234 is a typo - either H1234 or 0xH1234 was probably meant */
        return RC_INVALID_CONST_OPERAND;
      }

      value = strtoul(++aux, &end, 16);

      if (end == aux) {
        return RC_INVALID_CONST_OPERAND;
      }

      if (value > 0xffffffffU) {
        value = 0xffffffffU;
      }

      self->type = RC_OPERAND_CONST;
      self->value.num = (unsigned)value;

      aux = end;
      break;

    case 'f': case 'F': /* floating point constant */
      self->value.dbl = strtod(++aux, &end);

      if (end == aux) {
        return RC_INVALID_FP_OPERAND;
      }

      if (floor(self->value.dbl) == self->value.dbl) {
        self->type = RC_OPERAND_CONST;
        self->value.num = (unsigned)floor(self->value.dbl);
      }
      else {
        self->type = RC_OPERAND_FP;
      }

      aux = end;
      break;

    case 'v': case 'V': /* signed integer constant */
      ++aux;
      /* fallthrough */
    case '+': case '-': /* signed integer constant */
      negative = 0;
      if (*aux == '-')
      {
        negative = 1;
        ++aux;
      }
      else if (*aux == '+')
      {
        ++aux;
      }

      value = strtoul(aux, &end, 10);

      if (end == aux) {
        return RC_INVALID_CONST_OPERAND;
      }

      if (value > 0x7fffffffU) {
        value = 0x7fffffffU;
      }

      self->type = RC_OPERAND_CONST;

      if (negative)
        self->value.num = (unsigned)(-((long)value));
      else
        self->value.num = (unsigned)value;

      aux = end;
      break;

    case '0':
      if (aux[1] == 'x' || aux[1] == 'X') { /* hex integer constant */
        /* fall through */
    default:
        ret = rc_parse_operand_memory(self, &aux, parse, is_indirect);

        if (ret < 0) {
          return ret;
        }

        break;
      }

      /* fall through for case '0' where not '0x' */
    case '1': case '2': case '3': case '4': case '5': /* unsigned integer constant */
    case '6': case '7': case '8': case '9':
      value = strtoul(aux, &end, 10);

      if (end == aux) {
        return RC_INVALID_CONST_OPERAND;
      }

      if (value > 0xffffffffU) {
        value = 0xffffffffU;
      }

      self->type = RC_OPERAND_CONST;
      self->value.num = (unsigned)value;

      aux = end;
      break;

    case '@':
      ret = rc_parse_operand_lua(self, &aux, parse);

      if (ret < 0) {
        return ret;
      }

      break;
  }

  *memaddr = aux;
  return RC_OK;
}

#ifndef RC_DISABLE_LUA

typedef struct {
  rc_peek_t peek;
  void* ud;
}
rc_luapeek_t;

static int rc_luapeek(lua_State* L) {
  unsigned address = (unsigned)luaL_checkinteger(L, 1);
  unsigned num_bytes = (unsigned)luaL_checkinteger(L, 2);
  rc_luapeek_t* luapeek = (rc_luapeek_t*)lua_touserdata(L, 3);

  unsigned value = luapeek->peek(address, num_bytes, luapeek->ud);

  lua_pushinteger(L, value);
  return 1;
}

#endif /* RC_DISABLE_LUA */

int rc_operand_is_memref(rc_operand_t* self) {
  switch (self->type) {
    case RC_OPERAND_CONST:
    case RC_OPERAND_FP:
    case RC_OPERAND_LUA:
      return 0;

    default:
      return 1;
  }
}

static const unsigned char rc_bits_set[16] = { 0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4 };

unsigned rc_evaluate_operand(rc_operand_t* self, rc_eval_state_t* eval_state) {
#ifndef RC_DISABLE_LUA
  rc_luapeek_t luapeek;
#endif /* RC_DISABLE_LUA */

  unsigned value;

  /* step 1: read memory */
  switch (self->type) {
    case RC_OPERAND_CONST:
      return self->value.num;

    case RC_OPERAND_FP:
      /* This is handled by rc_evaluate_condition_value. */
      return 0;

    case RC_OPERAND_LUA:
      value = 0;

#ifndef RC_DISABLE_LUA
      if (eval_state->L != 0) {
        lua_rawgeti(eval_state->L, LUA_REGISTRYINDEX, self->value.luafunc);
        lua_pushcfunction(eval_state->L, rc_luapeek);

        luapeek.peek = eval_state->peek;
        luapeek.ud = eval_state->peek_userdata;

        lua_pushlightuserdata(eval_state->L, &luapeek);

        if (lua_pcall(eval_state->L, 2, 1, 0) == LUA_OK) {
          if (lua_isboolean(eval_state->L, -1)) {
            value = lua_toboolean(eval_state->L, -1);
          }
          else {
            value = (unsigned)lua_tonumber(eval_state->L, -1);
          }
        }

        lua_pop(eval_state->L, 1);
      }

#endif /* RC_DISABLE_LUA */

      break;

    default:
      value = rc_get_memref_value(self->value.memref, self->type, eval_state);
      break;
  }

  /* step 2: mask off appropriate bits */
  switch (self->size)
  {
    case RC_MEMSIZE_BIT_0:
      value = (value >> 0) & 1;
      break;

    case RC_MEMSIZE_BIT_1:
      value = (value >> 1) & 1;
      break;

    case RC_MEMSIZE_BIT_2:
      value = (value >> 2) & 1;
      break;

    case RC_MEMSIZE_BIT_3:
      value = (value >> 3) & 1;
      break;

    case RC_MEMSIZE_BIT_4:
      value = (value >> 4) & 1;
      break;

    case RC_MEMSIZE_BIT_5:
      value = (value >> 5) & 1;
      break;

    case RC_MEMSIZE_BIT_6:
      value = (value >> 6) & 1;
      break;

    case RC_MEMSIZE_BIT_7:
      value = (value >> 7) & 1;
      break;

    case RC_MEMSIZE_LOW:
      value = value & 0x0f;
      break;

    case RC_MEMSIZE_HIGH:
      value = (value >> 4) & 0x0f;
      break;

    case RC_MEMSIZE_BITCOUNT:
      value = rc_bits_set[(value & 0x0F)]
            + rc_bits_set[((value >> 4) & 0x0F)];
      break;

    default:
      break;
  }

  /* step 3: apply logic */
  switch (self->type)
  {
    case RC_OPERAND_BCD:
      switch (self->size)
      {
        case RC_MEMSIZE_8_BITS:
          value = ((value >> 4) & 0x0f) * 10
                + ((value     ) & 0x0f);
          break;

        case RC_MEMSIZE_16_BITS:
          value = ((value >> 12) & 0x0f) * 1000
                + ((value >> 8) & 0x0f) * 100
                + ((value >> 4) & 0x0f) * 10
                + ((value     ) & 0x0f);
          break;

        case RC_MEMSIZE_24_BITS:
          value = ((value >> 20) & 0x0f) * 100000
                + ((value >> 16) & 0x0f) * 10000
                + ((value >> 12) & 0x0f) * 1000
                + ((value >> 8) & 0x0f) * 100
                + ((value >> 4) & 0x0f) * 10
                + ((value     ) & 0x0f);
          break;

        case RC_MEMSIZE_32_BITS:
        case RC_MEMSIZE_VARIABLE:
          value = ((value >> 28) & 0x0f) * 10000000
                + ((value >> 24) & 0x0f) * 1000000
                + ((value >> 20) & 0x0f) * 100000
                + ((value >> 16) & 0x0f) * 10000
                + ((value >> 12) & 0x0f) * 1000
                + ((value >> 8) & 0x0f) * 100
                + ((value >> 4) & 0x0f) * 10
                + ((value     ) & 0x0f);
          break;

        default:
          break;
      }
      break;

    case RC_OPERAND_INVERTED:
      switch (self->size)
      {
        case RC_MEMSIZE_LOW:
        case RC_MEMSIZE_HIGH:
          value ^= 0x0f;
          break;

        case RC_MEMSIZE_8_BITS:
          value ^= 0xff;
          break;

        case RC_MEMSIZE_16_BITS:
          value ^= 0xffff;
          break;

        case RC_MEMSIZE_24_BITS:
          value ^= 0xffffff;
          break;

        case RC_MEMSIZE_32_BITS:
        case RC_MEMSIZE_VARIABLE:
          value ^= 0xffffffff;
          break;

        default:
          value ^= 0x01;
          break;
      }
      break;

    default:
      break;
  }

  return value;
}
