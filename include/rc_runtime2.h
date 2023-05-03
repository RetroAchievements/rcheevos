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
 * Callback used to read num_bytes bytes from memory starting at address into buffer.
 * Returns the number of bytes read. A return value of 0 indicates the address was invalid.
 */
typedef unsigned (*rc_runtime2_read_memory_t)(unsigned address, uint8_t* buffer, unsigned num_bytes, rc_runtime2_t* runtime);

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
rc_runtime2_t* rc_runtime2_create(rc_runtime2_read_memory_t read_memory_function, rc_runtime2_server_call_t server_call_function);

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
  RC_RUNTIME2_LOG_LEVEL_ERROR = 1,
  RC_RUNTIME2_LOG_LEVEL_WARN = 2,
  RC_RUNTIME2_LOG_LEVEL_INFO = 3,
  RC_RUNTIME2_LOG_LEVEL_VERBOSE = 4
};

/**
 * Attempt to login a user.
 */
void rc_runtime2_begin_login_with_password(rc_runtime2_t* runtime, const char* username, const char* password, rc_runtime2_callback_t callback);
void rc_runtime2_begin_login_with_token(rc_runtime2_t* runtime, const char* username, const char* token, rc_runtime2_callback_t callback);

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
void rc_runtime2_begin_load_game(rc_runtime2_t* runtime, const char* hash, rc_runtime2_callback_t callback);

/**
 * Unloads the current game.
 */
void rc_runtime2_unload_game(rc_runtime2_t* runtime);

typedef struct rc_runtime2_game_t {
  uint32_t id;
  uint32_t console_id;
  const char* title;
  const char* hash;
  char badge_name[16];

  uint32_t num_achievements;
  uint32_t num_leaderboards;
} rc_runtime2_game_t;

/**
 * Get information about the current game. Returns NULL if no game is loaded.
 */
const rc_runtime2_game_t* rc_runtime2_game_info(const rc_runtime2_t* runtime);

enum {
  RC_RUNTIME2_ACHIEVEMENT_STATE_INACTIVE = 0,
  RC_RUNTIME2_ACHIEVEMENT_STATE_ACTIVE = 1,
  RC_RUNTIME2_ACHIEVEMENT_STATE_UNLOCKED = 2,
  RC_RUNTIME2_ACHIEVEMENT_STATE_DISABLED = 3
};

enum {
  RC_RUNTIME2_ACHIEVEMENT_CATEGORY_NONE = 0,
  RC_RUNTIME2_ACHIEVEMENT_CATEGORY_CORE = (1 << 0),
  RC_RUNTIME2_ACHIEVEMENT_CATEGORY_UNOFFICIAL = (1 << 1),
  RC_RUNTIME2_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL = RC_RUNTIME2_ACHIEVEMENT_CATEGORY_CORE | RC_RUNTIME2_ACHIEVEMENT_CATEGORY_UNOFFICIAL
};

enum {
  RC_RUNTIME2_ACHIEVEMENT_BUCKET_UNKNOWN = 0,
  RC_RUNTIME2_ACHIEVEMENT_BUCKET_LOCKED = 1,
  RC_RUNTIME2_ACHIEVEMENT_BUCKET_UNLOCKED = 2,
  RC_RUNTIME2_ACHIEVEMENT_BUCKET_UNSUPPORTED = 3,
  RC_RUNTIME2_ACHIEVEMENT_BUCKET_UNOFFICIAL = 4,
  RC_RUNTIME2_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED = 5,
  RC_RUNTIME2_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE = 6,
  RC_RUNTIME2_ACHIEVEMENT_BUCKET_ALMOST_THERE = 7
};

enum {
  RC_RUNTIME2_ACHIEVEMENT_UNLOCKED_NONE = 0,
  RC_RUNTIME2_ACHIEVEMENT_UNLOCKED_SOFTCORE = (1 << 0),
  RC_RUNTIME2_ACHIEVEMENT_UNLOCKED_HARDCORE = (1 << 1),
  RC_RUNTIME2_ACHIEVEMENT_UNLOCKED_BOTH = RC_RUNTIME2_ACHIEVEMENT_UNLOCKED_SOFTCORE | RC_RUNTIME2_ACHIEVEMENT_UNLOCKED_HARDCORE
};

typedef struct rc_runtime2_achievement_t {
  const char* title;
  const char* description;
  char badge_name[8];
  char measured_progress[24];
  uint32_t id;
  uint32_t points;
  time_t unlock_time;
  uint8_t state;
  uint8_t category;
  uint8_t bucket;
  uint8_t unlocked;
} rc_runtime2_achievement_t;

/**
 * Get information about an achievement. Returns NULL if not found.
 */
const rc_runtime2_achievement_t* rc_runtime2_achievement_info(const rc_runtime2_t* runtime, uint32_t id);

uint32_t rc_runtime2_get_achievement_count(const rc_runtime2_t* runtime, int category);

typedef struct rc_runtime2_achievement_bucket_t {
  rc_runtime2_achievement_t** achievements;
  uint32_t num_achievements;

  const char* label;
  uint8_t id;
} rc_runtime2_achievement_bucket_t;

typedef struct rc_runtime2_achievement_list_t {
  rc_runtime2_achievement_bucket_t* buckets;
  uint32_t num_buckets;
} rc_runtime2_achievement_list_t;

enum {
  RC_RUNTIME2_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE = 0,
  RC_RUNTIME2_ACHIEVEMENT_LIST_GROUPING_PROGRESS = 1,
};

/**
 * Gets a list of achievements matching the specified category and grouping.
 * Returns an allocated list that must be free'd by calling rc_runtime2_destroy_achievement_list.
 */
rc_runtime2_achievement_list_t* rc_runtime2_get_achievement_list(rc_runtime2_t* runtime, int category, int grouping);

/**
 * Destroys a list allocated by rc_runtime2_get_achievement_list.
 */
void rc_runtime2_destroy_achievement_list(rc_runtime2_achievement_list_t* list);

enum {
  RC_RUNTIME2_LEADERBOARD_STATE_INACTIVE = 0,
  RC_RUNTIME2_LEADERBOARD_STATE_ACTIVE = 1,
  RC_RUNTIME2_LEADERBOARD_STATE_TRACKING = 2,
  RC_RUNTIME2_LEADERBOARD_STATE_DISABLED = 3
};

typedef struct rc_runtime2_leaderboard_t {
  const char* title;
  const char* description;
  uint32_t id;
  uint8_t format;
  uint8_t state;
} rc_runtime2_leaderboard_t;

#ifdef __cplusplus
}
#endif

#endif /* RC_RUNTIME_H */
