#include "rc_client_internal.h"

#include "rc_api_runtime.h"
#include "rc_api_user.h"
#include "rc_consoles.h"
#include "rc_internal.h"
#include "rc_hash.h"

#include "../rapi/rc_api_common.h"

#include <stdarg.h>

#define RC_CLIENT_UNKNOWN_GAME_ID (uint32_t)-1
#define RC_CLIENT_RECENT_UNLOCK_DELAY_SECONDS (10 * 60) /* ten minutes */

typedef struct rc_client_generic_callback_data_t {
  rc_client_t* runtime;
  rc_client_callback_t callback;
} rc_client_generic_callback_data_t;

typedef struct rc_client_pending_media_t
{
  const char* file_path;
  uint8_t* data;
  size_t data_size;
  rc_client_callback_t callback;
} rc_client_pending_media_t;

typedef struct rc_client_load_state_t
{
  rc_client_t* runtime;
  rc_client_callback_t callback;

  rc_client_game_info_t* game;
  rc_client_game_hash_t* hash;

  rc_hash_iterator_t hash_iterator;
  rc_client_pending_media_t* pending_media;

  uint32_t* hardcore_unlocks;
  uint32_t* softcore_unlocks;
  uint32_t num_hardcore_unlocks;
  uint32_t num_softcore_unlocks;

  uint8_t progress;
  uint8_t outstanding_requests;
} rc_client_load_state_t;

static void rc_client_begin_fetch_game_data(rc_client_load_state_t* callback_data);
static void rc_client_load_game(rc_client_load_state_t* load_state, const char* hash, const char* file_path);
static void rc_client_ping(rc_client_scheduled_callback_data_t* callback_data, rc_client_t* runtime, time_t now);
static void rc_client_raise_leaderboard_events(rc_client_t* runtime);
static void rc_client_release_leaderboard_tracker(rc_client_game_info_t* game, rc_client_leaderboard_info_t* leaderboard);

/* ===== Construction/Destruction ===== */

static void rc_client_dummy_event_handler(const rc_client_event_t* event)
{
}

rc_client_t* rc_client_create(rc_client_read_memory_t read_memory_function, rc_client_server_call_t server_call_function)
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

  rc_buf_init(&client->buffer);

  return client;
}

void rc_client_destroy(rc_client_t* client)
{
  if (!client)
    return;

  rc_client_unload_game(client);

  rc_buf_destroy(&client->buffer);

  rc_mutex_destroy(&client->state.mutex);

  free(client);
}

/* ===== Logging ===== */

void rc_client_log_message(rc_client_t* client, const char* format, ...)
{
  char buffer[256];
  int result;
  va_list args;

  if (!client->callbacks.log_call)
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

  client->callbacks.log_call(buffer);
}

void rc_client_enable_logging(rc_client_t* runtime, int level, rc_client_message_callback_t callback)
{
  runtime->callbacks.log_call = callback;
  runtime->state.log_level = callback ? level : RC_CLIENT_LOG_LEVEL_NONE;
}

/* ===== Common ===== */

static const char* rc_client_server_error_message(int* result, int http_status_code, const rc_api_response_t* response)
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

static void rc_client_raise_server_error_event(rc_client_t* runtime, const char* api, const char* error_message)
{
  rc_client_server_error_t server_error;
  rc_client_event_t client_event;

  server_error.api = api;
  server_error.error_message = error_message;

  memset(&client_event, 0, sizeof(client_event));
  client_event.type = RC_CLIENT_EVENT_SERVER_ERROR;
  client_event.server_error = &server_error;

  runtime->callbacks.event_handler(&client_event);
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

  return result;
}

/* ===== User ===== */

static void rc_client_login_callback(const char* server_response_body, int http_status_code, void* callback_data)
{
  rc_client_generic_callback_data_t* login_callback_data = (rc_client_generic_callback_data_t*)callback_data;
  rc_client_t* runtime = login_callback_data->runtime;
  rc_api_login_response_t login_response;

  int result = rc_api_process_login_response(&login_response, server_response_body);
  const char* error_message = rc_client_server_error_message(&result, http_status_code, &login_response.response);
  if (error_message) {
    rc_mutex_lock(&runtime->state.mutex);
    runtime->state.user = RC_CLIENT_USER_STATE_NONE;
    rc_mutex_unlock(&runtime->state.mutex);

    RC_CLIENT_LOG_ERR(runtime, "Login failed: %s", error_message);
    if (login_callback_data->callback)
      login_callback_data->callback(result, error_message, runtime);
  }
  else {
    rc_client_load_state_t* load_state;
    runtime->user.username = rc_buf_strcpy(&runtime->buffer, login_response.username);

    if (strcmp(login_response.username, login_response.display_name) == 0)
      runtime->user.display_name = runtime->user.username;
    else
      runtime->user.display_name = rc_buf_strcpy(&runtime->buffer, login_response.display_name);

    runtime->user.token = rc_buf_strcpy(&runtime->buffer, login_response.api_token);
    runtime->user.score = login_response.score;
    runtime->user.score_softcore = login_response.score_softcore;
    runtime->user.num_unread_messages = login_response.num_unread_messages;

    rc_mutex_lock(&runtime->state.mutex);
    runtime->state.user = RC_CLIENT_USER_STATE_LOGGED_IN;
    load_state = runtime->state.load;
    rc_mutex_unlock(&runtime->state.mutex);

    RC_CLIENT_LOG_INFO(runtime, "Login succeeded: %s", error_message);

    if (load_state && load_state->progress == RC_CLIENT_LOAD_STATE_AWAIT_LOGIN)
      rc_client_begin_fetch_game_data(load_state);

    if (login_callback_data->callback)
      login_callback_data->callback(RC_OK, NULL, runtime);
  }

  rc_api_destroy_login_response(&login_response);
  free(login_callback_data);
}

static void rc_client_begin_login(rc_client_t* runtime,
  const rc_api_login_request_t* login_request, rc_client_callback_t callback)
{
  rc_client_generic_callback_data_t* callback_data;
  rc_api_request_t request;
  int result = rc_api_init_login_request(&request, login_request);

  if (result == RC_OK) {
    rc_mutex_lock(&runtime->state.mutex);

    if (runtime->state.user == RC_CLIENT_USER_STATE_LOGIN_REQUESTED)
      result = RC_INVALID_STATE;
    runtime->state.user = RC_CLIENT_USER_STATE_LOGIN_REQUESTED;

    rc_mutex_unlock(&runtime->state.mutex);
  }

  if (result != RC_OK) {
    callback(result, rc_error_str(result), runtime);
    return;
  }

  callback_data = (rc_client_generic_callback_data_t*)malloc(sizeof(*callback_data));
  callback_data->runtime = runtime;
  callback_data->callback = callback;

  runtime->callbacks.server_call(&request, rc_client_login_callback, callback_data, runtime);
  rc_api_destroy_request(&request);
}

void rc_client_begin_login_with_password(rc_client_t* runtime,
  const char* username, const char* password, rc_client_callback_t callback)
{
  rc_api_login_request_t login_request;
  
  if (!username || !username[0]) {
    callback(RC_INVALID_STATE, "username is required", runtime);
    return;
  }

  if (!password || !password[0]) {
    callback(RC_INVALID_STATE, "password is required", runtime);
    return;
  }

  memset(&login_request, 0, sizeof(login_request));
  login_request.username = username;
  login_request.password = password;

  RC_CLIENT_LOG_INFO(runtime, "Attempting to log in %s (with password)", username);
  rc_client_begin_login(runtime, &login_request, callback);
}

void rc_client_begin_login_with_token(rc_client_t* runtime,
  const char* username, const char* token, rc_client_callback_t callback)
{
  rc_api_login_request_t login_request;

  if (!username || !username[0]) {
    callback(RC_INVALID_STATE, "username is required", runtime);
    return;
  }

  if (!token || !token[0]) {
    callback(RC_INVALID_STATE, "token is required", runtime);
    return;
  }

  memset(&login_request, 0, sizeof(login_request));
  login_request.username = username;
  login_request.api_token = token;

  RC_CLIENT_LOG_INFO(runtime, "Attempting to log in %s (with token)", username);
  rc_client_begin_login(runtime, &login_request, callback);
}

const rc_client_user_t* rc_client_get_user_info(const rc_client_t* client)
{
  return (client->state.user == RC_CLIENT_USER_STATE_LOGGED_IN) ? &client->user : NULL;
}

int rc_client_user_get_image_url(const rc_client_user_t* user, char buffer[], size_t buffer_size)
{
  if (!user)
    return RC_INVALID_STATE;

  return rc_client_get_image_url(buffer, buffer_size, RC_IMAGE_TYPE_USER, user->display_name);
}

/* ===== Game ===== */

static void rc_client_free_game(rc_client_game_info_t* game)
{
  rc_runtime_destroy(&game->runtime);

  if (game->leaderboard_trackers)
    free(game->leaderboard_trackers);

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
  rc_mutex_lock(&load_state->runtime->state.mutex);

  load_state->progress = state;
  load_state->outstanding_requests += num_requests;

  rc_mutex_unlock(&load_state->runtime->state.mutex);
}

static int rc_client_end_load_state(rc_client_load_state_t* load_state)
{
  int remaining_requests = 0;
  int aborted = 0;

  rc_mutex_lock(&load_state->runtime->state.mutex);

  if (load_state->outstanding_requests > 0)
    --load_state->outstanding_requests;
  remaining_requests = load_state->outstanding_requests;

  if (load_state->runtime->state.load != load_state)
    aborted = 1;

  rc_mutex_unlock(&load_state->runtime->state.mutex);

  if (aborted) {
    /* we can't actually free the load_state itself if there are any outstanding requests
     * or their callbacks will try to use the free'd memory. as they call end_load_state,
     * the outstanding_requests count will reach zero and the memory will be free'd then. */
    if (remaining_requests == 0)
      rc_client_free_load_state(load_state);

    return -1;
  }

  return remaining_requests;
}

static void rc_client_load_error(rc_client_load_state_t* load_state, int result, const char* error_message)
{
  int remaining_requests = 0;

  rc_mutex_lock(&load_state->runtime->state.mutex);

  load_state->progress = RC_CLIENT_LOAD_STATE_UNKNOWN_GAME;
  if (load_state->runtime->state.load == load_state)
    load_state->runtime->state.load = NULL;

  remaining_requests = load_state->outstanding_requests;

  rc_mutex_unlock(&load_state->runtime->state.mutex);

  if (load_state->callback)
    load_state->callback(result, error_message, load_state->runtime);

  /* we can't actually free the load_state itself if there are any outstanding requests
   * or their callbacks will try to use the free'd memory. as they call end_load_state,
   * the outstanding_requests count will reach zero and the memory will be free'd then. */
  if (remaining_requests == 0)
    rc_client_free_load_state(load_state);
}

static void rc_client_invalidate_memref_achievements(rc_client_game_info_t* game, rc_client_t* runtime, rc_memref_t* memref)
{
  rc_client_achievement_info_t* achievement = game->achievements;
  rc_client_achievement_info_t* stop = achievement + game->public.num_achievements;
  for (; achievement < stop; ++achievement) {
    if (achievement->public.state == RC_CLIENT_ACHIEVEMENT_STATE_DISABLED)
      continue;

    if (rc_trigger_contains_memref(achievement->trigger, memref)) {
      achievement->public.state = RC_CLIENT_ACHIEVEMENT_STATE_DISABLED;
      achievement->public.bucket = RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED;
      RC_CLIENT_LOG_WARN(runtime, "Disabled achievement %u. Invalid address %06X", achievement->public.id, memref->address);
    }
  }
}

static void rc_client_invalidate_memref_leaderboards(rc_client_game_info_t* game, rc_client_t* runtime, rc_memref_t* memref)
{
  rc_client_leaderboard_info_t* leaderboard = game->leaderboards;
  rc_client_leaderboard_info_t* stop = leaderboard + game->public.num_leaderboards;
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

    RC_CLIENT_LOG_WARN(runtime, "Disabled leaderboard %u. Invalid address %06X", leaderboard->public.id, memref->address);
  }
}

static void rc_client_validate_addresses(rc_client_game_info_t* game, rc_client_t* runtime)
{
  const rc_memory_regions_t* regions = rc_console_memory_regions(game->public.console_id);
  const uint32_t max_address = (regions && regions->num_regions > 0) ?
      regions->region[regions->num_regions - 1].end_address : 0xFFFFFFFF;
  uint8_t buffer[8];

  rc_memref_t** last_memref = &game->runtime.memrefs;
  rc_memref_t* memref = game->runtime.memrefs;
  for (; memref; memref = memref->next) {
    if (memref->value.is_indirect)
      continue;

    if (memref->address > max_address ||
        runtime->callbacks.read_memory(memref->address, buffer, 1, runtime) == 0) {
      /* invalid address, remove from chain so we don't have to evaluate it in the future.
       * it's still there, so anything referencing it will always fetch 0. */
      *last_memref = memref->next;

      rc_client_invalidate_memref_achievements(game, runtime, memref);
      rc_client_invalidate_memref_leaderboards(game, runtime, memref);
    }
  }
}

