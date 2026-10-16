#include "rc_client_internal.h"

#include "rc_api_info.h"
#include "rc_api_user.h"

#include "rc_client_async.h"
#include "rc_client_do_frame.h"
#include "rc_client_game.h"
#include "rc_client_load.h"
#include "rc_client_server.h"

#include "../api/rc_api_common.h"

#include "../util/rc_version.h"

#include <stdarg.h>

#ifdef _WIN32
 #define WIN32_LEAN_AND_MEAN
 #include <windows.h>
#else
 #include <time.h>
#endif

/* ===== natvis extensions ===== */

typedef struct __rc_client_achievement_state_enum_t { uint8_t value; } __rc_client_achievement_state_enum_t;
typedef struct __rc_client_achievement_category_enum_t { uint8_t value; } __rc_client_achievement_category_enum_t;
typedef struct __rc_client_achievement_type_enum_t { uint8_t value; } __rc_client_achievement_type_enum_t;
typedef struct __rc_client_achievement_bucket_enum_t { uint8_t value; } __rc_client_achievement_bucket_enum_t;
typedef struct __rc_client_achievement_unlocked_enum_t { uint8_t value; } __rc_client_achievement_unlocked_enum_t;
typedef struct __rc_client_leaderboard_state_enum_t { uint8_t value; } __rc_client_leaderboard_state_enum_t;
typedef struct __rc_client_leaderboard_format_enum_t { uint8_t value; } __rc_client_leaderboard_format_enum_t;
typedef struct __rc_client_log_level_enum_t { uint8_t value; } __rc_client_log_level_enum_t;
typedef struct __rc_client_event_type_enum_t { uint8_t value; } __rc_client_event_type_enum_t;
typedef struct __rc_client_load_game_state_enum_t { uint8_t value; } __rc_client_load_game_state_enum_t;
typedef struct __rc_client_user_state_enum_t { uint8_t value; } __rc_client_user_state_enum_t;
typedef struct __rc_client_mastery_state_enum_t { uint8_t value; } __rc_client_mastery_state_enum_t;
typedef struct __rc_client_spectator_mode_enum_t { uint8_t value; } __rc_client_spectator_mode_enum_t;
typedef struct __rc_client_disconnect_enum_t { uint8_t value; } __rc_client_disconnect_enum_t;
typedef struct __rc_client_leaderboard_tracker_list_t { rc_client_leaderboard_tracker_info_t* first; } __rc_client_leaderboard_tracker_list_t;
typedef struct __rc_client_subset_info_list_t { rc_client_subset_info_t* first; } __rc_client_subset_info_list_t;
typedef struct __rc_client_media_hash_list_t { rc_client_media_hash_t* first; } __rc_client_media_hash_list_t;
typedef struct __rc_client_subset_info_achievements_list_t { rc_client_subset_info_t info; } __rc_client_subset_info_achievements_list_t;
typedef struct __rc_client_subset_info_leaderboards_list_t { rc_client_subset_info_t info; } __rc_client_subset_info_leaderboards_list_t;
typedef struct __rc_client_scheduled_callback_list_t { rc_client_state_t state; } __rc_client_scheduled_callback_list_t;
typedef struct __rc_client_game_hash_list_t { rc_client_t client; } __rc_client_game_hash_list_t;

static void rc_client_natvis_helper(const rc_client_event_t* event, rc_client_t* client)
{
  struct natvis_extensions {
    __rc_client_achievement_state_enum_t achievement_state;
    __rc_client_achievement_category_enum_t achievement_category;
    __rc_client_achievement_type_enum_t achievement_type;
    __rc_client_achievement_bucket_enum_t achievement_bucket;
    __rc_client_achievement_unlocked_enum_t achievement_unlocked;
    __rc_client_leaderboard_state_enum_t leaderboard_state;
    __rc_client_leaderboard_format_enum_t leaderboard_format;
    __rc_client_log_level_enum_t log_level;
    __rc_client_event_type_enum_t event_type;
    __rc_client_load_game_state_enum_t load_game_state;
    __rc_client_user_state_enum_t user_state;
    __rc_client_mastery_state_enum_t mastery_state;
    __rc_client_spectator_mode_enum_t spectator_mode;
    __rc_client_disconnect_enum_t disconnect;
    __rc_client_leaderboard_tracker_list_t leaderboard_tracker_list;
    __rc_client_subset_info_list_t subset_info_list;
    __rc_client_media_hash_list_t media_hash_list;
    __rc_client_subset_info_achievements_list_t subset_info_achievements_list;
    __rc_client_subset_info_leaderboards_list_t subset_info_leaderboards_list;
    __rc_client_scheduled_callback_list_t scheduled_callback_list;
    __rc_client_game_hash_list_t client_game_hash_list;
  } natvis;

  memset(&natvis, 0, sizeof(natvis));
  (void)event;
  (void)client;

  /* this code should never be executed. it just ensures these constants get defined for
   * the natvis VisualStudio extension as they're not used directly in the code. */
  natvis.achievement_type.value = RC_CLIENT_ACHIEVEMENT_TYPE_STANDARD;
  natvis.achievement_type.value = RC_CLIENT_ACHIEVEMENT_TYPE_MISSABLE;
  natvis.achievement_type.value = RC_CLIENT_ACHIEVEMENT_TYPE_PROGRESSION;
  natvis.achievement_type.value = RC_CLIENT_ACHIEVEMENT_TYPE_WIN;
  natvis.achievement_category.value = RC_CLIENT_ACHIEVEMENT_CATEGORY_NONE;
  natvis.event_type.value = RC_CLIENT_EVENT_TYPE_NONE;
}

