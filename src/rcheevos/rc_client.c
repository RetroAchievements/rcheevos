#include "rc_client_internal.h"

#include "rc_api_info.h"
#include "rc_api_runtime.h"
#include "rc_api_user.h"
#include "rc_consoles.h"
#include "rc_internal.h"
#include "rc_hash.h"

#include "../rapi/rc_api_common.h"

#include <stdarg.h>

#define RC_CLIENT_UNKNOWN_GAME_ID (uint32_t)-1
#define RC_CLIENT_RECENT_UNLOCK_DELAY_SECONDS (10 * 60) /* ten minutes */

struct rc_client_async_handle_t {
  uint8_t aborted;
};

typedef struct rc_client_generic_callback_data_t {
  rc_client_t* client;
  rc_client_callback_t callback;
  void* callback_userdata;
  rc_client_async_handle_t async_handle;
} rc_client_generic_callback_data_t;

typedef struct rc_client_pending_media_t
{
  const char* file_path;
  uint8_t* data;
  size_t data_size;
  rc_client_callback_t callback;
  void* callback_userdata;
} rc_client_pending_media_t;

typedef struct rc_client_load_state_t
{
  rc_client_t* client;
  rc_client_callback_t callback;
  void* callback_userdata;

  rc_client_game_info_t* game;
  rc_client_subset_info_t* subset;
  rc_client_game_hash_t* hash;

  rc_hash_iterator_t hash_iterator;
  rc_client_pending_media_t* pending_media;

  uint32_t* hardcore_unlocks;
  uint32_t* softcore_unlocks;
  uint32_t num_hardcore_unlocks;
  uint32_t num_softcore_unlocks;

  rc_client_async_handle_t async_handle;

  uint8_t progress;
  uint8_t outstanding_requests;
  uint8_t hash_console_id;
} rc_client_load_state_t;

static void rc_client_load_error(rc_client_load_state_t* load_state, int result, const char* error_message);
static void rc_client_begin_fetch_game_data(rc_client_load_state_t* callback_data);
static rc_client_async_handle_t* rc_client_load_game(rc_client_load_state_t* load_state, const char* hash, const char* file_path);
static void rc_client_ping(rc_client_scheduled_callback_data_t* callback_data, rc_client_t* client, time_t now);
static void rc_client_raise_leaderboard_events(rc_client_t* client, rc_client_subset_info_t* subset);
static void rc_client_release_leaderboard_tracker(rc_client_game_info_t* game, rc_client_leaderboard_info_t* leaderboard);

/* ===== Construction/Destruction ===== */

static void rc_client_dummy_event_handler(const rc_client_event_t* event, rc_client_t* client)
{
}

rc_client_t* rc_client_create(rc_client_read_memory_func_t read_memory_function, rc_client_server_call_t server_call_function)
{
  rc_client_t* client = (rc_client_t*)calloc(1, sizeof(rc_client_t));
  if (!client)
    return NULL;

  client->state.hardcore = 1;

  client->callbacks.read_memory = read_memory_function;
  client->callbacks.server_call = server_call_function;
  client->callbacks.event_handler = rc_client_dummy_event_handler;
  rc_client_set_legacy_peek(client, RC_CLIENT_LEGACY_PEEK_AUTO);

  rc_mutex_init(&client->state.mutex);

  rc_buf_init(&client->state.buffer);

  return client;
}

void rc_client_destroy(rc_client_t* client)
{
  if (!client)
    return;

  rc_client_unload_game(client);

  rc_buf_destroy(&client->state.buffer);

  rc_mutex_destroy(&client->state.mutex);

  free(client);
}

/* ===== Logging ===== */

static rc_client_t* g_hash_client = NULL;

static void rc_client_log_hash_message(const char* message) {
  rc_client_log_message(g_hash_client, message);
}

void rc_client_log_message(const rc_client_t* client, const char* message)
{
  if (client->callbacks.log_call)
    client->callbacks.log_call(message, client);
}

static void rc_client_log_message_va(const rc_client_t* client, const char* format, va_list args)
{
  if (client->callbacks.log_call) {
    char buffer[256];

#ifdef __STDC_WANT_SECURE_LIB__
    vsprintf_s(buffer, sizeof(buffer), format, args);
#else
    vsprintf(buffer, format, args);
#endif

    client->callbacks.log_call(buffer, client);
  }
}

#ifdef RC_NO_VARIADIC_MACROS

void RC_CLIENT_LOG_ERR_FORMATTED(const rc_client_t* client, const char* format, ...)
{
  if (client->state.log_level >= RC_CLIENT_LOG_LEVEL_ERROR) {
    va_list args;
    va_start(args, format);
    rc_client_log_message_va(client, format, args);
    va_end(args);
  }
}

void RC_CLIENT_LOG_WARN_FORMATTED(const rc_client_t* client, const char* format, ...)
{
  if (client->state.log_level >= RC_CLIENT_LOG_LEVEL_WARN) {
    va_list args;
    va_start(args, format);
    rc_client_log_message_va(client, format, args);
    va_end(args);
  }
}

void RC_CLIENT_LOG_INFO_FORMATTED(const rc_client_t* client, const char* format, ...)
{
  if (client->state.log_level >= RC_CLIENT_LOG_LEVEL_INFO) {
    va_list args;
    va_start(args, format);
    rc_client_log_message_va(client, format, args);
    va_end(args);
  }
}

void RC_CLIENT_LOG_VERBOSE_FORMATTED(const rc_client_t* client, const char* format, ...)
{
  if (client->state.log_level >= RC_CLIENT_LOG_LEVEL_VERBOSE) {
    va_list args;
    va_start(args, format);
    rc_client_log_message_va(client, format, args);
    va_end(args);
  }
}

#else

void rc_client_log_message_formatted(const rc_client_t* client, const char* format, ...)
{
  va_list args;
  va_start(args, format);
  rc_client_log_message_va(client, format, args);
  va_end(args);
}

#endif /* RC_NO_VARIADIC_MACROS */

void rc_client_enable_logging(rc_client_t* client, int level, rc_client_message_callback_t callback)
{
  client->callbacks.log_call = callback;
  client->state.log_level = callback ? level : RC_CLIENT_LOG_LEVEL_NONE;
}

/* ===== Common ===== */

static int rc_client_async_handle_aborted(rc_client_t* client, rc_client_async_handle_t* async_handle)
{
  int aborted;

  rc_mutex_lock(&client->state.mutex);
  aborted = async_handle->aborted;
  rc_mutex_unlock(&client->state.mutex);

  return aborted;
}

void rc_client_abort_async(rc_client_t* client, rc_client_async_handle_t* async_handle)
{
  if (async_handle && client) {
    rc_mutex_lock(&client->state.mutex);
    async_handle->aborted = 1;
    rc_mutex_unlock(&client->state.mutex);
  }
}