static void rc_client_update_legacy_runtime_achievements(rc_client_game_info_t* game, uint32_t active_count)
{
  if (active_count > 0) {
    rc_client_achievement_info_t* achievement = game->achievements;
    rc_client_achievement_info_t* stop = achievement + game->public.num_achievements;
    rc_runtime_trigger_t* trigger;

    if (active_count <= game->runtime.trigger_capacity) {
      if (active_count != 0)
        memset(game->runtime.triggers, 0, active_count * sizeof(rc_runtime_trigger_t));
    }
    else {
      if (game->runtime.triggers)
        free(game->runtime.triggers);

      game->runtime.trigger_capacity = active_count;
      game->runtime.triggers = (rc_runtime_trigger_t*)calloc(1, active_count * sizeof(rc_runtime_trigger_t));
    }

    trigger = game->runtime.triggers;
    achievement = game->achievements;
    for (; achievement < stop; ++achievement) {
      if (achievement->public.state == RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE) {
        trigger->id = achievement->public.id;
        memcpy(trigger->md5, achievement->md5, 16);
        trigger->trigger = achievement->trigger;
        ++trigger;
      }
    }
  }

  game->runtime.trigger_count = active_count;
}

static void rc_client_update_active_achievements(rc_client_game_info_t* game)
{
  rc_client_achievement_info_t* achievement = game->achievements;
  rc_client_achievement_info_t* stop = achievement + game->public.num_achievements;
  uint32_t active_count = 0;

  for (; achievement < stop; ++achievement) {
    if (achievement->public.state == RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE)
      ++active_count;
  }

  rc_client_update_legacy_runtime_achievements(game, active_count);
}

static void rc_client_toggle_hardcore_achievements(rc_client_game_info_t* game, rc_client_t* runtime, uint8_t active_bit)
{
  rc_client_achievement_info_t* achievement = game->achievements;
  rc_client_achievement_info_t* stop = achievement + game->public.num_achievements;
  uint32_t active_count = 0;

  for (; achievement < stop; ++achievement) {
    if ((achievement->public.unlocked & active_bit) == 0) {
      switch (achievement->public.state) {
        case RC_CLIENT_ACHIEVEMENT_STATE_INACTIVE:
        case RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED:
          rc_reset_trigger(achievement->trigger);
          achievement->public.state = RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE;
          ++active_count;
          break;

        case RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE:
          ++active_count;
          break;
      }
    }
    else if (achievement->public.state == RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE) {
      achievement->public.state = RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED;

      if (achievement->trigger && achievement->trigger->state == RC_TRIGGER_STATE_PRIMED) {
        rc_client_event_t client_event;
        memset(&client_event, 0, sizeof(client_event));
        client_event.type = RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE;
        client_event.achievement = &achievement->public;
        client_event.client = runtime;
        runtime->callbacks.event_handler(&client_event);
      }
    }
  }

  rc_client_update_legacy_runtime_achievements(game, active_count);
}

static void rc_client_activate_achievements(rc_client_game_info_t* game, rc_client_t* runtime)
{
  const uint8_t active_bit = (runtime->state.encore_mode) ?
      RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE : (runtime->state.hardcore) ?
      RC_CLIENT_ACHIEVEMENT_UNLOCKED_HARDCORE : RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE;

  rc_client_toggle_hardcore_achievements(game, runtime, active_bit);
}

static void rc_client_activate_leaderboards(rc_client_game_info_t* game, rc_client_t* runtime)
{
  unsigned active_count = 0;
  rc_client_leaderboard_info_t* leaderboard = game->leaderboards;
  rc_client_leaderboard_info_t* stop = leaderboard + game->public.num_leaderboards;

  for (; leaderboard < stop; ++leaderboard) {
    switch (leaderboard->public.state) {
      case RC_CLIENT_LEADERBOARD_STATE_DISABLED:
        continue;

      case RC_CLIENT_LEADERBOARD_STATE_INACTIVE:
        if (runtime->state.hardcore) {
          rc_reset_lboard(leaderboard->lboard);
          leaderboard->public.state = RC_CLIENT_LEADERBOARD_STATE_ACTIVE;
          ++active_count;
        }
        break;

      default:
        if (runtime->state.hardcore)
          ++active_count;
        else
          leaderboard->public.state = RC_CLIENT_LEADERBOARD_STATE_INACTIVE;
        break;
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
    for (leaderboard = game->leaderboards; leaderboard < stop; ++leaderboard) {
      if (leaderboard->public.state == RC_CLIENT_LEADERBOARD_STATE_ACTIVE ||
          leaderboard->public.state == RC_CLIENT_LEADERBOARD_STATE_TRACKING) {
        lboard->id = leaderboard->public.id;
        memcpy(lboard->md5, leaderboard->md5, 16);
        lboard->lboard = leaderboard->lboard;
        ++lboard;
      }
    }
  }

  game->runtime.lboard_count = active_count;
}

static void rc_client_deactivate_leaderboards(rc_client_game_info_t* game, rc_client_t* runtime)
{
  rc_client_leaderboard_info_t* leaderboard = game->leaderboards;
  rc_client_leaderboard_info_t* stop = leaderboard + game->public.num_leaderboards;

  for (; leaderboard < stop; ++leaderboard) {
    switch (leaderboard->public.state) {
      case RC_CLIENT_LEADERBOARD_STATE_DISABLED:
      case RC_CLIENT_LEADERBOARD_STATE_INACTIVE:
        continue;

      case RC_CLIENT_LEADERBOARD_STATE_TRACKING:
        rc_client_release_leaderboard_tracker(runtime->game, leaderboard);
        /* fallthrough to default */
      default:
        leaderboard->public.state = RC_CLIENT_LEADERBOARD_STATE_INACTIVE;
        break;
    }
  }

  game->runtime.lboard_count = 0;
}

static void rc_client_apply_unlocks(rc_client_game_info_t* game, uint32_t* unlocks, uint32_t num_unlocks, uint8_t mode)
{
  rc_client_achievement_info_t* start = game->achievements;
  rc_client_achievement_info_t* stop = start + game->public.num_achievements;
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
  rc_client_t* runtime = load_state->runtime;

  rc_mutex_lock(&runtime->state.mutex);
  load_state->progress = (runtime->state.load == load_state) ?
      RC_CLIENT_LOAD_STATE_DONE : RC_CLIENT_LOAD_STATE_UNKNOWN_GAME;
  runtime->state.load = NULL;
  rc_mutex_unlock(&runtime->state.mutex);

  if (load_state->progress != RC_CLIENT_LOAD_STATE_DONE) {
    /* previous load state was aborted, silently quit */
  }
  else {
    rc_client_apply_unlocks(load_state->game, load_state->softcore_unlocks,
        load_state->num_softcore_unlocks, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    rc_client_apply_unlocks(load_state->game, load_state->hardcore_unlocks,
        load_state->num_hardcore_unlocks, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);

    rc_client_validate_addresses(load_state->game, runtime);

    rc_client_activate_achievements(load_state->game, runtime);
    rc_client_activate_leaderboards(load_state->game, runtime);

    rc_mutex_lock(&runtime->state.mutex);
    if (runtime->state.load == NULL)
      runtime->game = load_state->game;
    rc_mutex_unlock(&runtime->state.mutex);

    if (runtime->game != load_state->game) {
      /* previous load state was aborted, silently quit */
    }
    else {
      rc_client_scheduled_callback_data_t* callback_data = rc_buf_alloc(&load_state->game->buffer, sizeof(rc_client_scheduled_callback_data_t));
      memset(callback_data, 0, sizeof(*callback_data));
      callback_data->callback = rc_client_ping;
      callback_data->related_id = load_state->game->public.id;
      callback_data->when = time(NULL) + 30;
      rc_client_schedule_callback(runtime, callback_data);

      if (load_state->callback)
        load_state->callback(RC_OK, NULL, runtime);

      /* detach the game object so it doesn't get freed by free_load_state */
      load_state->game = NULL;
    }
  }

  rc_client_free_load_state(load_state);
}

static void rc_client_start_session_callback(const char* server_response_body, int http_status_code, void* callback_data)
{
  rc_client_load_state_t* load_state = (rc_client_load_state_t*)callback_data;
  rc_client_t* runtime = load_state->runtime;
  rc_api_start_session_response_t start_session_response;

  int result = rc_api_process_start_session_response(&start_session_response, server_response_body);
  const char* error_message = rc_client_server_error_message(&result, http_status_code, &start_session_response.response);

  int outstanding_requests = rc_client_end_load_state(load_state);
  if (error_message) {
    rc_client_load_error(callback_data, result, error_message);
  }
  else if (outstanding_requests < 0) {
    /* previous load state was aborted, silently quit */
  }
  else {
    if (outstanding_requests == 0)
      rc_client_activate_game(load_state);
  }

  rc_api_destroy_start_session_response(&start_session_response);
}

static void rc_client_unlocks_callback(const char* server_response_body, int http_status_code, void* callback_data, int mode)
{
  rc_client_load_state_t* load_state = (rc_client_load_state_t*)callback_data;
  rc_client_t* runtime = load_state->runtime;
  rc_api_fetch_user_unlocks_response_t fetch_user_unlocks_response;

  int result = rc_api_process_fetch_user_unlocks_response(&fetch_user_unlocks_response, server_response_body);
  const char* error_message = rc_client_server_error_message(&result, http_status_code, &fetch_user_unlocks_response.response);

  int outstanding_requests = rc_client_end_load_state(load_state);
  if (error_message) {
    rc_client_load_error(callback_data, result, error_message);
  }
  else if (outstanding_requests < 0) {
    /* previous load state was aborted, silently quit */
  }
  else {
    if (mode == RC_CLIENT_ACHIEVEMENT_UNLOCKED_HARDCORE) {
      const size_t array_size = fetch_user_unlocks_response.num_achievement_ids * sizeof(uint32_t);
      load_state->num_hardcore_unlocks = fetch_user_unlocks_response.num_achievement_ids;
      load_state->hardcore_unlocks = (uint32_t*)malloc(array_size);
      memcpy(load_state->hardcore_unlocks, fetch_user_unlocks_response.achievement_ids, array_size);
    }
    else {
      const size_t array_size = fetch_user_unlocks_response.num_achievement_ids * sizeof(uint32_t);
      load_state->num_softcore_unlocks = fetch_user_unlocks_response.num_achievement_ids;
      load_state->softcore_unlocks = (uint32_t*)malloc(array_size);
      memcpy(load_state->softcore_unlocks, fetch_user_unlocks_response.achievement_ids, array_size);
    }

    if (outstanding_requests == 0)
      rc_client_activate_game(load_state);
  }

  rc_api_destroy_fetch_user_unlocks_response(&fetch_user_unlocks_response);
}

static void rc_client_hardcore_unlocks_callback(const char* server_response_body, int http_status_code, void* callback_data)
{
  rc_client_unlocks_callback(server_response_body, http_status_code, callback_data, RC_CLIENT_ACHIEVEMENT_UNLOCKED_HARDCORE);
}

static void rc_client_softcore_unlocks_callback(const char* server_response_body, int http_status_code, void* callback_data)
{
  rc_client_unlocks_callback(server_response_body, http_status_code, callback_data, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);
}

static void rc_client_begin_start_session(rc_client_load_state_t* load_state)
{
  rc_api_start_session_request_t start_session_params;
  rc_api_fetch_user_unlocks_request_t unlock_params;
  rc_client_t* runtime = load_state->runtime;
  rc_api_request_t start_session_request;
  rc_api_request_t hardcore_unlock_request;
  rc_api_request_t softcore_unlock_request;
  int result;

  memset(&start_session_params, 0, sizeof(start_session_params));
  start_session_params.username = runtime->user.username;
  start_session_params.api_token = runtime->user.token;
  start_session_params.game_id = load_state->game->public.id;

  result = rc_api_init_start_session_request(&start_session_request, &start_session_params);
  if (result != RC_OK) {
    rc_client_load_error(load_state, result, rc_error_str(result));
  }
  else {
    memset(&unlock_params, 0, sizeof(unlock_params));
    unlock_params.username = runtime->user.username;
    unlock_params.api_token = runtime->user.token;
    unlock_params.game_id = load_state->game->public.id;
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
        runtime->callbacks.server_call(&start_session_request, rc_client_start_session_callback, load_state, runtime);
        runtime->callbacks.server_call(&hardcore_unlock_request, rc_client_hardcore_unlocks_callback, load_state, runtime);
        runtime->callbacks.server_call(&softcore_unlock_request, rc_client_softcore_unlocks_callback, load_state, runtime);

        rc_api_destroy_request(&softcore_unlock_request);
      }

      rc_api_destroy_request(&hardcore_unlock_request);
    }

    rc_api_destroy_request(&start_session_request);
  }
}