/* ===== Construction/Destruction ===== */

static void rc_client_dummy_event_handler(const rc_client_event_t* event, rc_client_t* client)
{
  (void)event;
  (void)client;
}

rc_client_t* rc_client_create(rc_client_read_memory_func_t read_memory_function, rc_client_server_call_t server_call_function)
{
  rc_client_t* client = (rc_client_t*)calloc(1, sizeof(rc_client_t));
  if (!client)
    return NULL;

  client->state.hardcore = 1;
  client->state.required_unpaused_frames = RC_MINIMUM_UNPAUSED_FRAMES;
  client->state.allow_background_memory_reads = 1;

  client->callbacks.read_memory = read_memory_function;
  client->callbacks.server_call = server_call_function;
  client->callbacks.event_handler = rc_client_natvis_helper;
  client->callbacks.event_handler = rc_client_dummy_event_handler;
  rc_client_set_get_time_millisecs_function(client, NULL);

  rc_mutex_init(&client->state.mutex);

  rc_buffer_init(&client->state.buffer);

  return client;
}

void rc_client_destroy(rc_client_t* client)
{
  if (!client)
    return;

  rc_mutex_lock(&client->state.mutex);
  {
    size_t i;
    for (i = 0; i < sizeof(client->state.async_handles) / sizeof(client->state.async_handles[0]); ++i) {
      if (client->state.async_handles[i])
        client->state.async_handles[i]->aborted = RC_CLIENT_ASYNC_DESTROYED;
    }

    if (client->state.load) {
      client->state.load->async_handle.aborted = RC_CLIENT_ASYNC_DESTROYED;
      client->state.load = NULL;
    }
  }
  rc_mutex_unlock(&client->state.mutex);

  rc_client_unload_game(client);

#ifdef RC_CLIENT_SUPPORTS_RAINTEGRATION
  rc_client_unload_raintegration(client);
#endif

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->destroy)
    client->state.external_client->destroy();
#endif

  rc_buffer_destroy(&client->state.buffer);

  rc_mutex_destroy(&client->state.mutex);

  free(client);
}

/* ===== Logging ===== */

void rc_client_log_message(const rc_client_t* client, const char* message)
{
  if (client->callbacks.log_call)
    client->callbacks.log_call(message, client);
}

static void rc_client_log_message_va(const rc_client_t* client, const char* format, va_list args)
{
  if (client->callbacks.log_call) {
    char buffer[2048];

#ifdef __STDC_SECURE_LIB__
    vsnprintf_s(buffer, sizeof(buffer), _TRUNCATE, format, args);
#elif __STDC_VERSION__ >= 199901L /* vsnprintf requires c99 */
    vsnprintf(buffer, sizeof(buffer), format, args);
#else /* c89 doesn't have a size-limited vsprintf function - assume the buffer is large enough */
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
  client->state.log_level = callback ? (uint8_t)level : RC_CLIENT_LOG_LEVEL_NONE;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->enable_logging)
    client->state.external_client->enable_logging(client, level, callback);
#endif
}

/* ===== Clock ===== */

#if defined(__APPLE__) && defined(__MACH__)
 #ifdef CLOCK_MONOTONIC
  /* clock_gettime() was added to Darwin in iOS 10.0 and macOS 10.12.
   * On earlier deployment targets (like Leopard 10.5), the symbol doesn't exist
   * in libSystem causing an "undefined reference to clock_gettime" linker error.
   * To get the code to use the #else block below, forcibly undefine CLOCK_MONOTONIC
   * when targeting earlier versions. */
  #include <AvailabilityMacros.h>
  #if (defined(MAC_OS_X_VERSION_MIN_REQUIRED) && MAC_OS_X_VERSION_MIN_REQUIRED < 101200)
   #undef CLOCK_MONOTONIC
  #elif (defined(__IPHONE_OS_VERSION_MIN_REQUIRED) && __IPHONE_OS_VERSION_MIN_REQUIRED < 100000)
   #undef CLOCK_MONOTONIC
  #endif
 #endif
#endif

