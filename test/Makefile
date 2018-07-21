SRC=../src
OBJ=test.o $(SRC)/trigger.o $(SRC)/condset.o $(SRC)/condition.o $(SRC)/operand.o \
    $(SRC)/term.o $(SRC)/expression.o $(SRC)/value.o $(SRC)/lboard.o $(SRC)/alloc.o \
    $(SRC)/format.o

all: test

%.o: %.c
	gcc -Wall -O3 -I../include -I$(SRC) -Ilua/src -c $< -o $@

test: $(OBJ)
	gcc -o $@ $+

clean:
	rm -f test $(OBJ)
