#include "../api/test_api.h"
#include "../client/test_client.h"
#include "../hash/test_hash.h"
#include "../libretro/test_libretro.h"
#include "../runtime/test_runtime.h"
#include "../test_framework.h"

#define TIMING_TEST 0

extern void test_timing();

TEST_FRAMEWORK_DECLARATIONS()

int main(void) {
  TEST_FRAMEWORK_INIT();

#if TIMING_TEST
  test_timing();
#else
  test_memref();
  test_operand();
  test_condition();
  test_condset();
  test_trigger();
  test_value();
  test_format();
  test_lboard();
  test_richpresence();
  test_runtime();
  test_runtime_progress();

  test_consoleinfo();
  test_rc_validate();

  test_rapi_common();
  test_rapi_user();
  test_rapi_runtime();
  test_rapi_info();
  test_rapi_editor();

  test_client();
#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  test_client_external();
#endif
#ifdef RC_CLIENT_SUPPORTS_RAINTEGRATION
  test_client_raintegration();
#endif
#ifdef RC_CLIENT_SUPPORTS_HASH
  /* no direct compile option for hash support, so leverage RC_CLIENT_SUPPORTS_HASH */
  test_rc_libretro(); /* libretro extensions require hash support */
  test_hash();
 #ifndef RC_HASH_NO_ROM
  test_hash_rom();
 #endif
 #ifndef RC_HASH_NO_DISC
  test_cdreader();
  test_hash_disc();
 #endif
 #ifndef RC_HASH_NO_ZIP
  test_hash_zip();
 #endif
#endif /* RC_CLIENT_SUPPORTS_HASH */
#endif /* !TIMING_TEST */

  TEST_FRAMEWORK_SHUTDOWN();

  return TEST_FRAMEWORK_PASSED() ? 0 : 1;
}