static rc_clock_t rc_client_clock_get_now_millisecs(const rc_client_t* client)
{
#if defined(CLOCK_MONOTONIC)
  struct timespec now;
  (void)client;

  if (clock_gettime(CLOCK_MONOTONIC, &now) < 0)
    return 0;

  /* round nanoseconds to nearest millisecond and add to seconds */
  return ((rc_clock_t)now.tv_sec * 1000 + ((rc_clock_t)now.tv_nsec / 1000000));
#elif defined(_WIN32)
  static LARGE_INTEGER freq;
  LARGE_INTEGER ticks;

  (void)client;

  /* Frequency is the number of ticks per second and is guaranteed to not change. */
  if (!freq.QuadPart) {
    if (!QueryPerformanceFrequency(&freq))
      return 0;

    /* convert to number of ticks per millisecond to simplify later calculations */
    freq.QuadPart /= 1000;
  }

  if (!QueryPerformanceCounter(&ticks))
    return 0;

  return (rc_clock_t)(ticks.QuadPart / freq.QuadPart);
#else
  const clock_t clock_now = clock();

  (void)client;

  if (sizeof(clock_t) == 4) {
    static uint32_t clock_wraps = 0;
    static clock_t last_clock = 0;
    static time_t last_timet = 0;
    const time_t time_now = time(NULL);

    if (last_timet != 0) {
      const time_t seconds_per_clock_t = (time_t)(((uint64_t)1 << 32) / CLOCKS_PER_SEC);
      if (clock_now < last_clock) {
        /* clock() has wrapped */
        ++clock_wraps;
      }
      else if (time_now - last_timet > seconds_per_clock_t) {
        /* it's been long enough that clock() has wrapped and is higher than the last time it was read */
        ++clock_wraps;
      }
    }

    last_timet = time_now;
    last_clock = clock_now;

    return (rc_clock_t)((((uint64_t)clock_wraps << 32) | clock_now) / (CLOCKS_PER_SEC / 1000));
  }
  else {
    return (rc_clock_t)(clock_now / (CLOCKS_PER_SEC / 1000));
  }
#endif
}

void rc_client_set_get_time_millisecs_function(rc_client_t* client, rc_get_time_millisecs_func_t handler)
{
  client->callbacks.get_time_millisecs = handler ? handler : rc_client_clock_get_now_millisecs;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->set_get_time_millisecs)
    client->state.external_client->set_get_time_millisecs(client, handler);
#endif
}

/* ===== Fetch Game Hashes ===== */

typedef struct rc_client_fetch_hash_library_callback_data_t {
  rc_client_t* client;
  rc_client_fetch_hash_library_callback_t callback;
  void* callback_userdata;
  uint32_t console_id;
  rc_client_async_handle_t async_handle;
} rc_client_fetch_hash_library_callback_data_t;

static void rc_client_fetch_hash_library_callback(const rc_api_server_response_t* server_response, void* callback_data)
{
  rc_client_fetch_hash_library_callback_data_t* hashlib_callback_data =
    (rc_client_fetch_hash_library_callback_data_t*)callback_data;
  rc_client_t* client = hashlib_callback_data->client;
  rc_api_fetch_hash_library_response_t hashlib_response;
  const char* error_message;
  int result;

  result = rc_client_end_async(client, &hashlib_callback_data->async_handle);
  if (result) {
    if (result != RC_CLIENT_ASYNC_DESTROYED)
      RC_CLIENT_LOG_VERBOSE(client, "Fetch hash library aborted");

    free(hashlib_callback_data);
    return;
  }

  result = rc_api_process_fetch_hash_library_server_response(&hashlib_response, server_response);
  error_message =
    rc_client_server_error_message(&result, server_response->http_status_code, &hashlib_response.response);
  if (error_message) {
    RC_CLIENT_LOG_ERR_FORMATTED(client, "Fetch hash library for console %u failed: %s",
                                hashlib_callback_data->console_id, error_message);
    hashlib_callback_data->callback(result, error_message, NULL, client, hashlib_callback_data->callback_userdata);
  } else {
    rc_client_hash_library_t* list;
    const size_t list_size = sizeof(*list) + sizeof(rc_client_hash_library_entry_t) * hashlib_response.num_entries;
    list = (rc_client_hash_library_t*)malloc(list_size);
    if (!list) {
      hashlib_callback_data->callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), NULL, client,
                                      hashlib_callback_data->callback_userdata);
    } else {
      rc_client_hash_library_entry_t* entry = list->entries =
        (rc_client_hash_library_entry_t*)((uint8_t*)list + sizeof(*list));
      const rc_api_hash_library_entry_t* hlentry = hashlib_response.entries;
      const rc_api_hash_library_entry_t* stop = hlentry + hashlib_response.num_entries;

      for (; hlentry < stop; ++hlentry, ++entry) {
        snprintf(entry->hash, sizeof(entry->hash), "%s", hlentry->hash);
        entry->game_id = hlentry->game_id;
      }

      list->num_entries = hashlib_response.num_entries;

      hashlib_callback_data->callback(RC_OK, NULL, list, client, hashlib_callback_data->callback_userdata);
    }
  }

  rc_api_destroy_fetch_hash_library_response(&hashlib_response);
  free(hashlib_callback_data);
}

