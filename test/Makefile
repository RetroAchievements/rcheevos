RC_SRC=../src/rcheevos
RC_JSON_SRC=../src/rjson
RC_URL_SRC=../src/rurl

OBJ=$(RC_SRC)/trigger.o $(RC_SRC)/condset.o $(RC_SRC)/condition.o $(RC_SRC)/operand.o \
    $(RC_SRC)/term.o $(RC_SRC)/expression.o $(RC_SRC)/value.o $(RC_SRC)/lboard.o \
    $(RC_SRC)/alloc.o $(RC_SRC)/format.o \
	$(RC_JSON_SRC)/dejson.o $(RC_JSON_SRC)/schema.o \
	$(RC_URL_SRC)/url.o \
	test.o

all: test

%.o: %.c
	gcc -Wall -O0 -g -I../include -I$(RC_SRC) -Ilua/src -c $< -o $@

test: $(OBJ)
	gcc -o $@ $+

test.o: smw_snes.h galaga_nes.h

smw_snes.h: smw_snes.json
	xxd -i $< | sed "s@unsigned@const@" > $@

galaga_nes.h: galaga_nes.json
	xxd -i $< | sed "s@unsigned@const@" > $@

clean:
	rm -f test $(OBJ) smw_snes.h galaga_nes.h
