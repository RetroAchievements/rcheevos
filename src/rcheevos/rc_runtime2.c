#include "rc_runtime2_internal.h"

#include "rc_api_runtime.h"
#include "rc_api_user.h"
#include "rc_internal.h"

#include "../rapi/rc_api_common.h"

#include <stdarg.h>

#define RC_UNKNOWN_GAME_ID (uint32_t)-1

typedef struct rc_runtime2_generic_callback_data_t {
  rc_runtime2_t* runtime;
  rc_runtime2_callback_t callback;
} rc_runtime2_generic_callback_data_t;

static void rc_runtime2_begin_fetch_game_data(rc_runtime2_load_state_t* callback_data);

/* ===== Construction/Destruction ===== */

rc_runtime2_t* rc_runtime2_create(rc_runtime2_read_memory_t read_memory_function, rc_runtime2_server_call_t server_call_function)
{
  rc_runtime2_t* runtime = (rc_runtime2_t*)calloc(1, sizeof(rc_runtime2_t));
  if (!runtime)
    return NULL;

  runtime->callbacks.read_memory = read_memory_function;
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

  rc_runtime2_unload_game(runtime);

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
    rc_runtime2_load_state_t* load_state;
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
    load_state = runtime->state.load;
    rc_mutex_unlock(&runtime->state.mutex);

    RC_RUNTIME2_LOG_INFO(runtime, "Login succeeded: %s", error_message);

    if (load_state && load_state->progress == RC_RUNTIME2_LOAD_STATE_AWAIT_LOGIN)
      rc_runtime2_begin_fetch_game_data(load_state);

    if (login_callback_data->callback)
      login_callback_data->callback(RC_OK, NULL, runtime);
  }

  rc_api_destroy_login_response(&login_response);
  free(login_callback_data);
}

static void rc_runtime2_begin_login(rc_runtime2_t* runtime,
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
  rc_api_destroy_request(&request);
}

void rc_runtime2_begin_login_with_password(rc_runtime2_t* runtime,
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
  rc_runtime2_begin_login(runtime, &login_request, callback);
}

void rc_runtime2_begin_login_with_token(rc_runtime2_t* runtime,
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
  rc_runtime2_begin_login(runtime, &login_request, callback);
}

const rc_runtime2_user_t* rc_runtime2_user_info(const rc_runtime2_t* runtime)
{
  return (runtime->state.user == RC_RUNTIME2_USER_STATE_LOGGED_IN) ? &runtime->user : NULL;
}

/* ===== load game ===== */

static void rc_runtime2_free_game(rc_runtime2_game_info_t* game)
{
  rc_runtime_destroy(&game->runtime);

  rc_buf_destroy(&game->buffer);

  free(game);
}

static void rc_runtime2_free_load_state(rc_runtime2_load_state_t* load_state)
{
  if (load_state->game)
    rc_runtime2_free_game(load_state->game);

  free(load_state);
}

static void rc_runtime2_begin_load_state(rc_runtime2_load_state_t* load_state, uint8_t state, uint8_t num_requests)
{
  rc_mutex_lock(&load_state->runtime->state.mutex);

  load_state->progress = state;
  load_state->outstanding_requests += num_requests;

  rc_mutex_unlock(&load_state->runtime->state.mutex);
}

static int rc_runtime2_end_load_state(rc_runtime2_load_state_t* load_state)
{
  int remaining_requests = 0;

  rc_mutex_lock(&load_state->runtime->state.mutex);

  if (load_state->outstanding_requests > 0)
    --load_state->outstanding_requests;
  remaining_requests = load_state->outstanding_requests;

  if (load_state->runtime->state.load != load_state)
    remaining_requests = -1;

  rc_mutex_unlock(&load_state->runtime->state.mutex);

  if (remaining_requests < 0)
    rc_runtime2_free_load_state(load_state);

  return remaining_requests;
}

static void rc_runtime2_load_error(rc_runtime2_load_state_t* load_state, int result, const char* error_message)
{
  rc_mutex_lock(&load_state->runtime->state.mutex);
  load_state->progress = RC_RUNTIME2_LOAD_STATE_UNKNOWN_GAME;
  if (load_state->runtime->state.load == load_state)
    load_state->runtime->state.load = NULL;
  rc_mutex_unlock(&load_state->runtime->state.mutex);

  if (load_state->callback)
    load_state->callback(result, error_message, load_state->runtime);

  rc_runtime2_free_load_state(load_state);
}