rc_client_async_handle_t* rc_client_begin_fetch_hash_library(rc_client_t* client, uint32_t console_id,
                                                             rc_client_fetch_hash_library_callback_t callback,
                                                             void* callback_userdata)
{
  rc_api_fetch_hash_library_request_t api_params;
  rc_client_fetch_hash_library_callback_data_t* callback_data;
  rc_client_async_handle_t* async_handle;
  rc_api_request_t request;
  int result;
  const char* error_message;

  if (!client) {
    callback(RC_INVALID_STATE, "client is required", NULL, client, callback_userdata);
    return NULL;
  }

  api_params.console_id = console_id;
  result = rc_api_init_fetch_hash_library_request_hosted(&request, &api_params, &client->state.host);

  if (result != RC_OK) {
    error_message = rc_error_str(result);
    callback(result, error_message, NULL, client, callback_userdata);
    return NULL;
  }

  callback_data = (rc_client_fetch_hash_library_callback_data_t*)calloc(1, sizeof(*callback_data));
  if (!callback_data) {
    callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), NULL, client, callback_userdata);
    return NULL;
  }

  callback_data->client = client;
  callback_data->callback = callback;
  callback_data->callback_userdata = callback_userdata;
  callback_data->console_id = console_id;

  async_handle = &callback_data->async_handle;
  rc_client_begin_async(client, async_handle);
  client->callbacks.server_call(&request, rc_client_fetch_hash_library_callback, callback_data, client);
  rc_api_destroy_request(&request);

  return rc_client_async_handle_valid(client, async_handle) ? async_handle : NULL;
}

void rc_client_destroy_hash_library(rc_client_hash_library_t* list)
{
  free(list);
}

/* ===== Fetch Game Titles ===== */

typedef struct rc_client_fetch_game_titles_callback_data_t {
  rc_client_t* client;
  rc_client_fetch_game_titles_callback_t callback;
  void* callback_userdata;
  rc_client_async_handle_t async_handle;
} rc_client_fetch_game_titles_callback_data_t;

static void rc_client_fetch_game_titles_callback(const rc_api_server_response_t* server_response, void* callback_data)
{
  rc_client_fetch_game_titles_callback_data_t* titles_callback_data =
    (rc_client_fetch_game_titles_callback_data_t*)callback_data;
  rc_client_t* client = titles_callback_data->client;
  rc_api_fetch_game_titles_response_t titles_response;
  const char* error_message;
  int result;

  result = rc_client_end_async(client, &titles_callback_data->async_handle);
  if (result) {
    if (result != RC_CLIENT_ASYNC_DESTROYED)
      RC_CLIENT_LOG_VERBOSE(client, "Fetch game titles aborted");

    free(titles_callback_data);
    return;
  }

  result = rc_api_process_fetch_game_titles_server_response(&titles_response, server_response);
  error_message =
    rc_client_server_error_message(&result, server_response->http_status_code, &titles_response.response);
  if (error_message) {
    RC_CLIENT_LOG_ERR_FORMATTED(client, "Fetch game titles failed: %s", error_message);
    titles_callback_data->callback(result, error_message, NULL, client, titles_callback_data->callback_userdata);
  } else {
    rc_client_game_title_list_t* list;
    size_t strings_size = 0;
    const rc_api_game_title_entry_t* src;
    const rc_api_game_title_entry_t* stop;
    size_t list_size;

    /* calculate string buffer size */
    for (src = titles_response.entries, stop = src + titles_response.num_entries; src < stop; ++src) {
      if (src->title)
        strings_size += strlen(src->title) + 1;
      if (src->image_url)
        strings_size += strlen(src->image_url) + 1;
    }

    list_size = sizeof(*list) + sizeof(rc_client_game_title_entry_t) * titles_response.num_entries + strings_size;
    list = (rc_client_game_title_list_t*)malloc(list_size);
    if (!list) {
      titles_callback_data->callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), NULL, client,
                                     titles_callback_data->callback_userdata);
    } else {
      rc_client_game_title_entry_t* entry = list->entries =
        (rc_client_game_title_entry_t*)((uint8_t*)list + sizeof(*list));
      char* strings = (char*)((uint8_t*)list + sizeof(*list) +
                              sizeof(rc_client_game_title_entry_t) * titles_response.num_entries);

      for (src = titles_response.entries, stop = src + titles_response.num_entries; src < stop; ++src, ++entry) {
        entry->game_id = src->id;

        if (src->title) {
          const size_t len = strlen(src->title) + 1;
          entry->title = strings;
          memcpy(strings, src->title, len);
          strings += len;
        } else {
          entry->title = NULL;
        }

        if (src->image_name)
          snprintf(entry->badge_name, sizeof(entry->badge_name), "%s", src->image_name);
        else
          entry->badge_name[0] = '\0';

        if (src->image_url) {
          const size_t len = strlen(src->image_url) + 1;
          entry->badge_url = strings;
          memcpy(strings, src->image_url, len);
          strings += len;
        }
        else {
          entry->badge_url = NULL;
        }
      }

      list->num_entries = titles_response.num_entries;

      titles_callback_data->callback(RC_OK, NULL, list, client, titles_callback_data->callback_userdata);
    }
  }

  rc_api_destroy_fetch_game_titles_response(&titles_response);
  free(titles_callback_data);
}