static rc_client_achievement_info_t* rc_client_copy_achievements(
    rc_client_load_state_t* load_state, const rc_api_fetch_game_data_response_t* game_data)
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
  uint32_t num_achievements = game_data->num_achievements;

  if (num_achievements == 0)
    return NULL;

  stop = game_data->achievements + num_achievements;

  /* if not testing unofficial, filter them out */
  if (!load_state->runtime->state.test_unofficial) {
    read = game_data->achievements;
    for (; read < stop; ++read) {
      if (read->category != RC_ACHIEVEMENT_CATEGORY_CORE)
        --num_achievements;
    }

    load_state->game->public.num_achievements = num_achievements;

    if (num_achievements == 0)
      return NULL;
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
  read = game_data->achievements;
  do {
    if (read->category != RC_ACHIEVEMENT_CATEGORY_CORE && !load_state->runtime->state.test_unofficial) {
      ++read;
      continue;
    }

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
      RC_CLIENT_LOG_WARN(load_state->runtime, "Parse error %d processing achievement %u", trigger_size, read->id);
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
        RC_CLIENT_LOG_WARN(load_state->runtime, "Parse error %d processing achievement %u", parse.offset, read->id);
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
    ++read;
  } while (read < stop);

  return achievements;
}

static rc_client_leaderboard_info_t* rc_client_copy_leaderboards(
    rc_client_load_state_t* load_state, const rc_api_fetch_game_data_response_t* game_data)
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

  if (game_data->num_leaderboards == 0)
    return NULL;

  /* preallocate space for achievements */
  size = 24 /* assume average title length of 24 */
      + 48 /* assume average description length of 48 */
      + sizeof(rc_lboard_t) /* lboard container */
      + (sizeof(rc_trigger_t) + sizeof(rc_condset_t) * 2) * 3 /* start/submit/cancel */
      + (sizeof(rc_value_t) + sizeof(rc_condset_t)) /* value */
      + sizeof(rc_condition_t) * 4 * 4 /* assume average of 4 conditions in each start/submit/cancel/value */
      + sizeof(rc_client_leaderboard_info_t);
  rc_buf_reserve(&load_state->game->buffer, size * game_data->num_leaderboards);

  /* allocate the achievement array */
  size = sizeof(rc_client_leaderboard_info_t) * game_data->num_leaderboards;
  buffer = &load_state->game->buffer;
  leaderboard = leaderboards = rc_buf_alloc(buffer, size);
  memset(leaderboards, 0, size);

  /* copy the achievement data */
  read = game_data->leaderboards;
  stop = read + game_data->num_leaderboards;
  do {
    leaderboard->public.title = rc_buf_strcpy(buffer, read->title);
    leaderboard->public.description = rc_buf_strcpy(buffer, read->description);
    leaderboard->public.id = read->id;
    leaderboard->format = (uint8_t)read->format;
    leaderboard->tracker_id = RC_CLIENT_LEADERBOARD_TRACKER_UNASSIGNED;

    memaddr = read->definition;
    rc_runtime_checksum(memaddr, leaderboard->md5);

    ptr = strstr(memaddr, "VAL:");
    if (ptr != NULL) {
      /* calculate the DJB2 hash of the VAL portion of the string*/
      uint32_t hash = 5381;
      while (*ptr && (ptr[0] != ':' || ptr[1] != ':'))
         hash = (hash << 5) + hash + *ptr++;
      leaderboard->value_djb2 = hash;
    }

    lboard_size = rc_lboard_size(memaddr);
    if (lboard_size < 0) {
      RC_CLIENT_LOG_WARN(load_state->runtime, "Parse error %d processing leaderboard %u", lboard_size, read->id);
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
        RC_CLIENT_LOG_WARN(load_state->runtime, "Parse error %d processing leaderboard %u", parse.offset, read->id);
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

  return leaderboards;
}

static void rc_client_fetch_game_data_callback(const char* server_response_body, int http_status_code, void* callback_data)
{
  rc_client_load_state_t* load_state = (rc_client_load_state_t*)callback_data;
  rc_client_t* runtime = load_state->runtime;
  rc_api_fetch_game_data_response_t fetch_game_data_response;

  int result = rc_api_process_fetch_game_data_response(&fetch_game_data_response, server_response_body);
  const char* error_message = rc_client_server_error_message(&result, http_status_code, &fetch_game_data_response.response);

  int outstanding_requests = rc_client_end_load_state(load_state);

  if (error_message) {
    rc_client_load_error(load_state, result, error_message);
  }
  else if (outstanding_requests < 0) {
    /* previous load state was aborted, silently quit */
  }
  else {
    /* if a change media request is pending, kick it off */
    rc_client_pending_media_t* pending_media;

    rc_mutex_lock(&load_state->runtime->state.mutex);
    pending_media = load_state->pending_media;
    load_state->pending_media = NULL;
    load_state->game->public.console_id = fetch_game_data_response.console_id;
    rc_mutex_unlock(&load_state->runtime->state.mutex);

    if (pending_media) {
      rc_client_begin_change_media(runtime, pending_media->file_path,
          pending_media->data, pending_media->data_size, pending_media->callback);
      if (pending_media->data)
        free(pending_media->data);
      free((void*)pending_media->file_path);
      free(pending_media);
    }

    /* kick off the start session request while we process the game data */
    rc_client_begin_load_state(load_state, RC_CLIENT_LOAD_STATE_STARTING_SESSION, 1);
    rc_client_begin_start_session(load_state);

    /* process the game data */
    load_state->game->public.title = rc_buf_strcpy(&load_state->game->buffer, fetch_game_data_response.title);
    snprintf(load_state->game->public.badge_name, sizeof(load_state->game->public.badge_name), "%s", fetch_game_data_response.image_name);

    load_state->game->public.num_achievements = fetch_game_data_response.num_achievements;
    load_state->game->achievements = rc_client_copy_achievements(load_state, &fetch_game_data_response);

    load_state->game->public.num_leaderboards = fetch_game_data_response.num_leaderboards;
    load_state->game->leaderboards = rc_client_copy_leaderboards(load_state, &fetch_game_data_response);

    result = rc_runtime_activate_richpresence(&load_state->game->runtime, fetch_game_data_response.rich_presence_script, NULL, 0);
    if (result != RC_OK) {
      RC_CLIENT_LOG_WARN(load_state->runtime, "Parse error %d processing rich presence", result);
    }

    outstanding_requests = rc_client_end_load_state(load_state);
    if (outstanding_requests < 0) {
      /* previous load state was aborted, silently quit */
    }
    else
    {
      if (outstanding_requests == 0)
        rc_client_activate_game(load_state);
    }
  }

  rc_api_destroy_fetch_game_data_response(&fetch_game_data_response);
}

static void rc_client_begin_fetch_game_data(rc_client_load_state_t* load_state)
{
  rc_api_fetch_game_data_request_t fetch_game_data_request;
  rc_client_t* runtime = load_state->runtime;
  rc_api_request_t request;
  int result;

  if (load_state->hash->game_id == 0) {
    char hash[33];
    if (rc_hash_iterate(hash, &load_state->hash_iterator)) {
      /* found another hash to try */
      rc_client_load_game(load_state, hash, NULL);
      return;
    }

    rc_client_load_error(load_state, RC_NO_GAME_LOADED, "Unknown game");
    return;
  }

  load_state->game->public.id = load_state->hash->game_id;
  load_state->game->public.hash = load_state->hash->hash;

  rc_mutex_lock(&runtime->state.mutex);
  result = runtime->state.user;
  if (result == RC_CLIENT_USER_STATE_LOGIN_REQUESTED)
    load_state->progress = RC_CLIENT_LOAD_STATE_AWAIT_LOGIN;
  rc_mutex_unlock(&runtime->state.mutex);

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
  fetch_game_data_request.username = runtime->user.username;
  fetch_game_data_request.api_token = runtime->user.token;
  fetch_game_data_request.game_id = load_state->game->public.id;

  result = rc_api_init_fetch_game_data_request(&request, &fetch_game_data_request);
  if (result != RC_OK) {
    rc_client_load_error(load_state, result, rc_error_str(result));
    return;
  }

  rc_client_begin_load_state(load_state, RC_CLIENT_LOAD_STATE_FETCHING_GAME_DATA, 1);

  runtime->callbacks.server_call(&request, rc_client_fetch_game_data_callback, load_state, runtime);
  rc_api_destroy_request(&request);
}

static void rc_client_identify_game_callback(const char* server_response_body, int http_status_code, void* callback_data)
{
  rc_client_load_state_t* load_state = (rc_client_load_state_t*)callback_data;
  rc_client_t* runtime = load_state->runtime;
  rc_api_resolve_hash_response_t resolve_hash_response;

  int result = rc_api_process_resolve_hash_response(&resolve_hash_response, server_response_body);
  const char* error_message = rc_client_server_error_message(&result, http_status_code, &resolve_hash_response.response);

  int outstanding_requests = rc_client_end_load_state(load_state);

  if (error_message) {
    rc_client_load_error(load_state, result, error_message);
  }
  else {
    /* hash exists outside the load state - always update it */
    load_state->hash->game_id = resolve_hash_response.game_id;
    RC_CLIENT_LOG_INFO(runtime, "Identified game: %u (%s)", load_state->hash->game_id, load_state->hash->hash);

    if (outstanding_requests < 0) {
      /* previous load state was aborted, silently quit */
    }
    else {
      rc_client_begin_fetch_game_data(load_state);
    }
  }

  rc_api_destroy_resolve_hash_response(&resolve_hash_response);
}

static rc_client_game_hash_t* rc_client_find_game_hash(rc_client_t* runtime, const char* hash)
{
  rc_client_game_hash_t* game_hash;

  rc_mutex_lock(&runtime->state.mutex);
  game_hash = runtime->hashes;
  while (game_hash) {
    if (strcasecmp(game_hash->hash, hash) == 0)
      break;

    game_hash = game_hash->next;
  }

  if (!game_hash) {
    game_hash = rc_buf_alloc(&runtime->buffer, sizeof(rc_client_game_hash_t));
    memset(game_hash, 0, sizeof(*game_hash));
    snprintf(game_hash->hash, sizeof(game_hash->hash), "%s", hash);
    game_hash->game_id = RC_CLIENT_UNKNOWN_GAME_ID;
    game_hash->next = runtime->hashes;
    runtime->hashes = game_hash;
  }
  rc_mutex_unlock(&runtime->state.mutex);

  return game_hash;
}

static void rc_client_load_game(rc_client_load_state_t* load_state, const char* hash, const char* file_path)
{
  rc_client_t* runtime = load_state->runtime;
  rc_client_game_hash_t* old_hash;

  if (runtime->state.load == NULL) {
    rc_client_unload_game(runtime);
    runtime->state.load = load_state;

    if (load_state->game == NULL) {
      load_state->game = (rc_client_game_info_t*)calloc(1, sizeof(*load_state->game));
      rc_buf_init(&load_state->game->buffer);
      rc_runtime_init(&load_state->game->runtime);
    }
  }
  else if (runtime->state.load != load_state) {
    /* previous load was aborted, silently quit */
    rc_client_free_load_state(load_state);
    return;
  }

  old_hash = load_state->hash;
  load_state->hash = rc_client_find_game_hash(runtime, hash);

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
      return;
    }

    rc_client_begin_load_state(load_state, RC_CLIENT_LOAD_STATE_IDENTIFYING_GAME, 1);

    runtime->callbacks.server_call(&request, rc_client_identify_game_callback, load_state, runtime);

    rc_api_destroy_request(&request);
  }
  else {
    RC_CLIENT_LOG_INFO(runtime, "Identified game: %u (%s)", load_state->hash->game_id, load_state->hash->hash);

    rc_client_begin_fetch_game_data(load_state);
  }
}

void rc_client_begin_load_game(rc_client_t* runtime, const char* hash, rc_client_callback_t callback)
{
  rc_client_load_state_t* load_state;

  if (!hash || !hash[0]) {
    callback(RC_INVALID_STATE, "hash is required", runtime);
    return;
  }

  load_state = (rc_client_load_state_t*)calloc(1, sizeof(*load_state));
  load_state->runtime = runtime;
  load_state->callback = callback;
  rc_client_load_game(load_state, hash, NULL);
}

