#include "../test_framework.h"

#include "test_rapi.h"

TEST_FRAMEWORK_DECLARATIONS()

int main(void) {
  TEST_FRAMEWORK_INIT();

  test_rapi_common();
  test_rapi_user();
  test_rapi_runtime();
  test_rapi_info();
  test_rapi_editor();

  TEST_FRAMEWORK_SHUTDOWN();

  return TEST_FRAMEWORK_PASSED() ? 0 : 1;
}