static const char* rc_client_server_error_message(int* result, int http_status_code, const rc_api_response_t* response)
{
  if (!response->succeeded) {
    if (*result == RC_OK) {
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

static void rc_client_raise_server_error_event(rc_client_t* client, const char* api, const char* error_message)
{
  rc_client_server_error_t server_error;
  rc_client_event_t client_event;

  server_error.api = api;
  server_error.error_message = error_message;

  memset(&client_event, 0, sizeof(client_event));
  client_event.type = RC_CLIENT_EVENT_SERVER_ERROR;
  client_event.server_error = &server_error;

  client->callbacks.event_handler(&client_event, client);
}

static int rc_client_get_image_url(char buffer[], size_t buffer_size, int image_type, const char* image_name)
{
  rc_api_fetch_image_request_t image_request;
  rc_api_request_t request;
  int result;

  if (!buffer)
    return RC_INVALID_STATE;

  memset(&image_request, 0, sizeof(image_request));
  image_request.image_type = image_type;
  image_request.image_name = image_name;
  result = rc_api_init_fetch_image_request(&request, &image_request);
  if (result == RC_OK)
    snprintf(buffer, buffer_size, "%s", request.url);

  rc_api_destroy_request(&request);
  return result;
}

/* ===== User ===== */

static void rc_client_login_callback(const rc_api_server_response_t* server_response, void* callback_data)
{
  rc_client_generic_callback_data_t* login_callback_data = (rc_client_generic_callback_data_t*)callback_data;
  rc_client_t* client = login_callback_data->client;
  rc_api_login_response_t login_response;
  rc_client_load_state_t* load_state;
  const char* error_message;
  int result;

  if (rc_client_async_handle_aborted(client, &login_callback_data->async_handle)) {
    RC_CLIENT_LOG_VERBOSE(client, "Login aborted");
    free(login_callback_data);
    return;
  }

  if (client->state.user == RC_CLIENT_USER_STATE_NONE) {
    /* logout was called */
    if (login_callback_data->callback)
      login_callback_data->callback(RC_ABORTED, "Login aborted", client, login_callback_data->callback_userdata);

    free(login_callback_data);
    return;
  }

  result = rc_api_process_login_response(&login_response, server_response->body);
  error_message = rc_client_server_error_message(&result, server_response->http_status_code, &login_response.response);
  if (error_message) {
    rc_mutex_lock(&client->state.mutex);
    client->state.user = RC_CLIENT_USER_STATE_NONE;
    load_state = client->state.load;
    rc_mutex_unlock(&client->state.mutex);

    RC_CLIENT_LOG_ERR_FORMATTED(client, "Login failed: %s", error_message);
    if (login_callback_data->callback)
      login_callback_data->callback(result, error_message, client, login_callback_data->callback_userdata);

    if (load_state && load_state->progress == RC_CLIENT_LOAD_STATE_AWAIT_LOGIN)
      rc_client_begin_fetch_game_data(load_state);
  }
  else {
    client->user.username = rc_buf_strcpy(&client->state.buffer, login_response.username);

    if (strcmp(login_response.username, login_response.display_name) == 0)
      client->user.display_name = client->user.username;
    else
      client->user.display_name = rc_buf_strcpy(&client->state.buffer, login_response.display_name);

    client->user.token = rc_buf_strcpy(&client->state.buffer, login_response.api_token);
    client->user.score = login_response.score;
    client->user.score_softcore = login_response.score_softcore;
    client->user.num_unread_messages = login_response.num_unread_messages;

    rc_mutex_lock(&client->state.mutex);
    client->state.user = RC_CLIENT_USER_STATE_LOGGED_IN;
    load_state = client->state.load;
    rc_mutex_unlock(&client->state.mutex);

    RC_CLIENT_LOG_INFO_FORMATTED(client, "%s logged in successfully", login_response.display_name);

    if (load_state && load_state->progress == RC_CLIENT_LOAD_STATE_AWAIT_LOGIN)
      rc_client_begin_fetch_game_data(load_state);

    if (login_callback_data->callback)
      login_callback_data->callback(RC_OK, NULL, client, login_callback_data->callback_userdata);
  }

  rc_api_destroy_login_response(&login_response);
  free(login_callback_data);
}

static rc_client_async_handle_t* rc_client_begin_login(rc_client_t* client,
  const rc_api_login_request_t* login_request, rc_client_callback_t callback, void* callback_userdata)
{
  rc_client_generic_callback_data_t* callback_data;
  rc_api_request_t request;
  int result = rc_api_init_login_request(&request, login_request);
  const char* error_message = rc_error_str(result);

  if (result == RC_OK) {
    rc_mutex_lock(&client->state.mutex);

    if (client->state.user == RC_CLIENT_USER_STATE_LOGIN_REQUESTED) {
      error_message = "Login already in progress";
      result = RC_INVALID_STATE;
    }
    client->state.user = RC_CLIENT_USER_STATE_LOGIN_REQUESTED;

    rc_mutex_unlock(&client->state.mutex);
  }

  if (result != RC_OK) {
    callback(result, error_message, client, callback_userdata);
    return NULL;
  }

  callback_data = (rc_client_generic_callback_data_t*)calloc(1, sizeof(*callback_data));
  if (!callback_data) {
    callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), client, callback_userdata);
    return NULL;
  }

  callback_data->client = client;
  callback_data->callback = callback;
  callback_data->callback_userdata = callback_userdata;

  client->callbacks.server_call(&request, rc_client_login_callback, callback_data, client);
  rc_api_destroy_request(&request);

  return &callback_data->async_handle;
}

rc_client_async_handle_t* rc_client_begin_login_with_password(rc_client_t* client,
  const char* username, const char* password, rc_client_callback_t callback, void* callback_userdata)
{
  rc_api_login_request_t login_request;

  if (!client) {
    callback(RC_INVALID_STATE, "client is required", client, callback_userdata);
    return NULL;
  }

  if (!username || !username[0]) {
    callback(RC_INVALID_STATE, "username is required", client, callback_userdata);
    return NULL;
  }

  if (!password || !password[0]) {
    callback(RC_INVALID_STATE, "password is required", client, callback_userdata);
    return NULL;
  }

  memset(&login_request, 0, sizeof(login_request));
  login_request.username = username;
  login_request.password = password;

  RC_CLIENT_LOG_INFO_FORMATTED(client, "Attempting to log in %s (with password)", username);
  return rc_client_begin_login(client, &login_request, callback, callback_userdata);
}

rc_client_async_handle_t* rc_client_begin_login_with_token(rc_client_t* client,
  const char* username, const char* token, rc_client_callback_t callback, void* callback_userdata)
{
  rc_api_login_request_t login_request;

  if (!client) {
    callback(RC_INVALID_STATE, "client is required", client, callback_userdata);
    return NULL;
  }

  if (!username || !username[0]) {
    callback(RC_INVALID_STATE, "username is required", client, callback_userdata);
    return NULL;
  }

  if (!token || !token[0]) {
    callback(RC_INVALID_STATE, "token is required", client, callback_userdata);
    return NULL;
  }

  memset(&login_request, 0, sizeof(login_request));
  login_request.username = username;
  login_request.api_token = token;

  RC_CLIENT_LOG_INFO_FORMATTED(client, "Attempting to log in %s (with token)", username);
  return rc_client_begin_login(client, &login_request, callback, callback_userdata);
}

void rc_client_logout(rc_client_t* client)
{
  rc_client_load_state_t* load_state;

  if (!client)
    return;

  switch (client->state.user) {
    case RC_CLIENT_USER_STATE_LOGGED_IN:
      RC_CLIENT_LOG_INFO_FORMATTED(client, "Logging %s out", client->user.display_name);
      break;

    case RC_CLIENT_USER_STATE_LOGIN_REQUESTED:
      RC_CLIENT_LOG_INFO(client, "Aborting login");
      break;
  }

  rc_mutex_lock(&client->state.mutex);

  client->state.user = RC_CLIENT_USER_STATE_NONE;
  memset(&client->user, 0, sizeof(client->user));

  load_state = client->state.load;

  rc_mutex_unlock(&client->state.mutex);

  rc_client_unload_game(client);

  if (load_state && load_state->progress == RC_CLIENT_LOAD_STATE_AWAIT_LOGIN)
    rc_client_load_error(load_state, RC_ABORTED, "Login aborted");
}

const rc_client_user_t* rc_client_get_user_info(const rc_client_t* client)
{
  return (client && client->state.user == RC_CLIENT_USER_STATE_LOGGED_IN) ? &client->user : NULL;
}

int rc_client_user_get_image_url(const rc_client_user_t* user, char buffer[], size_t buffer_size)
{
  if (!user)
    return RC_INVALID_STATE;

  return rc_client_get_image_url(buffer, buffer_size, RC_IMAGE_TYPE_USER, user->display_name);
}

static void rc_client_subset_get_user_game_summary(const rc_client_subset_info_t* subset,
    rc_client_user_game_summary_t* summary, const uint8_t unlock_bit)
{
  rc_client_achievement_info_t* achievement = subset->achievements;
  rc_client_achievement_info_t* stop = achievement + subset->public.num_achievements;
  for (; achievement < stop; ++achievement) {
    switch (achievement->public.category) {
      case RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE:
        ++summary->num_core_achievements;
        summary->points_core += achievement->public.points;

        if (achievement->public.unlocked & unlock_bit) {
          ++summary->num_unlocked_achievements;
          summary->points_unlocked += achievement->public.points;
        }
        else if (achievement->public.bucket == RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED) {
          ++summary->num_unsupported_achievements;
        }

        break;

      case RC_CLIENT_ACHIEVEMENT_CATEGORY_UNOFFICIAL:
        ++summary->num_unofficial_achievements;
        break;

      default:
        continue;
    }
  }
}

void rc_client_get_user_game_summary(const rc_client_t* client, rc_client_user_game_summary_t* summary)
{
  const uint8_t unlock_bit = (client->state.hardcore) ?
    RC_CLIENT_ACHIEVEMENT_UNLOCKED_HARDCORE : RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE;

  if (!summary)
    return;

  memset(summary, 0, sizeof(*summary));
  if (!client || !client->game)
    return;

  rc_mutex_lock((rc_mutex_t*)&client->state.mutex); /* remove const cast for mutex access */

  rc_client_subset_get_user_game_summary(client->game->subsets, summary, unlock_bit);

  rc_mutex_unlock((rc_mutex_t*)&client->state.mutex); /* remove const cast for mutex access */
}

/* ===== Game ===== */

static void rc_client_free_game(rc_client_game_info_t* game)
{
  rc_runtime_destroy(&game->runtime);

  rc_buf_destroy(&game->buffer);

  free(game);
}

static void rc_client_free_load_state(rc_client_load_state_t* load_state)
{
  if (load_state->game)
    rc_client_free_game(load_state->game);

  if (load_state->hardcore_unlocks)
    free(load_state->hardcore_unlocks);
  if (load_state->softcore_unlocks)
    free(load_state->softcore_unlocks);

  free(load_state);
}

static void rc_client_begin_load_state(rc_client_load_state_t* load_state, uint8_t state, uint8_t num_requests)
{
  rc_mutex_lock(&load_state->client->state.mutex);

  load_state->progress = state;
  load_state->outstanding_requests += num_requests;

  rc_mutex_unlock(&load_state->client->state.mutex);
}

static int rc_client_end_load_state(rc_client_load_state_t* load_state)
{
  int remaining_requests = 0;
  int aborted = 0;

  rc_mutex_lock(&load_state->client->state.mutex);

  if (load_state->outstanding_requests > 0)
    --load_state->outstanding_requests;
  remaining_requests = load_state->outstanding_requests;

  if (load_state->client->state.load != load_state)
    aborted = 1;

  rc_mutex_unlock(&load_state->client->state.mutex);

  if (aborted) {
    /* we can't actually free the load_state itself if there are any outstanding requests
     * or their callbacks will try to use the free'd memory. As they call end_load_state,
     * the outstanding_requests count will reach zero and the memory will be free'd then. */
    if (remaining_requests == 0) {
      /* if one of the callbacks called rc_client_load_error, progress will be set to
       * RC_CLIENT_LOAD_STATE_UNKNOWN. There's no need to call the callback with RC_ABORTED
       * in that case, as it will have already been called with something more appropriate. */
      if (load_state->progress != RC_CLIENT_LOAD_STATE_UNKNOWN_GAME && load_state->callback)
        load_state->callback(RC_ABORTED, "The requested game is no longer active", load_state->client, load_state->callback_userdata);

      rc_client_free_load_state(load_state);
    }

    return -1;
  }

  return remaining_requests;
}

static void rc_client_load_error(rc_client_load_state_t* load_state, int result, const char* error_message)
{
  int remaining_requests = 0;

  rc_mutex_lock(&load_state->client->state.mutex);

  load_state->progress = RC_CLIENT_LOAD_STATE_UNKNOWN_GAME;
  if (load_state->client->state.load == load_state)
    load_state->client->state.load = NULL;

  remaining_requests = load_state->outstanding_requests;

  rc_mutex_unlock(&load_state->client->state.mutex);

  if (load_state->callback)
    load_state->callback(result, error_message, load_state->client, load_state->callback_userdata);

  /* we can't actually free the load_state itself if there are any outstanding requests
   * or their callbacks will try to use the free'd memory. as they call end_load_state,
   * the outstanding_requests count will reach zero and the memory will be free'd then. */
  if (remaining_requests == 0)
    rc_client_free_load_state(load_state);
}

static void rc_client_load_aborted(rc_client_load_state_t* load_state)
{
  /* prevent callback from being called when manually aborted */
  load_state->callback = NULL;

  /* mark the game as no longer being loaded */
  rc_client_load_error(load_state, RC_ABORTED, NULL);

  /* decrement the async counter and potentially free the load_state object */
  rc_client_end_load_state(load_state);
}

static void rc_client_invalidate_memref_achievements(rc_client_game_info_t* game, rc_client_t* client, rc_memref_t* memref)
{
  rc_client_subset_info_t* subset = game->subsets;
  for (; subset; subset = subset->next) {
    rc_client_achievement_info_t* achievement = subset->achievements;
    rc_client_achievement_info_t* stop = achievement + subset->public.num_achievements;
    for (; achievement < stop; ++achievement) {
      if (achievement->public.state == RC_CLIENT_ACHIEVEMENT_STATE_DISABLED)
        continue;

      if (rc_trigger_contains_memref(achievement->trigger, memref)) {
        achievement->public.state = RC_CLIENT_ACHIEVEMENT_STATE_DISABLED;
        achievement->public.bucket = RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED;
        RC_CLIENT_LOG_WARN_FORMATTED(client, "Disabled achievement %u. Invalid address %06X", achievement->public.id, memref->address);
      }
    }
  }
}

static void rc_client_invalidate_memref_leaderboards(rc_client_game_info_t* game, rc_client_t* client, rc_memref_t* memref)
{
  rc_client_subset_info_t* subset = game->subsets;
  for (; subset; subset = subset->next) {
    rc_client_leaderboard_info_t* leaderboard = subset->leaderboards;
    rc_client_leaderboard_info_t* stop = leaderboard + subset->public.num_leaderboards;
    for (; leaderboard < stop; ++leaderboard) {
      if (leaderboard->public.state == RC_CLIENT_LEADERBOARD_STATE_DISABLED)
        continue;

      if (rc_trigger_contains_memref(&leaderboard->lboard->start, memref))
        leaderboard->public.state = RC_CLIENT_LEADERBOARD_STATE_DISABLED;
      else if (rc_trigger_contains_memref(&leaderboard->lboard->cancel, memref))
        leaderboard->public.state = RC_CLIENT_LEADERBOARD_STATE_DISABLED;
      else if (rc_trigger_contains_memref(&leaderboard->lboard->submit, memref))
        leaderboard->public.state = RC_CLIENT_LEADERBOARD_STATE_DISABLED;
      else if (rc_value_contains_memref(&leaderboard->lboard->value, memref))
        leaderboard->public.state = RC_CLIENT_LEADERBOARD_STATE_DISABLED;
      else
        continue;

      RC_CLIENT_LOG_WARN_FORMATTED(client, "Disabled leaderboard %u. Invalid address %06X", leaderboard->public.id, memref->address);
    }
  }
}

static void rc_client_validate_addresses(rc_client_game_info_t* game, rc_client_t* client)
{
  const rc_memory_regions_t* regions = rc_console_memory_regions(game->public.console_id);
  const uint32_t max_address = (regions && regions->num_regions > 0) ?
      regions->region[regions->num_regions - 1].end_address : 0xFFFFFFFF;
  uint8_t buffer[8];
  uint32_t total_count = 0;
  uint32_t invalid_count = 0;

  rc_memref_t** last_memref = &game->runtime.memrefs;
  rc_memref_t* memref = game->runtime.memrefs;
  for (; memref; memref = memref->next) {
    if (!memref->value.is_indirect) {
      total_count++;

      if (memref->address > max_address ||
        client->callbacks.read_memory(memref->address, buffer, 1, client) == 0) {
        /* invalid address, remove from chain so we don't have to evaluate it in the future.
         * it's still there, so anything referencing it will always fetch 0. */
        *last_memref = memref->next;

        rc_client_invalidate_memref_achievements(game, client, memref);
        rc_client_invalidate_memref_leaderboards(game, client, memref);

        invalid_count++;
        continue;
      }
    }

    last_memref = &memref->next;
  }

  game->max_valid_address = max_address;
  RC_CLIENT_LOG_VERBOSE_FORMATTED(client, "%u/%u memory addresses valid", total_count - invalid_count, total_count);
}

static void rc_client_update_legacy_runtime_achievements(rc_client_game_info_t* game, uint32_t active_count)
{
  if (active_count > 0) {
    rc_client_achievement_info_t* achievement;
    rc_client_achievement_info_t* stop;
    rc_runtime_trigger_t* trigger;

    rc_client_subset_info_t* subset = game->subsets;
    for (; subset; subset = subset->next) {
      if (!subset->active)
        continue;

      achievement = subset->achievements;
      stop = achievement + subset->public.num_achievements;

      if (active_count <= game->runtime.trigger_capacity) {
        if (active_count != 0)
          memset(game->runtime.triggers, 0, active_count * sizeof(rc_runtime_trigger_t));
      }
      else {
        if (game->runtime.triggers)
          free(game->runtime.triggers);

        game->runtime.trigger_capacity = active_count;
        game->runtime.triggers = (rc_runtime_trigger_t*)calloc(1, active_count * sizeof(rc_runtime_trigger_t));
        if (!game->runtime.triggers) {
          /* Unexpected, no callback available, just fail */
          break;
        }
      }

      trigger = game->runtime.triggers;
      achievement = subset->achievements;
      for (; achievement < stop; ++achievement) {
        if (achievement->public.state == RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE) {
          trigger->id = achievement->public.id;
          memcpy(trigger->md5, achievement->md5, 16);
          trigger->trigger = achievement->trigger;
          ++trigger;
        }
      }
    }
  }

  game->runtime.trigger_count = active_count;
}

static uint32_t rc_client_subset_count_active_achievements(const rc_client_subset_info_t* subset)
{
  rc_client_achievement_info_t* achievement = subset->achievements;
  rc_client_achievement_info_t* stop = achievement + subset->public.num_achievements;
  uint32_t active_count = 0;

  for (; achievement < stop; ++achievement) {
    if (achievement->public.state == RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE)
      ++active_count;
  }

  return active_count;
}

static void rc_client_update_active_achievements(rc_client_game_info_t* game)
{
  uint32_t active_count = 0;
  rc_client_subset_info_t* subset = game->subsets;
  for (; subset; subset = subset->next) {
    if (subset->active)
      active_count += rc_client_subset_count_active_achievements(subset);
  }

  rc_client_update_legacy_runtime_achievements(game, active_count);
}

static uint32_t rc_client_subset_toggle_hardcore_achievements(rc_client_subset_info_t* subset, rc_client_t* client, uint8_t active_bit)
{
  rc_client_achievement_info_t* achievement = subset->achievements;
  rc_client_achievement_info_t* stop = achievement + subset->public.num_achievements;
  uint32_t active_count = 0;

  for (; achievement < stop; ++achievement) {
    if ((achievement->public.unlocked & active_bit) == 0) {
      switch (achievement->public.state) {
        case RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED:
        case RC_CLIENT_ACHIEVEMENT_STATE_INACTIVE:
          rc_reset_trigger(achievement->trigger);
          achievement->public.state = RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE;
          ++active_count;
          break;

        case RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE:
          ++active_count;
          break;
      }
    }
    else if (achievement->public.state == RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE ||
             achievement->public.state == RC_CLIENT_ACHIEVEMENT_STATE_INACTIVE) {

      /* if it's active despite being unlocked, and we're in encore mode, leave it active */
      if (client->state.encore_mode) {
        ++active_count;
        continue;
      }

      achievement->public.state = RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED;
      achievement->public.unlock_time = (active_bit == RC_CLIENT_ACHIEVEMENT_UNLOCKED_HARDCORE) ?
        achievement->unlock_time_hardcore : achievement->unlock_time_softcore;

      if (achievement->trigger && achievement->trigger->state == RC_TRIGGER_STATE_PRIMED) {
        rc_client_event_t client_event;
        memset(&client_event, 0, sizeof(client_event));
        client_event.type = RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE;
        client_event.achievement = &achievement->public;
        client->callbacks.event_handler(&client_event, client);
      }
    }
  }

  return active_count;
}

static void rc_client_toggle_hardcore_achievements(rc_client_game_info_t* game, rc_client_t* client, uint8_t active_bit)
{
  uint32_t active_count = 0;
  rc_client_subset_info_t* subset = game->subsets;
  for (; subset; subset = subset->next) {
    if (subset->active)
      active_count += rc_client_subset_toggle_hardcore_achievements(subset, client, active_bit);
  }

  rc_client_update_legacy_runtime_achievements(game, active_count);
}

static void rc_client_activate_achievements(rc_client_game_info_t* game, rc_client_t* client)
{
  const uint8_t active_bit = (client->state.encore_mode) ?
      RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE : (client->state.hardcore) ?
      RC_CLIENT_ACHIEVEMENT_UNLOCKED_HARDCORE : RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE;

  rc_client_toggle_hardcore_achievements(game, client, active_bit);
}

static void rc_client_activate_leaderboards(rc_client_game_info_t* game, rc_client_t* client)
{
  rc_client_leaderboard_info_t* leaderboard;
  rc_client_leaderboard_info_t* stop;

  unsigned active_count = 0;
  rc_client_subset_info_t* subset = game->subsets;
  for (; subset; subset = subset->next) {
    if (!subset->active)
      continue;

    leaderboard = subset->leaderboards;
    stop = leaderboard + subset->public.num_leaderboards;

    for (; leaderboard < stop; ++leaderboard) {
      switch (leaderboard->public.state) {
        case RC_CLIENT_LEADERBOARD_STATE_DISABLED:
          continue;

        case RC_CLIENT_LEADERBOARD_STATE_INACTIVE:
          if (client->state.hardcore) {
            rc_reset_lboard(leaderboard->lboard);
            leaderboard->public.state = RC_CLIENT_LEADERBOARD_STATE_ACTIVE;
            ++active_count;
          }
          break;

        default:
          if (client->state.hardcore)
            ++active_count;
          else
            leaderboard->public.state = RC_CLIENT_LEADERBOARD_STATE_INACTIVE;
          break;
      }
    }
  }

  if (active_count > 0) {
    rc_runtime_lboard_t* lboard;

    if (active_count <= game->runtime.lboard_capacity) {
      if (active_count != 0)
        memset(game->runtime.lboards, 0, active_count * sizeof(rc_runtime_lboard_t));
    }
    else {
      if (game->runtime.lboards)
        free(game->runtime.lboards);

      game->runtime.lboard_capacity = active_count;
      game->runtime.lboards = (rc_runtime_lboard_t*)calloc(1, active_count * sizeof(rc_runtime_lboard_t));
    }

    lboard = game->runtime.lboards;

    subset = game->subsets;
    for (; subset; subset = subset->next) {
      if (!subset->active)
        continue;

      leaderboard = subset->leaderboards;
      stop = leaderboard + subset->public.num_leaderboards;
      for (; leaderboard < stop; ++leaderboard) {
        if (leaderboard->public.state == RC_CLIENT_LEADERBOARD_STATE_ACTIVE ||
            leaderboard->public.state == RC_CLIENT_LEADERBOARD_STATE_TRACKING) {
          lboard->id = leaderboard->public.id;
          memcpy(lboard->md5, leaderboard->md5, 16);
          lboard->lboard = leaderboard->lboard;
          ++lboard;
        }
      }
    }
  }

  game->runtime.lboard_count = active_count;
}

static void rc_client_deactivate_leaderboards(rc_client_game_info_t* game, rc_client_t* client)
{
  rc_client_leaderboard_info_t* leaderboard;
  rc_client_leaderboard_info_t* stop;

  rc_client_subset_info_t* subset = game->subsets;
  for (; subset; subset = subset->next) {
    if (!subset->active)
      continue;

    leaderboard = subset->leaderboards;
    stop = leaderboard + subset->public.num_leaderboards;

    for (; leaderboard < stop; ++leaderboard) {
      switch (leaderboard->public.state) {
        case RC_CLIENT_LEADERBOARD_STATE_DISABLED:
        case RC_CLIENT_LEADERBOARD_STATE_INACTIVE:
          continue;

        case RC_CLIENT_LEADERBOARD_STATE_TRACKING:
          rc_client_release_leaderboard_tracker(client->game, leaderboard);
          /* fallthrough to default */
        default:
          leaderboard->public.state = RC_CLIENT_LEADERBOARD_STATE_INACTIVE;
          break;
      }
    }
  }

  game->runtime.lboard_count = 0;
}

static void rc_client_apply_unlocks(rc_client_subset_info_t* subset, uint32_t* unlocks, uint32_t num_unlocks, uint8_t mode)
{
  rc_client_achievement_info_t* start = subset->achievements;
  rc_client_achievement_info_t* stop = start + subset->public.num_achievements;
  rc_client_achievement_info_t* scan;
  unsigned i;

  for (i = 0; i < num_unlocks; ++i) {
    uint32_t id = unlocks[i];
    for (scan = start; scan < stop; ++scan) {
      if (scan->public.id == id) {
        scan->public.unlocked |= mode;

        if (scan == start)
          ++start;
        else if (scan + 1 == stop)
          --stop;
        break;
      }
    }
  }
}

static void rc_client_activate_game(rc_client_load_state_t* load_state)
{
  rc_client_t* client = load_state->client;

  rc_mutex_lock(&client->state.mutex);
  load_state->progress = (client->state.load == load_state) ?
      RC_CLIENT_LOAD_STATE_DONE : RC_CLIENT_LOAD_STATE_UNKNOWN_GAME;
  client->state.load = NULL;
  rc_mutex_unlock(&client->state.mutex);

  if (load_state->progress != RC_CLIENT_LOAD_STATE_DONE) {
    /* previous load state was aborted */
    if (load_state->callback)
      load_state->callback(RC_ABORTED, "The requested game is no longer active", client, load_state->callback_userdata);
  }
  else if ((!load_state->softcore_unlocks || !load_state->hardcore_unlocks) &&
            client->state.spectator_mode == RC_CLIENT_SPECTATOR_MODE_OFF) {
    /* unlocks not available - assume malloc failed */
    if (load_state->callback)
      load_state->callback(RC_INVALID_STATE, "Unlock arrays were not allocated", client, load_state->callback_userdata);
  }
  else {
    if (client->state.spectator_mode == RC_CLIENT_SPECTATOR_MODE_OFF) {
      rc_client_apply_unlocks(load_state->subset, load_state->softcore_unlocks,
          load_state->num_softcore_unlocks, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);
      rc_client_apply_unlocks(load_state->subset, load_state->hardcore_unlocks,
          load_state->num_hardcore_unlocks, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    }

    rc_mutex_lock(&client->state.mutex);
    if (client->state.load == NULL)
      client->game = load_state->game;
    rc_mutex_unlock(&client->state.mutex);

    if (client->game != load_state->game) {
      /* previous load state was aborted */
      if (load_state->callback)
        load_state->callback(RC_ABORTED, "The requested game is no longer active", client, load_state->callback_userdata);
    }
    else {
      /* if a change media request is pending, kick it off */
      rc_client_pending_media_t* pending_media;

      rc_mutex_lock(&load_state->client->state.mutex);
      pending_media = load_state->pending_media;
      load_state->pending_media = NULL;
      rc_mutex_unlock(&load_state->client->state.mutex);

      if (pending_media) {
        rc_client_begin_change_media(client, pending_media->file_path,
          pending_media->data, pending_media->data_size, pending_media->callback, pending_media->callback_userdata);
        if (pending_media->data)
          free(pending_media->data);
        free((void*)pending_media->file_path);
        free(pending_media);
      }

      /* client->game must be set before calling this function so it can query the console_id */
      rc_client_validate_addresses(load_state->game, client);

      rc_client_activate_achievements(load_state->game, client);
      rc_client_activate_leaderboards(load_state->game, client);

      if (load_state->hash->hash[0] != '[') {
        if (load_state->client->state.spectator_mode != RC_CLIENT_SPECTATOR_MODE_LOCKED) {
          /* schedule the periodic ping */
          rc_client_scheduled_callback_data_t* callback_data = rc_buf_alloc(&load_state->game->buffer, sizeof(rc_client_scheduled_callback_data_t));
          memset(callback_data, 0, sizeof(*callback_data));
          callback_data->callback = rc_client_ping;
          callback_data->related_id = load_state->game->public.id;
          callback_data->when = time(NULL) + 30;
          rc_client_schedule_callback(client, callback_data);
        }

        RC_CLIENT_LOG_INFO_FORMATTED(client, "Game %u loaded, hardcode %s%s", load_state->game->public.id,
            client->state.hardcore ? "enabled" : "disabled",
            (client->state.spectator_mode != RC_CLIENT_SPECTATOR_MODE_OFF) ? ", spectating" : "");
      }
      else {
        RC_CLIENT_LOG_INFO_FORMATTED(client, "Subset %u loaded", load_state->subset->public.id);
      }

      if (load_state->callback)
        load_state->callback(RC_OK, NULL, client, load_state->callback_userdata);

      /* detach the game object so it doesn't get freed by free_load_state */
      load_state->game = NULL;
    }
  }

  rc_client_free_load_state(load_state);
}

static void rc_client_start_session_callback(const rc_api_server_response_t* server_response, void* callback_data)
{
  rc_client_load_state_t* load_state = (rc_client_load_state_t*)callback_data;
  rc_api_start_session_response_t start_session_response;
  int outstanding_requests;
  const char* error_message;
  int result;

  if (rc_client_async_handle_aborted(load_state->client, &load_state->async_handle)) {
    rc_client_t* client = load_state->client;
    rc_client_load_aborted(load_state);
    RC_CLIENT_LOG_VERBOSE(client, "Load aborted while starting session");
    return;
  }

  result = rc_api_process_start_session_response(&start_session_response, server_response->body);
  error_message = rc_client_server_error_message(&result, server_response->http_status_code, &start_session_response.response);
  outstanding_requests = rc_client_end_load_state(load_state);

  if (error_message) {
    rc_client_load_error(callback_data, result, error_message);
  }
  else if (outstanding_requests < 0) {
    /* previous load state was aborted, load_state was free'd */
  }
  else {
    if (outstanding_requests == 0)
      rc_client_activate_game(load_state);
  }

  rc_api_destroy_start_session_response(&start_session_response);
}

static void rc_client_unlocks_callback(const rc_api_server_response_t* server_response, void* callback_data, int mode)
{
  rc_client_load_state_t* load_state = (rc_client_load_state_t*)callback_data;
  rc_api_fetch_user_unlocks_response_t fetch_user_unlocks_response;
  int outstanding_requests;
  const char* error_message;
  int result;

  if (rc_client_async_handle_aborted(load_state->client, &load_state->async_handle)) {
    rc_client_t* client = load_state->client;
    rc_client_load_aborted(load_state);
    RC_CLIENT_LOG_VERBOSE(client, "Load aborted while fetching unlocks");
    return;
  }

  result = rc_api_process_fetch_user_unlocks_response(&fetch_user_unlocks_response, server_response->body);
  error_message = rc_client_server_error_message(&result, server_response->http_status_code, &fetch_user_unlocks_response.response);
  outstanding_requests = rc_client_end_load_state(load_state);

  if (error_message) {
    rc_client_load_error(callback_data, result, error_message);
  }
  else if (outstanding_requests < 0) {
    /* previous load state was aborted, load_state was free'd */
  }
  else {
    if (mode == RC_CLIENT_ACHIEVEMENT_UNLOCKED_HARDCORE) {
      const size_t array_size = fetch_user_unlocks_response.num_achievement_ids * sizeof(uint32_t);
      load_state->num_hardcore_unlocks = fetch_user_unlocks_response.num_achievement_ids;
      load_state->hardcore_unlocks = (uint32_t*)malloc(array_size);
      if (load_state->hardcore_unlocks)
        memcpy(load_state->hardcore_unlocks, fetch_user_unlocks_response.achievement_ids, array_size);
    }
    else {
      const size_t array_size = fetch_user_unlocks_response.num_achievement_ids * sizeof(uint32_t);
      load_state->num_softcore_unlocks = fetch_user_unlocks_response.num_achievement_ids;
      load_state->softcore_unlocks = (uint32_t*)malloc(array_size);
      if (load_state->softcore_unlocks)
        memcpy(load_state->softcore_unlocks, fetch_user_unlocks_response.achievement_ids, array_size);
    }

    if (outstanding_requests == 0)
      rc_client_activate_game(load_state);
  }

  rc_api_destroy_fetch_user_unlocks_response(&fetch_user_unlocks_response);
}

static void rc_client_hardcore_unlocks_callback(const rc_api_server_response_t* server_response, void* callback_data)
{
  rc_client_unlocks_callback(server_response, callback_data, RC_CLIENT_ACHIEVEMENT_UNLOCKED_HARDCORE);
}

static void rc_client_softcore_unlocks_callback(const rc_api_server_response_t* server_response, void* callback_data)
{
  rc_client_unlocks_callback(server_response, callback_data, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);
}

static void rc_client_begin_start_session(rc_client_load_state_t* load_state)
{
  rc_api_start_session_request_t start_session_params;
  rc_api_fetch_user_unlocks_request_t unlock_params;
  rc_client_t* client = load_state->client;
  rc_api_request_t start_session_request;
  rc_api_request_t hardcore_unlock_request;
  rc_api_request_t softcore_unlock_request;
  int result;

  memset(&start_session_params, 0, sizeof(start_session_params));
  start_session_params.username = client->user.username;
  start_session_params.api_token = client->user.token;
  start_session_params.game_id = load_state->hash->game_id;

  result = rc_api_init_start_session_request(&start_session_request, &start_session_params);
  if (result != RC_OK) {
    rc_client_load_error(load_state, result, rc_error_str(result));
  }
  else {
    memset(&unlock_params, 0, sizeof(unlock_params));
    unlock_params.username = client->user.username;
    unlock_params.api_token = client->user.token;
    unlock_params.game_id = load_state->hash->game_id;
    unlock_params.hardcore = 1;

    result = rc_api_init_fetch_user_unlocks_request(&hardcore_unlock_request, &unlock_params);
    if (result != RC_OK) {
      rc_client_load_error(load_state, result, rc_error_str(result));
    }
    else {
      unlock_params.hardcore = 0;

      result = rc_api_init_fetch_user_unlocks_request(&softcore_unlock_request, &unlock_params);
      if (result != RC_OK) {
        rc_client_load_error(load_state, result, rc_error_str(result));
      }
      else {
        rc_client_begin_load_state(load_state, RC_CLIENT_LOAD_STATE_STARTING_SESSION, 3);

        /* TODO: create single server request to do all three of these */
        RC_CLIENT_LOG_VERBOSE_FORMATTED(client, "Starting session for game %u", start_session_params.game_id);
        client->callbacks.server_call(&start_session_request, rc_client_start_session_callback, load_state, client);
        client->callbacks.server_call(&hardcore_unlock_request, rc_client_hardcore_unlocks_callback, load_state, client);
        client->callbacks.server_call(&softcore_unlock_request, rc_client_softcore_unlocks_callback, load_state, client);

        rc_api_destroy_request(&softcore_unlock_request);
      }

      rc_api_destroy_request(&hardcore_unlock_request);
    }

    rc_api_destroy_request(&start_session_request);
  }
}

static void rc_client_copy_achievements(rc_client_load_state_t* load_state,
    rc_client_subset_info_t* subset,
    const rc_api_achievement_definition_t* achievement_definitions, uint32_t num_achievements)
{
  const rc_api_achievement_definition_t* read;
  const rc_api_achievement_definition_t* stop;
  rc_client_achievement_info_t* achievements;
  rc_client_achievement_info_t* achievement;
  rc_api_buffer_t* buffer;
  rc_parse_state_t parse;
  const char* memaddr;
  size_t size;
  int trigger_size;

  subset->achievements = NULL;
  subset->public.num_achievements = num_achievements;

  if (num_achievements == 0)
    return;

  stop = achievement_definitions + num_achievements;

  /* if not testing unofficial, filter them out */
  if (!load_state->client->state.unofficial_enabled) {
    for (read = achievement_definitions; read < stop; ++read) {
      if (read->category != RC_ACHIEVEMENT_CATEGORY_CORE)
        --num_achievements;
    }

    subset->public.num_achievements = num_achievements;

    if (num_achievements == 0)
      return;
  }

  /* preallocate space for achievements */
  size = 24 /* assume average title length of 24 */
      + 48 /* assume average description length of 48 */
      + sizeof(rc_trigger_t) + sizeof(rc_condset_t) * 2 /* trigger container */
      + sizeof(rc_condition_t) * 8 /* assume average trigger length of 8 conditions */
      + sizeof(rc_client_achievement_info_t);
  rc_buf_reserve(&load_state->game->buffer, size * num_achievements);

  /* allocate the achievement array */
  size = sizeof(rc_client_achievement_info_t) * num_achievements;
  buffer = &load_state->game->buffer;
  achievement = achievements = rc_buf_alloc(buffer, size);
  memset(achievements, 0, size);

  /* copy the achievement data */
  for (read = achievement_definitions; read < stop; ++read) {
    if (read->category != RC_ACHIEVEMENT_CATEGORY_CORE && !load_state->client->state.unofficial_enabled)
      continue;

    achievement->public.title = rc_buf_strcpy(buffer, read->title);
    achievement->public.description = rc_buf_strcpy(buffer, read->description);
    snprintf(achievement->public.badge_name, sizeof(achievement->public.badge_name), "%s", read->badge_name);
    achievement->public.id = read->id;
    achievement->public.points = read->points;
    achievement->public.category = (read->category != RC_ACHIEVEMENT_CATEGORY_CORE) ?
      RC_CLIENT_ACHIEVEMENT_CATEGORY_UNOFFICIAL : RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE;

    memaddr = read->definition;
    rc_runtime_checksum(memaddr, achievement->md5);

    trigger_size = rc_trigger_size(memaddr);
    if (trigger_size < 0) {
      RC_CLIENT_LOG_WARN_FORMATTED(load_state->client, "Parse error %d processing achievement %u", trigger_size, read->id);
      achievement->public.state = RC_CLIENT_ACHIEVEMENT_STATE_DISABLED;
      achievement->public.bucket = RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED;
    }
    else {
      /* populate the item, using the communal memrefs pool */
      rc_init_parse_state(&parse, rc_buf_reserve(buffer, trigger_size), NULL, 0);
      parse.first_memref = &load_state->game->runtime.memrefs;
      parse.variables = &load_state->game->runtime.variables;
      achievement->trigger = RC_ALLOC(rc_trigger_t, &parse);
      rc_parse_trigger_internal(achievement->trigger, &memaddr, &parse);

      if (parse.offset < 0) {
        RC_CLIENT_LOG_WARN_FORMATTED(load_state->client, "Parse error %d processing achievement %u", parse.offset, read->id);
        achievement->public.state = RC_CLIENT_ACHIEVEMENT_STATE_DISABLED;
        achievement->public.bucket = RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED;
      }
      else {
        rc_buf_consume(buffer, parse.buffer, (char*)parse.buffer + parse.offset);
        achievement->trigger->memrefs = NULL; /* memrefs managed by runtime */
      }

      rc_destroy_parse_state(&parse);
    }

    ++achievement;
  }

  subset->achievements = achievements;
}

static void rc_client_copy_leaderboards(rc_client_load_state_t* load_state,
    rc_client_subset_info_t* subset,
    const rc_api_leaderboard_definition_t* leaderboard_definitions, uint32_t num_leaderboards)
{
  const rc_api_leaderboard_definition_t* read;
  const rc_api_leaderboard_definition_t* stop;
  rc_client_leaderboard_info_t* leaderboards;
  rc_client_leaderboard_info_t* leaderboard;
  rc_api_buffer_t* buffer;
  rc_parse_state_t parse;
  const char* memaddr;
  const char* ptr;
  size_t size;
  int lboard_size;

  subset->leaderboards = NULL;
  subset->public.num_leaderboards = num_leaderboards;

  if (num_leaderboards == 0)
    return;

  /* preallocate space for achievements */
  size = 24 /* assume average title length of 24 */
      + 48 /* assume average description length of 48 */
      + sizeof(rc_lboard_t) /* lboard container */
      + (sizeof(rc_trigger_t) + sizeof(rc_condset_t) * 2) * 3 /* start/submit/cancel */
      + (sizeof(rc_value_t) + sizeof(rc_condset_t)) /* value */
      + sizeof(rc_condition_t) * 4 * 4 /* assume average of 4 conditions in each start/submit/cancel/value */
      + sizeof(rc_client_leaderboard_info_t);
  rc_buf_reserve(&load_state->game->buffer, size * num_leaderboards);

  /* allocate the achievement array */
  size = sizeof(rc_client_leaderboard_info_t) * num_leaderboards;
  buffer = &load_state->game->buffer;
  leaderboard = leaderboards = rc_buf_alloc(buffer, size);
  memset(leaderboards, 0, size);

  /* copy the achievement data */
  read = leaderboard_definitions;
  stop = read + num_leaderboards;
  do {
    leaderboard->public.title = rc_buf_strcpy(buffer, read->title);
    leaderboard->public.description = rc_buf_strcpy(buffer, read->description);
    leaderboard->public.id = read->id;
    leaderboard->public.lower_is_better = read->lower_is_better;
    leaderboard->format = (uint8_t)read->format;

    memaddr = read->definition;
    rc_runtime_checksum(memaddr, leaderboard->md5);

    ptr = strstr(memaddr, "VAL:");
    if (ptr != NULL) {
      /* calculate the DJB2 hash of the VAL portion of the string*/
      uint32_t hash = 5381;
      ptr += 4; /* skip 'VAL:' */
      while (*ptr && (ptr[0] != ':' || ptr[1] != ':'))
         hash = (hash << 5) + hash + *ptr++;
      leaderboard->value_djb2 = hash;
    }

    lboard_size = rc_lboard_size(memaddr);
    if (lboard_size < 0) {
      RC_CLIENT_LOG_WARN_FORMATTED(load_state->client, "Parse error %d processing leaderboard %u", lboard_size, read->id);
      leaderboard->public.state = RC_CLIENT_LEADERBOARD_STATE_DISABLED;
    }
    else {
      /* populate the item, using the communal memrefs pool */
      rc_init_parse_state(&parse, rc_buf_reserve(buffer, lboard_size), NULL, 0);
      parse.first_memref = &load_state->game->runtime.memrefs;
      parse.variables = &load_state->game->runtime.variables;
      leaderboard->lboard = RC_ALLOC(rc_lboard_t, &parse);
      rc_parse_lboard_internal(leaderboard->lboard, memaddr, &parse);

      if (parse.offset < 0) {
        RC_CLIENT_LOG_WARN_FORMATTED(load_state->client, "Parse error %d processing leaderboard %u", parse.offset, read->id);
        leaderboard->public.state = RC_CLIENT_LEADERBOARD_STATE_DISABLED;
      }
      else {
        rc_buf_consume(buffer, parse.buffer, (char*)parse.buffer + parse.offset);
        leaderboard->lboard->memrefs = NULL; /* memrefs managed by runtime */
      }

      rc_destroy_parse_state(&parse);
    }

    ++leaderboard;
    ++read;
  } while (read < stop);

  subset->leaderboards = leaderboards;
}

static const char* rc_client_subset_extract_title(rc_client_game_info_t* game, const char* title)
{
  const char* subset_prefix = strstr(title, "[Subset - ");
  if (subset_prefix) {
    const char* start = subset_prefix + 10;
    const char* stop = strstr(start, "]");
    const size_t len = stop - start;
    char* result = (char*)rc_buf_alloc(&game->buffer, len + 1);

    memcpy(result, start, len);
    result[len] = '\0';
    return result;
  }

  return NULL;
}

static void rc_client_fetch_game_data_callback(const rc_api_server_response_t* server_response, void* callback_data)
{
  rc_client_load_state_t* load_state = (rc_client_load_state_t*)callback_data;
  rc_api_fetch_game_data_response_t fetch_game_data_response;
  int outstanding_requests;
  const char* error_message;
  int result;

  if (rc_client_async_handle_aborted(load_state->client, &load_state->async_handle)) {
    rc_client_t* client = load_state->client;
    rc_client_load_aborted(load_state);
    RC_CLIENT_LOG_VERBOSE(client, "Load aborted while fetching game data");
    return;
  }

  result = rc_api_process_fetch_game_data_response(&fetch_game_data_response, server_response->body);
  error_message = rc_client_server_error_message(&result, server_response->http_status_code, &fetch_game_data_response.response);

  outstanding_requests = rc_client_end_load_state(load_state);

  if (error_message) {
    rc_client_load_error(load_state, result, error_message);
  }
  else if (outstanding_requests < 0) {
    /* previous load state was aborted, load_state was free'd */
  }
  else {
    rc_client_subset_info_t* subset;

    subset = (rc_client_subset_info_t*)rc_buf_alloc(&load_state->game->buffer, sizeof(rc_client_subset_info_t));
    memset(subset, 0, sizeof(*subset));
    subset->public.id = fetch_game_data_response.id;
    subset->active = 1;
    snprintf(subset->public.badge_name, sizeof(subset->public.badge_name), "%s", fetch_game_data_response.image_name);
    load_state->subset = subset;

    /* kick off the start session request while we process the game data */
    rc_client_begin_load_state(load_state, RC_CLIENT_LOAD_STATE_STARTING_SESSION, 1);
    if (load_state->client->state.spectator_mode != RC_CLIENT_SPECTATOR_MODE_OFF) {
      /* we can't unlock achievements without a session, lock spectator mode for the game */
      load_state->client->state.spectator_mode = RC_CLIENT_SPECTATOR_MODE_LOCKED;
    }
    else {
      rc_client_begin_start_session(load_state);
    }

    /* process the game data */
    rc_client_copy_achievements(load_state, subset,
        fetch_game_data_response.achievements, fetch_game_data_response.num_achievements);
    rc_client_copy_leaderboards(load_state, subset,
        fetch_game_data_response.leaderboards, fetch_game_data_response.num_leaderboards);

    if (!load_state->game->subsets) {
      /* core set */
      rc_mutex_lock(&load_state->client->state.mutex);
      load_state->game->public.title = rc_buf_strcpy(&load_state->game->buffer, fetch_game_data_response.title);
      load_state->game->subsets = subset;
      load_state->game->public.badge_name = subset->public.badge_name;
      load_state->game->public.console_id = fetch_game_data_response.console_id;
      rc_mutex_unlock(&load_state->client->state.mutex);

      subset->public.title = load_state->game->public.title;

      if (fetch_game_data_response.rich_presence_script && fetch_game_data_response.rich_presence_script[0]) {
        result = rc_runtime_activate_richpresence(&load_state->game->runtime, fetch_game_data_response.rich_presence_script, NULL, 0);
        if (result != RC_OK) {
          RC_CLIENT_LOG_WARN_FORMATTED(load_state->client, "Parse error %d processing rich presence", result);
        }
      }
    }
    else {
      rc_client_subset_info_t* scan;

      /* subset - extract subset title */
      subset->public.title = rc_client_subset_extract_title(load_state->game, fetch_game_data_response.title);
      if (!subset->public.title) {
        const char* core_subset_title = rc_client_subset_extract_title(load_state->game, load_state->game->public.title);
        if (core_subset_title) {
           rc_client_subset_info_t* scan = load_state->game->subsets;
           for (; scan; scan = scan->next) {
              if (scan->public.title == load_state->game->public.title) {
                 scan->public.title = core_subset_title;
                 break;
              }
           }
        }

        subset->public.title = rc_buf_strcpy(&load_state->game->buffer, fetch_game_data_response.title);
      }

      /* append to subset list */
      scan = load_state->game->subsets;
      while (scan->next)
        scan = scan->next;
      scan->next = subset;
    }

    outstanding_requests = rc_client_end_load_state(load_state);
    if (outstanding_requests < 0) {
      /* previous load state was aborted, load_state was free'd */
    }
    else {
      if (outstanding_requests == 0)
        rc_client_activate_game(load_state);
    }
  }

  rc_api_destroy_fetch_game_data_response(&fetch_game_data_response);
}

static void rc_client_begin_fetch_game_data(rc_client_load_state_t* load_state)
{
  rc_api_fetch_game_data_request_t fetch_game_data_request;
  rc_client_t* client = load_state->client;
  rc_api_request_t request;
  int result;

  if (load_state->hash->game_id == 0) {
    char hash[33];

    if (rc_hash_iterate(hash, &load_state->hash_iterator)) {
      /* found another hash to try */
      load_state->hash_console_id = load_state->hash_iterator.consoles[load_state->hash_iterator.index - 1];
      rc_client_load_game(load_state, hash, NULL);
      return;
    }

    if (load_state->game->media_hash && load_state->game->media_hash->next) {
      /* multiple hashes were tried, create a CSV */
      struct rc_client_media_hash_t* media_hash = load_state->game->media_hash;
      int count = 1;
      char* ptr;
      size_t size, len;

      while (media_hash->next) {
        media_hash = media_hash->next;
        count++;
      }

      size = count * 33;
      load_state->game->public.hash = ptr = (char*)rc_buf_alloc(&load_state->game->buffer, size);
      for (media_hash = load_state->game->media_hash; media_hash; media_hash = media_hash->next) {
        if (ptr != load_state->game->public.hash) {
          *ptr++ = ',';
          size--;
        }
        len = snprintf(ptr, size, "%s", media_hash->game_hash->hash);
        ptr += len;
        size -= len;
      }
    } else {
      /* only a single hash was tried, capture it */
      load_state->game->public.console_id = load_state->hash_console_id;
      load_state->game->public.hash = load_state->hash->hash;
    }

    load_state->game->public.title = "Unknown Game";
    load_state->game->public.badge_name = "";
    client->game = load_state->game;
    load_state->game = NULL;

    rc_client_load_error(load_state, RC_NO_GAME_LOADED, "Unknown game");
    return;
  }

  if (load_state->hash->hash[0] != '[') {
    load_state->game->public.id = load_state->hash->game_id;
    load_state->game->public.hash = load_state->hash->hash;
  }

  /* done with the hashing code, release the global pointer */
  g_hash_client = NULL;

  rc_mutex_lock(&client->state.mutex);
  result = client->state.user;
  if (result == RC_CLIENT_USER_STATE_LOGIN_REQUESTED)
    load_state->progress = RC_CLIENT_LOAD_STATE_AWAIT_LOGIN;
  rc_mutex_unlock(&client->state.mutex);

  switch (result) {
    case RC_CLIENT_USER_STATE_LOGGED_IN:
      break;

    case RC_CLIENT_USER_STATE_LOGIN_REQUESTED:
      /* do nothing, this function will be called again after login completes */
      return;

    default:
      rc_client_load_error(load_state, RC_LOGIN_REQUIRED, rc_error_str(RC_LOGIN_REQUIRED));
      return;
  }

  memset(&fetch_game_data_request, 0, sizeof(fetch_game_data_request));
  fetch_game_data_request.username = client->user.username;
  fetch_game_data_request.api_token = client->user.token;
  fetch_game_data_request.game_id = load_state->hash->game_id;

  result = rc_api_init_fetch_game_data_request(&request, &fetch_game_data_request);
  if (result != RC_OK) {
    rc_client_load_error(load_state, result, rc_error_str(result));
    return;
  }

  rc_client_begin_load_state(load_state, RC_CLIENT_LOAD_STATE_FETCHING_GAME_DATA, 1);

  RC_CLIENT_LOG_VERBOSE_FORMATTED(client, "Fetching data for game %u", fetch_game_data_request.game_id);
  client->callbacks.server_call(&request, rc_client_fetch_game_data_callback, load_state, client);
  rc_api_destroy_request(&request);
}

static void rc_client_identify_game_callback(const rc_api_server_response_t* server_response, void* callback_data)
{
  rc_client_load_state_t* load_state = (rc_client_load_state_t*)callback_data;
  rc_client_t* client = load_state->client;
  rc_api_resolve_hash_response_t resolve_hash_response;
  int outstanding_requests;
  const char* error_message;
  int result;

  if (rc_client_async_handle_aborted(client, &load_state->async_handle)) {
    rc_client_load_aborted(load_state);
    RC_CLIENT_LOG_VERBOSE(client, "Load aborted during game identification");
    return;
  }

  result = rc_api_process_resolve_hash_response(&resolve_hash_response, server_response->body);
  error_message = rc_client_server_error_message(&result, server_response->http_status_code, &resolve_hash_response.response);

  if (error_message) {
    rc_client_end_load_state(load_state);
    rc_client_load_error(load_state, result, error_message);
  }
  else {
    /* hash exists outside the load state - always update it */
    load_state->hash->game_id = resolve_hash_response.game_id;
    RC_CLIENT_LOG_INFO_FORMATTED(client, "Identified game: %u (%s)", load_state->hash->game_id, load_state->hash->hash);

    /* have to call end_load_state after updating hash in case the load_state gets free'd */
    outstanding_requests = rc_client_end_load_state(load_state);
    if (outstanding_requests < 0) {
      /* previous load state was aborted, load_state was free'd */
    }
    else {
      rc_client_begin_fetch_game_data(load_state);
    }
  }

  rc_api_destroy_resolve_hash_response(&resolve_hash_response);
}

static rc_client_game_hash_t* rc_client_find_game_hash(rc_client_t* client, const char* hash)
{
  rc_client_game_hash_t* game_hash;

  rc_mutex_lock(&client->state.mutex);
  game_hash = client->hashes;
  while (game_hash) {
    if (strcasecmp(game_hash->hash, hash) == 0)
      break;

    game_hash = game_hash->next;
  }

  if (!game_hash) {
    game_hash = rc_buf_alloc(&client->state.buffer, sizeof(rc_client_game_hash_t));
    memset(game_hash, 0, sizeof(*game_hash));
    snprintf(game_hash->hash, sizeof(game_hash->hash), "%s", hash);
    game_hash->game_id = RC_CLIENT_UNKNOWN_GAME_ID;
    game_hash->next = client->hashes;
    client->hashes = game_hash;
  }
  rc_mutex_unlock(&client->state.mutex);

  return game_hash;
}

static rc_client_async_handle_t* rc_client_load_game(rc_client_load_state_t* load_state,
  const char* hash, const char* file_path)
{
  rc_client_t* client = load_state->client;
  rc_client_game_hash_t* old_hash;

  if (client->state.load == NULL) {
    rc_client_unload_game(client);
    client->state.load = load_state;

    if (load_state->game == NULL) {
      load_state->game = (rc_client_game_info_t*)calloc(1, sizeof(*load_state->game));
      if (!load_state->game) {
        if (load_state->callback)
          load_state->callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), client, load_state->callback_userdata);

        rc_client_free_load_state(load_state);
        return NULL;
      }

      rc_buf_init(&load_state->game->buffer);
      rc_runtime_init(&load_state->game->runtime);
    }
  }
  else if (client->state.load != load_state) {
    /* previous load was aborted */
    if (load_state->callback)
      load_state->callback(RC_ABORTED, "The requested game is no longer active", client, load_state->callback_userdata);

    rc_client_free_load_state(load_state);
    return NULL;
  }

  old_hash = load_state->hash;
  load_state->hash = rc_client_find_game_hash(client, hash);

  if (file_path) {
    rc_client_media_hash_t* media_hash =
        (rc_client_media_hash_t*)rc_buf_alloc(&load_state->game->buffer, sizeof(*media_hash));
    media_hash->game_hash = load_state->hash;
    media_hash->path_djb2 = rc_djb2(file_path);
    media_hash->next = load_state->game->media_hash;
    load_state->game->media_hash = media_hash;
  }
  else if (load_state->game->media_hash && load_state->game->media_hash->game_hash == old_hash) {
    load_state->game->media_hash->game_hash = load_state->hash;
  }

  if (load_state->hash->game_id == RC_CLIENT_UNKNOWN_GAME_ID) {
    rc_api_resolve_hash_request_t resolve_hash_request;
    rc_api_request_t request;
    int result;

    memset(&resolve_hash_request, 0, sizeof(resolve_hash_request));
    resolve_hash_request.game_hash = hash;

    result = rc_api_init_resolve_hash_request(&request, &resolve_hash_request);
    if (result != RC_OK) {
      rc_client_load_error(load_state, result, rc_error_str(result));
      return NULL;
    }

    rc_client_begin_load_state(load_state, RC_CLIENT_LOAD_STATE_IDENTIFYING_GAME, 1);

    client->callbacks.server_call(&request, rc_client_identify_game_callback, load_state, client);

    rc_api_destroy_request(&request);
  }
  else {
    RC_CLIENT_LOG_INFO_FORMATTED(client, "Identified game: %u (%s)", load_state->hash->game_id, load_state->hash->hash);

    rc_client_begin_fetch_game_data(load_state);
  }

  return &load_state->async_handle;
}