void rc_client_begin_identify_and_load_game(rc_client_t* runtime,
    uint32_t console_id, const char* file_path,
    const uint8_t* data, size_t data_size,
    rc_client_callback_t callback)
{
  rc_client_load_state_t* load_state;
  char hash[33];

  if (data) {
    if (file_path) {
      RC_CLIENT_LOG_INFO(runtime, "Identifying game: %zu bytes at %p (%s)", data_size, data, file_path);
    }
    else {
      RC_CLIENT_LOG_INFO(runtime, "Identifying game: %zu bytes at %p", data_size, data);
    }
  }
  else if (file_path) {
    RC_CLIENT_LOG_INFO(runtime, "Identifying game: %s", file_path);
  }
  else {
    callback(RC_INVALID_STATE, "either data or file_path is required", runtime);
    return;
  }

  if (runtime->state.log_level >= RC_CLIENT_LOG_LEVEL_INFO)
    rc_hash_init_verbose_message_callback(runtime->callbacks.log_call);

  if (!file_path)
    file_path = "?";

  load_state = (rc_client_load_state_t*)calloc(1, sizeof(*load_state));
  load_state->runtime = runtime;
  load_state->callback = callback;
  rc_hash_initialize_iterator(&load_state->hash_iterator, file_path, data, data_size);

  if (console_id == RC_CONSOLE_UNKNOWN) {
    if (!rc_hash_iterate(hash, &load_state->hash_iterator)) {
      rc_client_load_error(load_state, RC_INVALID_STATE, "hash generation failed");
      return;
    }
  }
  else {
    load_state->hash_iterator.consoles[0] = console_id;
    load_state->hash_iterator.consoles[1] = 0;
    load_state->hash_iterator.index = 1;

    if (data != NULL) {
      if (!rc_hash_generate_from_buffer(hash, console_id, data, data_size)) {
        rc_client_load_error(load_state, RC_INVALID_STATE, "hash generation failed");
        return;
      }
    }
    else {
      if (!rc_hash_generate_from_file(hash, console_id, file_path)) {
        rc_client_load_error(load_state, RC_INVALID_STATE, "hash generation failed");
        return;
      }
    }
  }

  rc_client_load_game(load_state, hash, file_path);
}

void rc_client_unload_game(rc_client_t* runtime)
{
  rc_client_game_info_t* game;
  rc_client_scheduled_callback_data_t** last;
  rc_client_scheduled_callback_data_t* next;

  rc_mutex_lock(&runtime->state.mutex);

  game = runtime->game;
  runtime->game = NULL;
  runtime->state.load = NULL;

  last = &runtime->state.scheduled_callbacks;
  do {
    next = *last;
    if (!next)
      break;

    /* remove rich presence ping scheduled event for game */
    if (next->callback == rc_client_ping && next->related_id == game->public.id) {
      *last = next->next;
      continue;
    }

    last = &next->next;
  } while (1);

  rc_mutex_unlock(&runtime->state.mutex);

  if (game != NULL) {
    RC_CLIENT_LOG_INFO(runtime, "Unloading game %u", runtime->game->public.id);
    rc_client_free_game(game);
  }
}

static void rc_client_change_media(rc_client_t* runtime, const rc_client_game_hash_t* game_hash, rc_client_callback_t callback)
{
  if (game_hash->game_id == runtime->game->public.id) {
    RC_CLIENT_LOG_INFO(runtime, "Switching to valid media for game %u: %s", game_hash->game_id, game_hash->hash);
  }
  else if (game_hash->game_id == RC_CLIENT_UNKNOWN_GAME_ID) {
    RC_CLIENT_LOG_INFO(runtime, "Switching to unknown media");
  }
  else if (game_hash->game_id == 0) {
    RC_CLIENT_LOG_INFO(runtime, "Switching to unrecognized media: %s", game_hash->hash);
  }
  else {
    RC_CLIENT_LOG_INFO(runtime, "Switching to known media for game %u: %s", game_hash->game_id, game_hash->hash);
  }

  runtime->game->public.hash = game_hash->hash;
  callback(RC_OK, NULL, runtime);
}

static void rc_client_identify_changed_media_callback(const char* server_response_body, int http_status_code, void* callback_data)
{
  rc_client_load_state_t* load_state = (rc_client_load_state_t*)callback_data;
  rc_client_t* runtime = load_state->runtime;
  rc_api_resolve_hash_response_t resolve_hash_response;

  int result = rc_api_process_resolve_hash_response(&resolve_hash_response, server_response_body);
  const char* error_message = rc_client_server_error_message(&result, http_status_code, &resolve_hash_response.response);

  if (runtime->game != load_state->game) {
    /* loaded game changed. return success regardless of result */
    load_state->callback(RC_OK, NULL, runtime);
  }
  else if (error_message) {
    load_state->callback(result, error_message, runtime);
  }
  else {
    load_state->hash->game_id = resolve_hash_response.game_id;

    if (resolve_hash_response.game_id == 0 && runtime->state.hardcore) {
      RC_CLIENT_LOG_WARN(runtime, "Disabling hardcore for unidentified media: %s", load_state->hash->hash);
      rc_client_set_hardcore_enabled(runtime, 0);
      runtime->game->public.hash = load_state->hash->hash; /* do still update the loaded hash */
      load_state->callback(RC_HARDCORE_DISABLED, "Hardcore disabled. Unidentified media inserted.", runtime);
    }
    else {
      RC_CLIENT_LOG_INFO(runtime, "Identified game: %u (%s)", load_state->hash->game_id, load_state->hash->hash);
      rc_client_change_media(runtime, load_state->hash, load_state->callback);
    }
  }

  free(load_state);
  rc_api_destroy_resolve_hash_response(&resolve_hash_response);
}

void rc_client_begin_change_media(rc_client_t* runtime, const char* file_path,
    const uint8_t* data, size_t data_size, rc_client_callback_t callback)
{
  rc_client_game_hash_t* game_hash = NULL;
  rc_client_media_hash_t* media_hash;
  rc_client_game_info_t* game;
  rc_client_pending_media_t* pending_media = NULL;
  uint32_t path_djb2;

  if (!data && !file_path) {
    callback(RC_INVALID_STATE, "either data or file_path is required", runtime);
    return;
  }

  rc_mutex_lock(&runtime->state.mutex);
  if (runtime->state.load) {
    game = runtime->state.load->game;
    if (game->public.console_id == 0) {
      /* still waiting for game data */
      pending_media = runtime->state.load->pending_media;
      if (pending_media) {
        if (pending_media->data)
          free(pending_media->data);
        free((void*)pending_media->file_path);
        free(pending_media);
      }

      pending_media = (rc_client_pending_media_t*)calloc(1, sizeof(*pending_media));
      pending_media->file_path = strdup(file_path);
      pending_media->callback = callback;
      if (data && data_size) {
        pending_media->data_size = data_size;
        pending_media->data = (uint8_t*)malloc(data_size);
        memcpy(pending_media->data, data, data_size);
      }

      runtime->state.load->pending_media = pending_media;
    }
  }
  else {
    game = runtime->game;
  }
  rc_mutex_unlock(&runtime->state.mutex);

  if (!game) {
    callback(RC_NO_GAME_LOADED, rc_error_str(RC_NO_GAME_LOADED), runtime);
    return;
  }

  /* still waiting for game data */
  if (pending_media) 
    return;

  /* check to see if we've already hashed this file */
  path_djb2 = rc_djb2(file_path);
  rc_mutex_lock(&runtime->state.mutex);
  for (media_hash = game->media_hash; media_hash; media_hash = media_hash->next) {
    if (media_hash->path_djb2 == path_djb2) {
      game_hash = media_hash->game_hash;
      break;
    }
  }
  rc_mutex_unlock(&runtime->state.mutex);

  if (!game_hash) {
    char hash[33];
    int result;

    if (data != NULL)
      result = rc_hash_generate_from_buffer(hash, game->public.console_id, data, data_size);
    else
      result = rc_hash_generate_from_file(hash, game->public.console_id, file_path);

    if (!result) {
      /* when changing discs, if the disc is not supported by the system, allow it. this is
       * primarily for games that support user-provided audio CDs, but does allow using discs
       * from other systems for games that leverage user-provided discs. */
      strcpy_s(hash, sizeof(hash), "[NO HASH]");
    }

    game_hash = rc_client_find_game_hash(runtime, hash);

    media_hash = (rc_client_media_hash_t*)rc_buf_alloc(&game->buffer, sizeof(*media_hash));
    media_hash->game_hash = game_hash;
    media_hash->path_djb2 = path_djb2;

    rc_mutex_lock(&runtime->state.mutex);
    media_hash->next = game->media_hash;
    game->media_hash = media_hash;
    rc_mutex_unlock(&runtime->state.mutex);

    if (!result) {
      rc_client_change_media(runtime, game_hash, callback);
      return;
    }
  }

  if (game_hash->game_id != RC_CLIENT_UNKNOWN_GAME_ID) {
    rc_client_change_media(runtime, game_hash, callback);
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
      callback(result, rc_error_str(result), runtime);
      return;
    }

    callback_data = (rc_client_load_state_t*)calloc(1, sizeof(rc_client_load_state_t));
    callback_data->callback = callback;
    callback_data->runtime = runtime;
    callback_data->hash = game_hash;
    callback_data->game = game;

    runtime->callbacks.server_call(&request, rc_client_identify_changed_media_callback, callback_data, runtime);

    rc_api_destroy_request(&request);
  }
}

const rc_client_game_t* rc_client_get_game_info(const rc_client_t* client)
{
  return (client->game != NULL) ? &client->game->public : NULL;
}

int rc_client_game_get_badge_url(const rc_client_game_t* game, char buffer[], size_t buffer_size)
{
  if (!game)
    return RC_INVALID_STATE;

  return rc_client_get_image_url(buffer, buffer_size, RC_IMAGE_TYPE_GAME, game->badge_name);
}

/* ===== Achievements ===== */

uint32_t rc_client_get_achievement_count(const rc_client_t* client, int category)
{
  rc_client_achievement_info_t* achievement;
  rc_client_achievement_info_t* stop;
  uint32_t count = 0;

  if (!client->game)
    return 0;

  if (category == RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE_AND_UNOFFICIAL)
    return client->game->public.num_achievements;

  achievement = client->game->achievements;
  stop = achievement + client->game->public.num_achievements;
  for (; achievement < stop; ++achievement) {
    if (achievement->public.category == category)
      ++count;
  }

  return count;
}

static void rc_client_update_achievement_display_information(rc_client_t* client, rc_client_achievement_info_t* achievement, time_t recent_unlock_time)
{
  uint8_t new_bucket = RC_CLIENT_ACHIEVEMENT_BUCKET_UNKNOWN;
  uint32_t new_measured_value = 0;
  int measured_progress = 0;

  if (achievement->public.bucket == RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED)
    return;

  achievement->public.measured_progress[0] = '\0';

  if (achievement->public.unlocked & RC_CLIENT_ACHIEVEMENT_UNLOCKED_HARDCORE) {
    /* achievement unlocked in hardcore */
    new_bucket = RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED;
  }
  else if (achievement->public.unlocked & RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE && !client->state.hardcore) {
    /* achievement unlocked in softcore while hardcore is disabled */
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
          /* value is 0, leave progress string empty */
        }
        else {
          /* clamp measured value at target (can't get more than 100%) */
          new_measured_value = (achievement->trigger->measured_value > achievement->trigger->measured_target) ?
              achievement->trigger->measured_target : achievement->trigger->measured_value;

          measured_progress = (int)(((uint64_t)new_measured_value * 100) / achievement->trigger->measured_target);

          if (!achievement->trigger->measured_as_percent)
            snprintf(achievement->public.measured_progress, sizeof(achievement->public.measured_progress), "%u/%u", new_measured_value, achievement->trigger->measured_target);
          else if (measured_progress)
            snprintf(achievement->public.measured_progress, sizeof(achievement->public.measured_progress), "%u%%", measured_progress);
        }
      }

      if (achievement->trigger->state == RC_TRIGGER_STATE_PRIMED)
        new_bucket = RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE;
      else if (measured_progress >= 80)
        new_bucket = RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE;
    }
  }

  if (new_bucket == RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED && achievement->public.unlock_time >= recent_unlock_time)
    new_bucket = RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED;

  achievement->public.bucket = new_bucket;
}

static const char* rc_client_get_bucket_label(uint8_t bucket_type)
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

static int rc_client_compare_achievement_unlock_times(const void* a, const void* b)
{
  const rc_client_achievement_t* unlock_a = (const rc_client_achievement_t*)a;
  const rc_client_achievement_t* unlock_b = (const rc_client_achievement_t*)b;
  return (int)(unlock_a->unlock_time - unlock_b->unlock_time);
}