static void rc_runtime2_activate_game(rc_runtime2_load_state_t* load_state)
{
  rc_runtime2_t* runtime = load_state->runtime;

  rc_mutex_lock(&load_state->runtime->state.mutex);
  load_state->progress = (runtime->state.load == load_state) ?
      RC_RUNTIME2_LOAD_STATE_DONE : RC_RUNTIME2_LOAD_STATE_UNKNOWN_GAME;
  runtime->state.load = NULL;
  rc_mutex_unlock(&load_state->runtime->state.mutex);

  if (load_state->progress != RC_RUNTIME2_LOAD_STATE_DONE)
  {
    /* previous load state was aborted, silently quit */
  }
  else
  {
    // TODO: activate achievements/leaderboards

    rc_mutex_lock(&load_state->runtime->state.mutex);
    if (runtime->state.load == NULL)
      runtime->game = load_state->game;
    rc_mutex_unlock(&load_state->runtime->state.mutex);

    if (runtime->game != load_state->game)
    {
      /* previous load state was aborted, silently quit */
    }
    else
    {
      if (load_state->callback)
        load_state->callback(RC_OK, NULL, runtime);

      // TODO: raise game loaded event?

      /* detach the game object so it doesn't get freed by free_load_state */
      load_state->game = NULL;
    }
  }

  rc_runtime2_free_load_state(load_state);
}

static void rc_runtime2_start_session_callback(const char* server_response_body, int http_status_code, void* callback_data)
{
  rc_runtime2_load_state_t* load_state = (rc_runtime2_load_state_t*)callback_data;
  rc_runtime2_t* runtime = load_state->runtime;
  rc_api_start_session_response_t start_session_response;

  int result = rc_api_process_start_session_response(&start_session_response, server_response_body);
  const char* error_message = rc_runtime2_server_error_message(&result, http_status_code, &start_session_response.response);

  int outstanding_requests = rc_runtime2_end_load_state(load_state);
  if (error_message)
  {
    rc_runtime2_load_error(callback_data, result, error_message);
  }
  else if (outstanding_requests < 0)
  {
    /* previous load state was aborted, silently quit */
  }
  else
  {
    if (outstanding_requests == 0)
      rc_runtime2_activate_game(load_state);
  }

  rc_api_destroy_start_session_response(&start_session_response);
}

static void rc_runtime2_unlocks_callback(const char* server_response_body, int http_status_code, void* callback_data, int hardcore)
{
  rc_runtime2_load_state_t* load_state = (rc_runtime2_load_state_t*)callback_data;
  rc_runtime2_t* runtime = load_state->runtime;
  rc_api_fetch_user_unlocks_response_t fetch_user_unlocks_response;

  int result = rc_api_process_fetch_user_unlocks_response(&fetch_user_unlocks_response, server_response_body);
  const char* error_message = rc_runtime2_server_error_message(&result, http_status_code, &fetch_user_unlocks_response.response);

  int outstanding_requests = rc_runtime2_end_load_state(load_state);
  if (error_message)
  {
    rc_runtime2_load_error(callback_data, result, error_message);
  }
  else if (outstanding_requests < 0)
  {
    /* previous load state was aborted, silently quit */
  }
  else
  {
    // TODO: process unlocks

    if (outstanding_requests == 0)
      rc_runtime2_activate_game(load_state);
  }

  rc_api_destroy_fetch_user_unlocks_response(&fetch_user_unlocks_response);
}

static void rc_runtime2_hardcore_unlocks_callback(const char* server_response_body, int http_status_code, void* callback_data)
{
  rc_runtime2_unlocks_callback(server_response_body, http_status_code, callback_data, 1);
}

static void rc_runtime2_softcore_unlocks_callback(const char* server_response_body, int http_status_code, void* callback_data)
{
  rc_runtime2_unlocks_callback(server_response_body, http_status_code, callback_data, 0);
}

