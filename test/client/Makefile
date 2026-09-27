# supported parameters
#  ARCH                  architecture - "x86" or "x64" [detected if not set]
#  BUILD                 use flags for specified upstream consumer - "c89" or "retroarch" [default to "c89"]
#  DEBUG                 if set to anything, builds with DEBUG symbols
#  HAVE_HASH             if set to 0, excludes all hash functionality
#  HAVE_HASH_ROM         if set to 0, excludes rom hash generation
#  HAVE_HASH_DISC        if set to 0, excludes disc hash generation
#  HAVE_HASH_ZIP         if set to 0, excludes zip hash generation
#  HAVE_HASH_ENCRYPTED   if set to 0, excludes encrypted hash generation

RC_SRC=../../src

include $(RC_SRC)/Makefile.common

OBJ=$(RC_UTIL_OBJS) \
    $(RC_API_OBJS) \
    $(RC_RUNTIME_OBJS) \
    $(RC_SRC)/runtime/rc_validate.o \
    $(RC_HASH_OBJS) \
    $(RC_CLIENT_OBJS) \
    ../runtime/test_rc_condition.o \
    ../runtime/test_rc_condset.o \
    ../runtime/test_rc_consoleinfo.o \
    ../runtime/test_rc_format.o \
    ../runtime/test_rc_lboard.o \
    ../runtime/test_rc_memref.o \
    ../runtime/test_rc_operand.o \
    ../runtime/test_rc_richpresence.o \
    ../runtime/test_rc_runtime.o \
    ../runtime/test_rc_runtime_progress.o \
    ../runtime/test_rc_trigger.o \
    ../runtime/test_rc_validate.o \
    ../runtime/test_rc_value.o \
    ../runtime/test_timing.o \
    ../api/test_rc_api_common.o \
    ../api/test_rc_api_editor.o \
    ../api/test_rc_api_info.o \
    ../api/test_rc_api_runtime.o \
    ../api/test_rc_api_user.o \
    test_rc_client.o \
    test.o

# support for building without hash subdirectory
ifeq ($(HAVE_HASH), 0)
    EXTRA += |NO_HASH
    CFLAGS += -DRC_HASH_NO_ROM -DRC_HASH_NO_DISC -DRC_HASH_NO_ZIP -DRC_HASH_NO_ENCRYPTED
else
    CFLAGS += -DRC_CLIENT_SUPPORTS_HASH
    OBJ += $(RC_SRC)/libretro/rc_libretro.o \
           ../hash/data.o \
           ../hash/mock_filereader.o \
           ../hash/test_rc_hash.o \
           ../libretro/test_rc_libretro.o

    ifeq ($(HAVE_HASH_ROM), 0)
        EXTRA += |NO_HASH_ROM
        CFLAGS += -DRC_HASH_NO_ROM
    else
        OBJ +=  ../hash/test_rc_hash_rom.o
    endif

    ifeq ($(HAVE_HASH_DISC), 0)
        EXTRA += |NO_HASH_DISC
        CFLAGS += -DRC_HASH_NO_DISC
    else
        OBJ += ../hash/test_rc_cdreader.o \
               ../hash/test_rc_hash_disc.o
    endif

    ifeq ($(HAVE_HASH_ZIP), 0)
        EXTRA += |NO_HASH_ZIP
        CFLAGS += -DRC_HASH_NO_ZIP
    else
        OBJ += ../hash/test_rc_hash_zip.o
    endif

    ifeq ($(HAVE_HASH_ENCRYPTED), 0)
        EXTRA += |NO_HASH_ENCRYPTED
        CFLAGS += -DRC_HASH_NO_ENCRYPTED
    else
        OBJ += $(RC_SRC)/hash/rc_hash_encrypted.o \
               $(RC_SRC)/hash/aes.o
    endif
endif

ifeq ($(BUILD), retroarch)
	OBJ += test_rc_client_external.o \
	       test_rc_client_raintegration.o
endif


# recipes
$(info ==== rc_client-test [$(BUILD)/$(ARCH)$(EXTRA)] ====)

all: rc_client-test

$(RC_SRC)/libretro/%.o: $(RC_SRC)/libretro/%.c
	$(CC) $(CFLAGS) $(SRC_CFLAGS) $(INCLUDES) -I../libretro -c $< -o $@

../api/%.o: ../api/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

../hash/%.o: ../hash/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

../libretro/%.o: ../libretro/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -I../libretro -c $< -o $@

../runtime/%.o: ../runtime/%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

rc_client-test: $(OBJ)
	$(CC) $(LDFLAGS) -o $@$(EXE) $+ -lm
	@echo ---------------------------------

check_ctype:
	@if grep -rnI "isalpha([^(u]" ../src/*; \
	then echo "*** Error: isalpha without unsigned char cast" && false; \
	fi
	@if grep -rnI "isalnum([^(u]" ../src/*; \
	then echo "*** Error: isalnum without unsigned char cast" && false; \
	fi
	@if grep -rnI "isdigit([^(u]" ../src/*; \
	then echo "*** Error: isdigit without unsigned char cast" && false; \
	fi
	@if grep -rnI "isspace([^(u]" ../src/*; \
	then echo "*** Error: isspace without unsigned char cast" && false; \
	fi

runtests: rc_client-test
	@./rc_client-test$(EXE)

valgrind: rc_client-test
	@valgrind --leak-check=full --error-exitcode=1 ./rc_client-test$(EXE)

clean:
	rm -f rc_client-test$(EXE) $(OBJ)
