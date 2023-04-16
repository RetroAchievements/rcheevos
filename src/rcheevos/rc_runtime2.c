#include "rc_runtime2_internal.h"

#include "rc_api_user.h"

#include "../rapi/rc_api_common.h"

#include <stdarg.h>

/* ===== Construction/Destruction ===== */

rc_runtime2_t* rc_runtime2_create(rc_runtime2_peek_t peek_function, rc_runtime2_server_call_t server_call_function)
{
  rc_runtime2_t* runtime = (rc_runtime2_t*)calloc(1, sizeof(rc_runtime2_t));
  if (!runtime)
    return NULL;

  runtime->callbacks.peek = peek_function;
  runtime->callbacks.server_call = server_call_function;
  runtime->state.hardcore = 1;

  rc_mutex_init(&runtime->state.mutex);

  rc_buf_init(&runtime->buffer);

  return runtime;
}

void rc_runtime2_destroy(rc_runtime2_t* runtime)
{
  if (!runtime)
    return;

  rc_buf_destroy(&runtime->buffer);

  rc_mutex_destroy(&runtime->state.mutex);

  free(runtime);
}

/* ===== Logging ===== */

void rc_runtime2_log_message(const rc_runtime2_t* runtime, const char* format, ...)
{
  char buffer[256];
  int result;
  va_list args;

  if (!runtime->callbacks.log_call)
    return;

  va_start(args, format);

#ifdef __STDC_WANT_SECURE_LIB__
  result = vsprintf_s(buffer, sizeof(buffer), format, args);
#else
  /* assume buffer is large enough and ignore size */
  (void)size;
  result = vsprintf(buffer, format, args);
#endif

  va_end(args);

  runtime->callbacks.log_call(buffer);
}

void rc_runtime2_enable_logging(rc_runtime2_t* runtime, int level, rc_runtime2_message_callback_t callback)
{
  runtime->callbacks.log_call = callback;
  runtime->state.log_level = callback ? level : RC_RUNTIME2_LOG_LEVEL_NONE;
}

/* ===== Common ===== */

static const char* rc_runtime2_server_error_message(int* result, int http_status_code, const rc_api_response_t* response)
{
  if (!response->succeeded)
  {
    if (*result == RC_OK)
    {
      *result = RC_API_FAILURE;
      if (!response->error_message)
        return "Unexpected API failure with no error message";
    }

    if (response->error_message)
      return response->error_message;
  }

  if (*result != RC_OK)
    return rc_error_str(*result);

  return NULL;
}

typedef struct rc_runtime2_generic_callback_data_t {
  rc_runtime2_t* runtime;
  rc_runtime2_callback_t callback;
} rc_runtime2_generic_callback_data_t;


/* ===== Login ===== */

static void rc_runtime2_login_callback(const char* server_response_body, int http_status_code, void* callback_data)
{
  rc_runtime2_generic_callback_data_t* login_callback_data = (rc_runtime2_generic_callback_data_t*)callback_data;
  rc_runtime2_t* runtime = login_callback_data->runtime;
  rc_api_login_response_t login_response;

  int result = rc_api_process_login_response(&login_response, server_response_body);
  const char* error_message = rc_runtime2_server_error_message(&result, http_status_code, &login_response.response);
  if (error_message)
  {
    rc_mutex_lock(&runtime->state.mutex);
    runtime->state.user = RC_RUNTIME2_USER_STATE_NONE;
    rc_mutex_unlock(&runtime->state.mutex);

    RC_RUNTIME2_LOG_ERR(runtime, "Login failed: %s", error_message);
    if (login_callback_data->callback)
      login_callback_data->callback(result, error_message, runtime);
  }
  else
  {
    runtime->user.username = rc_buf_strcpy(&runtime->buffer, login_response.username);

    if (strcmp(login_response.username, login_response.display_name) == 0)
      runtime->user.display_name = runtime->user.username;
    else
      runtime->user.display_name = rc_buf_strcpy(&runtime->buffer, login_response.display_name);

    runtime->user.token = rc_buf_strcpy(&runtime->buffer, login_response.api_token);
    runtime->user.score = login_response.score;
    runtime->user.num_unread_messages = login_response.num_unread_messages;

    rc_mutex_lock(&runtime->state.mutex);
    runtime->state.user = RC_RUNTIME2_USER_STATE_LOGGED_IN;
    rc_mutex_unlock(&runtime->state.mutex);

    RC_RUNTIME2_LOG_ERR(runtime, "Login succeeded: %s", error_message);
    if (login_callback_data->callback)
      login_callback_data->callback(RC_OK, NULL, runtime);
  }

  free(login_callback_data);
}

static void rc_runtime2_start_login(rc_runtime2_t* runtime,
  const rc_api_login_request_t* login_request, rc_runtime2_callback_t callback)
{
  rc_runtime2_generic_callback_data_t* callback_data;
  rc_api_request_t request;
  int result = rc_api_init_login_request(&request, login_request);

  if (result == RC_OK)
  {
    rc_mutex_lock(&runtime->state.mutex);

    if (runtime->state.user == RC_RUNTIME2_USER_STATE_LOGIN_REQUESTED)
      result = RC_INVALID_STATE;
    runtime->state.user = RC_RUNTIME2_USER_STATE_LOGIN_REQUESTED;

    rc_mutex_unlock(&runtime->state.mutex);
  }

  if (result != RC_OK)
  {
    callback(result, rc_error_str(result), runtime);
    return;
  }

  callback_data = (rc_runtime2_generic_callback_data_t*)malloc(sizeof(*callback_data));
  callback_data->runtime = runtime;
  callback_data->callback = callback;

  runtime->callbacks.server_call(&request, rc_runtime2_login_callback, callback_data, runtime);
}

void rc_runtime2_start_login_with_password(rc_runtime2_t* runtime,
  const char* username, const char* password, rc_runtime2_callback_t callback)
{
  rc_api_login_request_t login_request;
  
  if (!username || !username[0])
  {
    callback(RC_INVALID_STATE, "username is required", runtime);
    return;
  }

  if (!password || !password[0])
  {
    callback(RC_INVALID_STATE, "password is required", runtime);
    return;
  }

  memset(&login_request, 0, sizeof(login_request));
  login_request.username = username;
  login_request.password = password;

  RC_RUNTIME2_LOG_INFO(runtime, "Attempting to log in %s (with password)", username);
  rc_runtime2_start_login(runtime, &login_request, callback);
}

void rc_runtime2_start_login_with_token(rc_runtime2_t* runtime,
  const char* username, const char* token, rc_runtime2_callback_t callback)
{
  rc_api_login_request_t login_request;

  if (!username || !username[0])
  {
    callback(RC_INVALID_STATE, "username is required", runtime);
    return;
  }

  if (!token || !token[0])
  {
    callback(RC_INVALID_STATE, "token is required", runtime);
    return;
  }

  memset(&login_request, 0, sizeof(login_request));
  login_request.username = username;
  login_request.api_token = token;

  RC_RUNTIME2_LOG_INFO(runtime, "Attempting to log in %s (with token)", username);
  rc_runtime2_start_login(runtime, &login_request, callback);
}

const rc_runtime2_user_t* rc_runtime2_user_info(const rc_runtime2_t* runtime)
{
  return (runtime->state.user == RC_RUNTIME2_USER_STATE_LOGGED_IN) ? &runtime->user : NULL;
}

/* ===== ? ===== */


