#ifndef RC_CLIENT_INTERNAL_H
#define RC_CLIENT_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rc_client.h"

#include "rc_compat.h"
#include "rc_runtime.h"
#include "rc_runtime_types.h"

typedef struct rc_client_callbacks_t {
  rc_client_read_memory_t read_memory;
  rc_client_event_handler_t event_handler;
  rc_client_server_call_t server_call;
  rc_client_message_callback_t log_call;

  void* client_data;
} rc_client_callbacks_t;

struct rc_client_scheduled_callback_data_t;
typedef void (*rc_client_scheduled_callback_t)(struct rc_client_scheduled_callback_data_t* callback_data, rc_client_t* client, time_t now);

typedef struct rc_client_scheduled_callback_data_t
{
  time_t when;
  unsigned related_id;
  rc_client_scheduled_callback_t callback;
  void* data;
  struct rc_client_scheduled_callback_data_t* next;
} rc_client_scheduled_callback_data_t;

void rc_client_schedule_callback(rc_client_t* client, rc_client_scheduled_callback_data_t* scheduled_callback);

enum {
  RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_NONE = 0,
  RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_TRIGGERED = (1 << 1),
  RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_SHOW = (1 << 2),
  RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_HIDE = (1 << 3),
  RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_PROGRESS_UPDATED = (1 << 4),
};

typedef struct rc_client_achievement_info_t {
  rc_client_achievement_t public;

  rc_trigger_t* trigger;
  uint8_t md5[16];

  time_t unlock_time_hardcore;
  time_t unlock_time_softcore;

  uint8_t pending_events;
} rc_client_achievement_info_t;

#define RC_CLIENT_LEADERBOARD_TRACKER_UNASSIGNED (uint8_t)-1

enum {
  RC_CLIENT_LEADERBOARD_PENDING_EVENT_NONE = 0,
  RC_CLIENT_LEADERBOARD_PENDING_EVENT_STARTED = (1 << 1),
  RC_CLIENT_LEADERBOARD_PENDING_EVENT_FAILED = (1 << 2),
  RC_CLIENT_LEADERBOARD_PENDING_EVENT_SUBMITTED = (1 << 3),
};

typedef struct rc_client_leaderboard_info_t {
  rc_client_leaderboard_t public;

  rc_lboard_t* lboard;
  uint8_t md5[16];

  uint32_t value_djb2;
  int value;

  uint8_t format;
  uint8_t pending_events;
  uint8_t tracker_id;
} rc_client_leaderboard_info_t;

enum {
  RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_NONE = 0,
  RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_UPDATE = (1 << 1),
  RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_SHOW = (1 << 2),
  RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_HIDE = (1 << 3),
};

typedef struct rc_client_leaderboard_tracker_info_t {
  rc_client_leaderboard_tracker_t public;
  int raw_value;

  uint32_t value_djb2;

  uint8_t format;
  uint8_t pending_events;
  uint8_t reference_count;
  uint8_t value_from_hits;
} rc_client_leaderboard_tracker_info_t;

typedef struct rc_client_game_hash_t {
  char hash[33];
  uint32_t game_id;
  struct rc_client_game_hash_t* next;
} rc_client_game_hash_t;

typedef struct rc_client_media_hash_t {
  rc_client_game_hash_t* game_hash;
  struct rc_client_media_hash_t* next;
  uint32_t path_djb2;
} rc_client_media_hash_t;

typedef struct rc_client_game_info_t {
  rc_client_game_t public;

  rc_client_achievement_info_t* achievements;
  rc_client_leaderboard_info_t* leaderboards;
  rc_client_leaderboard_tracker_info_t* leaderboard_trackers;

  rc_client_media_hash_t* media_hash;

  rc_runtime_t runtime;
  uint8_t waiting_for_reset;

  uint8_t mastery;

  uint8_t leaderboard_trackers_capacity;
  uint8_t leaderboard_trackers_size;

  rc_api_buffer_t buffer;
} rc_client_game_info_t;

enum {
   RC_CLIENT_LOAD_STATE_NONE,
   RC_CLIENT_LOAD_STATE_IDENTIFYING_GAME,
   RC_CLIENT_LOAD_STATE_AWAIT_LOGIN,
   RC_CLIENT_LOAD_STATE_FETCHING_GAME_DATA,
   RC_CLIENT_LOAD_STATE_STARTING_SESSION,
   RC_CLIENT_LOAD_STATE_DONE,
   RC_CLIENT_LOAD_STATE_UNKNOWN_GAME
};

enum {
  RC_CLIENT_USER_STATE_NONE,
  RC_CLIENT_USER_STATE_LOGIN_REQUESTED,
  RC_CLIENT_USER_STATE_LOGGED_IN
};

enum {
  RC_CLIENT_MASTERY_STATE_NONE,
  RC_CLIENT_MASTERY_STATE_PENDING,
  RC_CLIENT_MASTERY_STATE_SHOWN,
};

struct rc_client_load_state_t;

typedef struct rc_client_state_t {
  rc_mutex_t mutex;

  rc_client_scheduled_callback_data_t* scheduled_callbacks;

  uint8_t hardcore;
  uint8_t encore_mode;
  uint8_t spectator_mode;
  uint8_t test_unofficial;
  uint8_t log_level;
  uint8_t user;

  struct rc_client_load_state_t* load;
  rc_memref_t* processing_memref;

  rc_peek_t legacy_peek;
} rc_client_state_t;

typedef struct rc_client_t {
  rc_client_game_info_t* game;
  rc_client_game_hash_t* hashes;

  rc_client_user_t user;

  rc_client_callbacks_t callbacks;

  rc_client_state_t state;

  rc_api_buffer_t buffer;
} rc_client_t;

void rc_client_log_message(const rc_client_t* client, const char* format, ...);
#define RC_CLIENT_LOG_ERR(client, format, ...) { if (client->state.log_level >= RC_CLIENT_LOG_LEVEL_ERROR) rc_client_log_message(client, format, __VA_ARGS__); }
#define RC_CLIENT_LOG_WARN(client, format, ...) { if (client->state.log_level >= RC_CLIENT_LOG_LEVEL_WARN) rc_client_log_message(client, format, __VA_ARGS__); }
#define RC_CLIENT_LOG_INFO(client, format, ...) { if (client->state.log_level >= RC_CLIENT_LOG_LEVEL_INFO) rc_client_log_message(client, format, __VA_ARGS__); }
#define RC_CLIENT_LOG_VERBOSE(client, format, ...) { if (client->state.log_level >= RC_CLIENT_LOG_LEVEL_VERBOSE) rc_client_log_message(client, format, __VA_ARGS__); }

/* internals pulled from runtime.c */
void rc_runtime_checksum(const char* memaddr, unsigned char* md5);
int rc_trigger_contains_memref(const rc_trigger_t* trigger, const rc_memref_t* memref);
int rc_value_contains_memref(const rc_value_t* value, const rc_memref_t* memref);
/* end runtime.c internals */

enum {
  RC_CLIENT_LEGACY_PEEK_AUTO,
  RC_CLIENT_LEGACY_PEEK_CONSTRUCTED,
  RC_CLIENT_LEGACY_PEEK_LITTLE_ENDIAN_READS
};

void rc_client_set_legacy_peek(rc_client_t* client, int method);

#ifdef __cplusplus
}
#endif

#endif /* RC_RUNTIME_H */