#ifndef RC_RUNTIME2_H
#define RC_RUNTIME2_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rc_api_request.h"
#include "rc_error.h"

#include <stddef.h>
#include <stdint.h>

/* implementation abstracted in rc_runtime2_internal.h */
typedef struct rc_runtime2_t rc_runtime2_t;

/*****************************************************************************\
| Callbacks                                                                   |
\*****************************************************************************/

/**
 * Callback used to read num_bytes bytes from memory starting at address. If
 * num_bytes is greater than 1, the value is read in little-endian from memory.
 */
typedef unsigned (*rc_runtime2_peek_t)(unsigned address, unsigned num_bytes, rc_runtime2_t* runtime);

/**
 * Internal method passed to rc_runtime2_server_call_t to process the server response.
 */
typedef void (*rc_runtime2_server_callback_t)(const char* server_response_body, int http_status_code, void* callback_data);

/**
 * Callback used to issue a request to the server.
 */
typedef void (*rc_runtime2_server_call_t)(const rc_api_request_t* request, rc_runtime2_server_callback_t callback, void* callback_data, rc_runtime2_t* runtime);

/**
 * Generic callback for asynchronous eventing.
 */
typedef void (*rc_runtime2_callback_t)(int result, const char* error_message, rc_runtime2_t* runtime);


/**
 * Callback for logging or displaying a message.
 */
typedef void (*rc_runtime2_message_callback_t)(const char* message);

/*****************************************************************************\
| Runtime                                                                     |
\*****************************************************************************/

/**
 * Creates a new rc_runtime2_t object.
 */
rc_runtime2_t* rc_runtime2_create(rc_runtime2_peek_t peek_function, rc_runtime2_server_call_t server_call_function);

/**
 * Releases resources associated to a rc_runtime2_t object.
 * Pointer will no longer be valid after making this call.
 */
void rc_runtime2_destroy(rc_runtime2_t* runtime);

/**
 * Sets the logging level and provides a callback to be called to do the logging.
 */
void rc_runtime2_enable_logging(rc_runtime2_t* runtime, int level, rc_runtime2_message_callback_t callback);
enum
{
  RC_RUNTIME2_LOG_LEVEL_NONE = 0,
  RC_RUNTIME2_LOG_LEVEL_ERROR,
  RC_RUNTIME2_LOG_LEVEL_WARN,
  RC_RUNTIME2_LOG_LEVEL_INFO,
  RC_RUNTIME2_LOG_LEVEL_VERBOSE
};

/**
 * Attempt to login a user.
 */
void rc_runtime2_start_login_with_password(rc_runtime2_t* runtime, const char* username, const char* password, rc_runtime2_callback_t callback);
void rc_runtime2_start_login_with_token(rc_runtime2_t* runtime, const char* username, const char* token, rc_runtime2_callback_t callback);

typedef struct rc_runtime2_user_t {
  const char* display_name;
  const char* username;
  const char* token;
  uint32_t score;
  uint32_t num_unread_messages;
} rc_runtime2_user_t;

/**
 * Get information about the logged in user. Will return NULL if user is not logged in.
 */
const rc_runtime2_user_t* rc_runtime2_user_info(const rc_runtime2_t* runtime);

/**
 * Start loading a game.
 */
void rc_runtime2_start_load_game(rc_runtime2_t* runtime, const char* hash, rc_runtime2_callback_t callback);

void rc_runtime2_unload_game(rc_runtime2_t* runtime);

typedef struct rc_runtime2_game_t {
  uint32_t id;
  uint32_t console_id;
  const char* title;
  const char* hash;
  char badge_name[16];
} rc_runtime2_game_t;

const rc_runtime2_game_t* rc_runtime2_game_info(const rc_runtime2_t* runtime);

#ifdef __cplusplus
}
#endif

#endif /* RC_RUNTIME_H */