rc_client_achievement_list_t* rc_client_get_achievement_list(rc_client_t* runtime, int category, int grouping)
{
  rc_client_achievement_info_t* achievement;
  rc_client_achievement_info_t* stop;
  rc_client_achievement_t** achievement_ptr;
  rc_client_achievement_bucket_t* bucket_ptr;
  rc_client_achievement_list_t* list;
  const uint32_t list_size = RC_ALIGN(sizeof(*list));
  uint32_t bucket_counts[16];
  uint32_t num_buckets;
  uint32_t num_achievements;
  size_t buckets_size;
  uint8_t bucket_type;
  uint32_t i;
  const uint8_t progress_bucket_order[] = {
    RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE,
    RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED,
    RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE,
    RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED,
    RC_CLIENT_ACHIEVEMENT_BUCKET_UNOFFICIAL,
    RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED,
    RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED,
  };
  const time_t recent_unlock_time = time(NULL) - RC_CLIENT_RECENT_UNLOCK_DELAY_SECONDS;

  if (!runtime->game)
    return calloc(1, sizeof(rc_client_achievement_list_t));

  memset(&bucket_counts, 0, sizeof(bucket_counts));

  rc_mutex_lock(&runtime->state.mutex);

  achievement = runtime->game->achievements;
  stop = achievement + runtime->game->public.num_achievements;
  for (; achievement < stop; ++achievement) {
    if (achievement->public.category & category) {
      rc_client_update_achievement_display_information(runtime, achievement, recent_unlock_time);
      bucket_counts[achievement->public.bucket]++;
    }
  }

  num_buckets = 0;
  num_achievements = 0;
  for (i = 0; i < sizeof(bucket_counts) / sizeof(bucket_counts[0]); ++i) {
    if (bucket_counts[i]) {
      num_achievements += bucket_counts[i];
      ++num_buckets;
    }
  }

  buckets_size = RC_ALIGN(num_buckets * sizeof(rc_client_achievement_bucket_t));

  list = (rc_client_achievement_list_t*)malloc(list_size + buckets_size + num_achievements * sizeof(rc_client_achievement_t*));
  bucket_ptr = list->buckets = (rc_client_achievement_bucket_t*)((uint8_t*)list + list_size);
  achievement_ptr = (rc_client_achievement_t**)((uint8_t*)bucket_ptr + buckets_size);

  for (i = 0; i < sizeof(progress_bucket_order) / sizeof(progress_bucket_order[0]); ++i) {
    bucket_type = progress_bucket_order[i];
    if (!bucket_counts[bucket_type])
      continue;

    bucket_ptr->id = bucket_type;
    bucket_ptr->achievements = achievement_ptr;

    if (grouping == RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE) {
      switch (bucket_type) {
        case RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED:
        case RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE:
        case RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE:
          /* these are loaded into LOCKED/UNLOCKED buckets when grouping by lock state */
          continue;

        default:
          break;
      }

      for (achievement = runtime->game->achievements; achievement < stop; ++achievement) {
        if (!(achievement->public.category & category))
          continue;

        switch (achievement->public.bucket) {
          case RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED:
          case RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED:
            if (bucket_type == RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED)
              *achievement_ptr++ = &achievement->public;
            break;

          case RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED:
          case RC_CLIENT_ACHIEVEMENT_BUCKET_UNOFFICIAL:
            if (bucket_type == achievement->public.bucket)
              *achievement_ptr++ = &achievement->public;
            break;

          default:
            if (bucket_type == RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED)
              *achievement_ptr++ = &achievement->public;
            break;
        }
      }
    }
    else /* RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS */ {
      for (achievement = runtime->game->achievements; achievement < stop; ++achievement) {
        if (achievement->public.bucket == bucket_type && achievement->public.category & category)
          *achievement_ptr++ = &achievement->public;
      }
    }

    bucket_ptr->num_achievements = (uint32_t)(achievement_ptr - bucket_ptr->achievements);
    bucket_ptr++;
  }

  rc_mutex_unlock(&runtime->state.mutex);

  list->num_buckets = (uint32_t)(bucket_ptr - list->buckets);

  while (bucket_ptr > list->buckets) {
    --bucket_ptr;
    bucket_ptr->label = rc_client_get_bucket_label(bucket_ptr->id);

    if (bucket_ptr->id == RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED)
      qsort(bucket_ptr->achievements, bucket_ptr->num_achievements, sizeof(rc_client_achievement_t*), rc_client_compare_achievement_unlock_times);
  }

  return list;
}

void rc_client_destroy_achievement_list(rc_client_achievement_list_t* list)
{
  if (list)
    free(list);
}

const rc_client_achievement_t* rc_client_get_achievement_info(rc_client_t* client, uint32_t id)
{
  rc_client_achievement_info_t* achievement;
  rc_client_achievement_info_t* stop;

  if (!client->game)
    return NULL;

  achievement = client->game->achievements;
  stop = achievement + client->game->public.num_achievements;
  while (achievement < stop) {
    if (achievement->public.id == id) {
      const time_t recent_unlock_time = time(NULL) - RC_CLIENT_RECENT_UNLOCK_DELAY_SECONDS;
      rc_mutex_lock((rc_mutex_t*)(&client->state.mutex));
      rc_client_update_achievement_display_information(client, achievement, recent_unlock_time);
      rc_mutex_unlock((rc_mutex_t*)(&client->state.mutex));
      return &achievement->public;
    }

    ++achievement;
  }

  return NULL;
}

int rc_client_achievement_get_badge_url(const rc_client_achievement_t* achievement, int state, char buffer[], size_t buffer_size)
{
  const int image_type = (state == RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED) ?
      RC_IMAGE_TYPE_ACHIEVEMENT : RC_IMAGE_TYPE_ACHIEVEMENT_LOCKED;

  if (!achievement)
    return RC_INVALID_STATE;

  return rc_client_get_image_url(buffer, buffer_size, image_type, achievement->badge_name);
}

typedef struct rc_client_award_achievement_callback_data_t
{
  uint32_t id;
  uint32_t retry_count;
  uint8_t hardcore;
  const char* game_hash;
  time_t unlock_time;
  rc_client_t* runtime;
  rc_client_scheduled_callback_data_t* scheduled_callback_data;
} rc_client_award_achievement_callback_data_t;

static void rc_client_award_achievement_server_call(rc_client_award_achievement_callback_data_t* ach_data);

static void rc_client_award_achievement_retry(rc_client_scheduled_callback_data_t* callback_data, rc_client_t* runtime, time_t now)
{
  rc_client_award_achievement_callback_data_t* ach_data =
    (rc_client_award_achievement_callback_data_t*)callback_data->data;

  rc_client_award_achievement_server_call(ach_data);
}