rc_client_async_handle_t* rc_client_begin_load_game(rc_client_t* client, const char* hash, rc_client_callback_t callback, void* callback_userdata)
{
  rc_client_load_state_t* load_state;

  if (!client) {
    callback(RC_INVALID_STATE, "client is required", client, callback_userdata);
    return NULL;
  }

  if (!hash || !hash[0]) {
    callback(RC_INVALID_STATE, "hash is required", client, callback_userdata);
    return NULL;
  }

  load_state = (rc_client_load_state_t*)calloc(1, sizeof(*load_state));
  if (!load_state) {
    callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), client, callback_userdata);
    return NULL;
  }

  load_state->client = client;
  load_state->callback = callback;
  load_state->callback_userdata = callback_userdata;

  return rc_client_load_game(load_state, hash, NULL);
}

rc_client_async_handle_t* rc_client_begin_identify_and_load_game(rc_client_t* client,
    uint32_t console_id, const char* file_path,
    const uint8_t* data, size_t data_size,
    rc_client_callback_t callback, void* callback_userdata)
{
  rc_client_load_state_t* load_state;
  char hash[33];

  if (!client) {
    callback(RC_INVALID_STATE, "client is required", client, callback_userdata);
    return NULL;
  }

  if (data) {
    if (file_path) {
      RC_CLIENT_LOG_INFO_FORMATTED(client, "Identifying game: %zu bytes at %p (%s)", data_size, data, file_path);
    }
    else {
      RC_CLIENT_LOG_INFO_FORMATTED(client, "Identifying game: %zu bytes at %p", data_size, data);
    }
  }
  else if (file_path) {
    RC_CLIENT_LOG_INFO_FORMATTED(client, "Identifying game: %s", file_path);
  }
  else {
    callback(RC_INVALID_STATE, "either data or file_path is required", client, callback_userdata);
    return NULL;
  }

  if (client->state.log_level >= RC_CLIENT_LOG_LEVEL_INFO) {
    g_hash_client = client;
    rc_hash_init_error_message_callback(rc_client_log_hash_message);
    rc_hash_init_verbose_message_callback(rc_client_log_hash_message);
  }

  if (!file_path)
    file_path = "?";

  load_state = (rc_client_load_state_t*)calloc(1, sizeof(*load_state));
  if (!load_state) {
    callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), client, callback_userdata);
    return NULL;
  }
  load_state->client = client;
  load_state->callback = callback;
  load_state->callback_userdata = callback_userdata;
  rc_hash_initialize_iterator(&load_state->hash_iterator, file_path, data, data_size);

  if (console_id == RC_CONSOLE_UNKNOWN) {
    if (!rc_hash_iterate(hash, &load_state->hash_iterator)) {
      rc_client_load_error(load_state, RC_INVALID_STATE, "hash generation failed");
      return NULL;
    }

    load_state->hash_console_id = load_state->hash_iterator.consoles[load_state->hash_iterator.index - 1];
  }
  else {
    load_state->hash_console_id = console_id;

    if (data != NULL) {
      if (!rc_hash_generate_from_buffer(hash, console_id, data, data_size)) {
        rc_client_load_error(load_state, RC_INVALID_STATE, "hash generation failed");
        return NULL;
      }
    }
    else {
      if (!rc_hash_generate_from_file(hash, console_id, file_path)) {
        rc_client_load_error(load_state, RC_INVALID_STATE, "hash generation failed");
        return NULL;
      }
    }
  }

  return rc_client_load_game(load_state, hash, file_path);
}

