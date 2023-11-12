#ifndef RC_CLIENT_EXTERNAL_H
#define RC_CLIENT_EXTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL

#include "rc_client.h"

/* NOTE: any function that is passed a callback also needs to be passed a client instance to pass
 * to the callback, and the external interface has to capture both. */

typedef void (*rc_client_external_enable_logging_func_t)(rc_client_t* client, int level, rc_client_message_callback_t callback);
typedef void (*rc_client_external_set_event_handler_func_t)(rc_client_t* client, rc_client_event_handler_t handler);
typedef void (*rc_client_external_set_read_memory_func_t)(rc_client_t* client, rc_client_read_memory_func_t handler);

typedef void (*rc_client_external_set_int_func_t)(int value);
typedef int (*rc_client_external_get_int_func_t)(void);
typedef size_t (*rc_client_external_get_string_func_t)(char buffer[], size_t buffer_size);

typedef void (*rc_client_external_async_handle_func_t)(rc_client_async_handle_t* handle);

typedef rc_client_async_handle_t* (*rc_client_external_begin_login_func_t)(rc_client_t* client,
    const char* username, const char* pass_token, rc_client_callback_t callback, void* callback_userdata);
typedef const rc_client_user_t* (*rc_client_external_get_user_info_func_t)(void);

typedef rc_client_async_handle_t* (*rc_client_external_begin_identify_and_load_game_func_t)(
  rc_client_t* client, uint32_t console_id, const char* file_path,
  const uint8_t* data, size_t data_size, rc_client_callback_t callback, void* callback_userdata);
typedef rc_client_async_handle_t* (*rc_client_external_begin_load_game_func_t)(rc_client_t* client,
  const char* hash, rc_client_callback_t callback, void* callback_userdata);
typedef const rc_client_game_t* (*rc_client_external_get_game_info_func_t)(void);
typedef void (*rc_client_external_get_user_game_summary_func_t)(rc_client_user_game_summary_t* summary);

/* NOTE: this returns an internal wrapper structure which contains the public list and a destructor function. */
struct rc_client_achievement_list_info_t;
typedef struct rc_client_achievement_list_info_t* (*rc_client_external_create_achievement_list_func_t)(int category, int grouping);
typedef const rc_client_achievement_t* (*rc_client_external_get_achievement_info_func_t)(uint32_t id);

/* NOTE: this returns an internal wrapper structure which contains the public list and a destructor function. */
struct rc_client_leaderboard_list_info_t;
typedef struct rc_client_leaderboard_list_info_t* (*rc_client_external_create_leaderboard_list_func_t)(int grouping);
typedef const rc_client_leaderboard_t* (*rc_client_external_get_leaderboard_info_func_t)(uint32_t id);

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

  rc_client_external_begin_identify_and_load_game_func_t begin_identify_and_load_game;
  rc_client_external_begin_load_game_func_t begin_load_game;
  rc_client_external_get_game_info_func_t get_game_info;
  rc_client_external_action_func_t unload_game;
  rc_client_external_get_user_game_summary_func_t get_user_game_summary;

  rc_client_external_create_achievement_list_func_t create_achievement_list;
  rc_client_external_get_int_func_t has_achievements;
  rc_client_external_get_achievement_info_func_t get_achievement_info;

  rc_client_external_create_leaderboard_list_func_t create_leaderboard_list;
  rc_client_external_get_int_func_t has_leaderboards;
  rc_client_external_get_leaderboard_info_func_t get_leaderboard_info;

  rc_client_external_get_string_func_t get_rich_presence_message;

} rc_client_external_t;

#define RC_CLIENT_EXTERNAL_VERSION 1

#endif /* RC_CLIENT_SUPPORTS_EXTERNAL */

#ifdef __cplusplus
}
#endif

#endif /* RC_CLIENT_EXTERNAL_H */