static void rc_client_award_achievement_callback(const char* server_response_body, int http_status_code, void* callback_data)
{
  rc_client_award_achievement_callback_data_t* ach_data =
      (rc_client_award_achievement_callback_data_t*)callback_data;
  rc_api_award_achievement_response_t award_achievement_response;

  int result = rc_api_process_award_achievement_response(&award_achievement_response, server_response_body);
  const char* error_message = rc_client_server_error_message(&result, http_status_code, &award_achievement_response.response);

  if (error_message) {
    RC_CLIENT_LOG_ERR(ach_data->runtime, "Error awarding achievement %u: %s", ach_data->id, error_message);

    if (award_achievement_response.response.error_message) {
      rc_client_raise_server_error_event(ach_data->runtime, "award_achievement", award_achievement_response.response.error_message);
    }
    else if (ach_data->retry_count++ == 0) {
      /* first retry is immediate */
      rc_client_award_achievement_server_call(ach_data);
      return;
    }
    else {
      if (!ach_data->scheduled_callback_data) {
        ach_data->scheduled_callback_data = (rc_client_scheduled_callback_data_t*)calloc(1, sizeof(*ach_data->scheduled_callback_data));
        ach_data->scheduled_callback_data->callback = rc_client_award_achievement_retry;
        ach_data->scheduled_callback_data->data = ach_data;
        ach_data->scheduled_callback_data->related_id = ach_data->id;
      }

      /* double wait time between each attempt until we hit a maximum delay of two minutes */
      /* 1s -> 2s -> 4s -> 8s -> 16s -> 32s -> 64s -> 120s -> 120s -> 120s ...*/
      if (ach_data->retry_count > 8)
        ach_data->scheduled_callback_data->when = time(NULL) + 120;
      else
        ach_data->scheduled_callback_data->when = time(NULL) + (time_t)(1 << (ach_data->retry_count - 1));

      rc_client_schedule_callback(ach_data->runtime, ach_data->scheduled_callback_data);
      return;
    }
  }
  else
  {
    ach_data->runtime->user.score = award_achievement_response.new_player_score;
    ach_data->runtime->user.score_softcore = award_achievement_response.new_player_score_softcore;

    if (award_achievement_response.awarded_achievement_id != ach_data->id)
    {
      RC_CLIENT_LOG_ERR(ach_data->runtime, "Awarded achievement %u instead of %u", award_achievement_response.awarded_achievement_id, error_message);
    }
    else {
      if (award_achievement_response.response.error_message) {
        /* previously unlocked achievements are returned as a success with an error message */
        RC_CLIENT_LOG_INFO(ach_data->runtime, "Achievement %u: %s", ach_data->id, award_achievement_response.response.error_message);
      }
      else if (ach_data->retry_count) {
        RC_CLIENT_LOG_INFO(ach_data->runtime, "Achievement %u awarded after %u attempts", ach_data->id, ach_data->retry_count + 1);
      }

      if (award_achievement_response.achievements_remaining == 0 &&
          ach_data->runtime->game->mastery == RC_CLIENT_MASTERY_STATE_NONE) {
        ach_data->runtime->game->mastery = RC_CLIENT_MASTERY_STATE_PENDING;
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
  api_params.username = ach_data->runtime->user.username;
  api_params.api_token = ach_data->runtime->user.token;
  api_params.achievement_id = ach_data->id;
  api_params.hardcore = ach_data->hardcore;
  api_params.game_hash = ach_data->game_hash;

  result = rc_api_init_award_achievement_request(&request, &api_params);
  if (result != RC_OK) {
    RC_CLIENT_LOG_ERR(ach_data->runtime, "Error constructing unlock request for achievement %u: %s", ach_data->id, rc_error_str(result));
    free(ach_data);
    return;
  }

  ach_data->runtime->callbacks.server_call(&request, rc_client_award_achievement_callback, ach_data, ach_data->runtime);

  rc_api_destroy_request(&request);
}

static void rc_client_award_achievement(rc_client_t* runtime, rc_client_achievement_info_t* achievement)
{
  rc_client_award_achievement_callback_data_t* callback_data;

  rc_mutex_lock(&runtime->state.mutex);

  if (runtime->state.hardcore) {
    achievement->public.unlock_time = achievement->unlock_time_hardcore = time(NULL);
    if (achievement->unlock_time_softcore == 0)
      achievement->unlock_time_softcore = achievement->unlock_time_hardcore;

    /* adjust score now - will get accurate score back from server */
    runtime->user.score += achievement->public.points;
  }
  else {
    achievement->public.unlock_time = achievement->unlock_time_softcore = time(NULL);

    /* adjust score now - will get accurate score back from server */
    runtime->user.score_softcore += achievement->public.points;
  }

  achievement->public.state = RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED;
  achievement->public.unlocked |= (runtime->state.hardcore) ?
    RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH : RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE;

  rc_mutex_unlock(&runtime->state.mutex);

  /* can't unlock unofficial achievements on the server */
  if (achievement->public.category != RC_CLIENT_ACHIEVEMENT_CATEGORY_CORE) {
    RC_CLIENT_LOG_INFO(runtime, "Unlocked unofficial achievement %u: %s", achievement->public.id, achievement->public.title);
    return;
  }

  /* don't actually unlock achievements when spectating */
  if (runtime->state.spectator_mode) {
    RC_CLIENT_LOG_INFO(runtime, "Spectated achievement %u: %s", achievement->public.id, achievement->public.title);
    return;
  }

  callback_data = (rc_client_award_achievement_callback_data_t*)calloc(1, sizeof(*callback_data));
  callback_data->runtime = runtime;
  callback_data->id = achievement->public.id;
  callback_data->hardcore = runtime->state.hardcore;
  callback_data->game_hash = runtime->game->public.hash;
  callback_data->unlock_time = achievement->public.unlock_time;

  RC_CLIENT_LOG_INFO(runtime, "Awarding achievement %u: %s", achievement->public.id, achievement->public.title);
  rc_client_award_achievement_server_call(callback_data);
}

static void rc_client_reset_achievements(rc_client_t* runtime)
{
  rc_client_achievement_info_t* achievement = runtime->game->achievements;
  rc_client_achievement_info_t* stop = achievement + runtime->game->public.num_achievements;

  for (; achievement < stop; ++achievement) {
    rc_trigger_t* trigger = achievement->trigger;
    if (!trigger || achievement->public.state != RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE)
      continue;

    if (trigger->state == RC_TRIGGER_STATE_PRIMED)
      achievement->pending_events |= RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_HIDE;

    rc_reset_trigger(trigger);
  }
}

/* ===== Leaderboards ===== */

const rc_client_leaderboard_t* rc_client_get_leaderboard_info(const rc_client_t* client, uint32_t id)
{
  rc_client_leaderboard_info_t* leaderboard;
  rc_client_leaderboard_info_t* stop;

  if (!client->game)
    return NULL;

  leaderboard = client->game->leaderboards;
  stop = leaderboard + client->game->public.num_leaderboards;
  while (leaderboard < stop) {
    if (leaderboard->public.id == id)
      return &leaderboard->public;

    ++leaderboard;
  }

  return NULL;
}

static void rc_client_allocate_leaderboard_tracker(rc_client_game_info_t* game, rc_client_leaderboard_info_t* leaderboard)
{
  rc_client_leaderboard_tracker_info_t* tracker = game->leaderboard_trackers;
  rc_client_leaderboard_tracker_info_t* stop = tracker + game->leaderboard_trackers_size;
  rc_client_leaderboard_tracker_info_t* available_tracker = NULL;

  for (; tracker < stop; ++tracker) {
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
    }

    /* attach to the existing tracker */
    ++tracker->reference_count;
    tracker->pending_events &= ~RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_HIDE;
    leaderboard->tracker_id = (uint8_t)tracker->public.id;
    leaderboard->public.tracker_value = tracker->public.display;
    return;
  }

  if (!available_tracker) {
    /* no unused trackers in queue, allocate one */
    if (game->leaderboard_trackers_size == game->leaderboard_trackers_capacity) {
      const uint8_t capacity_increase = 8;
      const uint8_t new_capacity = game->leaderboard_trackers_capacity + capacity_increase;
      const size_t new_size = new_capacity * sizeof(game->leaderboard_trackers[0]);
      uint8_t i;

      /* unexpected, but prevents overflow of uint8_t */
      if (new_capacity > 0xF0)
        return;

      if (game->leaderboard_trackers)
        game->leaderboard_trackers = (rc_client_leaderboard_tracker_info_t*)realloc(game->leaderboard_trackers, new_size);
      else
        game->leaderboard_trackers = (rc_client_leaderboard_tracker_info_t*)malloc(new_size);

      tracker = &game->leaderboard_trackers[game->leaderboard_trackers_capacity];
      memset(tracker, 0, capacity_increase * sizeof(game->leaderboard_trackers[0]));

      for (i = 0; i < capacity_increase; ++i, ++tracker)
        tracker->public.id = game->leaderboard_trackers_capacity + i + 1;
      game->leaderboard_trackers_capacity = new_capacity;
    }

    available_tracker = &game->leaderboard_trackers[game->leaderboard_trackers_size++];
  }

  /* update the claimed tracker */
  available_tracker->reference_count = 1;
  available_tracker->value_djb2 = leaderboard->value_djb2;
  available_tracker->format = leaderboard->format;
  available_tracker->raw_value = leaderboard->value;
  available_tracker->pending_events = RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_SHOW;
  available_tracker->value_from_hits = rc_value_from_hits(&leaderboard->lboard->value);
  leaderboard->tracker_id = (uint8_t)available_tracker->public.id;
  leaderboard->public.tracker_value = available_tracker->public.display;
}

static void rc_client_release_leaderboard_tracker(rc_client_game_info_t* game, rc_client_leaderboard_info_t* leaderboard)
{
  rc_client_leaderboard_tracker_info_t* tracker = &game->leaderboard_trackers[leaderboard->tracker_id - 1];
  leaderboard->tracker_id = 0;

  if (--tracker->reference_count == 0)
    tracker->pending_events |= RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_HIDE;
}

static void rc_client_update_leaderboard_tracker(rc_client_game_info_t* game, rc_client_leaderboard_info_t* leaderboard)
{
  rc_client_leaderboard_tracker_info_t* tracker = &game->leaderboard_trackers[leaderboard->tracker_id - 1];
  if (tracker->raw_value != leaderboard->value) {
    tracker->raw_value = leaderboard->value;
    tracker->pending_events |= RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_UPDATE;
  }
}

typedef struct rc_client_submit_leaderboard_entry_callback_data_t
{
  uint32_t id;
  int32_t score;
  uint32_t retry_count;
  const char* game_hash;
  time_t submit_time;
  rc_client_t* runtime;
  rc_client_scheduled_callback_data_t* scheduled_callback_data;
} rc_client_submit_leaderboard_entry_callback_data_t;

static void rc_client_submit_leaderboard_entry_server_call(rc_client_submit_leaderboard_entry_callback_data_t* lboard_data);

static void rc_client_submit_leaderboard_entry_retry(rc_client_scheduled_callback_data_t* callback_data, rc_client_t* runtime, time_t now)
{
  rc_client_submit_leaderboard_entry_callback_data_t* lboard_data =
      (rc_client_submit_leaderboard_entry_callback_data_t*)callback_data->data;

  rc_client_submit_leaderboard_entry_server_call(lboard_data);
}

static void rc_client_submit_leaderboard_entry_callback(const char* server_response_body, int http_status_code, void* callback_data)
{
  rc_client_submit_leaderboard_entry_callback_data_t* lboard_data =
      (rc_client_submit_leaderboard_entry_callback_data_t*)callback_data;
  rc_api_submit_lboard_entry_response_t submit_lboard_entry_response;

  int result = rc_api_process_submit_lboard_entry_response(&submit_lboard_entry_response, server_response_body);
  const char* error_message = rc_client_server_error_message(&result, http_status_code, &submit_lboard_entry_response.response);

  if (error_message) {
    RC_CLIENT_LOG_ERR(lboard_data->runtime, "Error submitting leaderboard entry %u: %s", lboard_data->id, error_message);

    if (submit_lboard_entry_response.response.error_message) {
      rc_client_raise_server_error_event(lboard_data->runtime, "submit_lboard_entry", submit_lboard_entry_response.response.error_message);
    }
    else if (lboard_data->retry_count++ == 0) {
      /* first retry is immediate */
      rc_client_submit_leaderboard_entry_server_call(lboard_data);
      return;
    }
    else {
      if (!lboard_data->scheduled_callback_data) {
        lboard_data->scheduled_callback_data = (rc_client_scheduled_callback_data_t*)calloc(1, sizeof(*lboard_data->scheduled_callback_data));
        lboard_data->scheduled_callback_data->callback = rc_client_submit_leaderboard_entry_retry;
        lboard_data->scheduled_callback_data->data = lboard_data;
        lboard_data->scheduled_callback_data->related_id = lboard_data->id;
      }

      /* double wait time between each attempt until we hit a maximum delay of two minutes */
      /* 1s -> 2s -> 4s -> 8s -> 16s -> 32s -> 64s -> 120s -> 120s -> 120s ...*/
      if (lboard_data->retry_count > 8)
        lboard_data->scheduled_callback_data->when = time(NULL) + 120;
      else
        lboard_data->scheduled_callback_data->when = time(NULL) + (time_t)(1 << (lboard_data->retry_count - 1));

      rc_client_schedule_callback(lboard_data->runtime, lboard_data->scheduled_callback_data);
      return;
    }
  }
  else {
    /* not currently doing anything with the response */
    if (lboard_data->retry_count) {
      RC_CLIENT_LOG_INFO(lboard_data->runtime, "Leaderboard %u submission %d completed after %u attempts",
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
  api_params.username = lboard_data->runtime->user.username;
  api_params.api_token = lboard_data->runtime->user.token;
  api_params.leaderboard_id = lboard_data->id;
  api_params.score = lboard_data->score;
  api_params.game_hash = lboard_data->game_hash;

  result = rc_api_init_submit_lboard_entry_request(&request, &api_params);
  if (result != RC_OK) {
    RC_CLIENT_LOG_ERR(lboard_data->runtime, "Error constructing submit leaderboard entry for leaderboard %u: %s", lboard_data->id, rc_error_str(result));
    return;
  }

  lboard_data->runtime->callbacks.server_call(&request, rc_client_submit_leaderboard_entry_callback, lboard_data, lboard_data->runtime);

  rc_api_destroy_request(&request);
}

static void rc_client_submit_leaderboard_entry(rc_client_t* runtime, rc_client_leaderboard_info_t* leaderboard)
{
  rc_client_submit_leaderboard_entry_callback_data_t* callback_data;

  /* don't actually submit leaderboard entries when spectating */
  if (runtime->state.spectator_mode) {
    RC_CLIENT_LOG_INFO(runtime, "Spectated %s (%d) for leaderboard %u: %s",
        leaderboard->public.tracker_value, leaderboard->value, leaderboard->public.id, leaderboard->public.title);
    return;
  }

  callback_data = (rc_client_submit_leaderboard_entry_callback_data_t*)calloc(1, sizeof(*callback_data));
  callback_data->runtime = runtime;
  callback_data->id = leaderboard->public.id;
  callback_data->score = leaderboard->value;
  callback_data->game_hash = runtime->game->public.hash;
  callback_data->submit_time = time(NULL);

  RC_CLIENT_LOG_INFO(runtime, "Submitting %s (%d) for leaderboard %u: %s",
      leaderboard->public.tracker_value, leaderboard->value, leaderboard->public.id, leaderboard->public.title);
  rc_client_submit_leaderboard_entry_server_call(callback_data);
}

static void rc_client_reset_leaderboards(rc_client_t* runtime)
{
  rc_client_leaderboard_info_t* leaderboard = runtime->game->leaderboards;
  rc_client_leaderboard_info_t* stop = leaderboard + runtime->game->public.num_leaderboards;

  for (; leaderboard < stop; ++leaderboard) {
    rc_lboard_t* lboard = leaderboard->lboard;
    if (!lboard)
      continue;

    switch (leaderboard->public.state)
    {
      case RC_CLIENT_LEADERBOARD_STATE_INACTIVE:
      case RC_CLIENT_LEADERBOARD_STATE_DISABLED:
        continue;

      case RC_CLIENT_LEADERBOARD_STATE_TRACKING:
        rc_client_release_leaderboard_tracker(runtime->game, leaderboard);
        /* fallthrough to default */
      default:
        leaderboard->public.state = RC_CLIENT_LEADERBOARD_STATE_ACTIVE;
        rc_reset_lboard(lboard);
        break;
    }
  }
}

/* ===== Rich Presence ===== */

static void rc_client_ping_callback(const char* server_response_body, int http_status_code, void* callback_data)
{
  rc_client_t* runtime = (rc_client_t*)callback_data;
  rc_api_ping_response_t response;

  int result = rc_api_process_ping_response(&response, server_response_body);
  const char* error_message = rc_client_server_error_message(&result, http_status_code, &response.response);
  if (error_message) {
    RC_CLIENT_LOG_WARN(runtime, "Ping response error: %s", error_message);
  }

  rc_api_destroy_ping_response(&response);
}

static void rc_client_ping(rc_client_scheduled_callback_data_t* callback_data, rc_client_t* runtime, time_t now)
{
  rc_api_ping_request_t api_params;
  rc_api_request_t request;
  char buffer[256];
  int result;

  rc_runtime_get_richpresence(&runtime->game->runtime, buffer, sizeof(buffer),
      runtime->state.legacy_peek, runtime, NULL);

  memset(&api_params, 0, sizeof(api_params));
  api_params.username = runtime->user.username;
  api_params.api_token = runtime->user.token;
  api_params.game_id = runtime->game->public.id;
  api_params.rich_presence = buffer;

  result = rc_api_init_ping_request(&request, &api_params);
  if (result != RC_OK) {
    RC_CLIENT_LOG_WARN(runtime, "Error generating ping request: %s", rc_error_str(result));
  }
  else {
    runtime->callbacks.server_call(&request, rc_client_ping_callback, runtime, runtime);
  }

  callback_data->when = now + 120;
  rc_client_schedule_callback(runtime, callback_data);
}

size_t rc_client_get_rich_presence_message(rc_client_t* runtime, char buffer[], size_t buffer_size)
{
  if (!runtime->game || !buffer)
    return 0;

  return rc_runtime_get_richpresence(&runtime->game->runtime, buffer, (unsigned)buffer_size,
      runtime->state.legacy_peek, runtime, NULL);
}

/* ===== Processing ===== */

void rc_client_set_event_handler(rc_client_t* runtime, rc_client_event_handler_t handler)
{
  runtime->callbacks.event_handler = handler;
}

static void rc_client_invalidate_processing_memref(rc_client_t* runtime)
{
  rc_memref_t** next_memref = &runtime->game->runtime.memrefs;
  rc_memref_t* memref;

  /* invalid memref. remove from chain so we don't have to evaluate it in the future.
   * it's still there, so anything referencing it will always fetch the current value. */
  while ((memref = *next_memref) != NULL) {
    if (memref == runtime->state.processing_memref) {
      *next_memref = memref->next;
      break;
    }
    next_memref = &memref->next;
  }

  rc_client_invalidate_memref_achievements(runtime->game, runtime, runtime->state.processing_memref);
  rc_client_invalidate_memref_leaderboards(runtime->game, runtime, runtime->state.processing_memref);

  runtime->state.processing_memref = NULL;
}

static unsigned rc_client_peek_le(unsigned address, unsigned num_bytes, void* ud)
{
  rc_client_t* runtime = (rc_client_t*)ud;
  unsigned value = 0;
  uint32_t num_read = 0;

  if (num_bytes <= sizeof(value)) {
    num_read = runtime->callbacks.read_memory(address, (uint8_t*)&value, num_bytes, runtime);
    if (num_read == num_bytes)
      return value;
  }

  if (num_read < num_bytes)
    rc_client_invalidate_processing_memref(runtime);

  return 0;
}

static unsigned rc_client_peek(unsigned address, unsigned num_bytes, void* ud)
{
  rc_client_t* runtime = (rc_client_t*)ud;
  uint8_t buffer[4];
  uint32_t num_read = 0;

  switch (num_bytes) {
    case 1:
      num_read = runtime->callbacks.read_memory(address, buffer, 1, runtime);
      if (num_read == 1)
        return buffer[0];
      break;
    case 2:
      num_read = runtime->callbacks.read_memory(address, buffer, 2, runtime);
      if (num_read == 2)
        return buffer[0] | (buffer[1] << 8);
      break;
    case 3:
      num_read = runtime->callbacks.read_memory(address, buffer, 3, runtime);
      if (num_read == 3)
        return buffer[0] | (buffer[1] << 8) | (buffer[2] << 16);
      break;
    case 4:
      num_read = runtime->callbacks.read_memory(address, buffer, 4, runtime);
      if (num_read == 4)
        return buffer[0] | (buffer[1] << 8) | (buffer[2] << 16) | (buffer[3] << 24);
      break;
    default:
      break;
  }

  if (num_read < num_bytes)
    rc_client_invalidate_processing_memref(runtime);

  return 0;
}

void rc_client_set_legacy_peek(rc_client_t* runtime, int method)
{
  if (method == RC_CLIENT_LEGACY_PEEK_AUTO) {
    uint8_t buffer[4] = { 1,0,0,0 };
    method = (*((uint32_t*)buffer) == 1) ?
        RC_CLIENT_LEGACY_PEEK_LITTLE_ENDIAN_READS : RC_CLIENT_LEGACY_PEEK_CONSTRUCTED;
  }

  runtime->state.legacy_peek = (method == RC_CLIENT_LEGACY_PEEK_LITTLE_ENDIAN_READS) ?
      rc_client_peek_le : rc_client_peek;
}

static void rc_client_update_memref_values(rc_client_t* runtime)
{
  rc_memref_t* memref = runtime->game->runtime.memrefs;
  unsigned value;
  int invalidated_memref = 0;

  for (; memref; memref = memref->next) {
    if (memref->value.is_indirect)
      continue;

    runtime->state.processing_memref = memref;

    value = rc_peek_value(memref->address, memref->value.size, runtime->state.legacy_peek, runtime);

    if (runtime->state.processing_memref) {
      rc_update_memref_value(&memref->value, value);
    }
    else {
      /* if the peek function cleared the processing_memref, the memref was invalidated */
      invalidated_memref = 1;
    }
  }

  runtime->state.processing_memref = NULL;

  if (invalidated_memref)
    rc_client_update_active_achievements(runtime->game);
}

static void rc_client_do_frame_process_achievements(rc_client_t* runtime)
{
  rc_client_achievement_info_t* achievement = runtime->game->achievements;
  rc_client_achievement_info_t* stop = achievement + runtime->game->public.num_achievements;

  for (; achievement < stop; ++achievement) {
    rc_trigger_t* trigger = achievement->trigger;
    int old_state, new_state;
    unsigned old_measured_value;

    if (!trigger || achievement->public.state != RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE)
      continue;

    old_measured_value = trigger->measured_value;
    old_state = trigger->state;
    new_state = rc_evaluate_trigger(trigger, runtime->state.legacy_peek, runtime, NULL);

    /* if the measured value changed and the achievement hasn't triggered, send a notification */
    if (trigger->measured_value != old_measured_value && old_measured_value != RC_MEASURED_UNKNOWN &&
        trigger->measured_target != 0 && trigger->measured_value <= trigger->measured_target &&
        new_state != RC_TRIGGER_STATE_TRIGGERED &&
        new_state != RC_TRIGGER_STATE_INACTIVE && new_state != RC_TRIGGER_STATE_WAITING) {

      if (trigger->measured_as_percent) {
        /* if reporting measured value as a percentage, only send the notification if the percentage changes */
        const unsigned old_percent = (unsigned)(((unsigned long long)old_measured_value * 100) / trigger->measured_target);
        const unsigned new_percent = (unsigned)(((unsigned long long)trigger->measured_value * 100) / trigger->measured_target);
        if (old_percent != new_percent)
          achievement->pending_events |= RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_PROGRESS_UPDATED;
      }
      else {
        achievement->pending_events |= RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_PROGRESS_UPDATED;
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
  }
}

static void rc_client_raise_achievement_events(rc_client_t* runtime)
{
  rc_client_achievement_info_t* achievement = runtime->game->achievements;
  rc_client_achievement_info_t* stop = achievement + runtime->game->public.num_achievements;
  rc_client_event_t client_event;
  time_t recent_unlock_time = 0;
  int achievements_unlocked = 0;

  memset(&client_event, 0, sizeof(client_event));
  client_event.client = runtime;

  for (; achievement < stop; ++achievement) {
    if (achievement->pending_events == RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_NONE)
      continue;

    /* kick off award achievement request first */
    if (achievement->pending_events & RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_TRIGGERED) {
      rc_client_award_achievement(runtime, achievement);
      achievements_unlocked = 1;
    }

    /* update display state */
    if (recent_unlock_time == 0)
      recent_unlock_time = time(NULL) - RC_CLIENT_RECENT_UNLOCK_DELAY_SECONDS;
    rc_client_update_achievement_display_information(runtime, achievement, recent_unlock_time);

    /* raise events*/
    client_event.achievement = &achievement->public;

    if (achievement->pending_events & RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_HIDE) {
      client_event.type = RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE;
      runtime->callbacks.event_handler(&client_event);
    }
    else if (achievement->pending_events & RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_SHOW) {
      client_event.type = RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW;
      runtime->callbacks.event_handler(&client_event);
    }

    if (achievement->pending_events & RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_PROGRESS_UPDATED) {
      client_event.type = RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_UPDATED;
      runtime->callbacks.event_handler(&client_event);
    }

    if (achievement->pending_events & RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_TRIGGERED) {
      client_event.type = RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED;
      runtime->callbacks.event_handler(&client_event);
    }

    /* clear pending flags */
    achievement->pending_events = RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_NONE;
  }

  /* raise mastery event if pending */
  if (runtime->game->mastery == RC_CLIENT_MASTERY_STATE_PENDING) {
    runtime->game->mastery = RC_CLIENT_MASTERY_STATE_SHOWN;

    client_event.type = RC_CLIENT_EVENT_GAME_COMPLETED;
    client_event.achievement = NULL;
    runtime->callbacks.event_handler(&client_event);
  }

  /* if any achievements were unlocked, resync the active achievements list */
  if (achievements_unlocked) {
    rc_mutex_lock(&runtime->state.mutex);
    rc_client_update_active_achievements(runtime->game);
    rc_mutex_unlock(&runtime->state.mutex);
  }
}

static void rc_client_do_frame_process_leaderboards(rc_client_t* runtime)
{
  rc_client_leaderboard_info_t* leaderboard = runtime->game->leaderboards;
  rc_client_leaderboard_info_t* stop = leaderboard + runtime->game->public.num_leaderboards;

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
    new_state = rc_evaluate_lboard(lboard, &leaderboard->value, runtime->state.legacy_peek, runtime, NULL);

    switch (new_state) {
      case RC_LBOARD_STATE_STARTED: /* leaderboard is running */
        if (old_state != RC_LBOARD_STATE_STARTED) {
          leaderboard->public.state = RC_CLIENT_LEADERBOARD_STATE_TRACKING;
          leaderboard->pending_events |= RC_CLIENT_LEADERBOARD_PENDING_EVENT_STARTED;
          rc_client_allocate_leaderboard_tracker(runtime->game, leaderboard);
        }
        else {
          rc_client_update_leaderboard_tracker(runtime->game, leaderboard);
        }
        break;

      case RC_LBOARD_STATE_CANCELED:
        if (old_state != RC_LBOARD_STATE_CANCELED) {
          leaderboard->public.state = RC_CLIENT_LEADERBOARD_STATE_ACTIVE;
          leaderboard->pending_events |= RC_CLIENT_LEADERBOARD_PENDING_EVENT_FAILED;
          rc_client_release_leaderboard_tracker(runtime->game, leaderboard);
        }
        break;

      case RC_LBOARD_STATE_TRIGGERED:
        if (old_state != RC_RUNTIME_EVENT_LBOARD_TRIGGERED) {
          leaderboard->public.state = RC_CLIENT_LEADERBOARD_STATE_ACTIVE;
          leaderboard->pending_events |= RC_CLIENT_LEADERBOARD_PENDING_EVENT_SUBMITTED;
          rc_client_release_leaderboard_tracker(runtime->game, leaderboard);
        }
        break;
    }
  }
}

static void rc_client_raise_leaderboard_events(rc_client_t* runtime)
{
  rc_client_leaderboard_info_t* leaderboard;
  rc_client_leaderboard_info_t* leaderboard_stop;
  rc_client_leaderboard_tracker_info_t* tracker;
  rc_client_leaderboard_tracker_info_t* tracker_stop;
  rc_client_event_t client_event;
  time_t recent_unlock_time = 0;
  int leaderboards_unlocked = 0;

  if (runtime->game->public.num_leaderboards == 0)
    return;

  memset(&client_event, 0, sizeof(client_event));
  client_event.client = runtime;

  /* process tracker events first so formatted values are updated for leaderboard events */
  tracker = runtime->game->leaderboard_trackers;
  tracker_stop = tracker + runtime->game->leaderboard_trackers_size;
  for (; tracker < tracker_stop; ++tracker) {
    if (tracker->pending_events == RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_NONE)
      continue;

    client_event.leaderboard_tracker = &tracker->public;

    if (tracker->pending_events & RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_HIDE) {
      /* if this is the last tracker in the list, shrink the list */
      if (tracker->public.id == runtime->game->leaderboard_trackers_size) {
        rc_client_leaderboard_tracker_info_t* scan = tracker;
        while (--runtime->game->leaderboard_trackers_size > 0) {
          --scan;
          if (scan->reference_count != 0)
            break;
        }
      }

      client_event.type = RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE;
      runtime->callbacks.event_handler(&client_event);
    }
    else {
      rc_format_value(tracker->public.display, sizeof(tracker->public.display), tracker->raw_value, tracker->format);

      if (tracker->pending_events & RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_SHOW) {
        client_event.type = RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW;
        runtime->callbacks.event_handler(&client_event);
      }
      else if (tracker->pending_events & RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_UPDATE) {
        client_event.type = RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE;
        runtime->callbacks.event_handler(&client_event);
      }
    }

    tracker->pending_events = RC_CLIENT_LEADERBOARD_PENDING_EVENT_NONE;
  }

  leaderboard = runtime->game->leaderboards;
  leaderboard_stop = leaderboard + runtime->game->public.num_leaderboards;
  for (; leaderboard < leaderboard_stop; ++leaderboard) {
    if (leaderboard->pending_events == RC_CLIENT_LEADERBOARD_PENDING_EVENT_NONE)
      continue;

    client_event.leaderboard = &leaderboard->public;

    if (leaderboard->pending_events & RC_CLIENT_LEADERBOARD_PENDING_EVENT_FAILED) {
      client_event.type = RC_CLIENT_EVENT_LEADERBOARD_FAILED;
      runtime->callbacks.event_handler(&client_event);
    }
    else if (leaderboard->pending_events & RC_CLIENT_LEADERBOARD_PENDING_EVENT_SUBMITTED) {
      /* kick off submission request before raising event */
      rc_client_submit_leaderboard_entry(runtime, leaderboard);

      client_event.type = RC_CLIENT_EVENT_LEADERBOARD_SUBMITTED;
      runtime->callbacks.event_handler(&client_event);
    }
    else if (leaderboard->pending_events & RC_CLIENT_LEADERBOARD_PENDING_EVENT_STARTED) {
      client_event.type = RC_CLIENT_EVENT_LEADERBOARD_STARTED;
      runtime->callbacks.event_handler(&client_event);
    }

    leaderboard->pending_events = RC_CLIENT_LEADERBOARD_PENDING_EVENT_NONE;
  }
}

void rc_client_do_frame(rc_client_t* runtime)
{
  if (runtime->game && !runtime->game->waiting_for_reset) {
    rc_runtime_richpresence_t* richpresence;

    rc_mutex_lock(&runtime->state.mutex);

    rc_client_update_memref_values(runtime);
    rc_update_variables(runtime->game->runtime.variables, runtime->state.legacy_peek, runtime, NULL);

    rc_client_do_frame_process_achievements(runtime);
    if (runtime->state.hardcore)
      rc_client_do_frame_process_leaderboards(runtime);

    richpresence = runtime->game->runtime.richpresence;
    if (richpresence && richpresence->richpresence)
      rc_update_richpresence(richpresence->richpresence, runtime->state.legacy_peek, runtime, NULL);

    rc_mutex_unlock(&runtime->state.mutex);

    rc_client_raise_achievement_events(runtime);
    if (runtime->state.hardcore)
      rc_client_raise_leaderboard_events(runtime);
  }

  rc_client_idle(runtime);
}

void rc_client_idle(rc_client_t* runtime)
{
  rc_client_scheduled_callback_data_t* scheduled_callback = runtime->state.scheduled_callbacks;

  if (scheduled_callback) {
    const time_t now = time(NULL);

    do
    {
      rc_mutex_lock(&runtime->state.mutex);
      scheduled_callback = runtime->state.scheduled_callbacks;
      if (scheduled_callback) {
        if (scheduled_callback->when > now) {
          /* not time for next callback yet, ignore it */
          scheduled_callback = NULL;
        }
        else {
          /* remove the callback from the queue while we process it. callback can requeue if desired */
          runtime->state.scheduled_callbacks = scheduled_callback->next;
        }
      }
      rc_mutex_unlock(&runtime->state.mutex);

      if (!scheduled_callback)
        break;

      scheduled_callback->callback(scheduled_callback, runtime, now);
    } while (1);
  }
}

void rc_client_schedule_callback(rc_client_t* runtime, rc_client_scheduled_callback_data_t* scheduled_callback)
{
  rc_client_scheduled_callback_data_t** last;
  rc_client_scheduled_callback_data_t* next;

  rc_mutex_lock(&runtime->state.mutex);

  last = &runtime->state.scheduled_callbacks;
  do
  {
    next = *last;
    if (next == NULL || next->when > scheduled_callback->when) {
      scheduled_callback->next = next;
      *last = scheduled_callback;
      break;
    }

    last = &next->next;
  } while (1);

  rc_mutex_unlock(&runtime->state.mutex);
}

static void rc_client_reset_richpresence(rc_client_t* runtime)
{
  rc_runtime_richpresence_t* richpresence = runtime->game->runtime.richpresence;
  if (richpresence && richpresence->richpresence)
    rc_reset_richpresence(richpresence->richpresence);
}

static void rc_client_reset_variables(rc_client_t* runtime)
{
  rc_value_t* variable = runtime->game->runtime.variables;
  for (; variable; variable = variable->next)
    rc_reset_value(variable);
}

void rc_client_reset(rc_client_t* runtime)
{
  rc_client_game_hash_t* game_hash;
  if (!runtime->game)
    return;

  game_hash = rc_client_find_game_hash(runtime, runtime->game->public.hash);
  if (game_hash && game_hash->game_id != runtime->game->public.id) {
    /* current media is not for loaded game. unload game */
    RC_CLIENT_LOG_WARN(runtime, "Disabling runtime. Reset with non-game media loaded: %u (%s)",
        (game_hash->game_id == RC_CLIENT_UNKNOWN_GAME_ID) ? 0 : game_hash->game_id, game_hash->hash);
    rc_client_unload_game(runtime);
    return;
  }

  RC_CLIENT_LOG_INFO(runtime, "Resetting runtime");

  rc_mutex_lock(&runtime->state.mutex);

  runtime->game->waiting_for_reset = 0;
  rc_client_reset_achievements(runtime);
  rc_client_reset_leaderboards(runtime);
  rc_client_reset_richpresence(runtime);
  rc_client_reset_variables(runtime);

  rc_mutex_unlock(&runtime->state.mutex);

  rc_client_raise_achievement_events(runtime);
  rc_client_raise_leaderboard_events(runtime);
}

size_t rc_client_progress_size(rc_client_t* runtime)
{
  size_t result;

  if (!runtime->game)
    return 0;

  rc_mutex_lock(&runtime->state.mutex);
  result = rc_runtime_progress_size(&runtime->game->runtime, NULL);
  rc_mutex_unlock(&runtime->state.mutex);

  return result;
}

int rc_client_serialize_progress(rc_client_t* runtime, uint8_t* buffer)
{
  int result;

  if (!runtime->game)
    return RC_NO_GAME_LOADED;

  if (!buffer)
    return RC_INVALID_STATE;

  rc_mutex_lock(&runtime->state.mutex);
  result = rc_runtime_serialize_progress(buffer, &runtime->game->runtime, NULL);
  rc_mutex_unlock(&runtime->state.mutex);

  return result;
}

int rc_client_deserialize_progress(rc_client_t* runtime, const uint8_t* serialized)
{
  rc_client_achievement_info_t* achievement;
  rc_client_achievement_info_t* achievement_stop;
  rc_client_leaderboard_info_t* leaderboard;
  rc_client_leaderboard_info_t* leaderboard_stop;
  int result;

  if (!runtime->game)
    return RC_NO_GAME_LOADED;

  rc_mutex_lock(&runtime->state.mutex);

  /* flag any visible challenge indicators to be hidden */
  achievement = runtime->game->achievements;
  achievement_stop = achievement + runtime->game->public.num_achievements;
  for (; achievement < achievement_stop; ++achievement) {
    rc_trigger_t* trigger = achievement->trigger;
    if (trigger && trigger->state == RC_TRIGGER_STATE_PRIMED &&
        achievement->public.state == RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE) {
      achievement->pending_events |= RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_HIDE;
    }
  }

  /* flag any visible trackers to be hidden */
  leaderboard = runtime->game->leaderboards;
  leaderboard_stop = leaderboard + runtime->game->public.num_leaderboards;
  for (; leaderboard < leaderboard_stop; ++leaderboard) {
    rc_lboard_t* lboard = leaderboard->lboard;
    if (lboard && lboard->state == RC_LBOARD_STATE_STARTED &&
        leaderboard->public.state == RC_CLIENT_LEADERBOARD_STATE_TRACKING) {
      leaderboard->pending_events |= RC_CLIENT_LEADERBOARD_PENDING_EVENT_FAILED;
    }
  }

  /* deserialize the runtime state */
  result = rc_runtime_deserialize_progress(&runtime->game->runtime, serialized, NULL);

  /* flag any challenge indicators that should be shown */
  for (achievement = runtime->game->achievements; achievement < achievement_stop; ++achievement) {
    rc_trigger_t* trigger = achievement->trigger;
    if (!trigger || achievement->public.state != RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE)
      continue;

    if (trigger->state == RC_TRIGGER_STATE_PRIMED) {
      /* if it's already shown, just keep it. otherwise flag it to be shown */
      if (achievement->pending_events & RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_HIDE)
        achievement->pending_events &= ~RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_HIDE;
      else
        achievement->pending_events |= RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_SHOW;
    }
    /* ASSERT: only active achievements are serialized, so we don't have to worry about
     *         deserialization deactiving them. */
  }

  /* flag any trackers that need to be shown */
  for (leaderboard = runtime->game->leaderboards; leaderboard < leaderboard_stop; ++leaderboard) {
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
        rc_client_update_leaderboard_tracker(runtime->game, leaderboard);
      }
      else {
        rc_client_allocate_leaderboard_tracker(runtime->game, leaderboard);
      }
    }
    else if (leaderboard->pending_events & RC_CLIENT_LEADERBOARD_PENDING_EVENT_FAILED) {
      /* deallocate the tracker (don't actually raise the failed event) */
      leaderboard->pending_events &= ~RC_CLIENT_LEADERBOARD_PENDING_EVENT_FAILED;
      leaderboard->public.state = RC_CLIENT_LEADERBOARD_STATE_ACTIVE;
      rc_client_release_leaderboard_tracker(runtime->game, leaderboard);
    }
  }

  rc_mutex_unlock(&runtime->state.mutex);

  rc_client_raise_achievement_events(runtime);
  rc_client_raise_leaderboard_events(runtime);

  return result;
}

/* ===== Toggles ===== */

static void rc_client_enable_hardcore(rc_client_t* runtime)
{
  runtime->state.hardcore = 1;

  if (runtime->game) {
    rc_client_toggle_hardcore_achievements(runtime->game, runtime, RC_CLIENT_ACHIEVEMENT_UNLOCKED_HARDCORE);
    rc_client_activate_leaderboards(runtime->game, runtime);

    /* disable processing until the client acknowledges the reset event by calling rc_runtime_reset() */
    RC_CLIENT_LOG_INFO(runtime, "Hardcore enabled, waiting for reset");
    runtime->game->waiting_for_reset = 1;
  }
  else {
    RC_CLIENT_LOG_INFO(runtime, "Hardcore enabled");
  }
}

static void rc_client_disable_hardcore(rc_client_t* runtime)
{
  runtime->state.hardcore = 0;
  RC_CLIENT_LOG_INFO(runtime, "Hardcore disabled");

  if (runtime->game) {
    rc_client_toggle_hardcore_achievements(runtime->game, runtime, RC_CLIENT_ACHIEVEMENT_UNLOCKED_SOFTCORE);
    rc_client_deactivate_leaderboards(runtime->game, runtime);
  }
}

void rc_client_set_hardcore_enabled(rc_client_t* runtime, int enabled)
{
  int changed = 0;
  rc_mutex_lock(&runtime->state.mutex);

  if (runtime->state.hardcore != enabled) {
    if (enabled)
      rc_client_enable_hardcore(runtime);
    else
      rc_client_disable_hardcore(runtime);

    changed = 1;
  }

  rc_mutex_unlock(&runtime->state.mutex);

  /* events must be raised outside of lock */
  if (changed && runtime->game) {
    if (enabled) {
      /* if enabling hardcore, notify client that a reset is requested */
      if (runtime->game->waiting_for_reset) {
        rc_client_event_t client_event;
        memset(&client_event, 0, sizeof(client_event));
        client_event.type = RC_CLIENT_EVENT_RESET;
        client_event.client = runtime;
        runtime->callbacks.event_handler(&client_event);
      }
    }
    else {
      /* if disabling hardcore, leaderboards will be deactivated. raise events for hiding trackers */
      rc_client_raise_leaderboard_events(runtime);
    }
  }
}

int rc_client_get_hardcore_enabled(const rc_client_t* client)
{
  return client->state.hardcore;
}

void rc_client_set_test_unofficial(rc_client_t* runtime, int enabled)
{
  runtime->state.test_unofficial = enabled ? 1 : 0;
}

int rc_client_get_test_unofficial(const rc_client_t* client)
{
  return client->state.test_unofficial;
}

void rc_client_set_encore_mode_enabled(rc_client_t* runtime, int enabled)
{
  runtime->state.encore_mode = enabled ? 1 : 0;
}

int rc_client_get_encore_mode_enabled(const rc_client_t* client)
{
  return client->state.encore_mode;
}

void rc_client_set_spectator_mode_enabled(rc_client_t* runtime, int enabled)
{
  runtime->state.spectator_mode = enabled ? 1 : 0;
}

int rc_client_get_spectator_mode_enabled(const rc_client_t* client)
{
  return client->state.spectator_mode;
}

void rc_client_set_user_data(rc_client_t* runtime, void* userdata)
{
  runtime->callbacks.client_data = userdata;
}

void* rc_client_get_user_data(const rc_client_t* client)
{
  return client->callbacks.client_data;
}

void rc_client_set_host(const rc_client_t* client, const char* host)
{
  /* if empty, just pass NULL */
  if (host && !host[0])
    host = NULL;

  /* clear the image host so it'll use the custom host for images too */
  rc_api_set_image_host(NULL);

  /* set the custom host */
  rc_api_set_host(host);
}
