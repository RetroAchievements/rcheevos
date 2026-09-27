# supported parameters
#  ARCH                  architecture - "x86" or "x64" [detected if not set]
#  BUILD                 use flags for specified upstream consumer - "c89" or "retroarch" [default to "c89"]
#  DEBUG                 if set to anything, builds with DEBUG symbols

RC_SRC=../src

include $(RC_SRC)/Makefile.common

OBJ=$(RC_UTIL_OBJS) \
    $(RC_RUNTIME_OBJS) \
    $(RC_SRC)/runtime/rc_validate.o \
    $(RC_SRC)/api/rc_api_common.o \
    $(RC_SRC)/api/rc_api_runtime.o \
    validator.o

all: validator

validator: $(OBJ)
	$(CC) $(LDFLAGS) -o $@$(EXE) $+ -lm

clean:
	rm -f validator$(EXE) $(OBJ)