void rc_client_unload_game(rc_client_t* client)
{
  rc_client_game_info_t* game;
  rc_client_scheduled_callback_data_t** last;
  rc_client_scheduled_callback_data_t* next;

  if (!client)
    return;

  rc_mutex_lock(&client->state.mutex);

  game = client->game;
  client->game = NULL;
  client->state.load = NULL;

  if (client->state.spectator_mode == RC_CLIENT_SPECTATOR_MODE_LOCKED)
    client->state.spectator_mode = RC_CLIENT_SPECTATOR_MODE_ON;

  last = &client->state.scheduled_callbacks;
  do {
    next = *last;
    if (!next)
      break;

    /* remove rich presence ping scheduled event for game */
    if (next->callback == rc_client_ping && game && next->related_id == game->public.id) {
      *last = next->next;
      continue;
    }

    last = &next->next;
  } while (1);

  rc_mutex_unlock(&client->state.mutex);

  if (game != NULL) {
    RC_CLIENT_LOG_INFO_FORMATTED(client, "Unloading game %u", game->public.id);
    rc_client_free_game(game);
  }
}

static void rc_client_change_media(rc_client_t* client, const rc_client_game_hash_t* game_hash, rc_client_callback_t callback, void* callback_userdata)
{
  if (game_hash->game_id == client->game->public.id) {
    RC_CLIENT_LOG_INFO_FORMATTED(client, "Switching to valid media for game %u: %s", game_hash->game_id, game_hash->hash);
  }
  else if (game_hash->game_id == RC_CLIENT_UNKNOWN_GAME_ID) {
    RC_CLIENT_LOG_INFO(client, "Switching to unknown media");
  }
  else if (game_hash->game_id == 0) {
    RC_CLIENT_LOG_INFO_FORMATTED(client, "Switching to unrecognized media: %s", game_hash->hash);
  }
  else {
    RC_CLIENT_LOG_INFO_FORMATTED(client, "Switching to known media for game %u: %s", game_hash->game_id, game_hash->hash);
  }

  client->game->public.hash = game_hash->hash;
  callback(RC_OK, NULL, client, callback_userdata);
}

static void rc_client_identify_changed_media_callback(const rc_api_server_response_t* server_response, void* callback_data)
{
  rc_client_load_state_t* load_state = (rc_client_load_state_t*)callback_data;
  rc_client_t* client = load_state->client;
  rc_api_resolve_hash_response_t resolve_hash_response;

  int result = rc_api_process_resolve_hash_response(&resolve_hash_response, server_response->body);
  const char* error_message = rc_client_server_error_message(&result, server_response->http_status_code, &resolve_hash_response.response);

  if (rc_client_async_handle_aborted(client, &load_state->async_handle)) {
    RC_CLIENT_LOG_VERBOSE(client, "Media change aborted");
    /* if lookup succeeded, still capture the new hash */
    if (result == RC_OK)
      load_state->hash->game_id = resolve_hash_response.game_id;
  }
  else if (client->game != load_state->game) {
    /* loaded game changed. return success regardless of result */
    load_state->callback(RC_ABORTED, "The requested game is no longer active", client, load_state->callback_userdata);
  }
  else if (error_message) {
    load_state->callback(result, error_message, client, load_state->callback_userdata);
  }
  else {
    load_state->hash->game_id = resolve_hash_response.game_id;

    if (resolve_hash_response.game_id == 0 && client->state.hardcore) {
      RC_CLIENT_LOG_WARN_FORMATTED(client, "Disabling hardcore for unidentified media: %s", load_state->hash->hash);
      rc_client_set_hardcore_enabled(client, 0);
      client->game->public.hash = load_state->hash->hash; /* do still update the loaded hash */
      load_state->callback(RC_HARDCORE_DISABLED, "Hardcore disabled. Unidentified media inserted.", client, load_state->callback_userdata);
    }
    else {
      RC_CLIENT_LOG_INFO_FORMATTED(client, "Identified game: %u (%s)", load_state->hash->game_id, load_state->hash->hash);
      rc_client_change_media(client, load_state->hash, load_state->callback, load_state->callback_userdata);
    }
  }

  free(load_state);
  rc_api_destroy_resolve_hash_response(&resolve_hash_response);
}

rc_client_async_handle_t* rc_client_begin_change_media(rc_client_t* client, const char* file_path,
    const uint8_t* data, size_t data_size, rc_client_callback_t callback, void* callback_userdata)
{
  rc_client_game_hash_t* game_hash = NULL;
  rc_client_media_hash_t* media_hash;
  rc_client_game_info_t* game;
  rc_client_pending_media_t* pending_media = NULL;
  uint32_t path_djb2;

  if (!client) {
    callback(RC_INVALID_STATE, "client is required", client, callback_userdata);
    return NULL;
  }

  if (!data && !file_path) {
    callback(RC_INVALID_STATE, "either data or file_path is required", client, callback_userdata);
    return NULL;
  }

  rc_mutex_lock(&client->state.mutex);
  if (client->state.load) {
    game = client->state.load->game;
    if (game->public.console_id == 0) {
      /* still waiting for game data */
      pending_media = client->state.load->pending_media;
      if (pending_media) {
        if (pending_media->data)
          free(pending_media->data);
        free((void*)pending_media->file_path);
        free(pending_media);
      }

      pending_media = (rc_client_pending_media_t*)calloc(1, sizeof(*pending_media));
      if (!pending_media) {
        rc_mutex_unlock(&client->state.mutex);
        callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), client, callback_userdata);
        return NULL;
      }

      pending_media->file_path = strdup(file_path);
      pending_media->callback = callback;
      pending_media->callback_userdata = callback_userdata;
      if (data && data_size) {
        pending_media->data_size = data_size;
        pending_media->data = (uint8_t*)malloc(data_size);
        if (!pending_media->data) {
          rc_mutex_unlock(&client->state.mutex);
          callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), client, callback_userdata);
          return NULL;
        }
        memcpy(pending_media->data, data, data_size);
      }

      client->state.load->pending_media = pending_media;
    }
  }
  else {
    game = client->game;
  }
  rc_mutex_unlock(&client->state.mutex);

  if (!game) {
    callback(RC_NO_GAME_LOADED, rc_error_str(RC_NO_GAME_LOADED), client, callback_userdata);
    return NULL;
  }

  /* still waiting for game data */
  if (pending_media) 
    return NULL;

  /* check to see if we've already hashed this file */
  path_djb2 = rc_djb2(file_path);
  rc_mutex_lock(&client->state.mutex);
  for (media_hash = game->media_hash; media_hash; media_hash = media_hash->next) {
    if (media_hash->path_djb2 == path_djb2) {
      game_hash = media_hash->game_hash;
      break;
    }
  }
  rc_mutex_unlock(&client->state.mutex);

  if (!game_hash) {
    char hash[33];
    int result;

    if (client->state.log_level >= RC_CLIENT_LOG_LEVEL_INFO) {
      g_hash_client = client;
      rc_hash_init_error_message_callback(rc_client_log_hash_message);
      rc_hash_init_verbose_message_callback(rc_client_log_hash_message);
    }

    if (data != NULL)
      result = rc_hash_generate_from_buffer(hash, game->public.console_id, data, data_size);
    else
      result = rc_hash_generate_from_file(hash, game->public.console_id, file_path);

    g_hash_client = NULL;

    if (!result) {
      /* when changing discs, if the disc is not supported by the system, allow it. this is
       * primarily for games that support user-provided audio CDs, but does allow using discs
       * from other systems for games that leverage user-provided discs. */
      strcpy_s(hash, sizeof(hash), "[NO HASH]");
    }

    game_hash = rc_client_find_game_hash(client, hash);

    media_hash = (rc_client_media_hash_t*)rc_buf_alloc(&game->buffer, sizeof(*media_hash));
    media_hash->game_hash = game_hash;
    media_hash->path_djb2 = path_djb2;

    rc_mutex_lock(&client->state.mutex);
    media_hash->next = game->media_hash;
    game->media_hash = media_hash;
    rc_mutex_unlock(&client->state.mutex);

    if (!result) {
      rc_client_change_media(client, game_hash, callback, callback_userdata);
      return NULL;
    }
  }

  if (game_hash->game_id != RC_CLIENT_UNKNOWN_GAME_ID) {
    rc_client_change_media(client, game_hash, callback, callback_userdata);
    return NULL;
  }
  else {
    /* call the server to make sure the hash is valid for the loaded game */
    rc_client_load_state_t* callback_data;
    rc_api_resolve_hash_request_t resolve_hash_request;
    rc_api_request_t request;
    int result;

    memset(&resolve_hash_request, 0, sizeof(resolve_hash_request));
    resolve_hash_request.game_hash = game_hash->hash;

    result = rc_api_init_resolve_hash_request(&request, &resolve_hash_request);
    if (result != RC_OK) {
      callback(result, rc_error_str(result), client, callback_userdata);
      return NULL;
    }

    callback_data = (rc_client_load_state_t*)calloc(1, sizeof(rc_client_load_state_t));
    if (!callback_data) {
      callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), client, callback_userdata);
      return NULL;
    }

    callback_data->callback = callback;
    callback_data->callback_userdata = callback_userdata;
    callback_data->client = client;
    callback_data->hash = game_hash;
    callback_data->game = game;

    client->callbacks.server_call(&request, rc_client_identify_changed_media_callback, callback_data, client);

    rc_api_destroy_request(&request);

    return &callback_data->async_handle;
  }
}

const rc_client_game_t* rc_client_get_game_info(const rc_client_t* client)
{
  return (client && client->game) ? &client->game->public : NULL;
}

int rc_client_game_get_image_url(const rc_client_game_t* game, char buffer[], size_t buffer_size)
{
  if (!game)
    return RC_INVALID_STATE;

  return rc_client_get_image_url(buffer, buffer_size, RC_IMAGE_TYPE_GAME, game->badge_name);
}

/* ===== Subsets ===== */

void rc_client_begin_load_subset(rc_client_t* client, uint32_t subset_id, rc_client_callback_t callback, void* callback_userdata)
{
  char buffer[32];
  rc_client_load_state_t* load_state;

  if (!client) {
    callback(RC_INVALID_STATE, "client is required", client, callback_userdata);
    return;
  }

  if (!client->game) {
    callback(RC_NO_GAME_LOADED, rc_error_str(RC_NO_GAME_LOADED), client, callback_userdata);
    return;
  }

  snprintf(buffer, sizeof(buffer), "[SUBSET%u]", subset_id);

  load_state = (rc_client_load_state_t*)calloc(1, sizeof(*load_state));
  if (!load_state) {
    callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), client, callback_userdata);
    return;
  }

  load_state->client = client;
  load_state->callback = callback;
  load_state->callback_userdata = callback_userdata;
  load_state->game = client->game;
  load_state->hash = rc_client_find_game_hash(client, buffer);
  load_state->hash->game_id = subset_id;
  client->state.load = load_state;

  rc_client_begin_fetch_game_data(load_state);
}

const rc_client_subset_t* rc_client_get_subset_info(rc_client_t* client, uint32_t subset_id)
{
  rc_client_subset_info_t* subset;

  if (!client || !client->game)
    return NULL;

  for (subset = client->game->subsets; subset; subset = subset->next) {
    if (subset->public.id == subset_id)
      return &subset->public;
  }

  return NULL;
}

/* ===== Achievements ===== */

static void rc_client_update_achievement_display_information(rc_client_t* client, rc_client_achievement_info_t* achievement, time_t recent_unlock_time)
{
  uint8_t new_bucket = RC_CLIENT_ACHIEVEMENT_BUCKET_UNKNOWN;
  uint32_t new_measured_value = 0;

  if (achievement->public.bucket == RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED)
    return;

  achievement->public.measured_progress[0] = '\0';

  if (achievement->public.state == RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED) {
    /* achievement unlocked */
    new_bucket = RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED;
  }
  else {
    /* active achievement */
    new_bucket = (achievement->public.category == RC_CLIENT_ACHIEVEMENT_CATEGORY_UNOFFICIAL) ?
        RC_CLIENT_ACHIEVEMENT_BUCKET_UNOFFICIAL : RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED;

    if (achievement->trigger) {
      if (achievement->trigger->measured_target) {
        if (achievement->trigger->measured_value == RC_MEASURED_UNKNOWN) {
          /* value hasn't been initialized yet, leave progress string empty */
        }
        else if (achievement->trigger->measured_value == 0) {
          /* value is 0, leave progress string empty. update progress to 0.0 */
          achievement->public.measured_percent = 0.0;
        }
        else {
          /* clamp measured value at target (can't get more than 100%) */
          new_measured_value = (achievement->trigger->measured_value > achievement->trigger->measured_target) ?
              achievement->trigger->measured_target : achievement->trigger->measured_value;

          achievement->public.measured_percent = ((float)new_measured_value * 100) / (float)achievement->trigger->measured_target;

          if (!achievement->trigger->measured_as_percent) {
            snprintf(achievement->public.measured_progress, sizeof(achievement->public.measured_progress),
                "%u/%u", new_measured_value, achievement->trigger->measured_target);
          }
          else if (achievement->public.measured_percent >= 1.0) {
            snprintf(achievement->public.measured_progress, sizeof(achievement->public.measured_progress),
                "%u%%", (uint32_t)achievement->public.measured_percent);
          }
        }
      }

      if (achievement->trigger->state == RC_TRIGGER_STATE_PRIMED)
        new_bucket = RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE;
      else if (achievement->public.measured_percent >= 80.0)
        new_bucket = RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE;
    }
  }

  if (new_bucket == RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED && achievement->public.unlock_time >= recent_unlock_time)
    new_bucket = RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED;

  achievement->public.bucket = new_bucket;
}