static void rc_runtime2_begin_start_session(rc_runtime2_load_state_t* load_state)
{
  rc_api_start_session_request_t start_session_params;
  rc_api_fetch_user_unlocks_request_t unlock_params;
  rc_runtime2_t* runtime = load_state->runtime;
  rc_api_request_t start_session_request;
  rc_api_request_t hardcore_unlock_request;
  rc_api_request_t softcore_unlock_request;
  int result;

  memset(&start_session_params, 0, sizeof(start_session_params));
  start_session_params.username = runtime->user.username;
  start_session_params.api_token = runtime->user.token;
  start_session_params.game_id = load_state->game->public.id;

  result = rc_api_init_start_session_request(&start_session_request, &start_session_params);
  if (result != RC_OK)
  {
    rc_runtime2_load_error(load_state, result, rc_error_str(result));
  }
  else
  {
    memset(&unlock_params, 0, sizeof(unlock_params));
    unlock_params.username = runtime->user.username;
    unlock_params.api_token = runtime->user.token;
    unlock_params.game_id = load_state->game->public.id;
    unlock_params.hardcore = 1;

    result = rc_api_init_fetch_user_unlocks_request(&hardcore_unlock_request, &unlock_params);
    if (result != RC_OK)
    {
      rc_runtime2_load_error(load_state, result, rc_error_str(result));
    }
    else
    {
      unlock_params.hardcore = 0;

      result = rc_api_init_fetch_user_unlocks_request(&softcore_unlock_request, &unlock_params);
      if (result != RC_OK)
      {
        rc_runtime2_load_error(load_state, result, rc_error_str(result));
      }
      else
      {
        rc_runtime2_begin_load_state(load_state, RC_RUNTIME2_LOAD_STATE_STARTING_SESSION, 3);

        // TODO: create single server request to do all three of these
        runtime->callbacks.server_call(&start_session_request, rc_runtime2_start_session_callback, load_state, runtime);
        runtime->callbacks.server_call(&hardcore_unlock_request, rc_runtime2_hardcore_unlocks_callback, load_state, runtime);
        runtime->callbacks.server_call(&softcore_unlock_request, rc_runtime2_softcore_unlocks_callback, load_state, runtime);

        rc_api_destroy_request(&softcore_unlock_request);
      }

      rc_api_destroy_request(&hardcore_unlock_request);
    }

    rc_api_destroy_request(&start_session_request);
  }
}

static rc_runtime2_achievement_info_t* rc_runtime2_copy_achievements(rc_runtime2_load_state_t* load_state, rc_api_fetch_game_data_response_t* game_data)
{
  const rc_api_achievement_definition_t* read;
  const rc_api_achievement_definition_t* stop;
  rc_runtime2_achievement_info_t* achievements;
  rc_runtime2_achievement_info_t* achievement;
  rc_api_buffer_t* buffer;
  rc_parse_state_t parse;
  const char* memaddr;
  size_t size;
  int trigger_size;

  if (game_data->num_achievements == 0)
    return NULL;

  /* preallocate space for achievements */
  size = 24 /* assume average title length of 24 */
      + 48 /* assume average description length of 48 */
      + sizeof(rc_trigger_t) + sizeof(rc_condset_t) * 2 /* trigger container */
      + sizeof(rc_condition_t) * 8 /* assume average trigger length of 8 conditions */
      + sizeof(rc_runtime2_achievement_info_t);
  rc_buf_reserve(&load_state->game->buffer, size * game_data->num_achievements);

  /* allocate the achievement array */
  size = sizeof(rc_runtime2_achievement_info_t) * game_data->num_achievements;
  buffer = &load_state->game->buffer;
  achievement = achievements = rc_buf_alloc(buffer, size);
  memset(achievements, 0, size);

  /* copy the achievement data */
  read = game_data->achievements;
  stop = read + game_data->num_achievements;
  do
  {
    achievement->public.title = rc_buf_strcpy(buffer, read->title);
    achievement->public.description = rc_buf_strcpy(buffer, read->description);
    snprintf(achievement->public.badge_name, sizeof(achievement->public.badge_name), "%s", read->badge_name);
    achievement->public.id = read->id;
    achievement->public.points = read->points;
    achievement->public.is_unofficial = read->category != RC_ACHIEVEMENT_CATEGORY_CORE;

    memaddr = read->definition;
    rc_runtime_checksum(memaddr, achievement->md5);

    trigger_size = rc_trigger_size(memaddr);
    if (trigger_size < 0)
    {
      RC_RUNTIME2_LOG_WARN(load_state->runtime, "Parse error %d processing achievement %u", trigger_size, read->id);
      achievement->public.state = RC_RUNTIME2_ACHIEVEMENT_STATE_DISABLED;
    }
    else
    {
      /* populate the item, using the communal memrefs pool */
      rc_init_parse_state(&parse, rc_buf_reserve(buffer, trigger_size), NULL, 0);
      parse.first_memref = &load_state->game->runtime.memrefs;
      parse.variables = &load_state->game->runtime.variables;
      achievement->trigger = RC_ALLOC(rc_trigger_t, &parse);
      rc_parse_trigger_internal(achievement->trigger, &memaddr, &parse);

      if (parse.offset < 0) {
        RC_RUNTIME2_LOG_WARN(load_state->runtime, "Parse error %d processing achievement %u", parse.offset, read->id);
        achievement->public.state = RC_RUNTIME2_ACHIEVEMENT_STATE_DISABLED;
      } else {
        rc_buf_consume(buffer, parse.buffer, (char*)parse.buffer + parse.offset);
      }

      rc_destroy_parse_state(&parse);
    }

    ++achievement;
    ++read;
  } while (read < stop);

  return achievements;
}

