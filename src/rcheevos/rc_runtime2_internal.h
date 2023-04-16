#ifndef RC_RUNTIME2_INTERNAL_H
#define RC_RUNTIME2_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rc_runtime2.h"

#include "rc_compat.h"

typedef struct rc_runtime2_callbacks_t {
  rc_runtime2_peek_t peek;
  rc_runtime2_server_call_t server_call;
  rc_runtime2_message_callback_t log_call;
} rc_runtime2_callbacks_t;

typedef struct rc_runtime2_state_t {
  uint8_t hardcore;
  uint8_t user;
  uint8_t log_level;

  rc_mutex_t mutex;
} rc_runtime2_state_t;

enum {
  RC_RUNTIME2_USER_STATE_NONE,
  RC_RUNTIME2_USER_STATE_LOGIN_REQUESTED,
  RC_RUNTIME2_USER_STATE_LOGGED_IN
};

typedef struct rc_runtime2_t {
  rc_runtime2_user_t user;

  rc_runtime2_callbacks_t callbacks;

  rc_runtime2_state_t state;

  rc_api_buffer_t buffer;
} rc_runtime2_t;

void rc_runtime2_log_message(const rc_runtime2_t* runtime, const char* format, ...);
#define RC_RUNTIME2_LOG_ERR(runtime, format, ...) { if (runtime->state.log_level >= RC_RUNTIME2_LOG_LEVEL_ERROR) rc_runtime2_log_message(runtime, format, __VA_ARGS__); }
#define RC_RUNTIME2_LOG_WARN(runtime, format, ...) { if (runtime->state.log_level >= RC_RUNTIME2_LOG_LEVEL_WARN) rc_runtime2_log_message(runtime, format, __VA_ARGS__); }
#define RC_RUNTIME2_LOG_INFO(runtime, format, ...) { if (runtime->state.log_level >= RC_RUNTIME2_LOG_LEVEL_INFO) rc_runtime2_log_message(runtime, format, __VA_ARGS__); }
#define RC_RUNTIME2_LOG_VERBOSE(runtime, format, ...) { if (runtime->state.log_level >= RC_RUNTIME2_LOG_LEVEL_VERBOSE) rc_runtime2_log_message(runtime, format, __VA_ARGS__); }

#ifdef __cplusplus
}
#endif

#endif /* RC_RUNTIME_H */