rc_client_async_handle_t* rc_client_begin_fetch_game_titles(rc_client_t* client, const uint32_t* game_ids,
                                                            uint32_t num_game_ids,
                                                            rc_client_fetch_game_titles_callback_t callback,
                                                            void* callback_userdata)
{
  rc_api_fetch_game_titles_request_t api_params;
  rc_client_fetch_game_titles_callback_data_t* callback_data;
  rc_client_async_handle_t* async_handle;
  rc_api_request_t request;
  int result;
  const char* error_message;

  if (!client) {
    callback(RC_INVALID_STATE, "client is required", NULL, client, callback_userdata);
    return NULL;
  }

  if (!game_ids || num_game_ids == 0) {
    callback(RC_INVALID_STATE, "game_ids is required", NULL, client, callback_userdata);
    return NULL;
  }

  api_params.game_ids = game_ids;
  api_params.num_game_ids = num_game_ids;
  result = rc_api_init_fetch_game_titles_request_hosted(&request, &api_params, &client->state.host);

  if (result != RC_OK) {
    error_message = rc_error_str(result);
    callback(result, error_message, NULL, client, callback_userdata);
    return NULL;
  }

  callback_data = (rc_client_fetch_game_titles_callback_data_t*)calloc(1, sizeof(*callback_data));
  if (!callback_data) {
    callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), NULL, client, callback_userdata);
    return NULL;
  }

  callback_data->client = client;
  callback_data->callback = callback;
  callback_data->callback_userdata = callback_userdata;

  async_handle = &callback_data->async_handle;
  rc_client_begin_async(client, async_handle);
  client->callbacks.server_call(&request, rc_client_fetch_game_titles_callback, callback_data, client);
  rc_api_destroy_request(&request);

  return rc_client_async_handle_valid(client, async_handle) ? async_handle : NULL;
}

void rc_client_destroy_game_title_list(rc_client_game_title_list_t* list)
{
  free(list);
}

/* ===== Fetch Games List ===== */

typedef struct rc_client_fetch_game_list_callback_data_t {
  rc_client_t* client;
  rc_client_fetch_game_list_callback_t callback;
  void* callback_userdata;
  rc_client_async_handle_t async_handle;
} rc_client_fetch_game_list_callback_data_t;

static void rc_client_fetch_game_list_callback(const rc_api_server_response_t* server_response, void* callback_data)
{
  rc_client_fetch_game_list_callback_data_t* list_callback_data =
    (rc_client_fetch_game_list_callback_data_t*)callback_data;
  rc_client_t* client = list_callback_data->client;
  rc_api_fetch_games_list_response_t list_response;
  const char* error_message;
  int result;

  result = rc_client_end_async(client, &list_callback_data->async_handle);
  if (result) {
    if (result != RC_CLIENT_ASYNC_DESTROYED)
      RC_CLIENT_LOG_VERBOSE(client, "Fetch game list aborted");

    free(list_callback_data);
    return;
  }

  result = rc_api_process_fetch_games_list_server_response(&list_response, server_response);
  error_message =
    rc_client_server_error_message(&result, server_response->http_status_code, &list_response.response);
  if (error_message) {
    RC_CLIENT_LOG_ERR_FORMATTED(client, "Fetch game list failed: %s", error_message);
    list_callback_data->callback(result, error_message, NULL, client, list_callback_data->callback_userdata);
  } else {
    rc_client_game_list_t* list;
    size_t strings_size = 0, hashes_size = 0;
    const rc_api_game_list_entry_t* src;
    const rc_api_game_list_entry_t* stop;
    size_t list_size;
    uint32_t i;

    /* calculate string buffer size */
    for (src = list_response.entries, stop = src + list_response.num_entries; src < stop; ++src) {
      if (src->name)
        strings_size += strlen(src->name) + 1;
      if (src->image_name)
        strings_size += strlen(src->image_name) + 1;
      if (src->image_url)
        strings_size += strlen(src->image_url) + 1;
      hashes_size += src->num_supported_hashes * sizeof(const char*);
      for (i = 0; i < src->num_supported_hashes; i++)
        strings_size += strlen(src->supported_hashes[i]) + 1;
      hashes_size += src->num_unsupported_hashes * sizeof(const char*);
      for (i = 0; i < src->num_unsupported_hashes; i++)
        strings_size += strlen(src->unsupported_hashes[i]) + 1;
    }

    list_size = sizeof(*list) + sizeof(rc_client_game_list_entry_t) * list_response.num_entries +
                sizeof(const char*) * hashes_size + strings_size;
    list = (rc_client_game_list_t*)malloc(list_size);
    if (!list) {
      list_callback_data->callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), NULL, client,
                                     list_callback_data->callback_userdata);
    } else {
      rc_client_game_list_entry_t* entry = list->entries =
        (rc_client_game_list_entry_t*)((uint8_t*)list + sizeof(*list));
      const char** hash_list =
        (const char**)((uint8_t*)entry + sizeof(rc_client_game_list_entry_t) * list_response.num_entries);
      char* strings = (char*)((uint8_t*)hash_list + sizeof(const char*) * hashes_size);
      size_t len;

      for (src = list_response.entries, stop = src + list_response.num_entries; src < stop; ++src, ++entry) {
        entry->id = src->id;
        entry->num_achievements = src->num_achievements;
        entry->num_leaderboards = src->num_leaderboards;
        entry->points = src->points;

        if (src->name) {
          len = strlen(src->name) + 1;
          entry->name = strings;
          memcpy(strings, src->name, len);
          strings += len;
        } else {
          entry->name = NULL;
        }

        if (src->image_name) {
          len = strlen(src->image_name) + 1;
          entry->image_name = strings;
          memcpy(strings, src->image_name, len);
          strings += len;
        } else {
          entry->image_name = NULL;
        }

        if (src->image_url) {
          len = strlen(src->image_url) + 1;
          entry->image_url = strings;
          memcpy(strings, src->image_url, len);
          strings += len;
        } else {
          entry->image_url = NULL;
        }

        if ((entry->num_supported_hashes = src->num_supported_hashes) > 0) {
          entry->supported_hashes = hash_list;
          for (i = 0; i < src->num_supported_hashes; i++) {
            len = strlen(src->supported_hashes[i]) + 1;
            *(hash_list++) = strings;
            memcpy(strings, src->supported_hashes[i], len);
            strings += len;
          }
        } else {
          entry->supported_hashes = NULL;
        }

        if ((entry->num_unsupported_hashes = src->num_unsupported_hashes) > 0) {
          entry->unsupported_hashes = hash_list;
          for (i = 0; i < src->num_unsupported_hashes; i++) {
            len = strlen(src->unsupported_hashes[i]) + 1;
            *(hash_list++) = strings;
            memcpy(strings, src->unsupported_hashes[i], len);
            strings += len;
          }
        } else {
          entry->unsupported_hashes = NULL;
        }
      }

      list->num_entries = list_response.num_entries;

      list_callback_data->callback(RC_OK, NULL, list, client, list_callback_data->callback_userdata);
    }
  }

  rc_api_destroy_fetch_games_list_response(&list_response);
  free(list_callback_data);
}