static rc_runtime2_leaderboard_info_t* rc_runtime2_copy_leaderboards(rc_runtime2_load_state_t* load_state, rc_api_fetch_game_data_response_t* game_data)
{
  const rc_api_leaderboard_definition_t* read;
  const rc_api_leaderboard_definition_t* stop;
  rc_runtime2_leaderboard_info_t* leaderboards;
  rc_runtime2_leaderboard_info_t* leaderboard;
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
      + sizeof(rc_runtime2_leaderboard_info_t);
  rc_buf_reserve(&load_state->game->buffer, size * game_data->num_leaderboards);

  /* allocate the achievement array */
  size = sizeof(rc_runtime2_leaderboard_info_t) * game_data->num_leaderboards;
  buffer = &load_state->game->buffer;
  leaderboard = leaderboards = rc_buf_alloc(buffer, size);
  memset(leaderboards, 0, size);

  /* copy the achievement data */
  read = game_data->leaderboards;
  stop = read + game_data->num_leaderboards;
  do
  {
    leaderboard->public.title = rc_buf_strcpy(buffer, read->title);
    leaderboard->public.description = rc_buf_strcpy(buffer, read->description);
    leaderboard->public.id = read->id;
    leaderboard->public.format = (uint8_t)read->format;
    leaderboard->tracker_id = RC_LEADERBOARD_TRACKER_UNASSIGNED;

    memaddr = read->definition;
    rc_runtime_checksum(memaddr, leaderboard->md5);

    ptr = strstr(memaddr, "VAL:");
    if (ptr != NULL)
    {
      /* calculate the DJB2 hash of the VAL portion of the string*/
      uint32_t hash = 5381;
      while (*ptr && (ptr[0] != ':' || ptr[1] != ':'))
         hash = (hash << 5) + hash + *ptr++;
      leaderboard->value_djb2 = hash;
    }

    lboard_size = rc_lboard_size(memaddr);
    if (lboard_size < 0)
    {
      RC_RUNTIME2_LOG_WARN(load_state->runtime, "Parse error %d processing leaderboard %u", lboard_size, read->id);
      leaderboard->public.state = RC_RUNTIME2_LEADERBOARD_STATE_DISABLED;
    }
    else
    {
      /* populate the item, using the communal memrefs pool */
      rc_init_parse_state(&parse, rc_buf_reserve(buffer, lboard_size), NULL, 0);
      parse.first_memref = &load_state->game->runtime.memrefs;
      parse.variables = &load_state->game->runtime.variables;
      leaderboard->lboard = RC_ALLOC(rc_lboard_t, &parse);
      rc_parse_lboard_internal(leaderboard->lboard, memaddr, &parse);

      if (parse.offset < 0) {
        RC_RUNTIME2_LOG_WARN(load_state->runtime, "Parse error %d processing leaderboard %u", parse.offset, read->id);
        leaderboard->public.state = RC_RUNTIME2_LEADERBOARD_STATE_DISABLED;
      } else {
        rc_buf_consume(buffer, parse.buffer, (char*)parse.buffer + parse.offset);
      }

      rc_destroy_parse_state(&parse);
    }

    ++leaderboard;
    ++read;
  } while (read < stop);

  return leaderboards;
}

