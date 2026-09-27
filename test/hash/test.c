#include "../test_framework.h"

#include "test_hash.h"

TEST_FRAMEWORK_DECLARATIONS()

int main(void) {
  TEST_FRAMEWORK_INIT();

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

  TEST_FRAMEWORK_SHUTDOWN();

  return TEST_FRAMEWORK_PASSED() ? 0 : 1;
}
