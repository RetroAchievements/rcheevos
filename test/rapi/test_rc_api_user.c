#include "rc_api.h"

#include "../test_framework.h"

#define DOREQUEST_URL "https://retroachievements.org/dorequest.php"

static void test_init_login_request_password()
{
  rc_api_login_request_t login_request;
  rc_api_request_t request;

  memset(&login_request, 0, sizeof(login_request));
  login_request.username = "Username";
  login_request.password = "Pa$$w0rd!";

  ASSERT_NUM_EQUALS(rc_api_init_login_request(&request, &login_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL "?r=login&u=Username");
  ASSERT_STR_EQUALS(request.post_data, "p=Pa%24%24w0rd%21");

  rc_api_destroy_request(&request);
}

static void test_init_login_request_password_long()
{
  char buffer[1024], *ptr;
  rc_api_login_request_t login_request;
  rc_api_request_t request;
  int i;

  /* this generates a password that's 830 characters long */
  buffer[0] = 'p';
  buffer[1] = '=';
  ptr = &buffer[2];
  for (i = 0; i < 30; i++)
	ptr += snprintf(ptr, sizeof(buffer) - (ptr - buffer), "%dABCDEFGHIJKLMNOPQRSTUVWXYZ", i);

  memset(&login_request, 0, sizeof(login_request));
  login_request.username = "ThisUsernameIsAlsoReallyLongAtRoughlyFiftyCharacters";
  login_request.password = &buffer[2];

  ASSERT_NUM_EQUALS(rc_api_init_login_request(&request, &login_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL "?r=login&u=ThisUsernameIsAlsoReallyLongAtRoughlyFiftyCharacters");
  ASSERT_STR_EQUALS(request.post_data, buffer);

  rc_api_destroy_request(&request);
}

static void test_init_login_request_token()
{
  rc_api_login_request_t login_request;
  rc_api_request_t request;

  memset(&login_request, 0, sizeof(login_request));
  login_request.username = "Username";
  login_request.api_token = "ABCDEFGHIJKLMNOP";

  ASSERT_NUM_EQUALS(rc_api_init_login_request(&request, &login_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL "?r=login&u=Username");
  ASSERT_STR_EQUALS(request.post_data, "t=ABCDEFGHIJKLMNOP");

  rc_api_destroy_request(&request);
}

static void test_init_login_request_alternate_host()
{
  rc_api_login_request_t login_request;
  rc_api_request_t request;

  memset(&login_request, 0, sizeof(login_request));
  login_request.username = "Username";
  login_request.password = "Pa$$w0rd!";

  rc_api_set_host("localhost");
  ASSERT_NUM_EQUALS(rc_api_init_login_request(&request, &login_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, "http://localhost/dorequest.php?r=login&u=Username");
  ASSERT_STR_EQUALS(request.post_data, "p=Pa%24%24w0rd%21");

  rc_api_set_host(NULL);
  rc_api_destroy_request(&request);
}

static void test_process_login_response_success()
{
  rc_api_login_response_t login_response;
  const char* server_response = "{\"Success\":true,\"User\":\"USER\",\"Token\":\"ApiTOKEN\",\"Score\":1234,\"Messages\":2}";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 1);
  ASSERT_PTR_NULL(login_response.response.error_message);
  ASSERT_STR_EQUALS(login_response.username, "USER");
  ASSERT_STR_EQUALS(login_response.api_token, "ApiTOKEN");
  ASSERT_NUM_EQUALS(login_response.score, 1234);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 2);

  rc_api_destroy_login_response(&login_response);
}

static void test_process_login_response_error()
{
  rc_api_login_response_t login_response;
  const char* server_response = "{\"Success\":false,\"Error\":\"Invalid User/Password combination. Please try again\"}";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(login_response.response.error_message, "Invalid User/Password combination. Please try again");
  ASSERT_PTR_NULL(login_response.username);
  ASSERT_PTR_NULL(login_response.api_token);
  ASSERT_NUM_EQUALS(login_response.score, 0);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 0);

  rc_api_destroy_login_response(&login_response);
}

static void test_process_login_response_generic_failure()
{
  rc_api_login_response_t login_response;
  const char* server_response = "{\"Success\":false}";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 0);
  ASSERT_PTR_NULL(login_response.response.error_message);
  ASSERT_PTR_NULL(login_response.username);
  ASSERT_PTR_NULL(login_response.api_token);
  ASSERT_NUM_EQUALS(login_response.score, 0);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 0);

  rc_api_destroy_login_response(&login_response);
}