static void rc_runtime2_fetch_game_data_callback(const char* server_response_body, int http_status_code, void* callback_data)
{
  rc_runtime2_load_state_t* load_state = (rc_runtime2_load_state_t*)callback_data;
  rc_runtime2_t* runtime = load_state->runtime;
  rc_api_fetch_game_data_response_t fetch_game_data_response;

  int result = rc_api_process_fetch_game_data_response(&fetch_game_data_response, server_response_body);
  const char* error_message = rc_runtime2_server_error_message(&result, http_status_code, &fetch_game_data_response.response);

  int outstanding_requests = rc_runtime2_end_load_state(load_state);

  if (error_message)
  {
    rc_runtime2_load_error(load_state, result, error_message);
  }
  else if (outstanding_requests < 0)
  {
    /* previous load state was aborted, silently quit */
  }
  else
  {
    load_state->game->public.console_id = fetch_game_data_response.console_id;
    load_state->game->public.title = rc_buf_strcpy(&load_state->game->buffer, fetch_game_data_response.title);
    snprintf(load_state->game->public.badge_name, sizeof(load_state->game->public.badge_name), "%s", fetch_game_data_response.image_name);

    /* kick off the start session request while we process the game data */
    rc_runtime2_begin_load_state(load_state, RC_RUNTIME2_LOAD_STATE_STARTING_SESSION, 1);
    rc_runtime2_begin_start_session(load_state);

    /* process the game data */
    load_state->game->achievements = rc_runtime2_copy_achievements(load_state, &fetch_game_data_response);
    load_state->game->public.num_achievements = fetch_game_data_response.num_achievements;

    load_state->game->leaderboards = rc_runtime2_copy_leaderboards(load_state, &fetch_game_data_response);
    load_state->game->public.num_leaderboards = fetch_game_data_response.num_leaderboards;

    result = rc_runtime_activate_richpresence(&load_state->game->runtime, fetch_game_data_response.rich_presence_script, NULL, 0);
    if (result != RC_OK)
    {
      RC_RUNTIME2_LOG_WARN(load_state->runtime, "Parse error %d processing rich presence", result);
    }

    outstanding_requests = rc_runtime2_end_load_state(load_state);
    if (outstanding_requests < 0)
    {
      /* previous load state was aborted, silently quit */
    }
    else
    {
      if (outstanding_requests == 0)
        rc_runtime2_activate_game(load_state);
    }
  }

  rc_api_destroy_fetch_game_data_response(&fetch_game_data_response);
}

static void rc_runtime2_begin_fetch_game_data(rc_runtime2_load_state_t* load_state)
{
  rc_api_fetch_game_data_request_t fetch_game_data_request;
  rc_runtime2_t* runtime = load_state->runtime;
  rc_api_request_t request;
  int result;

  if (load_state->hash->game_id == 0)
  {
    rc_runtime2_load_error(load_state, RC_NO_GAME_LOADED, "Unknown game");
    return;
  }

  load_state->game->public.id = load_state->hash->game_id;
  load_state->game->public.hash = load_state->hash->hash;

  rc_mutex_lock(&runtime->state.mutex);
  result = runtime->state.user;
  if (result == RC_RUNTIME2_USER_STATE_LOGIN_REQUESTED)
    load_state->progress = RC_RUNTIME2_LOAD_STATE_AWAIT_LOGIN;
  rc_mutex_unlock(&runtime->state.mutex);

  switch (result)
  {
    case RC_RUNTIME2_USER_STATE_LOGGED_IN:
      break;

    case RC_RUNTIME2_USER_STATE_LOGIN_REQUESTED:
      /* do nothing, this function will be called again after login completes */
      return;

    default:
      rc_runtime2_load_error(load_state, RC_LOGIN_REQUIRED, rc_error_str(RC_LOGIN_REQUIRED));
      return;
  }

  memset(&fetch_game_data_request, 0, sizeof(fetch_game_data_request));
  fetch_game_data_request.username = runtime->user.username;
  fetch_game_data_request.api_token = runtime->user.token;
  fetch_game_data_request.game_id = load_state->game->public.id;

  result = rc_api_init_fetch_game_data_request(&request, &fetch_game_data_request);
  if (result != RC_OK)
  {
    rc_runtime2_load_error(load_state, result, rc_error_str(result));
    return;
  }

  rc_runtime2_begin_load_state(load_state, RC_RUNTIME2_LOAD_STATE_FETCHING_GAME_DATA, 1);

  runtime->callbacks.server_call(&request, rc_runtime2_fetch_game_data_callback, load_state, runtime);
  rc_api_destroy_request(&request);
}

