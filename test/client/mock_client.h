#ifndef RHASH_TEST_MOCK_CLIENT_H
#define RHASH_TEST_MOCK_CLIENT_H

#include "rc_export.h"

#include "../test_framework.h"

#include <stdint.h>

RC_BEGIN_C_DECLS

struct rc_client_t;       /* rc_client.h */
struct rc_client_event_t; /* rc_client.h */

struct rc_client_t* g_client;
void* g_callback_userdata;
uint64_t g_now;

void mock_memory(uint8_t* memory, uint32_t size);

void mock_api_response(const char* request_params, const char* response_body);
void mock_api_error(const char* request_params, const char* response_body, int http_status_code);
void mock_api_error_with_length(const char* request_params, const char* response_body, size_t response_body_length, int http_status_code);
void async_api_response(const char* request_params, const char* response_body);
void async_api_error(const char* request_params, const char* response_body, int http_status_code);

void reset_mock_api_handlers(void);

struct rc_client_event_t* find_event(uint8_t type, uint32_t id);
void reset_mock_events(void);
int get_pending_event_count(void);

void _assert_api_called(const char* request_params, int count);
#define assert_api_called(request_params) ASSERT_HELPER(_assert_api_called(request_params, 1), "assert_api_called")
#define assert_api_not_called(request_params) ASSERT_HELPER(_assert_api_called(request_params, 0), "assert_api_not_called")
#define assert_api_call_count(request_params, num) ASSERT_HELPER(_assert_api_called(request_params, num), "assert_api_call_count")
#define assert_api_pending(request_params) ASSERT_HELPER(_assert_api_called(request_params, -1), "assert_api_pending")
#define assert_api_not_pending(request_params) ASSERT_HELPER(_assert_api_called(request_params, 0), "assert_api_not_pending")

struct rc_client_t* mock_client_not_logged_in(void);
struct rc_client_t* mock_client_not_logged_in_async(void);
struct rc_client_t* mock_client_logged_in(void);
struct rc_client_t* mock_client_logged_in_async(void);
struct rc_client_t* mock_client_game_loaded(const char* patchdata, const char* unlocks);
void mock_client_load_game(const char* patchdata, const char* unlocks);
void mock_client_load_game_casual(const char* patchdata, const char* unlocks);
void mock_client_enable_async(struct rc_client_t* client);

void rc_client_callback_expect_access_denied(int result, const char* error_message, struct rc_client_t* client, void* callback_userdata);
void rc_client_callback_expect_credentials_error(int result, const char* error_message, struct rc_client_t* client, void* callback_userdata);
void rc_client_callback_expect_game_no_longer_active(int result, const char* error_message, struct rc_client_t* client, void* callback_userdata);
void rc_client_callback_expect_login_aborted(int result, const char* error_message, struct rc_client_t* client, void* callback_userdata);
void rc_client_callback_expect_login_required(int result, const char* error_message, struct rc_client_t* client, void* callback_userdata);
void rc_client_callback_expect_no_game_loaded(int result, const char* error_message, struct rc_client_t* client, void* callback_data);
void rc_client_callback_expect_no_internet(int result, const char* error_message, struct rc_client_t* client, void* callback_userdata);
void rc_client_callback_expect_success(int result, const char* error_message, struct rc_client_t* client, void* callback_userdata);
void rc_client_callback_expect_timeout(int result, const char* error_message, struct rc_client_t* client, void* callback_userdata);
void rc_client_callback_expect_token_error(int result, const char* error_message, struct rc_client_t* client, void* callback_userdata);
void rc_client_callback_expect_uncalled(int result, const char* error_message, struct rc_client_t* client, void* callback_userdata);
void rc_client_callback_expect_unknown_game(int result, const char* error_message, struct rc_client_t* client, void* callback_userdata);

RC_END_C_DECLS

#endif /* RHASH_TEST_MOCK_CLIENT_H */