static const char* rc_client_get_achievement_bucket_label(uint8_t bucket_type)
{
  switch (bucket_type) {
    case RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED: return "Locked";
    case RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED: return "Unlocked";
    case RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED: return "Unsupported";
    case RC_CLIENT_ACHIEVEMENT_BUCKET_UNOFFICIAL: return "Unofficial";
    case RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED: return "Recently Unlocked";
    case RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE: return "Active Challenges";
    case RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE: return "Almost There";
    default: return "Unknown";
  }
}

static const char* rc_client_get_subset_achievement_bucket_label(uint8_t bucket_type, rc_client_game_info_t* game, rc_client_subset_info_t* subset)
{
  const char** ptr;
  const char* label;
  char* new_label;
  size_t new_label_len;

  switch (bucket_type) {
    case RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED: ptr = &subset->locked_label; break;
    case RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED: ptr = &subset->unlocked_label; break;
    case RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED: ptr = &subset->unsupported_label; break;
    case RC_CLIENT_ACHIEVEMENT_BUCKET_UNOFFICIAL: ptr = &subset->unofficial_label; break;
    default: return rc_client_get_achievement_bucket_label(bucket_type);
  }

  if (*ptr)
    return *ptr;

  label = rc_client_get_achievement_bucket_label(bucket_type);
  new_label_len = strlen(subset->public.title) + strlen(label) + 4;
  new_label = (char*)rc_buf_alloc(&game->buffer, new_label_len);
  snprintf(new_label, new_label_len, "%s - %s", subset->public.title, label);

  *ptr = new_label;
  return new_label;
}

static int rc_client_compare_achievement_unlock_times(const void* a, const void* b)
{
  const rc_client_achievement_t* unlock_a = *(const rc_client_achievement_t**)a;
  const rc_client_achievement_t* unlock_b = *(const rc_client_achievement_t**)b;
  return (int)(unlock_b->unlock_time - unlock_a->unlock_time);
}

static uint8_t rc_client_map_bucket(uint8_t bucket, int grouping)
{
  if (grouping == RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE) {
    switch (bucket) {
      case RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED:
        return RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED;

      case RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE:
      case RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE:
        return RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED;

      default:
        return bucket;
    }
  }

  return bucket;
}

rc_client_achievement_list_t* rc_client_create_achievement_list(rc_client_t* client, int category, int grouping)
{
  rc_client_achievement_info_t* achievement;
  rc_client_achievement_info_t* stop;
  rc_client_achievement_t** bucket_achievements;
  rc_client_achievement_t** achievement_ptr;
  rc_client_achievement_bucket_t* bucket_ptr;
  rc_client_achievement_list_t* list;
  rc_client_subset_info_t* subset;
  const uint32_t list_size = RC_ALIGN(sizeof(*list));
  uint32_t bucket_counts[16];
  uint32_t num_buckets;
  uint32_t num_achievements;
  size_t buckets_size;
  uint8_t bucket_type;
  uint32_t num_subsets = 0;
  uint32_t i, j;
  const uint8_t shared_bucket_order[] = {
    RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE,
    RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED,
    RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE
  };
  const uint8_t subset_bucket_order[] = {
    RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED,
    RC_CLIENT_ACHIEVEMENT_BUCKET_UNOFFICIAL,
    RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED,
    RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED
  };
  const time_t recent_unlock_time = time(NULL) - RC_CLIENT_RECENT_UNLOCK_DELAY_SECONDS;

  if (!client || !client->game)
    return calloc(1, sizeof(rc_client_achievement_list_t));

  memset(&bucket_counts, 0, sizeof(bucket_counts));

  rc_mutex_lock(&client->state.mutex);

  subset = client->game->subsets;
  for (; subset; subset = subset->next) {
    if (!subset->active)
      continue;

    num_subsets++;
    achievement = subset->achievements;
    stop = achievement + subset->public.num_achievements;
    for (; achievement < stop; ++achievement) {
      if (achievement->public.category & category) {
        rc_client_update_achievement_display_information(client, achievement, recent_unlock_time);
        bucket_counts[achievement->public.bucket]++;
      }
    }
  }

  num_buckets = 0;
  num_achievements = 0;
  for (i = 0; i < sizeof(bucket_counts) / sizeof(bucket_counts[0]); ++i) {
    if (bucket_counts[i]) {
      int needs_split = 0;

      num_achievements += bucket_counts[i];

      if (num_subsets > 1) {
        for (j = 0; j < sizeof(subset_bucket_order) / sizeof(subset_bucket_order[0]); ++j) {
          if (subset_bucket_order[j] == i) {
            needs_split = 1;
            break;
          }
        }
      }

      if (!needs_split) {
        ++num_buckets;
        continue;
      }

      subset = client->game->subsets;
      for (; subset; subset = subset->next) {
        if (!subset->active)
          continue;

        achievement = subset->achievements;
        stop = achievement + subset->public.num_achievements;
        for (; achievement < stop; ++achievement) {
          if (achievement->public.category & category) {
            if (achievement->public.bucket == i) {
              ++num_buckets;
              break;
            }
          }
        }
      }
    }
  }

  buckets_size = RC_ALIGN(num_buckets * sizeof(rc_client_achievement_bucket_t));

  list = (rc_client_achievement_list_t*)malloc(list_size + buckets_size + num_achievements * sizeof(rc_client_achievement_t*));
  bucket_ptr = list->buckets = (rc_client_achievement_bucket_t*)((uint8_t*)list + list_size);
  achievement_ptr = (rc_client_achievement_t**)((uint8_t*)bucket_ptr + buckets_size);

  if (grouping == RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS) {
    for (i = 0; i < sizeof(shared_bucket_order) / sizeof(shared_bucket_order[0]); ++i) {
      bucket_type = shared_bucket_order[i];
      if (!bucket_counts[bucket_type])
        continue;

      bucket_achievements = achievement_ptr;
      for (subset = client->game->subsets; subset; subset = subset->next) {
        if (!subset->active)
          continue;

        achievement = subset->achievements;
        stop = achievement + subset->public.num_achievements;
        for (; achievement < stop; ++achievement) {
          if (achievement->public.bucket == bucket_type && achievement->public.category & category)
            *achievement_ptr++ = &achievement->public;
        }
      }

      if (achievement_ptr > bucket_achievements) {
        bucket_ptr->achievements = bucket_achievements;
        bucket_ptr->num_achievements = (uint32_t)(achievement_ptr - bucket_achievements);
        bucket_ptr->subset_id = 0;
        bucket_ptr->label = rc_client_get_achievement_bucket_label(bucket_type);
        bucket_ptr->bucket_type = bucket_type;

        if (bucket_type == RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED)
          qsort(bucket_ptr->achievements, bucket_ptr->num_achievements, sizeof(rc_client_achievement_t*), rc_client_compare_achievement_unlock_times);

        ++bucket_ptr;
      }
    }
  }

  for (subset = client->game->subsets; subset; subset = subset->next) {
    if (!subset->active)
      continue;

    for (i = 0; i < sizeof(subset_bucket_order) / sizeof(subset_bucket_order[0]); ++i) {
      bucket_type = subset_bucket_order[i];
      if (!bucket_counts[bucket_type])
        continue;

      bucket_achievements = achievement_ptr;

      achievement = subset->achievements;
      stop = achievement + subset->public.num_achievements;
      for (; achievement < stop; ++achievement) {
        if (achievement->public.category & category &&
            rc_client_map_bucket(achievement->public.bucket, grouping) == bucket_type) {
          *achievement_ptr++ = &achievement->public;
        }
      }

      if (achievement_ptr > bucket_achievements) {
        bucket_ptr->achievements = bucket_achievements;
        bucket_ptr->num_achievements = (uint32_t)(achievement_ptr - bucket_achievements);
        bucket_ptr->subset_id = (num_subsets > 1) ? subset->public.id : 0;
        bucket_ptr->bucket_type = bucket_type;

        if (num_subsets > 1)
          bucket_ptr->label = rc_client_get_subset_achievement_bucket_label(bucket_type, client->game, subset);
        else
          bucket_ptr->label = rc_client_get_achievement_bucket_label(bucket_type);

        ++bucket_ptr;
      }
    }
  }

  rc_mutex_unlock(&client->state.mutex);

  list->num_buckets = (uint32_t)(bucket_ptr - list->buckets);
  return list;
}

void rc_client_destroy_achievement_list(rc_client_achievement_list_t* list)
{
  if (list)
    free(list);
}

static const rc_client_achievement_t* rc_client_subset_get_achievement_info(
    rc_client_t* client, rc_client_subset_info_t* subset, uint32_t id)
{
  rc_client_achievement_info_t* achievement = subset->achievements;
  rc_client_achievement_info_t* stop = achievement + subset->public.num_achievements;

  for (; achievement < stop; ++achievement) {
    if (achievement->public.id == id) {
      const time_t recent_unlock_time = time(NULL) - RC_CLIENT_RECENT_UNLOCK_DELAY_SECONDS;
      rc_mutex_lock((rc_mutex_t*)(&client->state.mutex));
      rc_client_update_achievement_display_information(client, achievement, recent_unlock_time);
      rc_mutex_unlock((rc_mutex_t*)(&client->state.mutex));
      return &achievement->public;
    }
  }

  return NULL;
}

const rc_client_achievement_t* rc_client_get_achievement_info(rc_client_t* client, uint32_t id)
{
  rc_client_subset_info_t* subset;

  if (!client || !client->game)
    return NULL;

  for (subset = client->game->subsets; subset; subset = subset->next) {
    const rc_client_achievement_t* achievement = rc_client_subset_get_achievement_info(client, subset, id);
    if (achievement != NULL)
      return achievement;
  }

  return NULL;
}

int rc_client_achievement_get_image_url(const rc_client_achievement_t* achievement, int state, char buffer[], size_t buffer_size)
{
  const int image_type = (state == RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED) ?
      RC_IMAGE_TYPE_ACHIEVEMENT : RC_IMAGE_TYPE_ACHIEVEMENT_LOCKED;

  if (!achievement || !achievement->badge_name[0])
    return rc_client_get_image_url(buffer, buffer_size, image_type, "00000");

  return rc_client_get_image_url(buffer, buffer_size, image_type, achievement->badge_name);
}

typedef struct rc_client_award_achievement_callback_data_t
{
  uint32_t id;
  uint32_t retry_count;
  uint8_t hardcore;
  const char* game_hash;
  time_t unlock_time;
  rc_client_t* client;
  rc_client_scheduled_callback_data_t* scheduled_callback_data;
} rc_client_award_achievement_callback_data_t;

static void rc_client_award_achievement_server_call(rc_client_award_achievement_callback_data_t* ach_data);

static void rc_client_award_achievement_retry(rc_client_scheduled_callback_data_t* callback_data, rc_client_t* client, time_t now)
{
  rc_client_award_achievement_callback_data_t* ach_data =
    (rc_client_award_achievement_callback_data_t*)callback_data->data;

  rc_client_award_achievement_server_call(ach_data);
}

static void rc_client_award_achievement_callback(const rc_api_server_response_t* server_response, void* callback_data)
{
  rc_client_award_achievement_callback_data_t* ach_data =
      (rc_client_award_achievement_callback_data_t*)callback_data;
  rc_api_award_achievement_response_t award_achievement_response;

  int result = rc_api_process_award_achievement_response(&award_achievement_response, server_response->body);
  const char* error_message = rc_client_server_error_message(&result, server_response->http_status_code, &award_achievement_response.response);

  if (error_message) {
    if (award_achievement_response.response.error_message) {
      /* actual error from server */
      RC_CLIENT_LOG_ERR_FORMATTED(ach_data->client, "Error awarding achievement %u: %s", ach_data->id, error_message);
      rc_client_raise_server_error_event(ach_data->client, "award_achievement", award_achievement_response.response.error_message);
    }
    else if (ach_data->retry_count++ == 0) {
      /* first retry is immediate */
      RC_CLIENT_LOG_ERR_FORMATTED(ach_data->client, "Error awarding achievement %u: %s, retrying immediately", ach_data->id, error_message);
      rc_client_award_achievement_server_call(ach_data);
      return;
    }
    else {
      /* double wait time between each attempt until we hit a maximum delay of two minutes */
      /* 1s -> 2s -> 4s -> 8s -> 16s -> 32s -> 64s -> 120s -> 120s -> 120s ...*/
      const uint32_t delay = (ach_data->retry_count > 7) ? 120 : (1 << (ach_data->retry_count - 1));
      RC_CLIENT_LOG_ERR_FORMATTED(ach_data->client, "Error awarding achievement %u: %s, retrying in %u seconds", ach_data->id, error_message, delay);

      if (!ach_data->scheduled_callback_data) {
        ach_data->scheduled_callback_data = (rc_client_scheduled_callback_data_t*)calloc(1, sizeof(*ach_data->scheduled_callback_data));
        if (!ach_data->scheduled_callback_data) {
          RC_CLIENT_LOG_ERR_FORMATTED(ach_data->client, "Failed to allocate scheduled callback data for reattempt to unlock achievement %u", ach_data->id);
          rc_client_raise_server_error_event(ach_data->client, "award_achievement", rc_error_str(RC_OUT_OF_MEMORY));
          return;
        }
        ach_data->scheduled_callback_data->callback = rc_client_award_achievement_retry;
        ach_data->scheduled_callback_data->data = ach_data;
        ach_data->scheduled_callback_data->related_id = ach_data->id;
      }

      ach_data->scheduled_callback_data->when = time(NULL) + delay;

      rc_client_schedule_callback(ach_data->client, ach_data->scheduled_callback_data);
      return;
    }
  }
  else {
    ach_data->client->user.score = award_achievement_response.new_player_score;
    ach_data->client->user.score_softcore = award_achievement_response.new_player_score_softcore;

    if (award_achievement_response.awarded_achievement_id != ach_data->id) {
      RC_CLIENT_LOG_ERR_FORMATTED(ach_data->client, "Awarded achievement %u instead of %u", award_achievement_response.awarded_achievement_id, error_message);
    }
    else {
      if (award_achievement_response.response.error_message) {
        /* previously unlocked achievements are returned as a success with an error message */
        RC_CLIENT_LOG_INFO_FORMATTED(ach_data->client, "Achievement %u: %s", ach_data->id, award_achievement_response.response.error_message);
      }
      else if (ach_data->retry_count) {
        RC_CLIENT_LOG_INFO_FORMATTED(ach_data->client, "Achievement %u awarded after %u attempts, new score: %u",
            ach_data->id, ach_data->retry_count + 1,
            ach_data->hardcore ? award_achievement_response.new_player_score : award_achievement_response.new_player_score_softcore);
      }
      else {
        RC_CLIENT_LOG_INFO_FORMATTED(ach_data->client, "Achievement %u awarded, new score: %u",
            ach_data->id,
            ach_data->hardcore ? award_achievement_response.new_player_score : award_achievement_response.new_player_score_softcore);
      }

      if (award_achievement_response.achievements_remaining == 0) {
        rc_client_subset_info_t* subset;
        for (subset = ach_data->client->game->subsets; subset; subset = subset->next) {
          if (subset->mastery == RC_CLIENT_MASTERY_STATE_NONE &&
              rc_client_subset_get_achievement_info(ach_data->client, subset, ach_data->id)) {
            if (subset->public.id == ach_data->client->game->public.id) {
              RC_CLIENT_LOG_INFO_FORMATTED(ach_data->client, "Game %u %s", ach_data->client->game->public.id,
                ach_data->client->state.hardcore ? "mastered" : "completed");
              subset->mastery = RC_CLIENT_MASTERY_STATE_PENDING;
            }
            else {
              RC_CLIENT_LOG_INFO_FORMATTED(ach_data->client, "Subset %u %s", ach_data->client->game->public.id,
                ach_data->client->state.hardcore ? "mastered" : "completed");

              /* TODO: subset mastery notification */
              subset->mastery = RC_CLIENT_MASTERY_STATE_SHOWN;
            }
          }
        }
      }
    }
  }

  if (ach_data->scheduled_callback_data)
    free(ach_data->scheduled_callback_data);
  free(ach_data);
}

static void rc_client_award_achievement_server_call(rc_client_award_achievement_callback_data_t* ach_data)
{ 
  rc_api_award_achievement_request_t api_params;
  rc_api_request_t request;
  int result;

  memset(&api_params, 0, sizeof(api_params));
  api_params.username = ach_data->client->user.username;
  api_params.api_token = ach_data->client->user.token;
  api_params.achievement_id = ach_data->id;
  api_params.hardcore = ach_data->hardcore;
  api_params.game_hash = ach_data->game_hash;

  result = rc_api_init_award_achievement_request(&request, &api_params);
  if (result != RC_OK) {
    RC_CLIENT_LOG_ERR_FORMATTED(ach_data->client, "Error constructing unlock request for achievement %u: %s", ach_data->id, rc_error_str(result));
    free(ach_data);
    return;
  }

  ach_data->client->callbacks.server_call(&request, rc_client_award_achievement_callback, ach_data, ach_data->client);

  rc_api_destroy_request(&request);
}

static void rc_client_award_achievement(rc_client_t* client, rc_client_achievement_info_t* achievement)
{
  rc_client_award_achievement_callback_data_t* callback_data;

  rc_mutex_lock(&client->state.mutex);

  if (client->state.hardcore) {
    achievement->public.unlock_time = achievement->unlock_time_hardcore = time(NULL);
    if (achievement->unlock_time_softcore == 0)
      achievement->unlock_time_softcore = achievement->unlock_time_hardcore;

    /* adjust score now - will get accurate score back from server */
    client->user.score += achievement->public.points;
  }
  else {
    achievement->public.unlock_time = achievement->unlock_time_softcore = time(NULL);

    /* adjust score now - will get accurate score back from server */
    client->user.score_softcore += achievement->public.points;
  }

  achievement->public.state = RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED;
  achievement->public.unlocked |= (client->state.hardcore) ?
    RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH : RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE;

  rc_mutex_unlock(&client->state.mutex);

  /* can't unlock unofficial achievements on the server */
  if (achievement->public.category != RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE) {
    RC_CLIENT_LOG_INFO_FORMATTED(client, "Unlocked unofficial achievement %u: %s", achievement->public.id, achievement->public.title);
    return;
  }

  /* don't actually unlock achievements when spectating */
  if (client->state.spectator_mode != RC_CLIENT_SPECTATOR_MODE_OFF) {
    RC_CLIENT_LOG_INFO_FORMATTED(client, "Spectated achievement %u: %s", achievement->public.id, achievement->public.title);
    return;
  }

  callback_data = (rc_client_award_achievement_callback_data_t*)calloc(1, sizeof(*callback_data));
  if (!callback_data) {
    RC_CLIENT_LOG_ERR_FORMATTED(client, "Failed to allocate callback data for unlocking achievement %u", achievement->public.id);
    rc_client_raise_server_error_event(client, "award_achievement", rc_error_str(RC_OUT_OF_MEMORY));
    return;
  }
  callback_data->client = client;
  callback_data->id = achievement->public.id;
  callback_data->hardcore = client->state.hardcore;
  callback_data->game_hash = client->game->public.hash;
  callback_data->unlock_time = achievement->public.unlock_time;

  RC_CLIENT_LOG_INFO_FORMATTED(client, "Awarding achievement %u: %s", achievement->public.id, achievement->public.title);
  rc_client_award_achievement_server_call(callback_data);
}

static void rc_client_subset_reset_achievements(rc_client_subset_info_t* subset)
{
  rc_client_achievement_info_t* achievement = subset->achievements;
  rc_client_achievement_info_t* stop = achievement + subset->public.num_achievements;

  for (; achievement < stop; ++achievement) {
    rc_trigger_t* trigger = achievement->trigger;
    if (!trigger || achievement->public.state != RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE)
      continue;

    if (trigger->state == RC_TRIGGER_STATE_PRIMED) {
      achievement->pending_events |= RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_HIDE;
      subset->pending_events |= RC_CLIENT_SUBSET_PENDING_EVENT_ACHIEVEMENT;
    }

    rc_reset_trigger(trigger);
  }
}

static void rc_client_reset_achievements(rc_client_t* client)
{
  rc_client_subset_info_t* subset;
  for (subset = client->game->subsets; subset; subset = subset->next)
    rc_client_subset_reset_achievements(subset);
}

/* ===== Leaderboards ===== */

static const rc_client_leaderboard_t* rc_client_subset_get_leaderboard_info(const rc_client_subset_info_t* subset, uint32_t id)
{
  rc_client_leaderboard_info_t* leaderboard = subset->leaderboards;
  rc_client_leaderboard_info_t* stop = leaderboard + subset->public.num_leaderboards;

  for (; leaderboard < stop; ++leaderboard) {
    if (leaderboard->public.id == id)
      return &leaderboard->public;
  }

  return NULL;
}