rc_client_async_handle_t* rc_client_begin_fetch_game_list(rc_client_t* client, uint32_t console_id,
                                                            rc_client_fetch_game_list_callback_t callback,
                                                            void* callback_userdata)
{
  rc_api_fetch_games_list_request_t api_params;
  rc_client_fetch_game_list_callback_data_t* callback_data;
  rc_client_async_handle_t* async_handle;
  rc_api_request_t request;
  int result;
  const char* error_message;

  if (!client) {
    callback(RC_INVALID_STATE, "client is required", NULL, client, callback_userdata);
    return NULL;
  }

  api_params.console_id = console_id;
  result = rc_api_init_fetch_games_list_request_hosted(&request, &api_params, &client->state.host);

  if (result != RC_OK) {
    error_message = rc_error_str(result);
    callback(result, error_message, NULL, client, callback_userdata);
    return NULL;
  }

  callback_data = (rc_client_fetch_game_list_callback_data_t*)calloc(1, sizeof(*callback_data));
  if (!callback_data) {
    callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), NULL, client, callback_userdata);
    return NULL;
  }

  callback_data->client = client;
  callback_data->callback = callback;
  callback_data->callback_userdata = callback_userdata;

  async_handle = &callback_data->async_handle;
  rc_client_begin_async(client, async_handle);
  client->callbacks.server_call(&request, rc_client_fetch_game_list_callback, callback_data, client);
  rc_api_destroy_request(&request);

  return rc_client_async_handle_valid(client, async_handle) ? async_handle : NULL;
}

void rc_client_destroy_game_list(rc_client_game_list_t* list)
{
  free(list);
}


typedef struct rc_client_fetch_all_user_progress_callback_data_t {
  rc_client_t* client;
  rc_client_fetch_all_user_progress_callback_t callback;
  void* callback_userdata;
  uint32_t console_id;
  rc_client_async_handle_t async_handle;
} rc_client_fetch_all_user_progress_callback_data_t;

