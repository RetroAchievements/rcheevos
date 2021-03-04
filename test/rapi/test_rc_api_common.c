#include "../rapi/rc_api_common.h"

#include "../test_framework.h"

static void test_url_build_dorequest_default_host() {
  rc_api_url_builder_t builder;
  rc_api_buffer_t buffer;
  const char* url;

  rc_buf_init(&buffer);
  rc_api_url_build_dorequest(&builder, &buffer, "login", "Username");
  url = rc_url_builder_finalize(&builder);

  ASSERT_STR_EQUALS(url, "https://retroachievements.org/dorequest.php?r=login&u=Username");

  rc_buf_destroy(&buffer);
}

static void test_url_build_dorequest_custom_host() {
  rc_api_url_builder_t builder;
  rc_api_buffer_t buffer;
  const char* url;

  rc_api_set_host("http://localhost");

  rc_buf_init(&buffer);
  rc_api_url_build_dorequest(&builder, &buffer, "test", "Guy");
  url = rc_url_builder_finalize(&builder);

  ASSERT_STR_EQUALS(url, "http://localhost/dorequest.php?r=test&u=Guy");

  rc_api_set_host(NULL);
  rc_buf_destroy(&buffer);
}

static void test_url_build_dorequest_custom_host_no_protocol() {
  rc_api_url_builder_t builder;
  rc_api_buffer_t buffer;
  const char* url;

  rc_api_set_host("my.host");

  rc_buf_init(&buffer);
  rc_api_url_build_dorequest(&builder, &buffer, "photo", "Dude");
  url = rc_url_builder_finalize(&builder);

  ASSERT_STR_EQUALS(url, "http://my.host/dorequest.php?r=photo&u=Dude");

  rc_api_set_host(NULL);
  rc_buf_destroy(&buffer);
}

static void test_url_builder_append_encoded_str(const char* input, const char* expected) {
  rc_api_url_builder_t builder;
  rc_api_buffer_t buffer;
  const char* output;

  rc_buf_init(&buffer);
  rc_url_builder_init(&builder, &buffer, 128);
  rc_url_builder_append_encoded_str(&builder, input);
  output = rc_url_builder_finalize(&builder);

  ASSERT_STR_EQUALS(output, expected);

  rc_buf_destroy(&buffer);
}

static void test_url_builder_append_str_param() {
  rc_api_url_builder_t builder;
  rc_api_buffer_t buffer;
  const char* output;

  rc_buf_init(&buffer);
  rc_url_builder_init(&builder, &buffer, 64);
  rc_url_builder_append_str_param(&builder, "a", "Apple");
  rc_url_builder_append_str_param(&builder, "b", "Banana");
  rc_url_builder_append_str_param(&builder, "t", "Test 1");
  output = rc_url_builder_finalize(&builder);

  ASSERT_STR_EQUALS(output, "a=Apple&b=Banana&t=Test+1");

  rc_buf_destroy(&buffer);
}

static void test_url_builder_append_num_param() {
  rc_api_url_builder_t builder;
  rc_api_buffer_t buffer;
  const char* output;

  rc_buf_init(&buffer);
  rc_url_builder_init(&builder, &buffer, 32);
  rc_url_builder_append_num_param(&builder, "a", 0);
  rc_url_builder_append_num_param(&builder, "b", 123456);
  rc_url_builder_append_num_param(&builder, "t", (unsigned)-1);
  output = rc_url_builder_finalize(&builder);

  ASSERT_STR_EQUALS(output, "a=0&b=123456&t=4294967295");

  rc_buf_destroy(&buffer);
}

static void test_url_builder_append_signed_num_param() {
  rc_api_url_builder_t builder;
  rc_api_buffer_t buffer;
  const char* output;

  rc_buf_init(&buffer);
  rc_url_builder_init(&builder, &buffer, 32);
  rc_url_builder_append_signed_num_param(&builder, "a", 0);
  rc_url_builder_append_signed_num_param(&builder, "b", 123456);
  rc_url_builder_append_signed_num_param(&builder, "t", -1);
  output = rc_url_builder_finalize(&builder);

  ASSERT_STR_EQUALS(output, "a=0&b=123456&t=-1");

  rc_buf_destroy(&buffer);
}

void test_rapi_common(void) {
  TEST_SUITE_BEGIN();

  /* rc_api_url_build_dorequest / rc_api_set_host */
  TEST(test_url_build_dorequest_default_host);
  TEST(test_url_build_dorequest_custom_host);
  TEST(test_url_build_dorequest_custom_host_no_protocol);

  /* rc_api_url_builder_append_encoded_str */
  TEST_PARAMS2(test_url_builder_append_encoded_str, "", "");
  TEST_PARAMS2(test_url_builder_append_encoded_str, "Apple", "Apple");
  TEST_PARAMS2(test_url_builder_append_encoded_str, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~", "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~");
  TEST_PARAMS2(test_url_builder_append_encoded_str, "Test 1", "Test+1");
  TEST_PARAMS2(test_url_builder_append_encoded_str, "Test+1", "Test%2b1");
  TEST_PARAMS2(test_url_builder_append_encoded_str, "Test%1", "Test%251");
  TEST_PARAMS2(test_url_builder_append_encoded_str, "%Test%", "%25Test%25");
  TEST_PARAMS2(test_url_builder_append_encoded_str, "%%", "%25%25");

  /* rc_api_url_builder_append_param */
  TEST(test_url_builder_append_str_param);
  TEST(test_url_builder_append_num_param);
  TEST(test_url_builder_append_signed_num_param);

  TEST_SUITE_END();
}