const rc_client_leaderboard_t* rc_client_get_leaderboard_info(const rc_client_t* client, uint32_t id)
{
  rc_client_subset_info_t* subset;

  if (!client || !client->game)
    return NULL;

  for (subset = client->game->subsets; subset; subset = subset->next) {
    const rc_client_leaderboard_t* leaderboard = rc_client_subset_get_leaderboard_info(subset, id);
    if (leaderboard != NULL)
      return leaderboard;
  }
 
  return NULL;
}

static const char* rc_client_get_leaderboard_bucket_label(uint8_t bucket_type)
{
  switch (bucket_type) {
    case RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE: return "Inactive";
    case RC_CLIENT_LEADERBOARD_BUCKET_ACTIVE: return "Active";
    case RC_CLIENT_LEADERBOARD_BUCKET_UNSUPPORTED: return "Unsupported";
    case RC_CLIENT_LEADERBOARD_BUCKET_ALL: return "All";
    default: return "Unknown";
  }
}

static const char* rc_client_get_subset_leaderboard_bucket_label(uint8_t bucket_type, rc_client_game_info_t* game, rc_client_subset_info_t* subset)
{
  const char** ptr;
  const char* label;
  char* new_label;
  size_t new_label_len;

  switch (bucket_type) {
    case RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE: ptr = &subset->inactive_label; break;
    case RC_CLIENT_LEADERBOARD_BUCKET_UNSUPPORTED: ptr = &subset->unsupported_label; break;
    case RC_CLIENT_LEADERBOARD_BUCKET_ALL: ptr = &subset->all_label; break;
    default: return rc_client_get_achievement_bucket_label(bucket_type);
  }

  if (*ptr)
    return *ptr;

  label = rc_client_get_leaderboard_bucket_label(bucket_type);
  new_label_len = strlen(subset->public.title) + strlen(label) + 4;
  new_label = (char*)rc_buf_alloc(&game->buffer, new_label_len);
  snprintf(new_label, new_label_len, "%s - %s", subset->public.title, label);

  *ptr = new_label;
  return new_label;
}

static uint8_t rc_client_get_leaderboard_bucket(const rc_client_leaderboard_info_t* leaderboard, int grouping)
{
  switch (leaderboard->public.state) {
    case RC_CLIENT_LEADERBOARD_STATE_TRACKING:
      return (grouping == RC_CLIENT_LEADERBOARD_LIST_GROUPING_NONE) ?
        RC_CLIENT_LEADERBOARD_BUCKET_ALL : RC_CLIENT_LEADERBOARD_BUCKET_ACTIVE;

    case RC_CLIENT_LEADERBOARD_STATE_DISABLED:
      return RC_CLIENT_LEADERBOARD_BUCKET_UNSUPPORTED;

    default:
      return (grouping == RC_CLIENT_LEADERBOARD_LIST_GROUPING_NONE) ?
        RC_CLIENT_LEADERBOARD_BUCKET_ALL : RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE;
  }
}

rc_client_leaderboard_list_t* rc_client_create_leaderboard_list(rc_client_t* client, int grouping)
{
  rc_client_leaderboard_info_t* leaderboard;
  rc_client_leaderboard_info_t* stop;
  rc_client_leaderboard_t** bucket_leaderboards;
  rc_client_leaderboard_t** leaderboard_ptr;
  rc_client_leaderboard_bucket_t* bucket_ptr;
  rc_client_leaderboard_list_t* list;
  rc_client_subset_info_t* subset;
  const uint32_t list_size = RC_ALIGN(sizeof(*list));
  uint32_t bucket_counts[8];
  uint32_t num_buckets;
  uint32_t num_leaderboards;
  size_t buckets_size;
  uint8_t bucket_type;
  uint32_t num_subsets = 0;
  uint32_t i, j;
  const uint8_t shared_bucket_order[] = {
    RC_CLIENT_LEADERBOARD_BUCKET_ACTIVE
  };
  const uint8_t subset_bucket_order[] = {
    RC_CLIENT_LEADERBOARD_BUCKET_ALL,
    RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE,
    RC_CLIENT_LEADERBOARD_BUCKET_UNSUPPORTED
  };

  if (!client || !client->game)
    return calloc(1, sizeof(rc_client_leaderboard_list_t));

  memset(&bucket_counts, 0, sizeof(bucket_counts));

  rc_mutex_lock(&client->state.mutex);

  subset = client->game->subsets;
  for (; subset; subset = subset->next) {
    if (!subset->active)
      continue;

    num_subsets++;
    leaderboard = subset->leaderboards;
    stop = leaderboard + subset->public.num_leaderboards;
    for (; leaderboard < stop; ++leaderboard) {
      leaderboard->bucket = rc_client_get_leaderboard_bucket(leaderboard, grouping);
      bucket_counts[leaderboard->bucket]++;
    }
  }

  num_buckets = 0;
  num_leaderboards = 0;
  for (i = 0; i < sizeof(bucket_counts) / sizeof(bucket_counts[0]); ++i) {
    if (bucket_counts[i]) {
      int needs_split = 0;

      num_leaderboards += bucket_counts[i];

      if (num_subsets > 1) {
        for (j = 0; j < sizeof(subset_bucket_order) / sizeof(subset_bucket_order[0]); ++j) {
          if (subset_bucket_order[j] == i) {
            needs_split = 1;
            break;
          }
        }
      }

      if (!needs_split) {
        ++num_buckets;
        continue;
      }

      subset = client->game->subsets;
      for (; subset; subset = subset->next) {
        if (!subset->active)
          continue;

        leaderboard = subset->leaderboards;
        stop = leaderboard + subset->public.num_leaderboards;
        for (; leaderboard < stop; ++leaderboard) {
          if (leaderboard->bucket == i) {
            ++num_buckets;
            break;
          }
        }
      }
    }
  }

  buckets_size = RC_ALIGN(num_buckets * sizeof(rc_client_leaderboard_bucket_t));

  list = (rc_client_leaderboard_list_t*)malloc(list_size + buckets_size + num_leaderboards * sizeof(rc_client_leaderboard_t*));
  bucket_ptr = list->buckets = (rc_client_leaderboard_bucket_t*)((uint8_t*)list + list_size);
  leaderboard_ptr = (rc_client_leaderboard_t**)((uint8_t*)bucket_ptr + buckets_size);

  if (grouping == RC_CLIENT_LEADERBOARD_LIST_GROUPING_TRACKING) {
    for (i = 0; i < sizeof(shared_bucket_order) / sizeof(shared_bucket_order[0]); ++i) {
      bucket_type = shared_bucket_order[i];
      if (!bucket_counts[bucket_type])
        continue;

      bucket_leaderboards = leaderboard_ptr;
      for (subset = client->game->subsets; subset; subset = subset->next) {
        if (!subset->active)
          continue;

        leaderboard = subset->leaderboards;
        stop = leaderboard + subset->public.num_leaderboards;
        for (; leaderboard < stop; ++leaderboard) {
          if (leaderboard->bucket == bucket_type)
            *leaderboard_ptr++ = &leaderboard->public;
        }
      }

      if (leaderboard_ptr > bucket_leaderboards) {
        bucket_ptr->leaderboards = bucket_leaderboards;
        bucket_ptr->num_leaderboards = (uint32_t)(leaderboard_ptr - bucket_leaderboards);
        bucket_ptr->subset_id = 0;
        bucket_ptr->label = rc_client_get_leaderboard_bucket_label(bucket_type);
        bucket_ptr->bucket_type = bucket_type;
        ++bucket_ptr;
      }
    }
  }

  for (subset = client->game->subsets; subset; subset = subset->next) {
    if (!subset->active)
      continue;

    for (i = 0; i < sizeof(subset_bucket_order) / sizeof(subset_bucket_order[0]); ++i) {
      bucket_type = subset_bucket_order[i];
      if (!bucket_counts[bucket_type])
        continue;

      bucket_leaderboards = leaderboard_ptr;

      leaderboard = subset->leaderboards;
      stop = leaderboard + subset->public.num_leaderboards;
      for (; leaderboard < stop; ++leaderboard) {
        if (leaderboard->bucket == bucket_type)
          *leaderboard_ptr++ = &leaderboard->public;
      }

      if (leaderboard_ptr > bucket_leaderboards) {
        bucket_ptr->leaderboards = bucket_leaderboards;
        bucket_ptr->num_leaderboards = (uint32_t)(leaderboard_ptr - bucket_leaderboards);
        bucket_ptr->subset_id = (num_subsets > 1) ? subset->public.id : 0;
        bucket_ptr->bucket_type = bucket_type;

        if (num_subsets > 1)
          bucket_ptr->label = rc_client_get_subset_leaderboard_bucket_label(bucket_type, client->game, subset);
        else
          bucket_ptr->label = rc_client_get_leaderboard_bucket_label(bucket_type);

        ++bucket_ptr;
      }
    }
  }

  rc_mutex_unlock(&client->state.mutex);

  list->num_buckets = (uint32_t)(bucket_ptr - list->buckets);
  return list;
}

void rc_client_destroy_leaderboard_list(rc_client_leaderboard_list_t* list)
{
  if (list)
    free(list);
}

static void rc_client_allocate_leaderboard_tracker(rc_client_game_info_t* game, rc_client_leaderboard_info_t* leaderboard)
{
  rc_client_leaderboard_tracker_info_t* tracker;
  rc_client_leaderboard_tracker_info_t* available_tracker = NULL;

  for (tracker = game->leaderboard_trackers; tracker; tracker = tracker->next) {
    if (tracker->reference_count == 0) {
      if (available_tracker == NULL)
        available_tracker = tracker;

      continue;
    }

    if (tracker->value_djb2 != leaderboard->value_djb2 || tracker->format != leaderboard->format)
      continue;

    if (tracker->raw_value != leaderboard->value) {
      /* if the value comes from tracking hits, we can't assume the trackers started in the
       * same frame, so we can't share the tracker */
      if (tracker->value_from_hits)
        continue;

      /* value has changed. prepare an update event */
      tracker->raw_value = leaderboard->value;
      tracker->pending_events |= RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_UPDATE;
      game->pending_events |= RC_CLIENT_GAME_PENDING_EVENT_LEADERBOARD_TRACKER;
    }

    /* attach to the existing tracker */
    ++tracker->reference_count;
    tracker->pending_events &= ~RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_HIDE;
    leaderboard->tracker = tracker;
    leaderboard->public.tracker_value = tracker->public.display;
    return;
  }

  if (!available_tracker) {
    rc_client_leaderboard_tracker_info_t** next = &game->leaderboard_trackers;

    available_tracker = (rc_client_leaderboard_tracker_info_t*)rc_buf_alloc(&game->buffer, sizeof(*available_tracker));
    memset(available_tracker, 0, sizeof(*available_tracker));
    available_tracker->public.id = 1;

    for (tracker = *next; tracker; next = &tracker->next, tracker = *next)
      available_tracker->public.id++;

    *next = available_tracker;
  }

  /* update the claimed tracker */
  available_tracker->reference_count = 1;
  available_tracker->value_djb2 = leaderboard->value_djb2;
  available_tracker->format = leaderboard->format;
  available_tracker->raw_value = leaderboard->value;
  available_tracker->pending_events = RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_SHOW;
  available_tracker->value_from_hits = rc_value_from_hits(&leaderboard->lboard->value);
  leaderboard->tracker = available_tracker;
  leaderboard->public.tracker_value = available_tracker->public.display;
  game->pending_events |= RC_CLIENT_GAME_PENDING_EVENT_LEADERBOARD_TRACKER;
}

static void rc_client_release_leaderboard_tracker(rc_client_game_info_t* game, rc_client_leaderboard_info_t* leaderboard)
{
  rc_client_leaderboard_tracker_info_t* tracker = leaderboard->tracker;
  leaderboard->tracker = NULL;

  if (tracker && --tracker->reference_count == 0) {
    tracker->pending_events |= RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_HIDE;
    game->pending_events |= RC_CLIENT_GAME_PENDING_EVENT_LEADERBOARD_TRACKER;
  }
}

static void rc_client_update_leaderboard_tracker(rc_client_game_info_t* game, rc_client_leaderboard_info_t* leaderboard)
{
  rc_client_leaderboard_tracker_info_t* tracker = leaderboard->tracker;
  if (tracker && tracker->raw_value != leaderboard->value) {
    tracker->raw_value = leaderboard->value;
    tracker->pending_events |= RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_UPDATE;
    game->pending_events |= RC_CLIENT_GAME_PENDING_EVENT_LEADERBOARD_TRACKER;
  }
}

typedef struct rc_client_submit_leaderboard_entry_callback_data_t
{
  uint32_t id;
  int32_t score;
  uint32_t retry_count;
  const char* game_hash;
  time_t submit_time;
  rc_client_t* client;
  rc_client_scheduled_callback_data_t* scheduled_callback_data;
} rc_client_submit_leaderboard_entry_callback_data_t;

static void rc_client_submit_leaderboard_entry_server_call(rc_client_submit_leaderboard_entry_callback_data_t* lboard_data);

static void rc_client_submit_leaderboard_entry_retry(rc_client_scheduled_callback_data_t* callback_data, rc_client_t* client, time_t now)
{
  rc_client_submit_leaderboard_entry_callback_data_t* lboard_data =
      (rc_client_submit_leaderboard_entry_callback_data_t*)callback_data->data;

  rc_client_submit_leaderboard_entry_server_call(lboard_data);
}

static void rc_client_submit_leaderboard_entry_callback(const rc_api_server_response_t* server_response, void* callback_data)
{
  rc_client_submit_leaderboard_entry_callback_data_t* lboard_data =
      (rc_client_submit_leaderboard_entry_callback_data_t*)callback_data;
  rc_api_submit_lboard_entry_response_t submit_lboard_entry_response;

  int result = rc_api_process_submit_lboard_entry_response(&submit_lboard_entry_response, server_response->body);
  const char* error_message = rc_client_server_error_message(&result, server_response->http_status_code, &submit_lboard_entry_response.response);

  if (error_message) {
    if (submit_lboard_entry_response.response.error_message) {
      /* actual error from server */
      RC_CLIENT_LOG_ERR_FORMATTED(lboard_data->client, "Error submitting leaderboard entry %u: %s", lboard_data->id, error_message);
      rc_client_raise_server_error_event(lboard_data->client, "submit_lboard_entry", submit_lboard_entry_response.response.error_message);
    }
    else if (lboard_data->retry_count++ == 0) {
      /* first retry is immediate */
      RC_CLIENT_LOG_ERR_FORMATTED(lboard_data->client, "Error submitting leaderboard entry %u: %s, retrying immediately", lboard_data->id, error_message);
      rc_client_submit_leaderboard_entry_server_call(lboard_data);
      return;
    }
    else {
      /* double wait time between each attempt until we hit a maximum delay of two minutes */
      /* 1s -> 2s -> 4s -> 8s -> 16s -> 32s -> 64s -> 120s -> 120s -> 120s ...*/
      const uint32_t delay = (lboard_data->retry_count > 7) ? 120 : (1 << (lboard_data->retry_count - 1));
      RC_CLIENT_LOG_ERR_FORMATTED(lboard_data->client, "Error submitting leaderboard entry %u: %s, retrying in %u seconds", lboard_data->id, error_message, delay);

      if (!lboard_data->scheduled_callback_data) {
        lboard_data->scheduled_callback_data = (rc_client_scheduled_callback_data_t*)calloc(1, sizeof(*lboard_data->scheduled_callback_data));
        if (!lboard_data->scheduled_callback_data) {
          RC_CLIENT_LOG_ERR_FORMATTED(lboard_data->client, "Failed to allocate scheduled callback data for reattempt to submit entry for leaderboard %u", lboard_data->id);
          rc_client_raise_server_error_event(lboard_data->client, "submit_lboard_entry", rc_error_str(RC_OUT_OF_MEMORY));
          return;
        }
        lboard_data->scheduled_callback_data->callback = rc_client_submit_leaderboard_entry_retry;
        lboard_data->scheduled_callback_data->data = lboard_data;
        lboard_data->scheduled_callback_data->related_id = lboard_data->id;
      }

      lboard_data->scheduled_callback_data->when = time(NULL) + delay;

      rc_client_schedule_callback(lboard_data->client, lboard_data->scheduled_callback_data);
      return;
    }
  }
  else {
    /* TODO: raise event for scoreboard (if retry_count < 2) */

    /* not currently doing anything with the response */
    if (lboard_data->retry_count) {
      RC_CLIENT_LOG_INFO_FORMATTED(lboard_data->client, "Leaderboard %u submission %d completed after %u attempts",
          lboard_data->id, lboard_data->score, lboard_data->retry_count);
    }
  }

  if (lboard_data->scheduled_callback_data)
    free(lboard_data->scheduled_callback_data);
  free(lboard_data);
}

static void rc_client_submit_leaderboard_entry_server_call(rc_client_submit_leaderboard_entry_callback_data_t* lboard_data)
{
  rc_api_submit_lboard_entry_request_t api_params;
  rc_api_request_t request;
  int result;

  memset(&api_params, 0, sizeof(api_params));
  api_params.username = lboard_data->client->user.username;
  api_params.api_token = lboard_data->client->user.token;
  api_params.leaderboard_id = lboard_data->id;
  api_params.score = lboard_data->score;
  api_params.game_hash = lboard_data->game_hash;

  result = rc_api_init_submit_lboard_entry_request(&request, &api_params);
  if (result != RC_OK) {
    RC_CLIENT_LOG_ERR_FORMATTED(lboard_data->client, "Error constructing submit leaderboard entry for leaderboard %u: %s", lboard_data->id, rc_error_str(result));
    return;
  }

  lboard_data->client->callbacks.server_call(&request, rc_client_submit_leaderboard_entry_callback, lboard_data, lboard_data->client);

  rc_api_destroy_request(&request);
}

static void rc_client_submit_leaderboard_entry(rc_client_t* client, rc_client_leaderboard_info_t* leaderboard)
{
  rc_client_submit_leaderboard_entry_callback_data_t* callback_data;

  /* don't actually submit leaderboard entries when spectating */
  if (client->state.spectator_mode != RC_CLIENT_SPECTATOR_MODE_OFF) {
    RC_CLIENT_LOG_INFO_FORMATTED(client, "Spectated %s (%d) for leaderboard %u: %s",
        leaderboard->public.tracker_value, leaderboard->value, leaderboard->public.id, leaderboard->public.title);
    return;
  }

  callback_data = (rc_client_submit_leaderboard_entry_callback_data_t*)calloc(1, sizeof(*callback_data));
  if (!callback_data) {
    RC_CLIENT_LOG_ERR_FORMATTED(client, "Failed to allocate callback data for submitting entry for leaderboard %u", leaderboard->public.id);
    rc_client_raise_server_error_event(client, "submit_lboard_entry", rc_error_str(RC_OUT_OF_MEMORY));
    return;
  }
  callback_data->client = client;
  callback_data->id = leaderboard->public.id;
  callback_data->score = leaderboard->value;
  callback_data->game_hash = client->game->public.hash;
  callback_data->submit_time = time(NULL);

  RC_CLIENT_LOG_INFO_FORMATTED(client, "Submitting %s (%d) for leaderboard %u: %s",
      leaderboard->public.tracker_value, leaderboard->value, leaderboard->public.id, leaderboard->public.title);
  rc_client_submit_leaderboard_entry_server_call(callback_data);
}

static void rc_client_subset_reset_leaderboards(rc_client_game_info_t* game, rc_client_subset_info_t* subset)
{
  rc_client_leaderboard_info_t* leaderboard = subset->leaderboards;
  rc_client_leaderboard_info_t* stop = leaderboard + subset->public.num_leaderboards;

  for (; leaderboard < stop; ++leaderboard) {
    rc_lboard_t* lboard = leaderboard->lboard;
    if (!lboard)
      continue;

    switch (leaderboard->public.state) {
      case RC_CLIENT_LEADERBOARD_STATE_INACTIVE:
      case RC_CLIENT_LEADERBOARD_STATE_DISABLED:
        continue;

      case RC_CLIENT_LEADERBOARD_STATE_TRACKING:
        rc_client_release_leaderboard_tracker(game, leaderboard);
        /* fallthrough to default */
      default:
        leaderboard->public.state = RC_CLIENT_LEADERBOARD_STATE_ACTIVE;
        rc_reset_lboard(lboard);
        break;
    }
  }
}

static void rc_client_reset_leaderboards(rc_client_t* client)
{
  rc_client_subset_info_t* subset;
  for (subset = client->game->subsets; subset; subset = subset->next)
    rc_client_subset_reset_leaderboards(client->game, subset);
}

