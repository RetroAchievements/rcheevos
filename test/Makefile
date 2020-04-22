RC_SRC=../src/rcheevos
RC_URL_SRC=../src/rurl
RC_HASH_SRC=../src/rhash
LUA_SRC=lua/src

OBJ=$(RC_SRC)/trigger.o $(RC_SRC)/condset.o $(RC_SRC)/condition.o $(RC_SRC)/operand.o \
    $(RC_SRC)/value.o $(RC_SRC)/lboard.o $(RC_SRC)/richpresence.o $(RC_SRC)/runtime.o \
    $(RC_SRC)/alloc.o $(RC_SRC)/memref.o $(RC_SRC)/format.o $(RC_SRC)/compat.o $(RC_SRC)/consoleinfo.o \
    $(RC_HASH_SRC)/md5.o $(RC_HASH_SRC)/hash.o \
    $(RC_URL_SRC)/url.o \
    rcheevos/test_memref.o \
    rcheevos/test_operand.o \
    rcheevos/test_condition.o \
    rcheevos/test_condset.o \
    rcheevos/test_trigger.o \
    rcheevos/test_value.o \
    rcheevos/test_format.o \
    rcheevos/test_lboard.o \
    rcheevos/test_richpresence.o \
    rcheevos/test_runtime.o \
    rcheevos/test_consoleinfo.o \
    rhash/data.o \
    rhash/test_hash.o \
    test.o

CFLAGS=-Wall -O0 -g -std=c89 -ansi -Wno-long-long -I../include -I$(RC_SRC)

ifdef HAVE_LUA
  OBJ += $(LUA_SRC)/lapi.o $(LUA_SRC)/lcode.o $(LUA_SRC)/lctype.o $(LUA_SRC)/ldebug.o \
         $(LUA_SRC)/ldo.o $(LUA_SRC)/ldump.o $(LUA_SRC)/lfunc.o $(LUA_SRC)/lgc.o $(LUA_SRC)/llex.o \
         $(LUA_SRC)/lmem.o $(LUA_SRC)/lobject.o $(LUA_SRC)/lopcodes.o $(LUA_SRC)/lparser.o \
         $(LUA_SRC)/lstate.o $(LUA_SRC)/lstring.o $(LUA_SRC)/ltable.o $(LUA_SRC)/ltm.o \
         $(LUA_SRC)/lundump.o $(LUA_SRC)/lvm.o $(LUA_SRC)/lzio.o $(LUA_SRC)/lauxlib.o \
         $(LUA_SRC)/lbaselib.o $(LUA_SRC)/lbitlib.o $(LUA_SRC)/lcorolib.o $(LUA_SRC)/ldblib.o \
         $(LUA_SRC)/liolib.o $(LUA_SRC)/lmathlib.o $(LUA_SRC)/loslib.o $(LUA_SRC)/lstrlib.o \
         $(LUA_SRC)/ltablib.o $(LUA_SRC)/lutf8lib.o $(LUA_SRC)/loadlib.o $(LUA_SRC)/linit.o

  CFLAGS += -DLUA_32BITS -I$(LUA_SRC)
else
  CFLAGS += -DRC_DISABLE_LUA
endif



all: test

%.o: %.c
	gcc $(CFLAGS) -c $< -o $@

test: $(OBJ)
	gcc -o $@ $+ -lm

clean:
	rm -f test $(OBJ)
