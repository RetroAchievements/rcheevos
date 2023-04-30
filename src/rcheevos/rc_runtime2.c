#include "rc_runtime2_internal.h"

#include "rc_api_runtime.h"
#include "rc_api_user.h"

#include "../rapi/rc_api_common.h"

#include <stdarg.h>

typedef struct rc_runtime2_generic_callback_data_t {
  rc_runtime2_t* runtime;
  rc_runtime2_callback_t callback;
} rc_runtime2_generic_callback_data_t;

static void rc_runtime2_begin_fetch_game_data(rc_runtime2_load_state_t* callback_data);

/* ===== Construction/Destruction ===== */

rc_runtime2_t* rc_runtime2_create(rc_runtime2_peek_t peek_function, rc_runtime2_server_call_t server_call_function)
{
  rc_runtime2_t* runtime = (rc_runtime2_t*)calloc(1, sizeof(rc_runtime2_t));
  if (!runtime)
    return NULL;

  runtime->callbacks.peek = peek_function;
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

static void rc_runtime2_start_login(rc_runtime2_t* runtime,
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

void rc_runtime2_start_login_with_password(rc_runtime2_t* runtime,
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
  rc_runtime2_start_login(runtime, &login_request, callback);
}

void rc_runtime2_start_login_with_token(rc_runtime2_t* runtime,
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
  rc_runtime2_start_login(runtime, &login_request, callback);
}

const rc_runtime2_user_t* rc_runtime2_user_info(const rc_runtime2_t* runtime)
{
  return (runtime->state.user == RC_RUNTIME2_USER_STATE_LOGGED_IN) ? &runtime->user : NULL;
}

/* ===== load game ===== */

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
    free(load_state);

  return remaining_requests;
}

static void rc_runtime2_load_error(rc_runtime2_load_state_t* load_state, int result, const char* error_message)
{
  rc_mutex_lock(&load_state->runtime->state.mutex);
  load_state->progress = RC_RUNTIME2_LOAD_STATE_UNKNOWN_GAME;
  if (load_state->runtime->state.load == load_state)
    load_state->runtime->state.load = NULL;
  rc_mutex_unlock(&load_state->runtime->state.mutex);

  rc_runtime2_unload_game(load_state->runtime);

  if (load_state->callback)
    load_state->callback(result, error_message, load_state->runtime);

  free(load_state);
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
    // TODO: activate achievements
    // TODO: raise game loaded event
  }

  free(load_state);
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

  memset(&start_session_params, 0, sizeof(start_session_request));
  start_session_params.username = runtime->user.username;
  start_session_params.api_token = runtime->user.token;
  start_session_params.game_id = runtime->game.public.id;

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
    unlock_params.game_id = runtime->game.public.id;
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
    runtime->game.public.console_id = fetch_game_data_response.console_id;
    runtime->game.public.title = rc_buf_strcpy(&runtime->game.buffer, fetch_game_data_response.title);
    snprintf(runtime->game.public.badge_name, sizeof(runtime->game.public.badge_name), "%s", fetch_game_data_response.image_name);

    // TODO: copy achievements and leaderboards
    // TODO: load achievements, leaderboards, and rich presence into runtime

    rc_runtime2_begin_start_session(load_state);
  }

  rc_api_destroy_fetch_game_data_response(&fetch_game_data_response);
}

static void rc_runtime2_begin_fetch_game_data(rc_runtime2_load_state_t* load_state)
{
  rc_api_fetch_game_data_request_t fetch_game_data_request;
  rc_runtime2_t* runtime = load_state->runtime;
  rc_api_request_t request;
  int result;

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
      /* do nothing, this function will be called again after login completes*/
      return;

    default:
      rc_runtime2_load_error(load_state, RC_LOGIN_REQUIRED, rc_error_str(RC_LOGIN_REQUIRED));
      return;
  }

  memset(&fetch_game_data_request, 0, sizeof(fetch_game_data_request));
  fetch_game_data_request.username = runtime->user.username;
  fetch_game_data_request.api_token = runtime->user.token;
  fetch_game_data_request.game_id = runtime->game.public.id;

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
  else if (outstanding_requests < 0)
  {
    /* previous load state was aborted, silently quit */
  }
  else if (resolve_hash_response.game_id == 0)
  {
    rc_runtime2_load_error(load_state, RC_NO_GAME_LOADED, "Unknown game");
  }
  else
  {
    runtime->game.public.hash = runtime->game.hashes->hash;
    runtime->game.public.id = resolve_hash_response.game_id;
    runtime->game.hashes->game_id = resolve_hash_response.game_id;

    RC_RUNTIME2_LOG_INFO(runtime, "Identified game: %u (%s)", runtime->game.hashes->game_id, runtime->game.hashes->hash);

    rc_runtime2_begin_fetch_game_data(load_state);
  }

  rc_api_destroy_resolve_hash_response(&resolve_hash_response);
}

void rc_runtime2_start_load_game(rc_runtime2_t* runtime, const char* hash, rc_runtime2_callback_t callback)
{
  rc_api_resolve_hash_request_t resolve_hash_request;
  rc_runtime2_load_state_t* load_state;
  rc_runtime2_game_hash_t* game_hash;
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

  memset(&runtime->state.load, sizeof(runtime->state.load), 0);

  rc_buf_init(&runtime->game.buffer);

  load_state = (rc_runtime2_load_state_t*)calloc(1, sizeof(*load_state));
  load_state->runtime = runtime;
  load_state->callback = callback;
  runtime->state.load = load_state;

  game_hash = runtime->game.hashes;
  while (game_hash)
  {
    if (strcasecmp(game_hash->hash, hash) == 0)
      break;

    game_hash = game_hash->next;
  }

  if (game_hash)
  {
    load_state->hash = game_hash;
    runtime->game.public.hash = game_hash->hash;
    runtime->game.public.id = game_hash->game_id;

    RC_RUNTIME2_LOG_INFO(runtime, "Identified game: %u (%s)", game_hash->game_id, game_hash->hash);

    rc_runtime2_begin_fetch_game_data(load_state);
  }
  else
  {
    game_hash = rc_buf_alloc(&runtime->buffer, sizeof(rc_runtime2_game_hash_t));
    memset(game_hash, 0, sizeof(*game_hash));
    game_hash->hash = rc_buf_strcpy(&runtime->buffer, hash);
    game_hash->next = runtime->game.hashes;
    runtime->game.hashes = game_hash;
    load_state->hash = game_hash;

    rc_runtime2_begin_load_state(load_state, RC_RUNTIME2_LOAD_STATE_IDENTIFYING_GAME, 1);

    runtime->callbacks.server_call(&request, rc_runtime2_identify_game_callback, load_state, runtime);
  }
  rc_api_destroy_request(&request);
}

void rc_runtime2_unload_game(rc_runtime2_t* runtime)
{
  if (runtime->game.public.id == 0)
    return;

  rc_mutex_lock(&runtime->state.mutex);

  rc_buf_destroy(&runtime->game.buffer);
  memset(&runtime->game, 0, sizeof(runtime->game));

  runtime->state.load = NULL;

  rc_mutex_unlock(&runtime->state.mutex);
}


