#ifndef RC_RUNTIME2_INTERNAL_H
#define RC_RUNTIME2_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rc_runtime2.h"

#include "rc_compat.h"
#include "rc_runtime.h"
#include "rc_runtime_types.h"

typedef struct rc_runtime2_callbacks_t {
  rc_runtime2_read_memory_t read_memory;
  rc_runtime2_server_call_t server_call;
  rc_runtime2_message_callback_t log_call;
} rc_runtime2_callbacks_t;

typedef struct rc_runtime2_achievement_info_t {
  rc_runtime2_achievement_t public;

  rc_trigger_t* trigger;
  uint8_t md5[16];

} rc_runtime2_achievement_info_t;

#define RC_RUNTIME2_LEADERBOARD_TRACKER_UNASSIGNED (uint8_t)-1

typedef struct rc_runtime2_leaderboard_info_t {
  rc_runtime2_leaderboard_t public;

  rc_lboard_t* lboard;
  uint8_t md5[16];

  uint32_t value_djb2;
  int value;

  uint8_t tracker_id;

} rc_runtime2_leaderboard_info_t;

typedef struct rc_runtime2_game_hash_t {
  const char* hash;
  struct rc_runtime2_game_hash_t* next;
  uint32_t game_id;
} rc_runtime2_game_hash_t;

typedef struct rc_runtime2_game_info_t {
  rc_runtime2_game_t public;

  rc_runtime2_achievement_info_t* achievements;
  rc_runtime2_leaderboard_info_t* leaderboards;

  rc_runtime_t runtime;

  rc_api_buffer_t buffer;
} rc_runtime2_game_info_t;

enum {
   RC_RUNTIME2_LOAD_STATE_NONE,
   RC_RUNTIME2_LOAD_STATE_IDENTIFYING_GAME,
   RC_RUNTIME2_LOAD_STATE_AWAIT_LOGIN,
   RC_RUNTIME2_LOAD_STATE_FETCHING_GAME_DATA,
   RC_RUNTIME2_LOAD_STATE_STARTING_SESSION,
   RC_RUNTIME2_LOAD_STATE_DONE,
   RC_RUNTIME2_LOAD_STATE_UNKNOWN_GAME
};

enum {
  RC_RUNTIME2_USER_STATE_NONE,
  RC_RUNTIME2_USER_STATE_LOGIN_REQUESTED,
  RC_RUNTIME2_USER_STATE_LOGGED_IN
};

typedef struct rc_runtime2_load_state_t {
  rc_runtime2_t* runtime;
  rc_runtime2_callback_t callback;

  rc_runtime2_game_info_t* game;
  rc_runtime2_game_hash_t* hash;

  uint32_t* hardcore_unlocks;
  uint32_t* softcore_unlocks;
  uint32_t num_hardcore_unlocks;
  uint32_t num_softcore_unlocks;

  uint8_t progress;
  uint8_t outstanding_requests;
} rc_runtime2_load_state_t;

typedef struct rc_runtime2_state_t {
  uint8_t hardcore;
  uint8_t encore_mode;
  uint8_t user;
  uint8_t log_level;

  struct rc_runtime2_load_state_t* load;

  rc_mutex_t mutex;
} rc_runtime2_state_t;

typedef struct rc_runtime2_t {
  rc_runtime2_game_info_t* game;
  rc_runtime2_game_hash_t* hashes;

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

void rc_runtime_checksum(const char* memaddr, unsigned char* md5);

#ifdef __cplusplus
}
#endif

#endif /* RC_RUNTIME_H */