static void rc_client_fetch_all_user_progress_callback(const rc_api_server_response_t* server_response,
  void* callback_data)
{
  rc_client_fetch_all_user_progress_callback_data_t* ap_callback_data =
    (rc_client_fetch_all_user_progress_callback_data_t*)callback_data;
  rc_client_t* client = ap_callback_data->client;
  rc_api_fetch_all_user_progress_response_t ap_response;
  const char* error_message;
  int result;

  result = rc_client_end_async(client, &ap_callback_data->async_handle);
  if (result) {
    if (result != RC_CLIENT_ASYNC_DESTROYED)
      RC_CLIENT_LOG_VERBOSE(client, "Fetch all progress aborted");

    free(ap_callback_data);
    return;
  }

  result = rc_api_process_fetch_all_user_progress_server_response(&ap_response, server_response);
  error_message = rc_client_server_error_message(&result, server_response->http_status_code, &ap_response.response);
  if (error_message) {
    RC_CLIENT_LOG_ERR_FORMATTED(client, "Fetch all progress for console %u failed: %s", ap_callback_data->console_id,
      error_message);
    ap_callback_data->callback(result, error_message, NULL, client, ap_callback_data->callback_userdata);
  }
  else {
    rc_client_all_user_progress_t* list;
    const size_t list_size = sizeof(*list) + sizeof(rc_client_all_user_progress_entry_t) * ap_response.num_entries;

    list = (rc_client_all_user_progress_t*)malloc(list_size);
    if (!list) {
      ap_callback_data->callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), NULL, client,
        ap_callback_data->callback_userdata);
    }
    else {
      rc_client_all_user_progress_entry_t* entry = list->entries =
        (rc_client_all_user_progress_entry_t*)((uint8_t*)list + sizeof(*list));
      const rc_api_all_user_progress_entry_t* hlentry = ap_response.entries;
      const rc_api_all_user_progress_entry_t* stop = hlentry + ap_response.num_entries;

      for (; hlentry < stop; ++hlentry, ++entry)
      {
        entry->game_id = hlentry->game_id;
        entry->num_achievements = hlentry->num_achievements;
        entry->num_unlocked_achievements = hlentry->num_unlocked_achievements;
        entry->num_unlocked_achievements_hardcore = hlentry->num_unlocked_achievements_hardcore;
      }

      list->num_entries = ap_response.num_entries;

      ap_callback_data->callback(RC_OK, NULL, list, client, ap_callback_data->callback_userdata);
    }
  }

  rc_api_destroy_fetch_all_user_progress_response(&ap_response);
  free(ap_callback_data);
}

rc_client_async_handle_t* rc_client_begin_fetch_all_user_progress(rc_client_t* client, uint32_t console_id,
  rc_client_fetch_all_user_progress_callback_t callback,
  void* callback_userdata)
{
  rc_api_fetch_all_user_progress_request_t api_params;
  rc_client_fetch_all_user_progress_callback_data_t* callback_data;
  rc_client_async_handle_t* async_handle;
  rc_api_request_t request;
  int result;
  const char* error_message;

  if (!client) {
    callback(RC_INVALID_STATE, "client is required", NULL, client, callback_userdata);
    return NULL;
  }
  else if (client->state.user != RC_CLIENT_USER_STATE_LOGGED_IN) {
    callback(RC_INVALID_STATE, "client must be logged in", NULL, client, callback_userdata);
    return NULL;
  }

  api_params.username = client->user.username;
  api_params.api_token = client->user.token;
  api_params.console_id = console_id;

  result = rc_api_init_fetch_all_user_progress_request_hosted(&request, &api_params, &client->state.host);

  if (result != RC_OK) {
    error_message = rc_error_str(result);
    callback(result, error_message, NULL, client, callback_userdata);
    return NULL;
  }

  callback_data = (rc_client_fetch_all_user_progress_callback_data_t*)calloc(1, sizeof(*callback_data));
  if (!callback_data) {
    callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), NULL, client, callback_userdata);
    return NULL;
  }

  callback_data->client = client;
  callback_data->callback = callback;
  callback_data->callback_userdata = callback_userdata;
  callback_data->console_id = console_id;

  async_handle = &callback_data->async_handle;
  rc_client_begin_async(client, async_handle);
  client->callbacks.server_call(&request, rc_client_fetch_all_user_progress_callback, callback_data, client);
  rc_api_destroy_request(&request);

  return rc_client_async_handle_valid(client, async_handle) ? async_handle : NULL;
}

void rc_client_destroy_all_user_progress(rc_client_all_user_progress_t* list)
{
  free(list);
}

/* ===== Processing ===== */

void rc_client_set_event_handler(rc_client_t* client, rc_client_event_handler_t handler)
{
  if (!client)
    return;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->set_event_handler)
    client->state.external_client->set_event_handler(client, handler);
#endif

  client->callbacks.event_handler = handler;
}

void rc_client_set_read_memory_function(rc_client_t* client, rc_client_read_memory_func_t handler)
{
  if (!client)
    return;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->set_read_memory)
    client->state.external_client->set_read_memory(client, handler);
#endif

  client->callbacks.read_memory = handler;
}

void rc_client_set_allow_background_memory_reads(rc_client_t* client, int allowed)
{
  if (!client)
    return;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->set_allow_background_memory_reads)
    client->state.external_client->set_allow_background_memory_reads(allowed);
#endif

  client->state.allow_background_memory_reads = (uint8_t)allowed;
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
    rc_client_toggle_hardcore_achievements(client->game, client, RC_CLIENT_ACHIEVEMENT_UNLOCKED_CASUAL);

    if (!client->state.allow_leaderboards_in_casual)
      rc_client_deactivate_leaderboards(client->game, client);
  }
}

