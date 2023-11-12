#ifndef RC_CLIENT_EXTERNAL_H
#define RC_CLIENT_EXTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL

#include "rc_client.h"

typedef void (*rc_client_external_enable_logging_func_t)(int level, rc_client_message_callback_t callback);
typedef void (*rc_client_external_set_event_handler_func_t)(rc_client_event_handler_t handler);
typedef void (*rc_client_external_set_read_memory_func_t)(rc_client_read_memory_func_t handler);

typedef void (*rc_client_external_set_int_func_t)(int value);
typedef int (*rc_client_external_get_int_func_t)(void);

typedef void (*rc_client_external_async_handle_func_t)(rc_client_async_handle_t* handle);

/* client must be passed back to callback along with callback_userdata */
typedef rc_client_async_handle_t* (*rc_client_external_begin_login_func_t)(rc_client_t* client,
    const char* username, const char* pass_token, rc_client_callback_t callback, void* callback_userdata);

typedef const rc_client_user_t* (*rc_client_external_get_user_info_func_t)(void);

typedef void (*rc_client_external_action_func_t)(void);

typedef struct rc_client_external_t
{
  rc_client_external_action_func_t destroy;

  rc_client_external_enable_logging_func_t enable_logging;
  rc_client_external_set_event_handler_func_t set_event_handler;
  rc_client_external_set_read_memory_func_t set_read_memory;

  rc_client_external_set_int_func_t set_hardcore_enabled;
  rc_client_external_get_int_func_t get_hardcore_enabled;
  rc_client_external_set_int_func_t set_unofficial_enabled;
  rc_client_external_get_int_func_t get_unofficial_enabled;
  rc_client_external_set_int_func_t set_encore_mode_enabled;
  rc_client_external_get_int_func_t get_encore_mode_enabled;
  rc_client_external_set_int_func_t set_spectator_mode_enabled;
  rc_client_external_get_int_func_t get_spectator_mode_enabled;

  rc_client_external_async_handle_func_t abort_async;

  rc_client_external_begin_login_func_t begin_login_with_password;
  rc_client_external_begin_login_func_t begin_login_with_token;
  rc_client_external_action_func_t logout;
  rc_client_external_get_user_info_func_t get_user_info;

} rc_client_external_t;

#define RC_CLIENT_EXTERNAL_VERSION 1

#endif /* RC_CLIENT_SUPPORTS_EXTERNAL */

#ifdef __cplusplus
}
#endif

#endif /* RC_CLIENT_EXTERNAL_H */