typedef struct rc_client_fetch_leaderboard_entries_callback_data_t {
  rc_client_t* client;
  rc_client_fetch_leaderboard_entries_callback_t callback;
  void* callback_userdata;
  uint32_t leaderboard_id;
  rc_client_async_handle_t async_handle;
} rc_client_fetch_leaderboard_entries_callback_data_t;

static void rc_client_fetch_leaderboard_entries_callback(const rc_api_server_response_t* server_response, void* callback_data)
{
  rc_client_fetch_leaderboard_entries_callback_data_t* lbinfo_callback_data = (rc_client_fetch_leaderboard_entries_callback_data_t*)callback_data;
  rc_client_t* client = lbinfo_callback_data->client;
  rc_api_fetch_leaderboard_info_response_t lbinfo_response;
  const char* error_message;
  int result;

  if (rc_client_async_handle_aborted(client, &lbinfo_callback_data->async_handle)) {
    RC_CLIENT_LOG_VERBOSE(client, "Fetch leaderbord entries aborted");
    free(lbinfo_callback_data);
    return;
  }

  result = rc_api_process_fetch_leaderboard_info_response(&lbinfo_response, server_response->body);
  error_message = rc_client_server_error_message(&result, server_response->http_status_code, &lbinfo_response.response);
  if (error_message) {
    RC_CLIENT_LOG_ERR_FORMATTED(client, "Fetch leaderboard %u info failed: %s", lbinfo_callback_data->leaderboard_id, error_message);
    lbinfo_callback_data->callback(result, error_message, NULL, client, lbinfo_callback_data->callback_userdata);
  }
  else {
    rc_client_leaderboard_entry_list_t* list;
    const size_t list_size = sizeof(*list) + sizeof(rc_client_leaderboard_entry_t) * lbinfo_response.num_entries;
    size_t needed_size = list_size;
    unsigned i;

    for (i = 0; i < lbinfo_response.num_entries; i++)
      needed_size += strlen(lbinfo_response.entries[i].username) + 1;

    list = (rc_client_leaderboard_entry_list_t*)malloc(needed_size);
    if (!list) {
      lbinfo_callback_data->callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), NULL, client, lbinfo_callback_data->callback_userdata);
    }
    else {
      rc_client_leaderboard_entry_t* entry = list->entries = (rc_client_leaderboard_entry_t*)((uint8_t*)list + sizeof(*list));
      char* user = (char*)((uint8_t*)list + list_size);
      const rc_api_lboard_info_entry_t* lbentry = lbinfo_response.entries;
      const rc_api_lboard_info_entry_t* stop = lbentry + lbinfo_response.num_entries;
      const size_t logged_in_user_len = strlen(client->user.display_name) + 1;
      list->user_index = -1;

      for (; lbentry < stop; ++lbentry, ++entry) {
        const size_t len = strlen(lbentry->username) + 1;
        entry->user = user;
        memcpy(user, lbentry->username, len);
        user += len;

        if (len == logged_in_user_len && memcmp(entry->user, client->user.display_name, len) == 0)
          list->user_index = (int)(entry - list->entries);

        entry->index = lbentry->index;
        entry->rank = lbentry->rank;
        entry->submitted = lbentry->submitted;

        rc_format_value(entry->display, sizeof(entry->display), lbentry->score, lbinfo_response.format);
      }

      list->num_entries = lbinfo_response.num_entries;

      lbinfo_callback_data->callback(RC_OK, NULL, list, client, lbinfo_callback_data->callback_userdata);
    }
  }

  rc_api_destroy_fetch_leaderboard_info_response(&lbinfo_response);
  free(lbinfo_callback_data);
}

static rc_client_async_handle_t* rc_client_begin_fetch_leaderboard_info(rc_client_t* client,
    const rc_api_fetch_leaderboard_info_request_t* lbinfo_request,
    rc_client_fetch_leaderboard_entries_callback_t callback, void* callback_userdata)
{
  rc_client_fetch_leaderboard_entries_callback_data_t* callback_data;
  rc_api_request_t request;
  int result;
  const char* error_message;

  result = rc_api_init_fetch_leaderboard_info_request(&request, lbinfo_request);

  if (result != RC_OK) {
    error_message = rc_error_str(result);
    callback(result, error_message, NULL, client, callback_userdata);
    return NULL;
  }

  callback_data = (rc_client_fetch_leaderboard_entries_callback_data_t*)calloc(1, sizeof(*callback_data));
  if (!callback_data) {
    callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), NULL, client, callback_userdata);
    return NULL;
  }

  callback_data->client = client;
  callback_data->callback = callback;
  callback_data->callback_userdata = callback_userdata;
  callback_data->leaderboard_id = lbinfo_request->leaderboard_id;

  client->callbacks.server_call(&request, rc_client_fetch_leaderboard_entries_callback, callback_data, client);
  rc_api_destroy_request(&request);

  return &callback_data->async_handle;
}

rc_client_async_handle_t* rc_client_begin_fetch_leaderboard_entries(rc_client_t* client, uint32_t leaderboard_id,
    uint32_t first_entry, uint32_t count, rc_client_fetch_leaderboard_entries_callback_t callback, void* callback_userdata)
{
  rc_api_fetch_leaderboard_info_request_t lbinfo_request;

  memset(&lbinfo_request, 0, sizeof(lbinfo_request));
  lbinfo_request.leaderboard_id = leaderboard_id;
  lbinfo_request.first_entry = first_entry;
  lbinfo_request.count = count;

  return rc_client_begin_fetch_leaderboard_info(client, &lbinfo_request, callback, callback_userdata);
}

rc_client_async_handle_t* rc_client_begin_fetch_leaderboard_entries_around_user(rc_client_t* client, uint32_t leaderboard_id,
  uint32_t count, rc_client_fetch_leaderboard_entries_callback_t callback, void* callback_userdata)
{
  rc_api_fetch_leaderboard_info_request_t lbinfo_request;

  memset(&lbinfo_request, 0, sizeof(lbinfo_request));
  lbinfo_request.leaderboard_id = leaderboard_id;
  lbinfo_request.username = client->user.username;
  lbinfo_request.count = count;

  if (!lbinfo_request.username) {
    callback(RC_LOGIN_REQUIRED, rc_error_str(RC_LOGIN_REQUIRED), NULL, client, callback_userdata);
    return NULL;
  }

  return rc_client_begin_fetch_leaderboard_info(client, &lbinfo_request, callback, callback_userdata);
}

void rc_client_destroy_leaderboard_entry_list(rc_client_leaderboard_entry_list_t* list)
{
  if (list)
    free(list);
}

int rc_client_leaderboard_entry_get_user_image_url(const rc_client_leaderboard_entry_t* entry, char buffer[], size_t buffer_size)
{
  if (!entry)
    return RC_INVALID_STATE;

  return rc_client_get_image_url(buffer, buffer_size, RC_IMAGE_TYPE_USER, entry->user);
}

/* ===== Rich Presence ===== */

static void rc_client_ping_callback(const rc_api_server_response_t* server_response, void* callback_data)
{
  rc_client_t* client = (rc_client_t*)callback_data;
  rc_api_ping_response_t response;

  int result = rc_api_process_ping_response(&response, server_response->body);
  const char* error_message = rc_client_server_error_message(&result, server_response->http_status_code, &response.response);
  if (error_message) {
    RC_CLIENT_LOG_WARN_FORMATTED(client, "Ping response error: %s", error_message);
  }

  rc_api_destroy_ping_response(&response);
}

static void rc_client_ping(rc_client_scheduled_callback_data_t* callback_data, rc_client_t* client, time_t now)
{
  rc_api_ping_request_t api_params;
  rc_api_request_t request;
  char buffer[256];
  int result;

  rc_runtime_get_richpresence(&client->game->runtime, buffer, sizeof(buffer),
      client->state.legacy_peek, client, NULL);

  memset(&api_params, 0, sizeof(api_params));
  api_params.username = client->user.username;
  api_params.api_token = client->user.token;
  api_params.game_id = client->game->public.id;
  api_params.rich_presence = buffer;

  result = rc_api_init_ping_request(&request, &api_params);
  if (result != RC_OK) {
    RC_CLIENT_LOG_WARN_FORMATTED(client, "Error generating ping request: %s", rc_error_str(result));
  }
  else {
    client->callbacks.server_call(&request, rc_client_ping_callback, client, client);
  }

  callback_data->when = now + 120;
  rc_client_schedule_callback(client, callback_data);
}

size_t rc_client_get_rich_presence_message(rc_client_t* client, char buffer[], size_t buffer_size)
{
  int result;

  if (!client || !client->game || !buffer)
    return 0;

  result = rc_runtime_get_richpresence(&client->game->runtime, buffer, (unsigned)buffer_size,
      client->state.legacy_peek, client, NULL);

  if (result == 0)
    result = snprintf(buffer, buffer_size, "Playing %s", client->game->public.title);

  return result;
}

/* ===== Processing ===== */

void rc_client_set_event_handler(rc_client_t* client, rc_client_event_handler_t handler)
{
  if (client)
    client->callbacks.event_handler = handler;
}

void rc_client_set_read_memory_function(rc_client_t* client, rc_client_read_memory_func_t handler)
{
  if (client)
    client->callbacks.read_memory = handler;
}

static void rc_client_invalidate_processing_memref(rc_client_t* client)
{
  rc_memref_t** next_memref = &client->game->runtime.memrefs;
  rc_memref_t* memref;

  /* if processing_memref is not set, this occurred following a pointer chain. ignore it. */
  if (!client->state.processing_memref)
    return;

  /* invalid memref. remove from chain so we don't have to evaluate it in the future.
   * it's still there, so anything referencing it will always fetch the current value. */
  while ((memref = *next_memref) != NULL) {
    if (memref == client->state.processing_memref) {
      *next_memref = memref->next;
      break;
    }
    next_memref = &memref->next;
  }

  rc_client_invalidate_memref_achievements(client->game, client, client->state.processing_memref);
  rc_client_invalidate_memref_leaderboards(client->game, client, client->state.processing_memref);

  client->state.processing_memref = NULL;
}

static unsigned rc_client_peek_le(unsigned address, unsigned num_bytes, void* ud)
{
  rc_client_t* client = (rc_client_t*)ud;
  unsigned value = 0;
  uint32_t num_read = 0;

  /* if we know the address is out of range, and it's part of a pointer chain
   * (processing_memref is null), don't bother processing it. */
  if (address > client->game->max_valid_address && !client->state.processing_memref)
    return 0;

  if (num_bytes <= sizeof(value)) {
    num_read = client->callbacks.read_memory(address, (uint8_t*)&value, num_bytes, client);
    if (num_read == num_bytes)
      return value;
  }

  if (num_read < num_bytes)
    rc_client_invalidate_processing_memref(client);

  return 0;
}

static unsigned rc_client_peek(unsigned address, unsigned num_bytes, void* ud)
{
  rc_client_t* client = (rc_client_t*)ud;
  uint8_t buffer[4];
  uint32_t num_read = 0;

  /* if we know the address is out of range, and it's part of a pointer chain
   * (processing_memref is null), don't bother processing it. */
  if (address > client->game->max_valid_address && !client->state.processing_memref)
    return 0;

  switch (num_bytes) {
    case 1:
      num_read = client->callbacks.read_memory(address, buffer, 1, client);
      if (num_read == 1)
        return buffer[0];
      break;
    case 2:
      num_read = client->callbacks.read_memory(address, buffer, 2, client);
      if (num_read == 2)
        return buffer[0] | (buffer[1] << 8);
      break;
    case 3:
      num_read = client->callbacks.read_memory(address, buffer, 3, client);
      if (num_read == 3)
        return buffer[0] | (buffer[1] << 8) | (buffer[2] << 16);
      break;
    case 4:
      num_read = client->callbacks.read_memory(address, buffer, 4, client);
      if (num_read == 4)
        return buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);
      break;
    default:
      break;
  }

  if (num_read < num_bytes)
    rc_client_invalidate_processing_memref(client);

  return 0;
}

void rc_client_set_legacy_peek(rc_client_t* client, int method)
{
  if (method == RC_CLIENT_LEGACY_PEEK_AUTO) {
    uint8_t buffer[4] = { 1,0,0,0 };
    method = (*((uint32_t*)buffer) == 1) ?
        RC_CLIENT_LEGACY_PEEK_LITTLE_ENDIAN_READS : RC_CLIENT_LEGACY_PEEK_CONSTRUCTED;
  }

  client->state.legacy_peek = (method == RC_CLIENT_LEGACY_PEEK_LITTLE_ENDIAN_READS) ?
      rc_client_peek_le : rc_client_peek;
}

int rc_client_is_processing_required(rc_client_t* client)
{
  if (!client || !client->game)
    return 0;

  if (client->game->runtime.trigger_count || client->game->runtime.lboard_count)
    return 1;

  return (client->game->runtime.richpresence && client->game->runtime.richpresence->richpresence);
}

static void rc_client_update_memref_values(rc_client_t* client)
{
  rc_memref_t* memref = client->game->runtime.memrefs;
  unsigned value;
  int invalidated_memref = 0;

  for (; memref; memref = memref->next) {
    if (memref->value.is_indirect)
      continue;

    client->state.processing_memref = memref;

    value = rc_peek_value(memref->address, memref->value.size, client->state.legacy_peek, client);

    if (client->state.processing_memref) {
      rc_update_memref_value(&memref->value, value);
    }
    else {
      /* if the peek function cleared the processing_memref, the memref was invalidated */
      invalidated_memref = 1;
    }
  }

  client->state.processing_memref = NULL;

  if (invalidated_memref)
    rc_client_update_active_achievements(client->game);
}

static void rc_client_do_frame_process_achievements(rc_client_t* client, rc_client_subset_info_t* subset)
{
  rc_client_achievement_info_t* achievement = subset->achievements;
  rc_client_achievement_info_t* stop = achievement + subset->public.num_achievements;
  float best_progress = 0.0;
  rc_client_achievement_info_t* best_progress_achievement = NULL;

  for (; achievement < stop; ++achievement) {
    rc_trigger_t* trigger = achievement->trigger;
    int old_state, new_state;
    unsigned old_measured_value;

    if (!trigger || achievement->public.state != RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE)
      continue;

    old_measured_value = trigger->measured_value;
    old_state = trigger->state;
    new_state = rc_evaluate_trigger(trigger, client->state.legacy_peek, client, NULL);

    /* if the measured value changed and the achievement hasn't triggered, show a progress indicator */
    if (trigger->measured_value != old_measured_value && old_measured_value != RC_MEASURED_UNKNOWN &&
        trigger->measured_value <= trigger->measured_target &&
        rc_trigger_state_active(new_state) && new_state != RC_TRIGGER_STATE_WAITING) {

      /* only show a popup for the achievement closest to triggering */
      float progress = (float)trigger->measured_value / (float)trigger->measured_target;

      if (trigger->measured_as_percent) {
        /* if reporting the measured value as a percentage, only show the popup if the percentage changes */
        const unsigned old_percent = (unsigned)(((unsigned long long)old_measured_value * 100) / trigger->measured_target);
        const unsigned new_percent = (unsigned)(((unsigned long long)trigger->measured_value * 100) / trigger->measured_target);
        if (old_percent == new_percent)
          progress = -1.0;
      }

      if (progress > best_progress) {
        best_progress = progress;

        if (best_progress_achievement)
          best_progress_achievement->pending_events &= ~RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_PROGRESS_INDICATOR_SHOW;

        achievement->pending_events |= RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_PROGRESS_INDICATOR_SHOW;
        subset->pending_events |= RC_CLIENT_SUBSET_PENDING_EVENT_ACHIEVEMENT;
        best_progress_achievement = achievement;
      }
    }

    /* if the state hasn't changed, there won't be any events raised */
    if (new_state == old_state)
      continue;

    /* raise a CHALLENGE_INDICATOR_HIDE event when changing from PRIMED to anything else */
    if (old_state == RC_TRIGGER_STATE_PRIMED)
      achievement->pending_events |= RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_HIDE;

    /* raise events for each of the possible new states */
    if (new_state == RC_TRIGGER_STATE_TRIGGERED)
      achievement->pending_events |= RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_TRIGGERED;
    else if (new_state == RC_TRIGGER_STATE_PRIMED)
      achievement->pending_events |= RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_SHOW;

    subset->pending_events |= RC_CLIENT_SUBSET_PENDING_EVENT_ACHIEVEMENT;
  }
}

static void rc_client_raise_achievement_events(rc_client_t* client, rc_client_subset_info_t* subset)
{
  rc_client_achievement_info_t* achievement = subset->achievements;
  rc_client_achievement_info_t* stop = achievement + subset->public.num_achievements;
  rc_client_event_t client_event;
  time_t recent_unlock_time = 0;

  memset(&client_event, 0, sizeof(client_event));

  for (; achievement < stop; ++achievement) {
    if (achievement->pending_events == RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_NONE)
      continue;

    /* kick off award achievement request first */
    if (achievement->pending_events & RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_TRIGGERED) {
      rc_client_award_achievement(client, achievement);
      client->game->pending_events |= RC_CLIENT_GAME_PENDING_EVENT_UPDATE_ACTIVE_ACHIEVEMENTS;
    }

    /* update display state */
    if (recent_unlock_time == 0)
      recent_unlock_time = time(NULL) - RC_CLIENT_RECENT_UNLOCK_DELAY_SECONDS;
    rc_client_update_achievement_display_information(client, achievement, recent_unlock_time);

    /* raise events*/
    client_event.achievement = &achievement->public;

    if (achievement->pending_events & RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_HIDE) {
      client_event.type = RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE;
      client->callbacks.event_handler(&client_event, client);
    }
    else if (achievement->pending_events & RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_SHOW) {
      client_event.type = RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW;
      client->callbacks.event_handler(&client_event, client);
    }

    if (achievement->pending_events & RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_PROGRESS_INDICATOR_SHOW) {
      client_event.type = RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW;
      client->callbacks.event_handler(&client_event, client);
    }

    if (achievement->pending_events & RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_TRIGGERED) {
      client_event.type = RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED;
      client->callbacks.event_handler(&client_event, client);
    }

    /* clear pending flags */
    achievement->pending_events = RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_NONE;
  }
}

static void rc_client_raise_mastery_event(rc_client_t* client, rc_client_subset_info_t* subset)
{
  rc_client_event_t client_event;

  memset(&client_event, 0, sizeof(client_event));
  client_event.type = RC_CLIENT_EVENT_GAME_COMPLETED;

  subset->mastery = RC_CLIENT_MASTERY_STATE_SHOWN;

  client->callbacks.event_handler(&client_event, client);
}

static void rc_client_do_frame_process_leaderboards(rc_client_t* client, rc_client_subset_info_t* subset)
{
  rc_client_leaderboard_info_t* leaderboard = subset->leaderboards;
  rc_client_leaderboard_info_t* stop = leaderboard + subset->public.num_leaderboards;

  for (; leaderboard < stop; ++leaderboard) {
    rc_lboard_t* lboard = leaderboard->lboard;
    int old_state, new_state;

    switch (leaderboard->public.state) {
      case RC_CLIENT_LEADERBOARD_STATE_INACTIVE:
      case RC_CLIENT_LEADERBOARD_STATE_DISABLED:
        continue;

      default:
        if (!lboard)
          continue;

        break;
    }

    old_state = lboard->state;
    new_state = rc_evaluate_lboard(lboard, &leaderboard->value, client->state.legacy_peek, client, NULL);

    switch (new_state) {
      case RC_LBOARD_STATE_STARTED: /* leaderboard is running */
        if (old_state != RC_LBOARD_STATE_STARTED) {
          leaderboard->public.state = RC_CLIENT_LEADERBOARD_STATE_TRACKING;
          leaderboard->pending_events |= RC_CLIENT_LEADERBOARD_PENDING_EVENT_STARTED;
          rc_client_allocate_leaderboard_tracker(client->game, leaderboard);
        }
        else {
          rc_client_update_leaderboard_tracker(client->game, leaderboard);
        }
        break;

      case RC_LBOARD_STATE_CANCELED:
        if (old_state != RC_LBOARD_STATE_CANCELED) {
          leaderboard->public.state = RC_CLIENT_LEADERBOARD_STATE_ACTIVE;
          leaderboard->pending_events |= RC_CLIENT_LEADERBOARD_PENDING_EVENT_FAILED;
          rc_client_release_leaderboard_tracker(client->game, leaderboard);
        }
        break;

      case RC_LBOARD_STATE_TRIGGERED:
        if (old_state != RC_RUNTIME_EVENT_LBOARD_TRIGGERED) {
          leaderboard->public.state = RC_CLIENT_LEADERBOARD_STATE_ACTIVE;
          leaderboard->pending_events |= RC_CLIENT_LEADERBOARD_PENDING_EVENT_SUBMITTED;
          rc_client_release_leaderboard_tracker(client->game, leaderboard);
        }
        break;
    }

    if (leaderboard->pending_events)
      subset->pending_events |= RC_CLIENT_SUBSET_PENDING_EVENT_LEADERBOARD;
  }
}

