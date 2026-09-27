# supported parameters
#  ARCH                  architecture - "x86" or "x64" [detected if not set]
#  BUILD                 use flags for specified upstream consumer - "c89" or "retroarch" [default to "c89"]
#  DEBUG                 if set to anything, builds with DEBUG symbols
#  HAVE_HASH_ROM         if set to 0, excludes rom hash generation
#  HAVE_HASH_DISC        if set to 0, excludes disc hash generation
#  HAVE_HASH_ZIP         if set to 0, excludes zip hash generation
#  HAVE_HASH_ENCRYPTED   if set to 0, excludes encrypted hash generation

RC_SRC=../../src

include $(RC_SRC)/Makefile.common

# source files
OBJ=$(RC_UTIL_OBJS) \
    $(RC_HASH_OBJS) \
    data.o \
    mock_filereader.o \
    test.o \
    test_rc_hash.o

ifeq ($(HAVE_HASH_ROM), 0)
	EXTRA += |NO_HASH_ROM
	CFLAGS += -DRC_HASH_NO_ROM
else
	OBJ += test_rc_hash_rom.o
endif

ifeq ($(HAVE_HASH_DISC), 0)
	EXTRA += |NO_HASH_DISC
	CFLAGS += -DRC_HASH_NO_DISC
else
	OBJ += test_rc_cdreader.o \
		   test_rc_hash_disc.o
endif

ifeq ($(HAVE_HASH_ZIP), 0)
	EXTRA += |NO_HASH_ZIP
	CFLAGS += -DRC_HASH_NO_ZIP
else
	OBJ += test_rc_hash_zip.o
endif

ifeq ($(HAVE_HASH_ENCRYPTED), 0)
	EXTRA += |NO_HASH_ENCRYPTED
	CFLAGS += -DRC_HASH_NO_ENCRYPTED
else
    OBJ += $(RC_SRC)/hash/rc_hash_encrypted.o \
           $(RC_SRC)/hash/aes.o
endif

# recipes
$(info ==== rc_hash-test [$(BUILD)/$(ARCH)$(EXTRA)] ====)

all: rc_hash-test

../hash/%.o: ../hash/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

rc_hash-test: $(OBJ)
	$(CC) $(LDFLAGS) -o $@$(EXE) $+ -lm
	@echo ---------------------------------

runtests: rc_hash-test
	@./rc_hash-test$(EXE)

clean:
	rm -f rc_hash-test$(EXE) $(OBJ)
