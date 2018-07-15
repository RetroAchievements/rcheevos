#include "rcheevos.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef void* lua_KContext;
typedef int (*lua_KFunction) (lua_State *L, int status, lua_KContext ctx);
typedef double lua_Number;
typedef long long lua_Integer;

const char *lua_pushlstring (lua_State *L, const char *s, size_t len) {}
int lua_gettable (lua_State *L, int index) {}
int lua_type (lua_State *L, int index) {}
int luaL_ref (lua_State *L, int t) {}
void lua_settop (lua_State *L, int index) {}
int lua_rawgeti (lua_State *L, int index, lua_Integer n) {}
int lua_pcallk (lua_State *L,
                int nargs,
                int nresults,
                int msgh,
                lua_KContext ctx,
                lua_KFunction k) {}
int lua_toboolean (lua_State *L, int index) {}
lua_Number lua_tonumberx (lua_State *L, int index, int *isnum) {}

int main() {
  //const char* memaddr = "0xfe20>=20_0xfff0=0_0xHfffb=0_0xHf601=12";
  const char* memaddr = "0xHfe10>d0xHfe10.1._R:0xfff0=1_R:0xffe1=1_0xHfffb=0S0xHfe10==10";
  char buffer[65536];

  memset(buffer, 0xcd, sizeof(buffer));

  int ret;
  rc_trigger_t* trigger =rc_parse_trigger(&ret, buffer, memaddr, NULL, 0);

  /*rc_trigger_t* trigger = (rc_trigger_t*)buffer;
  trigger->requirement = (rc_condset_t*)((char*)trigger->requirement - buffer);

  for (res = 0; res < trigger->count; res++) {
    trigger->alternative[res] = (rc_condset_t*)((char*)trigger->alternative[res] - buffer);
  }*/

  FILE* file = fopen("parsed.bin", "wb");
  fwrite(buffer, 1, sizeof(buffer), file);
  fclose(file);

  return 0;
}