static void test_process_login_response_empty()
{
  rc_api_login_response_t login_response;
  const char* server_response = "";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_INVALID_JSON);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 0);
  ASSERT_PTR_NULL(login_response.response.error_message);
  ASSERT_PTR_NULL(login_response.username);
  ASSERT_PTR_NULL(login_response.api_token);
  ASSERT_NUM_EQUALS(login_response.score, 0);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 0);

  rc_api_destroy_login_response(&login_response);
}

static void test_process_login_response_text()
{
  rc_api_login_response_t login_response;
  const char* server_response = "You do not have access to that resource";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_INVALID_JSON);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(login_response.response.error_message, "You do not have access to that resource");
  ASSERT_PTR_NULL(login_response.username);
  ASSERT_PTR_NULL(login_response.api_token);
  ASSERT_NUM_EQUALS(login_response.score, 0);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 0);

  rc_api_destroy_login_response(&login_response);
}

static void test_process_login_response_html()
{
  rc_api_login_response_t login_response;
  const char* server_response = "<b>You do not have access to that resource</b>";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_INVALID_JSON);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(login_response.response.error_message, "<b>You do not have access to that resource</b>");
  ASSERT_PTR_NULL(login_response.username);
  ASSERT_PTR_NULL(login_response.api_token);
  ASSERT_NUM_EQUALS(login_response.score, 0);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 0);

  rc_api_destroy_login_response(&login_response);
}

static void test_process_login_response_no_required_fields()
{
  rc_api_login_response_t login_response;
  const char* server_response = "{\"Success\":true}";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_MISSING_VALUE);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(login_response.response.error_message, "User not found in response");
  ASSERT_PTR_NULL(login_response.username);
  ASSERT_PTR_NULL(login_response.api_token);
  ASSERT_NUM_EQUALS(login_response.score, 0);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 0);

  rc_api_destroy_login_response(&login_response);
}

static void test_process_login_response_no_token()
{
  rc_api_login_response_t login_response;
  const char* server_response = "{\"Success\":true,\"User\":\"Username\"}";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_MISSING_VALUE);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(login_response.response.error_message, "Token not found in response");
  ASSERT_STR_EQUALS(login_response.username, "Username");
  ASSERT_PTR_NULL(login_response.api_token);
  ASSERT_NUM_EQUALS(login_response.score, 0);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 0);

  rc_api_destroy_login_response(&login_response);
}

static void test_process_login_response_no_optional_fields()
{
  rc_api_login_response_t login_response;
  const char* server_response = "{\"Success\":true,\"User\":\"USER\",\"Token\":\"ApiTOKEN\"}";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 1);
  ASSERT_PTR_NULL(login_response.response.error_message);
  ASSERT_STR_EQUALS(login_response.username, "USER");
  ASSERT_STR_EQUALS(login_response.api_token, "ApiTOKEN");
  ASSERT_NUM_EQUALS(login_response.score, 0);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 0);

  rc_api_destroy_login_response(&login_response);
}

static void test_process_login_response_null_score()
{
  rc_api_login_response_t login_response;
  const char* server_response = "{\"Success\":true,\"User\":\"USER\",\"Token\":\"ApiTOKEN\",\"Score\":null}";

  memset(&login_response, 0, sizeof(login_response));

  ASSERT_NUM_EQUALS(rc_api_process_login_response(&login_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(login_response.response.succeeded, 1);
  ASSERT_PTR_NULL(login_response.response.error_message);
  ASSERT_STR_EQUALS(login_response.username, "USER");
  ASSERT_STR_EQUALS(login_response.api_token, "ApiTOKEN");
  ASSERT_NUM_EQUALS(login_response.score, 0);
  ASSERT_NUM_EQUALS(login_response.num_unread_messages, 0);

  rc_api_destroy_login_response(&login_response);
}

void test_rapi_user(void) {
  TEST_SUITE_BEGIN();

  /* login */
  TEST(test_init_login_request_password);
  TEST(test_init_login_request_password_long);
  TEST(test_init_login_request_token);
  TEST(test_init_login_request_alternate_host);

  TEST(test_process_login_response_success);
  TEST(test_process_login_response_error);
  TEST(test_process_login_response_generic_failure);
  TEST(test_process_login_response_empty);
  TEST(test_process_login_response_text);
  TEST(test_process_login_response_html);
  TEST(test_process_login_response_no_required_fields);
  TEST(test_process_login_response_no_token);
  TEST(test_process_login_response_no_optional_fields);
  TEST(test_process_login_response_null_score);

  TEST_SUITE_END();
}
