#ifndef RC_CLIENT_INTERNAL_H
#define RC_CLIENT_INTERNAL_H

#include "rc_client.h"

#include "rc_api_request.h"
#include "rc_util.h"

#include "rc_client_do_frame.h"

#ifdef RC_CLIENT_SUPPORTS_RAINTEGRATION
 #include "rc_client_raintegration_internal.h"
#endif
#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
 #include "rc_client_external.h"
#endif
#ifdef RC_CLIENT_SUPPORTS_HASH
 #include "rc_hash.h"
#endif

#include "../util/rc_compat.h"

RC_BEGIN_C_DECLS

/*****************************************************************************\
| Forward Declarations (internal types)                                       |
\*****************************************************************************/

struct rc_api_fetch_game_sets_response_t; /* rc_api_runtime.h */
struct rc_client_async_handle_t;          /* rc_client_async.h */
struct rc_client_game_hash_t;             /* rc_client_game.h */
struct rc_client_game_info_t;             /* rc_client_game.h */
struct rc_client_load_state_t;            /* rc_client_load.h */
struct rc_memref_t;                       /* rc_memref.h */

/*****************************************************************************\
| Callbacks                                                                   |
\*****************************************************************************/

typedef void (RC_CCONV *rc_client_post_process_game_sets_response_t)(const struct rc_api_server_response_t* server_response,
              struct rc_api_fetch_game_sets_response_t* game_sets_response, rc_client_t* client, void* userdata);
typedef int (RC_CCONV *rc_client_can_submit_achievement_unlock_t)(uint32_t achievement_id, rc_client_t* client);
typedef int (RC_CCONV *rc_client_can_submit_leaderboard_entry_t)(uint32_t leaderboard_id, rc_client_t* client);
typedef int (RC_CCONV *rc_client_rich_presence_override_t)(rc_client_t* client, char buffer[], size_t buffersize);
typedef uint32_t (RC_CCONV* rc_client_identify_hash_func_t)(uint32_t console_id, const char* hash,
                  rc_client_t* client, void* callback_userdata);

typedef struct rc_client_callbacks_t {
  rc_client_read_memory_func_t read_memory;
  rc_client_event_handler_t event_handler;
  rc_client_server_call_t server_call;
  rc_client_message_callback_t log_call;
  rc_get_time_millisecs_func_t get_time_millisecs;
  rc_client_identify_hash_func_t identify_unknown_hash;
  rc_client_post_process_game_sets_response_t post_process_game_sets_response;
  rc_client_can_submit_achievement_unlock_t can_submit_achievement_unlock;
  rc_client_can_submit_leaderboard_entry_t can_submit_leaderboard_entry;
  rc_client_rich_presence_override_t rich_presence_override;

#ifdef RC_CLIENT_SUPPORTS_HASH
  rc_hash_callbacks_t hash;
#endif

  void* client_data;
} rc_client_callbacks_t;

/*****************************************************************************\
| Client                                                                      |
\*****************************************************************************/

enum {
  RC_CLIENT_USER_STATE_NONE,
  RC_CLIENT_USER_STATE_LOGIN_REQUESTED,
  RC_CLIENT_USER_STATE_LOGGED_IN
};

enum {
  RC_CLIENT_MASTERY_STATE_NONE,
  RC_CLIENT_MASTERY_STATE_PENDING,
  RC_CLIENT_MASTERY_STATE_SHOWN
};

enum {
  RC_CLIENT_SPECTATOR_MODE_OFF,
  RC_CLIENT_SPECTATOR_MODE_ON,
  RC_CLIENT_SPECTATOR_MODE_LOCKED
};

enum {
  RC_CLIENT_DISCONNECT_HIDDEN = 0,
  RC_CLIENT_DISCONNECT_VISIBLE = (1 << 0),
  RC_CLIENT_DISCONNECT_SHOW_PENDING = (1 << 1),
  RC_CLIENT_DISCONNECT_HIDE_PENDING = (1 << 2)
};