static void rc_client_raise_leaderboard_tracker_events(rc_client_t* client)
{
  rc_client_leaderboard_tracker_info_t* tracker = client->game->leaderboard_trackers;
  rc_client_event_t client_event;

  memset(&client_event, 0, sizeof(client_event));

  tracker = client->game->leaderboard_trackers;
  for (; tracker; tracker = tracker->next) {
    if (tracker->pending_events == RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_NONE)
      continue;

    client_event.leaderboard_tracker = &tracker->public;

    if (tracker->pending_events & RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_HIDE) {
      client_event.type = RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE;
      client->callbacks.event_handler(&client_event, client);
    }
    else {
      rc_format_value(tracker->public.display, sizeof(tracker->public.display), tracker->raw_value, tracker->format);

      if (tracker->pending_events & RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_SHOW) {
        client_event.type = RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW;
        client->callbacks.event_handler(&client_event, client);
      }
      else if (tracker->pending_events & RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_UPDATE) {
        client_event.type = RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE;
        client->callbacks.event_handler(&client_event, client);
      }
    }

    tracker->pending_events = RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_NONE;
  }
}

static void rc_client_raise_leaderboard_events(rc_client_t* client, rc_client_subset_info_t* subset)
{
  rc_client_leaderboard_info_t* leaderboard = subset->leaderboards;
  rc_client_leaderboard_info_t* leaderboard_stop = leaderboard + subset->public.num_leaderboards;
  rc_client_event_t client_event;

  memset(&client_event, 0, sizeof(client_event));

  for (; leaderboard < leaderboard_stop; ++leaderboard) {
    if (leaderboard->pending_events == RC_CLIENT_LEADERBOARD_PENDING_EVENT_NONE)
      continue;

    client_event.leaderboard = &leaderboard->public;

    if (leaderboard->pending_events & RC_CLIENT_LEADERBOARD_PENDING_EVENT_FAILED) {
      RC_CLIENT_LOG_VERBOSE_FORMATTED(client, "Leaderboard %u canceled: %s", leaderboard->public.id, leaderboard->public.title);
      client_event.type = RC_CLIENT_EVENT_LEADERBOARD_FAILED;
      client->callbacks.event_handler(&client_event, client);
    }
    else if (leaderboard->pending_events & RC_CLIENT_LEADERBOARD_PENDING_EVENT_SUBMITTED) {
      /* kick off submission request before raising event */
      rc_client_submit_leaderboard_entry(client, leaderboard);

      client_event.type = RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED;
      client->callbacks.event_handler(&client_event, client);
    }
    else if (leaderboard->pending_events & RC_CLIENT_LEADERBOARD_PENDING_EVENT_STARTED) {
      RC_CLIENT_LOG_VERBOSE_FORMATTED(client, "Leaderboard %u started: %s", leaderboard->public.id, leaderboard->public.title);
      client_event.type = RC_CLIENT_EVENT_LEADERBOARD_STARTED;
      client->callbacks.event_handler(&client_event, client);
    }

    leaderboard->pending_events = RC_CLIENT_LEADERBOARD_PENDING_EVENT_NONE;
  }
}

static void rc_client_reset_pending_events(rc_client_t* client)
{
  rc_client_subset_info_t* subset;

  client->game->pending_events = RC_CLIENT_GAME_PENDING_EVENT_NONE;

  for (subset = client->game->subsets; subset; subset = subset->next)
    subset->pending_events = RC_CLIENT_SUBSET_PENDING_EVENT_NONE;
}

static void rc_client_subset_raise_pending_events(rc_client_t* client, rc_client_subset_info_t* subset)
{
  /* raise any pending achievement events */
  if (subset->pending_events & RC_CLIENT_SUBSET_PENDING_EVENT_ACHIEVEMENT)
    rc_client_raise_achievement_events(client, subset);

  /* raise any pending leaderboard events */
  if (subset->pending_events & RC_CLIENT_SUBSET_PENDING_EVENT_LEADERBOARD)
    rc_client_raise_leaderboard_events(client, subset);

  /* raise mastery event if pending */
  if (subset->mastery == RC_CLIENT_MASTERY_STATE_PENDING)
    rc_client_raise_mastery_event(client, subset);
}

static void rc_client_raise_pending_events(rc_client_t* client)
{
  rc_client_subset_info_t* subset;

  /* raise tracker events before leaderboard events so formatted values are updated for leaderboard events */
  if (client->game->pending_events & RC_CLIENT_GAME_PENDING_EVENT_LEADERBOARD_TRACKER)
    rc_client_raise_leaderboard_tracker_events(client);

  for (subset = client->game->subsets; subset; subset = subset->next)
    rc_client_subset_raise_pending_events(client, subset);

  /* if any achievements were unlocked, resync the active achievements list */
  if (client->game->pending_events & RC_CLIENT_GAME_PENDING_EVENT_UPDATE_ACTIVE_ACHIEVEMENTS) {
    rc_mutex_lock(&client->state.mutex);
    rc_client_update_active_achievements(client->game);
    rc_mutex_unlock(&client->state.mutex);
  }
}

void rc_client_do_frame(rc_client_t* client)
{
  if (!client)
    return;

  if (client->game && !client->game->waiting_for_reset) {
    rc_runtime_richpresence_t* richpresence;
    rc_client_subset_info_t* subset;

    rc_mutex_lock(&client->state.mutex);

    rc_client_reset_pending_events(client);

    rc_client_update_memref_values(client);
    rc_update_variables(client->game->runtime.variables, client->state.legacy_peek, client, NULL);

    for (subset = client->game->subsets; subset; subset = subset->next) {
      if (subset->active)
        rc_client_do_frame_process_achievements(client, subset);
    }

    if (client->state.hardcore) {
      for (subset = client->game->subsets; subset; subset = subset->next) {
        if (subset->active)
          rc_client_do_frame_process_leaderboards(client, subset);
      }
    }

    richpresence = client->game->runtime.richpresence;
    if (richpresence && richpresence->richpresence)
      rc_update_richpresence(richpresence->richpresence, client->state.legacy_peek, client, NULL);

    rc_mutex_unlock(&client->state.mutex);

    rc_client_raise_pending_events(client);
  }

  rc_client_idle(client);
}

void rc_client_idle(rc_client_t* client)
{
  rc_client_scheduled_callback_data_t* scheduled_callback;

  if (!client)
    return;

  scheduled_callback = client->state.scheduled_callbacks;
  if (scheduled_callback) {
    const time_t now = time(NULL);

    do {
      rc_mutex_lock(&client->state.mutex);
      scheduled_callback = client->state.scheduled_callbacks;
      if (scheduled_callback) {
        if (scheduled_callback->when > now) {
          /* not time for next callback yet, ignore it */
          scheduled_callback = NULL;
        }
        else {
          /* remove the callback from the queue while we process it. callback can requeue if desired */
          client->state.scheduled_callbacks = scheduled_callback->next;
        }
      }
      rc_mutex_unlock(&client->state.mutex);

      if (!scheduled_callback)
        break;

      scheduled_callback->callback(scheduled_callback, client, now);
    } while (1);
  }
}

void rc_client_schedule_callback(rc_client_t* client, rc_client_scheduled_callback_data_t* scheduled_callback)
{
  rc_client_scheduled_callback_data_t** last;
  rc_client_scheduled_callback_data_t* next;

  rc_mutex_lock(&client->state.mutex);

  last = &client->state.scheduled_callbacks;
  do {
    next = *last;
    if (next == NULL || next->when > scheduled_callback->when) {
      scheduled_callback->next = next;
      *last = scheduled_callback;
      break;
    }

    last = &next->next;
  } while (1);

  rc_mutex_unlock(&client->state.mutex);
}

static void rc_client_reset_richpresence(rc_client_t* client)
{
  rc_runtime_richpresence_t* richpresence = client->game->runtime.richpresence;
  if (richpresence && richpresence->richpresence)
    rc_reset_richpresence(richpresence->richpresence);
}

static void rc_client_reset_variables(rc_client_t* client)
{
  rc_value_t* variable = client->game->runtime.variables;
  for (; variable; variable = variable->next)
    rc_reset_value(variable);
}

static void rc_client_reset_all(rc_client_t* client)
{
  rc_client_reset_achievements(client);
  rc_client_reset_leaderboards(client);
  rc_client_reset_richpresence(client);
  rc_client_reset_variables(client);
}

void rc_client_reset(rc_client_t* client)
{
  rc_client_game_hash_t* game_hash;
  if (!client || !client->game)
    return;

  game_hash = rc_client_find_game_hash(client, client->game->public.hash);
  if (game_hash && game_hash->game_id != client->game->public.id) {
    /* current media is not for loaded game. unload game */
    RC_CLIENT_LOG_WARN_FORMATTED(client, "Disabling runtime. Reset with non-game media loaded: %u (%s)",
        (game_hash->game_id == RC_CLIENT_UNKNOWN_GAME_ID) ? 0 : game_hash->game_id, game_hash->hash);
    rc_client_unload_game(client);
    return;
  }

  RC_CLIENT_LOG_INFO(client, "Resetting runtime");

  rc_mutex_lock(&client->state.mutex);

  client->game->waiting_for_reset = 0;
  rc_client_reset_pending_events(client);

  rc_client_reset_all(client);

  rc_mutex_unlock(&client->state.mutex);

  rc_client_raise_pending_events(client);
}

size_t rc_client_progress_size(rc_client_t* client)
{
  size_t result;

  if (!client || !client->game)
    return 0;

  rc_mutex_lock(&client->state.mutex);
  result = rc_runtime_progress_size(&client->game->runtime, NULL);
  rc_mutex_unlock(&client->state.mutex);

  return result;
}

int rc_client_serialize_progress(rc_client_t* client, uint8_t* buffer)
{
  int result;

  if (!client || !client->game)
    return RC_NO_GAME_LOADED;

  if (!buffer)
    return RC_INVALID_STATE;

  rc_mutex_lock(&client->state.mutex);
  result = rc_runtime_serialize_progress(buffer, &client->game->runtime, NULL);
  rc_mutex_unlock(&client->state.mutex);

  return result;
}

static void rc_client_subset_before_deserialize_progress(rc_client_subset_info_t* subset)
{
  rc_client_achievement_info_t* achievement;
  rc_client_achievement_info_t* achievement_stop;
  rc_client_leaderboard_info_t* leaderboard;
  rc_client_leaderboard_info_t* leaderboard_stop;

  /* flag any visible challenge indicators to be hidden */
  achievement = subset->achievements;
  achievement_stop = achievement + subset->public.num_achievements;
  for (; achievement < achievement_stop; ++achievement) {
    rc_trigger_t* trigger = achievement->trigger;
    if (trigger && trigger->state == RC_TRIGGER_STATE_PRIMED &&
        achievement->public.state == RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE) {
      achievement->pending_events |= RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_HIDE;
      subset->pending_events |= RC_CLIENT_SUBSET_PENDING_EVENT_ACHIEVEMENT;
    }
  }

  /* flag any visible trackers to be hidden */
  leaderboard = subset->leaderboards;
  leaderboard_stop = leaderboard + subset->public.num_leaderboards;
  for (; leaderboard < leaderboard_stop; ++leaderboard) {
    rc_lboard_t* lboard = leaderboard->lboard;
    if (lboard && lboard->state == RC_LBOARD_STATE_STARTED &&
        leaderboard->public.state == RC_CLIENT_LEADERBOARD_STATE_TRACKING) {
      leaderboard->pending_events |= RC_CLIENT_LEADERBOARD_PENDING_EVENT_FAILED;
      subset->pending_events |= RC_CLIENT_SUBSET_PENDING_EVENT_ACHIEVEMENT;
    }
  }
}

static void rc_client_subset_after_deserialize_progress(rc_client_game_info_t* game, rc_client_subset_info_t* subset)
{
  rc_client_achievement_info_t* achievement;
  rc_client_achievement_info_t* achievement_stop;
  rc_client_leaderboard_info_t* leaderboard;
  rc_client_leaderboard_info_t* leaderboard_stop;

  /* flag any challenge indicators that should be shown */
  achievement = subset->achievements;
  achievement_stop = achievement + subset->public.num_achievements;
  for (; achievement < achievement_stop; ++achievement) {
    rc_trigger_t* trigger = achievement->trigger;
    if (!trigger || achievement->public.state != RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE)
      continue;

    if (trigger->state == RC_TRIGGER_STATE_PRIMED) {
      /* if it's already shown, just keep it. otherwise flag it to be shown */
      if (achievement->pending_events & RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_HIDE) {
        achievement->pending_events &= ~RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_HIDE;
      }
      else {
        achievement->pending_events |= RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_SHOW;
        subset->pending_events |= RC_CLIENT_SUBSET_PENDING_EVENT_ACHIEVEMENT;
      }
    }
    /* ASSERT: only active achievements are serialized, so we don't have to worry about
     *         deserialization deactiving them. */
  }

  /* flag any trackers that need to be shown */
  leaderboard = subset->leaderboards;
  leaderboard_stop = leaderboard + subset->public.num_leaderboards;
  for (; leaderboard < leaderboard_stop; ++leaderboard) {
    rc_lboard_t* lboard = leaderboard->lboard;
    if (!lboard ||
        leaderboard->public.state == RC_CLIENT_LEADERBOARD_STATE_INACTIVE ||
        leaderboard->public.state == RC_CLIENT_LEADERBOARD_STATE_DISABLED)
      continue;

    if (lboard->state == RC_LBOARD_STATE_STARTED) {
      leaderboard->value = (int)lboard->value.value.value;
      leaderboard->public.state = RC_CLIENT_LEADERBOARD_STATE_TRACKING;

      /* if it's already being tracked, just update tracker. otherwise, allocate one */
      if (leaderboard->pending_events & RC_CLIENT_LEADERBOARD_PENDING_EVENT_FAILED) {
        leaderboard->pending_events &= ~RC_CLIENT_LEADERBOARD_PENDING_EVENT_FAILED;
        rc_client_update_leaderboard_tracker(game, leaderboard);
      }
      else {
        rc_client_allocate_leaderboard_tracker(game, leaderboard);
      }
    }
    else if (leaderboard->pending_events & RC_CLIENT_LEADERBOARD_PENDING_EVENT_FAILED) {
      /* deallocate the tracker (don't actually raise the failed event) */
      leaderboard->pending_events &= ~RC_CLIENT_LEADERBOARD_PENDING_EVENT_FAILED;
      leaderboard->public.state = RC_CLIENT_LEADERBOARD_STATE_ACTIVE;
      rc_client_release_leaderboard_tracker(game, leaderboard);
    }
  }
}

int rc_client_deserialize_progress(rc_client_t* client, const uint8_t* serialized)
{
  rc_client_subset_info_t* subset;
  int result;

  if (!client || !client->game)
    return RC_NO_GAME_LOADED;

  rc_mutex_lock(&client->state.mutex);

  rc_client_reset_pending_events(client);

  for (subset = client->game->subsets; subset; subset = subset->next)
    rc_client_subset_before_deserialize_progress(subset);

  if (!serialized) {
    rc_client_reset_all(client);
    result = RC_OK;
  }
  else {
    result = rc_runtime_deserialize_progress(&client->game->runtime, serialized, NULL);
  }

  for (subset = client->game->subsets; subset; subset = subset->next)
    rc_client_subset_after_deserialize_progress(client->game, subset);

  rc_mutex_unlock(&client->state.mutex);

  rc_client_raise_pending_events(client);

  return result;
}

/* ===== Toggles ===== */

static void rc_client_enable_hardcore(rc_client_t* client)
{
  client->state.hardcore = 1;

  if (client->game) {
    rc_client_toggle_hardcore_achievements(client->game, client, RC_CLIENT_ACHIEVEMENT_UNLOCKED_HARDCORE);
    rc_client_activate_leaderboards(client->game, client);

    /* disable processing until the client acknowledges the reset event by calling rc_runtime_reset() */
    RC_CLIENT_LOG_INFO(client, "Hardcore enabled, waiting for reset");
    client->game->waiting_for_reset = 1;
  }
  else {
    RC_CLIENT_LOG_INFO(client, "Hardcore enabled");
  }
}

static void rc_client_disable_hardcore(rc_client_t* client)
{
  client->state.hardcore = 0;
  RC_CLIENT_LOG_INFO(client, "Hardcore disabled");

  if (client->game) {
    rc_client_toggle_hardcore_achievements(client->game, client, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    rc_client_deactivate_leaderboards(client->game, client);
  }
}

void rc_client_set_hardcore_enabled(rc_client_t* client, int enabled)
{
  int changed = 0;

  if (!client)
    return;

  rc_mutex_lock(&client->state.mutex);

  enabled = enabled ? 1 : 0;
  if (client->state.hardcore != enabled) {
    if (enabled)
      rc_client_enable_hardcore(client);
    else
      rc_client_disable_hardcore(client);

    changed = 1;
  }

  rc_mutex_unlock(&client->state.mutex);

  /* events must be raised outside of lock */
  if (changed && client->game) {
    if (enabled) {
      /* if enabling hardcore, notify client that a reset is requested */
      if (client->game->waiting_for_reset) {
        rc_client_event_t client_event;
        memset(&client_event, 0, sizeof(client_event));
        client_event.type = RC_CLIENT_EVENT_RESET;
        client->callbacks.event_handler(&client_event, client);
      }
    }
    else {
      /* if disabling hardcore, leaderboards will be deactivated. raise events for hiding trackers */
      rc_client_raise_pending_events(client);
    }
  }
}

int rc_client_get_hardcore_enabled(const rc_client_t* client)
{
  return client && client->state.hardcore;
}

void rc_client_set_unofficial_enabled(rc_client_t* client, int enabled)
{
  if (client) {
    RC_CLIENT_LOG_INFO_FORMATTED(client, "Unofficial %s", enabled ? "enabled" : "disabled");
    client->state.unofficial_enabled = enabled ? 1 : 0;
  }
}

int rc_client_get_unofficial_enabled(const rc_client_t* client)
{
  return client && client->state.unofficial_enabled;
}

void rc_client_set_encore_mode_enabled(rc_client_t* client, int enabled)
{
  if (client) {
    RC_CLIENT_LOG_INFO_FORMATTED(client, "Encore mode %s", enabled ? "enabled" : "disabled");
    client->state.encore_mode = enabled ? 1 : 0;
  }
}

int rc_client_get_encore_mode_enabled(const rc_client_t* client)
{
  return client && client->state.encore_mode;
}

void rc_client_set_spectator_mode_enabled(rc_client_t* client, int enabled)
{
  if (client) {
    if (!enabled && client->state.spectator_mode == RC_CLIENT_SPECTATOR_MODE_LOCKED) {
      RC_CLIENT_LOG_WARN(client, "Spectator mode cannot be disabled if it was enabled prior to loading game.");
      return;
    }

    RC_CLIENT_LOG_INFO_FORMATTED(client, "Spectator mode %s", enabled ? "enabled" : "disabled");
    client->state.spectator_mode = enabled ? RC_CLIENT_SPECTATOR_MODE_ON : RC_CLIENT_SPECTATOR_MODE_OFF;
  }
}

int rc_client_get_spectator_mode_enabled(const rc_client_t* client)
{
  return client && (client->state.spectator_mode == RC_CLIENT_SPECTATOR_MODE_OFF) ? 0 : 1;
}

void rc_client_set_userdata(rc_client_t* client, void* userdata)
{
  if (client)
    client->callbacks.client_data = userdata;
}

void* rc_client_get_userdata(const rc_client_t* client)
{
  return client ? client->callbacks.client_data : NULL;
}

void rc_client_set_host(const rc_client_t* client, const char* hostname)
{
  /* if empty, just pass NULL */
  if (hostname && !hostname[0])
    hostname = NULL;

  /* clear the image host so it'll use the custom host for images too */
  rc_api_set_image_host(NULL);

  /* set the custom host */
  if (hostname && client) {
    RC_CLIENT_LOG_VERBOSE_FORMATTED(client, "Using host: %s", hostname);
  }
  rc_api_set_host(hostname);
}