static void rc_runtime2_identify_game_callback(const char* server_response_body, int http_status_code, void* callback_data)
{
  rc_runtime2_load_state_t* load_state = (rc_runtime2_load_state_t*)callback_data;
  rc_runtime2_t* runtime = load_state->runtime;
  rc_api_resolve_hash_response_t resolve_hash_response;

  int result = rc_api_process_resolve_hash_response(&resolve_hash_response, server_response_body);
  const char* error_message = rc_runtime2_server_error_message(&result, http_status_code, &resolve_hash_response.response);

  int outstanding_requests = rc_runtime2_end_load_state(load_state);

  if (error_message)
  {
    rc_runtime2_load_error(load_state, result, error_message);
  }
  else
  {
    /* hash exists outside the load state - always update it */
    load_state->hash->game_id = resolve_hash_response.game_id;
    RC_RUNTIME2_LOG_INFO(runtime, "Identified game: %u (%s)", load_state->hash->game_id, load_state->hash->hash);

    if (outstanding_requests < 0)
    {
      /* previous load state was aborted, silently quit */
    }
    else
    {
      rc_runtime2_begin_fetch_game_data(load_state);
    }
  }

  rc_api_destroy_resolve_hash_response(&resolve_hash_response);
}

static rc_runtime2_game_hash_t* rc_runtime2_find_game_hash(rc_runtime2_t* runtime, const char* hash)
{
  rc_runtime2_game_hash_t* game_hash;

  rc_mutex_lock(&runtime->state.mutex);
  game_hash = runtime->hashes;
  while (game_hash)
  {
    if (strcasecmp(game_hash->hash, hash) == 0)
      break;

    game_hash = game_hash->next;
  }

  if (!game_hash)
  {
    game_hash = rc_buf_alloc(&runtime->buffer, sizeof(rc_runtime2_game_hash_t));
    memset(game_hash, 0, sizeof(*game_hash));
    game_hash->hash = rc_buf_strcpy(&runtime->buffer, hash);
    game_hash->game_id = RC_UNKNOWN_GAME_ID;
    game_hash->next = runtime->hashes;
    runtime->hashes = game_hash;
  }
  rc_mutex_unlock(&runtime->state.mutex);

  return game_hash;
}

void rc_runtime2_begin_load_game(rc_runtime2_t* runtime, const char* hash, rc_runtime2_callback_t callback)
{
  rc_api_resolve_hash_request_t resolve_hash_request;
  rc_runtime2_load_state_t* load_state;
  rc_api_request_t request;
  int result;

  if (!hash || !hash[0])
  {
    callback(RC_INVALID_STATE, "hash is required", runtime);
    return;
  }

  memset(&resolve_hash_request, 0, sizeof(resolve_hash_request));
  resolve_hash_request.game_hash = hash;

  result = rc_api_init_resolve_hash_request(&request, &resolve_hash_request);
  if (result != RC_OK)
  {
    callback(result, rc_error_str(result), runtime);
    return;
  }

  rc_runtime2_unload_game(runtime);

  load_state = (rc_runtime2_load_state_t*)calloc(1, sizeof(*load_state));
  load_state->runtime = runtime;
  load_state->callback = callback;
  runtime->state.load = load_state;

  load_state->game = (rc_runtime2_game_info_t*)calloc(1, sizeof(*load_state->game));
  rc_buf_init(&load_state->game->buffer);
  rc_runtime_init(&load_state->game->runtime);

  load_state->hash = rc_runtime2_find_game_hash(runtime, hash);

  if (load_state->hash->game_id == RC_UNKNOWN_GAME_ID)
  {
    rc_runtime2_begin_load_state(load_state, RC_RUNTIME2_LOAD_STATE_IDENTIFYING_GAME, 1);

    runtime->callbacks.server_call(&request, rc_runtime2_identify_game_callback, load_state, runtime);
  }
  else
  {
    RC_RUNTIME2_LOG_INFO(runtime, "Identified game: %u (%s)", load_state->hash->game_id, load_state->hash->hash);

    rc_runtime2_begin_fetch_game_data(load_state);
  }

  rc_api_destroy_request(&request);
}

void rc_runtime2_unload_game(rc_runtime2_t* runtime)
{
  rc_runtime2_game_info_t* game;

  rc_mutex_lock(&runtime->state.mutex);
  game = runtime->game;
  runtime->game = NULL;
  runtime->state.load = NULL;
  rc_mutex_unlock(&runtime->state.mutex);

  if (game != NULL)
    rc_runtime2_free_game(game);
}

const rc_runtime2_game_t* rc_runtime2_game_info(const rc_runtime2_t* runtime)
{
  return (runtime->game != NULL) ? &runtime->game->public : NULL;
}