void rc_client_set_hardcore_enabled(rc_client_t* client, int enabled)
{
  int changed = 0;

  if (!client)
    return;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->get_hardcore_enabled) {
    client->state.external_client->set_hardcore_enabled(enabled);
    return;
  }
#endif

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
      rc_client_raise_pending_events(client, client->game);
    }
  }
}

int rc_client_get_hardcore_enabled(const rc_client_t* client)
{
  if (!client)
    return 0;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->get_hardcore_enabled)
    return client->state.external_client->get_hardcore_enabled();
#endif

  return client->state.hardcore;
}

void rc_client_set_unpromoted_enabled(rc_client_t* client, int enabled)
{
  if (!client)
    return;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->set_unpromoted_enabled) {
    client->state.external_client->set_unpromoted_enabled(enabled);
    return;
  }
#endif

  RC_CLIENT_LOG_INFO_FORMATTED(client, "Unpromoted %s", enabled ? "enabled" : "disabled");
  client->state.unpromoted_enabled = enabled ? 1 : 0;
}

int rc_client_get_unpromoted_enabled(const rc_client_t* client)
{
  if (!client)
    return 0;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->get_unpromoted_enabled)
    return client->state.external_client->get_unpromoted_enabled();
#endif

  return client->state.unpromoted_enabled;
}

void rc_client_set_encore_mode_enabled(rc_client_t* client, int enabled)
{
  if (!client)
    return;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->set_encore_mode_enabled) {
    client->state.external_client->set_encore_mode_enabled(enabled);
    return;
  }
#endif

  RC_CLIENT_LOG_INFO_FORMATTED(client, "Encore mode %s", enabled ? "enabled" : "disabled");
  client->state.encore_mode = enabled ? 1 : 0;
}

int rc_client_get_encore_mode_enabled(const rc_client_t* client)
{
  if (!client)
    return 0;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->get_encore_mode_enabled)
    return client->state.external_client->get_encore_mode_enabled();
#endif

  return client->state.encore_mode;
}

void rc_client_set_spectator_mode_enabled(rc_client_t* client, int enabled)
{
  if (!client)
    return;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->set_spectator_mode_enabled) {
    client->state.external_client->set_spectator_mode_enabled(enabled);
    return;
  }
#endif

  if (!enabled && client->state.spectator_mode == RC_CLIENT_SPECTATOR_MODE_LOCKED) {
    RC_CLIENT_LOG_WARN(client, "Spectator mode cannot be disabled if it was enabled prior to loading game.");
    return;
  }

  RC_CLIENT_LOG_INFO_FORMATTED(client, "Spectator mode %s", enabled ? "enabled" : "disabled");
  client->state.spectator_mode = enabled ? RC_CLIENT_SPECTATOR_MODE_ON : RC_CLIENT_SPECTATOR_MODE_OFF;
}

int rc_client_get_spectator_mode_enabled(const rc_client_t* client)
{
  if (!client)
    return 0;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->get_spectator_mode_enabled)
    return client->state.external_client->get_spectator_mode_enabled();
#endif

  return (client->state.spectator_mode == RC_CLIENT_SPECTATOR_MODE_OFF) ? 0 : 1;
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

void rc_client_set_host(rc_client_t* client, const char* hostname)
{
  if (!client)
    return;

  if (client->state.host.host && hostname && strcmp(hostname, client->state.host.host) == 0)
    return;

  /* clear out any previously specified host information */
  memset(&client->state.host, 0, sizeof(client->state.host));

  if (hostname && (!hostname[0] || strcmp(hostname, rc_api_default_host()) == 0)) {
    RC_CLIENT_LOG_VERBOSE_FORMATTED(client, "Using host: %s", rc_api_default_host());
    hostname = rc_api_default_host();
  }
  else {
    RC_CLIENT_LOG_VERBOSE_FORMATTED(client, "Using host: %s", hostname);
    client->state.host.host = rc_buffer_strcpy(&client->state.buffer, hostname);
  }

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->set_host)
    client->state.external_client->set_host(hostname);
#endif
}

size_t rc_client_get_user_agent_clause(rc_client_t* client, char buffer[], size_t buffer_size)
{
  size_t result;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client && client->state.external_client && client->state.external_client->get_user_agent_clause) {
    result = client->state.external_client->get_user_agent_clause(buffer, buffer_size);
    if (result > 0) {
      result += snprintf(buffer + result, buffer_size - result, " rc_client/" RCHEEVOS_VERSION_STRING);
      buffer[buffer_size - 1] = '\0';
      return result;
    }
  }
#else
  (void)client;
#endif

  result = snprintf(buffer, buffer_size, "rcheevos/" RCHEEVOS_VERSION_STRING);

  /* some implementations of snprintf will fill the buffer without null terminating.
   * make sure the buffer is null terminated */
  buffer[buffer_size - 1] = '\0';
  return result;
}
