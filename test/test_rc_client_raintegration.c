#include "rc_client.h"

#include "rc_consoles.h"
#include "rc_hash.h"
#include "rc_internal.h"
#include "rc_api_runtime.h"

#include "../src/rc_client_internal.h"
#include "../src/rc_version.h"

#include "rhash/data.h"
#include "test_framework.h"

#ifdef RC_CLIENT_SUPPORTS_RAINTEGRATION

static rc_client_t* g_client;
static const char* g_integration_event;
static void* g_callback_userdata = &g_client; /* dummy object to use for callback userdata validation */

/* begin from test_rc_client.c */

extern void rc_client_server_call(const rc_api_request_t* request, rc_client_server_callback_t callback, void* callback_data, rc_client_t* client);
extern void rc_client_server_call_async(const rc_api_request_t* request, rc_client_server_callback_t callback, void* callback_data, rc_client_t* client);
extern void reset_mock_api_handlers(void);
extern void mock_api_response(const char* request_params, const char* response_body);
extern void mock_api_error(const char* request_params, const char* response_body, int http_status_code);
extern void async_api_response(const char* request_params, const char* response_body);
extern void async_api_error(const char* request_params, const char* response_body, int http_status_code);

/* end from test_rc_client.c */

static uint32_t rc_client_read_memory(uint32_t address, uint8_t* buffer, uint32_t num_bytes, rc_client_t* client)
{
  return 0;
}

static rc_client_t* mock_client_with_integration()
{
  rc_client_t* client = rc_client_create(rc_client_read_memory, rc_client_server_call);
  client->state.raintegration = (rc_client_raintegration_t*)
      rc_buffer_alloc(&client->state.buffer, sizeof(*client->state.raintegration));
  memset(client->state.raintegration, 0, sizeof(*client->state.raintegration));

  rc_api_set_host(NULL);
  reset_mock_api_handlers();
  g_integration_event = "none";

  return client;
}

static rc_client_t* mock_client_with_integration_async()
{
  rc_client_t* client = mock_client_with_integration();
  client->callbacks.server_call = rc_client_server_call_async;
  return client;
}

static void rc_client_callback_expect_success(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_OK);
  ASSERT_PTR_NULL(error_message);
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void rc_client_callback_expect_uncalled(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_FAIL("Callback should not have been called.");
}

/* ----- login ----- */

static void assert_init_params(HWND hWnd, const char* client_name, const char* client_version)
{
  ASSERT_PTR_NOT_NULL((void*)hWnd);
  ASSERT_STR_EQUALS(client_name, "TestClient");
  ASSERT_STR_EQUALS(client_version, "1.0.1");
}

static int rc_client_integration_init(HWND hWnd, const char* client_name, const char* client_version)
{
  assert_init_params(hWnd, client_name, client_version);

  g_integration_event = "init";
  return 1;
}

static int rc_client_get_external_client(rc_client_external_t* client, int nVersion)
{
  if (strcmp(g_integration_event, "init") == 0)
    g_integration_event = "init2";

  return 1;
}

static const char* rc_client_integration_get_version(void)
{
  return "1.3.0";
}

static const char* rc_client_integration_get_host_url_offline(void)
{
  return "OFFLINE";
}

static void test_load_raintegration(void)
{
  g_client = mock_client_with_integration();
  g_client->state.raintegration->get_version = rc_client_integration_get_version;
  g_client->state.raintegration->init_client = rc_client_integration_init;
  g_client->state.raintegration->get_external_client = rc_client_get_external_client;

  mock_api_response("r=latestintegration", "{\"Success\":true,\"MinimumVersion\":\"1.3.0\"}");

  rc_client_begin_load_raintegration(g_client, L"C:\\Client", (HWND)0x1234, "TestClient", "1.0.1",
      rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_integration_event, "init2");

  rc_client_destroy(g_client);
}

static void test_load_raintegration_aborted(void)
{
  rc_client_async_handle_t* handle;

  g_client = mock_client_with_integration_async();
  g_client->state.raintegration->get_version = rc_client_integration_get_version;
  g_client->state.raintegration->init_client = rc_client_integration_init;
  g_client->state.raintegration->get_external_client = rc_client_get_external_client;

  handle = rc_client_begin_load_raintegration(g_client, L"C:\\Client", (HWND)0x1234, "TestClient", "1.0.1",
      rc_client_callback_expect_uncalled, g_callback_userdata);

  rc_client_abort_async(g_client, handle);

  async_api_response("r=latestintegration", "{\"Success\":true,\"MinimumVersion\":\"1.3.0\"}");

  ASSERT_STR_EQUALS(g_integration_event, "none");

  rc_client_destroy(g_client);
}

static void rc_client_callback_expect_outdated_version(int result, const char* error_message, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_ABORTED);
  ASSERT_STR_EQUALS(error_message, "RA_Integration version 1.3.0 is lower than minimum version 1.3.1");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
}

static void test_load_raintegration_outdated_version(void)
{
  g_client = mock_client_with_integration();
  g_client->state.raintegration->get_version = rc_client_integration_get_version;
  g_client->state.raintegration->init_client = rc_client_integration_init;
  g_client->state.raintegration->get_external_client = rc_client_get_external_client;

  mock_api_response("r=latestintegration", "{\"Success\":true,\"MinimumVersion\":\"1.3.1\"}");

  rc_client_begin_load_raintegration(g_client, L"C:\\Client", (HWND)0x1234, "TestClient", "1.0.1",
      rc_client_callback_expect_outdated_version, g_callback_userdata);

  ASSERT_STR_EQUALS(g_integration_event, "none");

  rc_client_destroy(g_client);
}

static void test_load_raintegration_supported_version(void)
{
  g_client = mock_client_with_integration();
  g_client->state.raintegration->get_version = rc_client_integration_get_version;
  g_client->state.raintegration->init_client = rc_client_integration_init;
  g_client->state.raintegration->get_external_client = rc_client_get_external_client;

  mock_api_response("r=latestintegration", "{\"Success\":true,\"MinimumVersion\":\"1.2.1\"}");

  rc_client_begin_load_raintegration(g_client, L"C:\\Client", (HWND)0x1234, "TestClient", "1.0.1",
      rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_integration_event, "init2");

  rc_client_destroy(g_client);
}

static void test_load_raintegration_offline(void)
{
  g_client = mock_client_with_integration();
  g_client->state.raintegration->get_host_url = rc_client_integration_get_host_url_offline;
  g_client->state.raintegration->get_version = rc_client_integration_get_version;
  g_client->state.raintegration->init_client_offline = rc_client_integration_init;
  g_client->state.raintegration->get_external_client = rc_client_get_external_client;

  mock_api_response("r=latestintegration", "{\"Success\":true,\"MinimumVersion\":\"1.2.1\"}");

  rc_client_begin_load_raintegration(g_client, L"C:\\Client", (HWND)0x1234, "TestClient", "1.0.1",
    rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_STR_EQUALS(g_integration_event, "init2");

  rc_client_destroy(g_client);
}

/* ----- harness ----- */

void test_client_raintegration(void) {
  TEST_SUITE_BEGIN();

  /* login */
  TEST(test_load_raintegration);
  TEST(test_load_raintegration_aborted);
  TEST(test_load_raintegration_outdated_version);
  TEST(test_load_raintegration_supported_version);
  TEST(test_load_raintegration_offline);

  TEST_SUITE_END();
}

#endif /* RC_CLIENT_SUPPORTS_RAINTEGRATION */