typedef struct rc_client_state_t {
  rc_mutex_t mutex;
  rc_buffer_t buffer;

  rc_client_scheduled_callback_data_t* scheduled_callbacks;
  rc_api_host_t host;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  rc_client_external_t* external_client;
  struct rc_client_external_conversions_t* external_client_conversions;
#endif
#ifdef RC_CLIENT_SUPPORTS_RAINTEGRATION
  rc_client_raintegration_t* raintegration;
#endif

  uint32_t frames_processed;
  uint32_t frames_at_last_ping;
  uint16_t unpaused_frame_decay;
  uint16_t required_unpaused_frames;

  uint8_t hardcore;
  uint8_t encore_mode;
  uint8_t spectator_mode;
  uint8_t unpromoted_enabled;
  uint8_t log_level;
  uint8_t user;
  uint8_t disconnect;
  uint8_t allow_leaderboards_in_casual;
  uint8_t allow_background_memory_reads;

  struct rc_client_load_state_t* load;
  struct rc_client_async_handle_t* async_handles[4];
  struct rc_memref_t* processing_memref;
} rc_client_state_t;

struct rc_client_t {
  struct rc_client_game_info_t* game;
  struct rc_client_game_hash_t* hashes;

  rc_client_user_t user;

  rc_client_callbacks_t callbacks;

  rc_client_state_t state;
};

/*****************************************************************************\
| Helpers                                                                     |
\*****************************************************************************/

#ifdef RC_NO_VARIADIC_MACROS
 void RC_CLIENT_LOG_ERR_FORMATTED(const rc_client_t* client, const char* format, ...);
 void RC_CLIENT_LOG_WARN_FORMATTED(const rc_client_t* client, const char* format, ...);
 void RC_CLIENT_LOG_INFO_FORMATTED(const rc_client_t* client, const char* format, ...);
 void RC_CLIENT_LOG_VERBOSE_FORMATTED(const rc_client_t* client, const char* format, ...);
#else
 void rc_client_log_message_formatted(const rc_client_t* client, const char* format, ...);
 #define RC_CLIENT_LOG_ERR_FORMATTED(client, format, ...) { if (client->state.log_level >= RC_CLIENT_LOG_LEVEL_ERROR) rc_client_log_message_formatted(client, format, __VA_ARGS__); }
 #define RC_CLIENT_LOG_WARN_FORMATTED(client, format, ...) { if (client->state.log_level >= RC_CLIENT_LOG_LEVEL_WARN) rc_client_log_message_formatted(client, format, __VA_ARGS__); }
 #define RC_CLIENT_LOG_INFO_FORMATTED(client, format, ...) { if (client->state.log_level >= RC_CLIENT_LOG_LEVEL_INFO) rc_client_log_message_formatted(client, format, __VA_ARGS__); }
 #define RC_CLIENT_LOG_VERBOSE_FORMATTED(client, format, ...) { if (client->state.log_level >= RC_CLIENT_LOG_LEVEL_VERBOSE) rc_client_log_message_formatted(client, format, __VA_ARGS__); }
#endif

void rc_client_log_message(const rc_client_t* client, const char* message);
#define RC_CLIENT_LOG_ERR(client, message) { if (client->state.log_level >= RC_CLIENT_LOG_LEVEL_ERROR) rc_client_log_message(client, message); }
#define RC_CLIENT_LOG_WARN(client, message) { if (client->state.log_level >= RC_CLIENT_LOG_LEVEL_WARN) rc_client_log_message(client, message); }
#define RC_CLIENT_LOG_INFO(client, message) { if (client->state.log_level >= RC_CLIENT_LOG_LEVEL_INFO) rc_client_log_message(client, message); }
#define RC_CLIENT_LOG_VERBOSE(client, message) { if (client->state.log_level >= RC_CLIENT_LOG_LEVEL_VERBOSE) rc_client_log_message(client, message); }

RC_END_C_DECLS

#endif /* RC_CLIENT_INTERNAL_H */
