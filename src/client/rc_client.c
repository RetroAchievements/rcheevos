#include "rc_client_internal.h"

#include "rc_api_info.h"
#include "rc_api_runtime.h"
#include "rc_api_user.h"
#include "rc_consoles.h"
#include "rc_hash.h"

#include "rc_client_async.h"
#include "rc_client_load.h"
#include "rc_client_server.h"
#include "rc_client_user.h"

#include "../api/rc_api_common.h"

#include "../runtime/rc_alloc.h"
#include "../runtime/rc_eval_state.h"
#include "../runtime/rc_format.h"
#include "../runtime/rc_lboard.h"
#include "../runtime/rc_memref.h"
#include "../runtime/rc_parse_state.h"
#include "../runtime/rc_richpresence.h"
#include "../runtime/rc_trigger.h"
#include "../runtime/rc_value.h"

#include "../util/rc_version.h"

#include <stdarg.h>

#ifdef _WIN32
 #define WIN32_LEAN_AND_MEAN
 #include <windows.h>
#else
 #include <time.h>
#endif

#define RC_CLIENT_RECENT_UNLOCK_DELAY_SECONDS (10 * 60) /* ten minutes */
#define RC_CLIENT_ACHIEVEMENT_WARNING_ID 101000001

#define RC_MINIMUM_UNPAUSED_FRAMES 20
#define RC_PAUSE_DECAY_MULTIPLIER 4

static void rc_client_hide_progress_tracker(rc_client_t* client, rc_client_game_info_t* game);
static void rc_client_ping(rc_client_scheduled_callback_data_t* callback_data, rc_client_t* client, rc_clock_t now);
static void rc_client_raise_leaderboard_events(rc_client_t* client, rc_client_subset_info_t* subset);
static void rc_client_reschedule_callback(rc_client_t* client, rc_client_scheduled_callback_data_t* callback, rc_clock_t when);
static void rc_client_award_achievement_retry(rc_client_scheduled_callback_data_t* callback_data, rc_client_t* client, rc_clock_t now);
static int rc_client_is_award_achievement_pending(const rc_client_t* client, uint32_t achievement_id);
static void rc_client_submit_leaderboard_entry_retry(rc_client_scheduled_callback_data_t* callback_data, rc_client_t* client, rc_clock_t now);
static uint32_t rc_client_read_modified_memory_helper(uint32_t address, uint8_t* buffer, uint32_t num_bytes, void* ud);

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

/* ===== Common ===== */

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

/* ===== User ===== */

static void rc_client_subset_get_user_game_summary(const rc_client_t* client,
    const rc_client_subset_info_t* subset, rc_client_user_game_summary_t* summary)
{
  rc_client_achievement_info_t* achievement = subset->achievements;
  rc_client_achievement_info_t* stop = achievement + subset->public_.num_achievements;
  time_t last_unlock_time = 0;
  time_t last_progression_time = 0;
  time_t first_win_time = 0;
  int num_progression_achievements = 0;
  int num_win_achievements = 0;
  int num_unlocked_progression_achievements = 0;
  const uint8_t unlock_bit = (client->state.hardcore) ?
    RC_CLIENT_ACHIEVEMENT_UNLOCKED_HARDCORE : RC_CLIENT_ACHIEVEMENT_UNLOCKED_CASUAL;

  rc_mutex_lock((rc_mutex_t*)&client->state.mutex); /* remove const cast for mutex access */

  for (; achievement < stop; ++achievement) {
    switch (achievement->public_.category) {
      case RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED:
        if (achievement->public_.id >= RC_CLIENT_ACHIEVEMENT_WARNING_ID) {
          /* ignore warning achievements */
          continue;
        }

        ++summary->num_promoted_achievements;
        summary->points_available += achievement->public_.points;

        if (achievement->public_.unlocked & unlock_bit) {
          ++summary->num_unlocked_achievements;
          summary->points_unlocked += achievement->public_.points;

          if (achievement->public_.unlock_time > last_unlock_time)
            last_unlock_time = achievement->public_.unlock_time;

          if (achievement->public_.type == RC_CLIENT_ACHIEVEMENT_TYPE_PROGRESSION) {
            ++num_unlocked_progression_achievements;
            if (achievement->public_.unlock_time > last_progression_time)
              last_progression_time = achievement->public_.unlock_time;
          }
          else if (achievement->public_.type == RC_CLIENT_ACHIEVEMENT_TYPE_WIN) {
            if (first_win_time == 0 || achievement->public_.unlock_time < first_win_time)
              first_win_time = achievement->public_.unlock_time;
          }
        }

        if (achievement->public_.type == RC_CLIENT_ACHIEVEMENT_TYPE_PROGRESSION)
          ++num_progression_achievements;
        else if (achievement->public_.type == RC_CLIENT_ACHIEVEMENT_TYPE_WIN)
          ++num_win_achievements;

        if (achievement->public_.bucket == RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED)
          ++summary->num_unsupported_achievements;

        break;

      case RC_CLIENT_ACHIEVEMENT_CATEGORY_UNPROMOTED:
        ++summary->num_unpromoted_achievements;
        break;

      default:
        continue;
    }
  }

  rc_mutex_unlock((rc_mutex_t*)&client->state.mutex); /* remove const cast for mutex access */

  if (summary->num_unlocked_achievements == summary->num_promoted_achievements)
    summary->completed_time = last_unlock_time;

  if ((first_win_time || num_win_achievements == 0) && num_unlocked_progression_achievements == num_progression_achievements)
    summary->beaten_time = (first_win_time > last_progression_time) ? first_win_time : last_progression_time;
}

void rc_client_get_user_game_summary(const rc_client_t* client, rc_client_user_game_summary_t* summary)
{
  if (!summary)
    return;

  memset(summary, 0, sizeof(*summary));
  if (!client)
    return;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client) {
    if (client->state.external_client->get_user_game_summary_v5) {
      client->state.external_client->get_user_game_summary_v5(summary);
      return;
    }
    if (client->state.external_client->get_user_game_summary) {
      client->state.external_client->get_user_game_summary(summary);
      return;
    }
  }
#endif

  if (rc_client_is_game_loaded(client))
    rc_client_subset_get_user_game_summary(client, client->game->subsets, summary);
}

void rc_client_get_user_subset_summary(const rc_client_t* client, uint32_t subset_id, rc_client_user_game_summary_t* summary)
{
  if (!summary)
    return;

  memset(summary, 0, sizeof(*summary));
  if (!client || !subset_id)
    return;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->get_user_subset_summary) {
    client->state.external_client->get_user_subset_summary(subset_id, summary);
    return;
  }
#endif

  if (rc_client_is_game_loaded(client)) {
    const rc_client_subset_info_t* subset = client->game->subsets;
    for (; subset; subset = subset->next) {
      if (subset->public_.id == subset_id) {
        rc_client_subset_get_user_game_summary(client, subset, summary);
        break;
      }
    }
  }
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
  } else {
    rc_client_all_user_progress_t* list;
    const size_t list_size = sizeof(*list) + sizeof(rc_client_all_user_progress_entry_t) * ap_response.num_entries;

    list = (rc_client_all_user_progress_t*)malloc(list_size);
    if (!list) {
      ap_callback_data->callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), NULL, client,
                                 ap_callback_data->callback_userdata);
    } else {
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
  } else if (client->state.user != RC_CLIENT_USER_STATE_LOGGED_IN) {
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

/* ===== Game ===== */

static void rc_client_invalidate_memref_achievements(rc_client_game_info_t* game, rc_client_t* client, rc_memref_t* memref)
{
  rc_client_subset_info_t* subset = game->subsets;
  for (; subset; subset = subset->next) {
    rc_client_achievement_info_t* achievement = subset->achievements;
    rc_client_achievement_info_t* stop = achievement + subset->public_.num_achievements;
    for (; achievement < stop; ++achievement) {
      if (achievement->public_.state == RC_CLIENT_ACHIEVEMENT_STATE_DISABLED)
        continue;

      if (rc_trigger_contains_memref(achievement->trigger, memref)) {
        achievement->public_.state = RC_CLIENT_ACHIEVEMENT_STATE_DISABLED;
        achievement->public_.bucket = RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED;

        if (achievement->trigger)
          achievement->trigger->state = RC_TRIGGER_STATE_DISABLED;

        RC_CLIENT_LOG_WARN_FORMATTED(client, "Disabled achievement %u. Invalid address %06X", achievement->public_.id, memref->address);
      }
    }
  }
}

static void rc_client_invalidate_memref_leaderboards(rc_client_game_info_t* game, rc_client_t* client, rc_memref_t* memref)
{
  rc_client_subset_info_t* subset = game->subsets;
  for (; subset; subset = subset->next) {
    rc_client_leaderboard_info_t* leaderboard = subset->leaderboards;
    rc_client_leaderboard_info_t* stop = leaderboard + subset->public_.num_leaderboards;
    for (; leaderboard < stop; ++leaderboard) {
      if (leaderboard->public_.state == RC_CLIENT_LEADERBOARD_STATE_DISABLED)
        continue;
      if (!leaderboard->lboard)
        continue;

      if (rc_trigger_contains_memref(&leaderboard->lboard->start, memref))
        leaderboard->public_.state = RC_CLIENT_LEADERBOARD_STATE_DISABLED;
      else if (rc_trigger_contains_memref(&leaderboard->lboard->cancel, memref))
        leaderboard->public_.state = RC_CLIENT_LEADERBOARD_STATE_DISABLED;
      else if (rc_trigger_contains_memref(&leaderboard->lboard->submit, memref))
        leaderboard->public_.state = RC_CLIENT_LEADERBOARD_STATE_DISABLED;
      else if (rc_value_contains_memref(&leaderboard->lboard->value, memref))
        leaderboard->public_.state = RC_CLIENT_LEADERBOARD_STATE_DISABLED;
      else
        continue;

      leaderboard->lboard->state = RC_LBOARD_STATE_DISABLED;

      RC_CLIENT_LOG_WARN_FORMATTED(client, "Disabled leaderboard %u. Invalid address %06X", leaderboard->public_.id, memref->address);
    }
  }
}

void rc_client_validate_addresses(rc_client_game_info_t* game, rc_client_t* client)
{
  const rc_memory_regions_t* regions = rc_console_memory_regions(game->public_.console_id);
  const uint32_t max_address = (regions && regions->num_regions > 0) ?
    regions->region[regions->num_regions - 1].end_address : 0xFFFFFFFF;
  uint8_t buffer[8];
  uint32_t total_count = 0;
  uint32_t invalid_count = 0;

  rc_memref_list_t* memref_list = &game->runtime.memrefs->memrefs;
  for (; memref_list; memref_list = memref_list->next) {
    rc_memref_t* memref = memref_list->items;
    const rc_memref_t* memref_end = memref + memref_list->count;
    total_count += memref_list->count;

    for (; memref < memref_end; ++memref) {
      if (memref->address > max_address ||
        client->callbacks.read_memory(memref->address, buffer, 1, client) == 0) {
        memref->value.type = RC_VALUE_TYPE_NONE;

        rc_client_invalidate_memref_achievements(game, client, memref);
        rc_client_invalidate_memref_leaderboards(game, client, memref);

        invalid_count++;
      }
    }
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
    rc_client_subset_info_t* subset;

    if (active_count <= game->runtime.trigger_capacity) {
      if (active_count != 0)
        memset(game->runtime.triggers, 0, active_count * sizeof(rc_runtime_trigger_t));
    } else {
      if (game->runtime.triggers)
        free(game->runtime.triggers);

      game->runtime.trigger_capacity = active_count;
      game->runtime.triggers = (rc_runtime_trigger_t*)calloc(1, active_count * sizeof(rc_runtime_trigger_t));
    }

    trigger = game->runtime.triggers;
    if (!trigger) {
      /* malloc failed, no way to report error, just bail */
      game->runtime.trigger_count = 0;
      return;
    }

    for (subset = game->subsets; subset; subset = subset->next) {
      if (!subset->active)
        continue;

      achievement = subset->achievements;
      stop = achievement + subset->public_.num_achievements;

      for (; achievement < stop; ++achievement) {
        if (achievement->public_.state == RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE) {
          trigger->id = achievement->public_.id;
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
  rc_client_achievement_info_t* stop = achievement + subset->public_.num_achievements;
  uint32_t active_count = 0;

  for (; achievement < stop; ++achievement) {
    if (achievement->public_.state == RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE)
      ++active_count;
  }

  return active_count;
}

void rc_client_update_active_achievements(rc_client_game_info_t* game)
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
  rc_client_achievement_info_t* stop = achievement + subset->public_.num_achievements;
  uint32_t active_count = 0;

  for (; achievement < stop; ++achievement) {
    if ((achievement->public_.unlocked & active_bit) == 0) {
      switch (achievement->public_.state) {
        case RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED:
        case RC_CLIENT_ACHIEVEMENT_STATE_INACTIVE:
          rc_reset_trigger(achievement->trigger);
          achievement->public_.state = RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE;
          ++active_count;
          break;

        case RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE:
          ++active_count;
          break;
      }
    }
    else {
      achievement->public_.unlock_time = (active_bit == RC_CLIENT_ACHIEVEMENT_UNLOCKED_HARDCORE) ?
          achievement->unlock_time_hardcore : achievement->unlock_time_casual;

      if (achievement->public_.state == RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE ||
          achievement->public_.state == RC_CLIENT_ACHIEVEMENT_STATE_INACTIVE) {
        /* if it's active despite being unlocked, and we're in encore mode, leave it active */
        if (client->state.encore_mode) {
          ++active_count;
          continue;
        }

        /* switch to inactive */
        achievement->public_.state = RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED;

        if (achievement->trigger && rc_trigger_state_active(achievement->trigger->state)) {
          /* hide any active challenge indicators */
          if (achievement->trigger->state == RC_TRIGGER_STATE_PRIMED) {
            rc_client_event_t client_event;
            memset(&client_event, 0, sizeof(client_event));
            client_event.type = RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE;
            client_event.achievement = &achievement->public_;
            client->callbacks.event_handler(&client_event, client);
          }

          achievement->trigger->state = RC_TRIGGER_STATE_TRIGGERED;
        }
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

void rc_client_activate_achievements(rc_client_game_info_t* game, rc_client_t* client)
{
  const uint8_t active_bit = (client->state.encore_mode) ?
      RC_CLIENT_ACHIEVEMENT_UNLOCKED_NONE : (client->state.hardcore) ?
      RC_CLIENT_ACHIEVEMENT_UNLOCKED_HARDCORE : RC_CLIENT_ACHIEVEMENT_UNLOCKED_CASUAL;

  rc_client_toggle_hardcore_achievements(game, client, active_bit);
}

static void rc_client_update_legacy_runtime_leaderboards(rc_client_game_info_t* game, uint32_t active_count)
{
  if (active_count > 0) {
    rc_client_leaderboard_info_t* leaderboard;
    rc_client_leaderboard_info_t* stop;
    rc_client_subset_info_t* subset;
    rc_runtime_lboard_t* lboard;

    if (active_count <= game->runtime.lboard_capacity) {
      if (active_count != 0)
        memset(game->runtime.lboards, 0, active_count * sizeof(rc_runtime_lboard_t));
    } else {
      if (game->runtime.lboards)
        free(game->runtime.lboards);

      game->runtime.lboard_capacity = active_count;
      game->runtime.lboards = (rc_runtime_lboard_t*)calloc(1, active_count * sizeof(rc_runtime_lboard_t));
    }

    lboard = game->runtime.lboards;
    if (!lboard) {
      /* malloc failed. no way to report error, just bail */
      game->runtime.lboard_count = 0;
      return;
    }

    for (subset = game->subsets; subset; subset = subset->next) {
      if (!subset->active)
        continue;

      leaderboard = subset->leaderboards;
      stop = leaderboard + subset->public_.num_leaderboards;
      for (; leaderboard < stop; ++leaderboard) {
        if (leaderboard->public_.state == RC_CLIENT_LEADERBOARD_STATE_ACTIVE ||
            leaderboard->public_.state == RC_CLIENT_LEADERBOARD_STATE_TRACKING) {
          lboard->id = leaderboard->public_.id;
          memcpy(lboard->md5, leaderboard->md5, 16);
          lboard->lboard = leaderboard->lboard;
          ++lboard;
        }
      }
    }
  }

  game->runtime.lboard_count = active_count;
}

void rc_client_update_active_leaderboards(rc_client_game_info_t* game)
{
  rc_client_leaderboard_info_t* leaderboard;
  rc_client_leaderboard_info_t* stop;

  uint32_t active_count = 0;
  rc_client_subset_info_t* subset = game->subsets;
  for (; subset; subset = subset->next)
  {
    if (!subset->active)
      continue;

    leaderboard = subset->leaderboards;
    stop = leaderboard + subset->public_.num_leaderboards;

    for (; leaderboard < stop; ++leaderboard)
    {
      switch (leaderboard->public_.state)
      {
        case RC_CLIENT_LEADERBOARD_STATE_ACTIVE:
        case RC_CLIENT_LEADERBOARD_STATE_TRACKING:
          ++active_count;
          break;
      }
    }
  }

  rc_client_update_legacy_runtime_leaderboards(game, active_count);
}

void rc_client_activate_leaderboards(rc_client_game_info_t* game, rc_client_t* client)
{
  rc_client_leaderboard_info_t* leaderboard;
  rc_client_leaderboard_info_t* stop;
  const uint8_t leaderboards_allowed =
      client->state.hardcore || client->state.allow_leaderboards_in_casual;

  uint32_t active_count = 0;
  rc_client_subset_info_t* subset = game->subsets;
  for (; subset; subset = subset->next) {
    if (!subset->active)
      continue;

    leaderboard = subset->leaderboards;
    stop = leaderboard + subset->public_.num_leaderboards;

    for (; leaderboard < stop; ++leaderboard) {
      switch (leaderboard->public_.state) {
        case RC_CLIENT_LEADERBOARD_STATE_DISABLED:
          continue;

        case RC_CLIENT_LEADERBOARD_STATE_INACTIVE:
          if (leaderboards_allowed) {
            rc_reset_lboard(leaderboard->lboard);
            leaderboard->public_.state = RC_CLIENT_LEADERBOARD_STATE_ACTIVE;
            ++active_count;
          }
          break;

        default:
          if (leaderboards_allowed)
            ++active_count;
          else
            leaderboard->public_.state = RC_CLIENT_LEADERBOARD_STATE_INACTIVE;
          break;
      }
    }
  }

  rc_client_update_legacy_runtime_leaderboards(game, active_count);
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
    stop = leaderboard + subset->public_.num_leaderboards;

    for (; leaderboard < stop; ++leaderboard) {
      switch (leaderboard->public_.state) {
        case RC_CLIENT_LEADERBOARD_STATE_DISABLED:
        case RC_CLIENT_LEADERBOARD_STATE_INACTIVE:
          continue;

        case RC_CLIENT_LEADERBOARD_STATE_TRACKING:
          rc_client_release_leaderboard_tracker(client->game, leaderboard);
          /* fallthrough */ /* to default */
        default:
          leaderboard->public_.state = RC_CLIENT_LEADERBOARD_STATE_INACTIVE;
          break;
      }
    }
  }

  game->runtime.lboard_count = 0;
}

uint8_t rc_client_map_leaderboard_format(int format)
{
  switch (format) {
    case RC_FORMAT_SECONDS:
    case RC_FORMAT_CENTISECS:
    case RC_FORMAT_MINUTES:
    case RC_FORMAT_SECONDS_AS_MINUTES:
    case RC_FORMAT_FRAMES:
      return RC_CLIENT_LEADERBOARD_FORMAT_TIME;

    case RC_FORMAT_SCORE:
      return RC_CLIENT_LEADERBOARD_FORMAT_SCORE;

    case RC_FORMAT_VALUE:
    case RC_FORMAT_FLOAT1:
    case RC_FORMAT_FLOAT2:
    case RC_FORMAT_FLOAT3:
    case RC_FORMAT_FLOAT4:
    case RC_FORMAT_FLOAT5:
    case RC_FORMAT_FLOAT6:
    case RC_FORMAT_FIXED1:
    case RC_FORMAT_FIXED2:
    case RC_FORMAT_FIXED3:
    case RC_FORMAT_TENS:
    case RC_FORMAT_HUNDREDS:
    case RC_FORMAT_THOUSANDS:
    case RC_FORMAT_UNSIGNED_VALUE:
    default:
      return RC_CLIENT_LEADERBOARD_FORMAT_VALUE;
  }
}

rc_client_game_hash_t* rc_client_find_game_hash(rc_client_t* client, const char* hash)
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
    game_hash = (rc_client_game_hash_t*)rc_buffer_alloc(&client->state.buffer, sizeof(rc_client_game_hash_t));
    memset(game_hash, 0, sizeof(*game_hash));
    snprintf(game_hash->hash, sizeof(game_hash->hash), "%s", hash);
    game_hash->game_id = RC_CLIENT_UNKNOWN_GAME_ID;
    game_hash->next = client->hashes;
    client->hashes = game_hash;
  }
  rc_mutex_unlock(&client->state.mutex);

  return game_hash;
}

void rc_client_add_game_hash(rc_client_t* client, const char* hash, uint32_t game_id)
{
  /* store locally, even if passing to external client */
  rc_client_game_hash_t* game_hash = rc_client_find_game_hash(client, hash);
  game_hash->game_id = game_id;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->add_game_hash)
    client->state.external_client->add_game_hash(hash, game_id);
#endif
}

void rc_client_game_mark_ui_to_be_hidden(rc_client_t* client, rc_client_game_info_t* game)
{
  rc_client_achievement_info_t* achievement;
  rc_client_achievement_info_t* achievement_stop;
  rc_client_leaderboard_info_t* leaderboard;
  rc_client_leaderboard_info_t* leaderboard_stop;
  rc_client_subset_info_t* subset;

  for (subset = game->subsets; subset; subset = subset->next) {
    achievement = subset->achievements;
    achievement_stop = achievement + subset->public_.num_achievements;
    for (; achievement < achievement_stop; ++achievement) {
      if (achievement->public_.state == RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE &&
        achievement->trigger && achievement->trigger->state == RC_TRIGGER_STATE_PRIMED) {
        achievement->pending_events |= RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_HIDE;
        subset->pending_events |= RC_CLIENT_SUBSET_PENDING_EVENT_ACHIEVEMENT;
      }
    }

    leaderboard = subset->leaderboards;
    leaderboard_stop = leaderboard + subset->public_.num_leaderboards;
    for (; leaderboard < leaderboard_stop; ++leaderboard) {
      if (leaderboard->public_.state == RC_CLIENT_LEADERBOARD_STATE_TRACKING)
        rc_client_release_leaderboard_tracker(game, leaderboard);
    }
  }

  rc_client_hide_progress_tracker(client, game);
}

const rc_client_game_t* rc_client_get_game_info(const rc_client_t* client)
{
  if (!client)
    return NULL;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client) {
    if (client->state.external_client->get_game_info_v3)
      return client->state.external_client->get_game_info_v3();

    if (client->state.external_client->get_game_info)
      return rc_client_external_convert_v1_game(client, client->state.external_client->get_game_info());
  }
#endif

  return client->game ? &client->game->public_ : NULL;
}

int rc_client_game_get_image_url(const rc_client_game_t* game, char buffer[], size_t buffer_size)
{
  if (!game)
    return RC_INVALID_STATE;

  if (game->badge_url) {
    const size_t len = strlen(game->badge_url);
    if (len >= buffer_size)
      return RC_INSUFFICIENT_BUFFER;

    memcpy(buffer, game->badge_url, len + 1);
    return RC_OK;
  }

  return rc_client_get_image_url(buffer, buffer_size, RC_IMAGE_TYPE_GAME, game->badge_name);
}

/* ===== Subsets ===== */

const rc_client_subset_t* rc_client_get_subset_info(rc_client_t* client, uint32_t subset_id)
{
  rc_client_subset_info_t* subset;

  if (!client)
    return NULL;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client) {
    if (client->state.external_client->get_subset_info_v3)
      return client->state.external_client->get_subset_info_v3(subset_id);

    if (client->state.external_client->get_subset_info)
      return rc_client_external_convert_v1_subset(client, client->state.external_client->get_subset_info(subset_id));
  }
#endif

  if (!client->game)
    return NULL;

  for (subset = client->game->subsets; subset; subset = subset->next) {
    if (subset->public_.id == subset_id)
      return &subset->public_;
  }

  return NULL;
}

rc_client_subset_list_t* rc_client_create_subset_list(rc_client_t* client)
{
  rc_client_subset_list_info_t* list;
  const rc_client_subset_info_t* subset;
  const rc_client_subset_t** subset_ptr;
  const uint32_t list_size = RC_ALIGN(sizeof(*list));
  uint32_t num_subsets = 0;

  if (!client)
    return (rc_client_subset_list_t*)calloc(1, list_size);

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->create_subset_list)
    return (rc_client_subset_list_t*)client->state.external_client->create_subset_list();
#endif

  if (!client->game)
    return (rc_client_subset_list_t*)calloc(1, list_size);

  rc_mutex_lock(&client->state.mutex);

  subset = client->game->subsets;
  for (; subset; subset = subset->next) {
    if (subset->active)
      num_subsets++;
  }

  list = (rc_client_subset_list_info_t*)malloc(list_size + num_subsets * sizeof(rc_client_subset_t*));
  if (!list)
    return NULL;
  list->public_.subsets = subset_ptr = (const rc_client_subset_t**)((uint8_t*)list + list_size);

  subset = client->game->subsets;
  for (; subset; subset = subset->next) {
    if (subset->active)
      *subset_ptr++ = &subset->public_;
  }

  rc_mutex_unlock(&client->state.mutex);

  list->destroy_func = NULL;
  list->public_.num_subsets = (uint32_t)(subset_ptr - list->public_.subsets);
  return &list->public_;
}

void rc_client_destroy_subset_list(rc_client_subset_list_t* list)
{
  rc_client_subset_list_info_t* info = (rc_client_subset_list_info_t*)list;
  if (info->destroy_func)
    info->destroy_func(info);
  else
    free(list);
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

/* ===== Achievements ===== */

static void rc_client_update_achievement_display_information(rc_client_t* client, rc_client_achievement_info_t* achievement, time_t recent_unlock_time)
{
  uint8_t new_bucket = RC_CLIENT_ACHIEVEMENT_BUCKET_UNKNOWN;
  uint32_t new_measured_value = 0;

  if (achievement->public_.bucket == RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED)
    return;

  achievement->public_.measured_progress[0] = '\0';

  if (achievement->public_.state == RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED) {
    /* achievement unlocked */
    if (achievement->public_.unlock_time >= recent_unlock_time) {
      new_bucket = RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED;
    } else {
      new_bucket = RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED;

      if (client->state.disconnect && rc_client_is_award_achievement_pending(client, achievement->public_.id))
        new_bucket = RC_CLIENT_ACHIEVEMENT_BUCKET_UNSYNCED;
    }
  }
  else {
    /* active achievement */
    new_bucket = (achievement->public_.category == RC_CLIENT_ACHIEVEMENT_CATEGORY_UNPROMOTED) ?
        RC_CLIENT_ACHIEVEMENT_BUCKET_UNPROMOTED : RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED;

    if (achievement->trigger) {
      if (achievement->trigger->measured_target) {
        if (achievement->trigger->measured_value == RC_MEASURED_UNKNOWN) {
          /* value hasn't been initialized yet, leave progress string empty */
        }
        else if (achievement->trigger->measured_value == 0) {
          /* value is 0, leave progress string empty. update progress to 0.0 */
          achievement->public_.measured_percent = 0.0;
        }
        else {
          /* clamp measured value at target (can't get more than 100%) */
          new_measured_value = (achievement->trigger->measured_value > achievement->trigger->measured_target) ?
              achievement->trigger->measured_target : achievement->trigger->measured_value;

          achievement->public_.measured_percent = ((float)new_measured_value * 100) / (float)achievement->trigger->measured_target;

          if (!achievement->trigger->measured_as_percent) {
            char* ptr = achievement->public_.measured_progress;
            const int buffer_size = (int)sizeof(achievement->public_.measured_progress);
            const int chars = rc_format_value(ptr, buffer_size - 1, (int32_t)new_measured_value, RC_FORMAT_UNSIGNED_VALUE);
            ptr[chars] = '/';
            rc_format_value(ptr + chars + 1, buffer_size - chars - 1, (int32_t)achievement->trigger->measured_target, RC_FORMAT_UNSIGNED_VALUE);
          }
          else if (achievement->public_.measured_percent >= 1.0) {
            snprintf(achievement->public_.measured_progress, sizeof(achievement->public_.measured_progress),
                "%lu%%", (unsigned long)achievement->public_.measured_percent);
          }
        }
      }

      if (achievement->trigger->state == RC_TRIGGER_STATE_PRIMED)
        new_bucket = RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE;
      else if (achievement->public_.measured_percent >= 80.0)
        new_bucket = RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE;
    }
  }

  achievement->public_.bucket = new_bucket;
}

static const char* rc_client_get_achievement_bucket_label(uint8_t bucket_type)
{
  switch (bucket_type) {
    case RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED: return "Locked";
    case RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED: return "Unlocked";
    case RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED: return "Unsupported";
    case RC_CLIENT_ACHIEVEMENT_BUCKET_UNPROMOTED: return "Unpromoted";
    case RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED: return "Recently Unlocked";
    case RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE: return "Active Challenges";
    case RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE: return "Almost There";
    case RC_CLIENT_ACHIEVEMENT_BUCKET_UNSYNCED: return "Unlocks Not Synced to Server";
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
    case RC_CLIENT_ACHIEVEMENT_BUCKET_UNPROMOTED: ptr = &subset->unpromoted_label; break;
    default: return rc_client_get_achievement_bucket_label(bucket_type);
  }

  if (*ptr)
    return *ptr;

  label = rc_client_get_achievement_bucket_label(bucket_type);
  new_label_len = strlen(subset->public_.title) + strlen(label) + 4;
  new_label = (char*)rc_buffer_alloc(&game->buffer, new_label_len);
  snprintf(new_label, new_label_len, "%s - %s", subset->public_.title, label);

  *ptr = new_label;
  return new_label;
}

static int rc_client_compare_achievement_unlock_times(const void* a, const void* b)
{
  const rc_client_achievement_t* unlock_a = *(const rc_client_achievement_t**)a;
  const rc_client_achievement_t* unlock_b = *(const rc_client_achievement_t**)b;
  if (unlock_b->unlock_time == unlock_a->unlock_time)
    return 0;
  return (unlock_b->unlock_time < unlock_a->unlock_time) ? -1 : 1;
}

static int rc_client_compare_achievement_progress(const void* a, const void* b)
{
  const rc_client_achievement_t* unlock_a = *(const rc_client_achievement_t**)a;
  const rc_client_achievement_t* unlock_b = *(const rc_client_achievement_t**)b;
  if (unlock_b->measured_percent == unlock_a->measured_percent) {
    if (unlock_a->id == unlock_b->id)
      return 0;
    return (unlock_a->id < unlock_b->id) ? -1 : 1;
  }
  return (unlock_b->measured_percent < unlock_a->measured_percent) ? -1 : 1;
}

static uint8_t rc_client_map_bucket(uint8_t bucket, int grouping)
{
  if (grouping == RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE) {
    switch (bucket) {
      case RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED:
      case RC_CLIENT_ACHIEVEMENT_BUCKET_UNSYNCED:
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

static int rc_client_is_subset_visible_in_achievement_list(const rc_client_subset_info_t* subset, const rc_client_achievement_list_params_t* params)
{
  if (!subset->active)
    return 0;

  return (params->subset_id == 0 || params->subset_id == subset->public_.id);
}

static int rc_client_is_achievement_visible_in_list(const rc_client_achievement_info_t* achievement, const rc_client_achievement_list_params_t* params)
{
  return (achievement->public_.category & params->category) != 0;
}

rc_client_achievement_list_t* rc_client_create_achievement_list(rc_client_t* client, int category, int grouping)
{
  rc_client_achievement_list_params_t params;
  params.subset_id = 0;
  params.grouping = grouping;
  params.category = category;

  return rc_client_create_subset_achievement_list(client, &params);
}

rc_client_achievement_list_t* rc_client_create_subset_achievement_list(rc_client_t* client, const rc_client_achievement_list_params_t* params)
{
  rc_client_achievement_info_t* achievement;
  rc_client_achievement_info_t* stop;
  const rc_client_achievement_t** bucket_achievements;
  const rc_client_achievement_t** achievement_ptr;
  rc_client_achievement_bucket_t* bucket_ptr;
  rc_client_achievement_list_info_t* list;
  rc_client_subset_info_t* subset;
  const uint32_t list_size = RC_ALIGN(sizeof(*list));
  uint32_t bucket_counts[NUM_RC_CLIENT_ACHIEVEMENT_BUCKETS];
  uint32_t num_buckets;
  uint32_t num_achievements;
  size_t buckets_size;
  uint8_t bucket_type;
  uint32_t num_subsets = 0;
  uint32_t i, j;
  const uint8_t shared_bucket_order[] = {
    RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE,
    RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED,
    RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE,
    RC_CLIENT_ACHIEVEMENT_BUCKET_UNSYNCED,
  };
  const uint8_t subset_bucket_order[] = {
    RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED,
    RC_CLIENT_ACHIEVEMENT_BUCKET_UNPROMOTED,
    RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED,
    RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED
  };
  const time_t recent_unlock_time = time(NULL) - RC_CLIENT_RECENT_UNLOCK_DELAY_SECONDS;

  if (!client)
    return (rc_client_achievement_list_t*)calloc(1, sizeof(rc_client_achievement_list_info_t));

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client) {
    if (client->state.external_client->create_subset_achievement_list)
      return (rc_client_achievement_list_t*)client->state.external_client->create_subset_achievement_list(params);

    if (client->state.external_client->create_achievement_list_v3)
      return (rc_client_achievement_list_t*)client->state.external_client->create_achievement_list_v3(params->category, params->grouping);

    if (client->state.external_client->create_achievement_list)
      return rc_client_external_convert_v1_achievement_list(client,
        (rc_client_achievement_list_t*)client->state.external_client->create_achievement_list(params->category, params->grouping));
  }
#endif

  if (!client->game)
    return (rc_client_achievement_list_t*)calloc(1, sizeof(rc_client_achievement_list_info_t));

  memset(&bucket_counts, 0, sizeof(bucket_counts));

  rc_mutex_lock(&client->state.mutex);

  subset = client->game->subsets;
  for (; subset; subset = subset->next) {
    if (!rc_client_is_subset_visible_in_achievement_list(subset, params))
      continue;

    num_subsets++;
    achievement = subset->achievements;
    stop = achievement + subset->public_.num_achievements;
    for (; achievement < stop; ++achievement) {
      if (rc_client_is_achievement_visible_in_list(achievement, params)) {
        rc_client_update_achievement_display_information(client, achievement, recent_unlock_time);
        bucket_counts[rc_client_map_bucket(achievement->public_.bucket, params->grouping)]++;
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
        if (!rc_client_is_subset_visible_in_achievement_list(subset, params))
          continue;

        achievement = subset->achievements;
        stop = achievement + subset->public_.num_achievements;
        for (; achievement < stop; ++achievement) {
          if (rc_client_is_achievement_visible_in_list(achievement, params)) {
            if (rc_client_map_bucket(achievement->public_.bucket, params->grouping) == i) {
              ++num_buckets;
              break;
            }
          }
        }
      }
    }
  }

  buckets_size = RC_ALIGN(num_buckets * sizeof(rc_client_achievement_bucket_t));

  list = (rc_client_achievement_list_info_t*)malloc(list_size + buckets_size + num_achievements * sizeof(rc_client_achievement_t*));
  list->public_.buckets = bucket_ptr = (rc_client_achievement_bucket_t*)((uint8_t*)list + list_size);
  achievement_ptr = (const rc_client_achievement_t**)((uint8_t*)bucket_ptr + buckets_size);

  if (params->grouping == RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS) {
    for (i = 0; i < sizeof(shared_bucket_order) / sizeof(shared_bucket_order[0]); ++i) {
      bucket_type = shared_bucket_order[i];
      if (!bucket_counts[bucket_type])
        continue;

      bucket_achievements = achievement_ptr;
      for (subset = client->game->subsets; subset; subset = subset->next) {
        if (!rc_client_is_subset_visible_in_achievement_list(subset, params))
          continue;

        achievement = subset->achievements;
        stop = achievement + subset->public_.num_achievements;
        for (; achievement < stop; ++achievement) {
          if (rc_client_is_achievement_visible_in_list(achievement, params) &&
              rc_client_map_bucket(achievement->public_.bucket, params->grouping) == bucket_type) {
            *achievement_ptr++ = &achievement->public_;
          }
        }
      }

      if (achievement_ptr > bucket_achievements) {
        bucket_ptr->achievements = bucket_achievements;
        bucket_ptr->num_achievements = (uint32_t)(achievement_ptr - bucket_achievements);
        bucket_ptr->subset_id = 0;
        bucket_ptr->label = rc_client_get_achievement_bucket_label(bucket_type);
        bucket_ptr->bucket_type = bucket_type;

        if (bucket_type == RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED)
          qsort((void*)bucket_ptr->achievements, bucket_ptr->num_achievements, sizeof(rc_client_achievement_t*), rc_client_compare_achievement_unlock_times);
        else if (bucket_type == RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE)
          qsort((void*)bucket_ptr->achievements, bucket_ptr->num_achievements, sizeof(rc_client_achievement_t*), rc_client_compare_achievement_progress);

        ++bucket_ptr;
      }
    }
  }

  for (subset = client->game->subsets; subset; subset = subset->next) {
    if (!rc_client_is_subset_visible_in_achievement_list(subset, params))
      continue;

    for (i = 0; i < sizeof(subset_bucket_order) / sizeof(subset_bucket_order[0]); ++i) {
      bucket_type = subset_bucket_order[i];
      if (!bucket_counts[bucket_type])
        continue;

      bucket_achievements = achievement_ptr;

      achievement = subset->achievements;
      stop = achievement + subset->public_.num_achievements;
      for (; achievement < stop; ++achievement) {
        if (rc_client_is_achievement_visible_in_list(achievement, params) &&
            rc_client_map_bucket(achievement->public_.bucket, params->grouping) == bucket_type) {
          *achievement_ptr++ = &achievement->public_;
        }
      }

      if (achievement_ptr > bucket_achievements) {
        bucket_ptr->achievements = bucket_achievements;
        bucket_ptr->num_achievements = (uint32_t)(achievement_ptr - bucket_achievements);
        bucket_ptr->subset_id = (num_subsets > 1) ? subset->public_.id : 0;
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

  list->destroy_func = NULL;
  list->public_.num_buckets = (uint32_t)(bucket_ptr - list->public_.buckets);
  return &list->public_;
}

void rc_client_destroy_achievement_list(rc_client_achievement_list_t* list)
{
  rc_client_achievement_list_info_t* info = (rc_client_achievement_list_info_t*)list;
  if (info->destroy_func)
    info->destroy_func(info);
  else
    free(list);
}

int rc_client_has_achievements(rc_client_t* client)
{
  rc_client_subset_info_t* subset;
  int result;

  if (!client)
    return 0;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->has_achievements)
    return client->state.external_client->has_achievements();
#endif

  if (!client->game)
    return 0;

  rc_mutex_lock(&client->state.mutex);

  subset = client->game->subsets;
  result = 0;
  for (; subset; subset = subset->next)
  {
    if (!subset->active)
      continue;

    if (subset->public_.num_achievements > 0) {
      result = 1;
      break;
    }
  }

  rc_mutex_unlock(&client->state.mutex);

  return result;
}

static const rc_client_achievement_t* rc_client_subset_get_achievement_info(
    rc_client_t* client, rc_client_subset_info_t* subset, uint32_t id)
{
  rc_client_achievement_info_t* achievement = subset->achievements;
  rc_client_achievement_info_t* stop = achievement + subset->public_.num_achievements;

  for (; achievement < stop; ++achievement) {
    if (achievement->public_.id == id) {
      const time_t recent_unlock_time = time(NULL) - RC_CLIENT_RECENT_UNLOCK_DELAY_SECONDS;
      rc_mutex_lock((rc_mutex_t*)(&client->state.mutex));
      rc_client_update_achievement_display_information(client, achievement, recent_unlock_time);
      rc_mutex_unlock((rc_mutex_t*)(&client->state.mutex));
      return &achievement->public_;
    }
  }

  return NULL;
}

const rc_client_achievement_t* rc_client_get_achievement_info(rc_client_t* client, uint32_t id)
{
  rc_client_subset_info_t* subset;

  if (!client)
    return NULL;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client) {
    if (client->state.external_client->get_achievement_info_v3)
      return client->state.external_client->get_achievement_info_v3(id);

    if (client->state.external_client->get_achievement_info)
      return rc_client_external_convert_v1_achievement(client, client->state.external_client->get_achievement_info(id));
  }
#endif

  if (!client->game)
    return NULL;

  for (subset = client->game->subsets; subset; subset = subset->next) {
    const rc_client_achievement_t* achievement = rc_client_subset_get_achievement_info(client, subset, id);
    if (achievement != NULL)
      return achievement;
  }

  return NULL;
}

const rc_client_achievement_t* rc_client_get_next_achievement_info(rc_client_t* client,
    const rc_client_achievement_t* achievement, int bucket)
{
  const rc_client_achievement_info_t* after = (const rc_client_achievement_info_t*)achievement;
  rc_client_achievement_info_t* achievement_info;
  time_t recent_unlock_time;
  rc_client_subset_info_t* subset;

  if (!client)
    return NULL;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->get_next_achievement_info)
    return client->state.external_client->get_next_achievement_info(achievement ? achievement->id : 0, bucket);
#endif

  if (!client->game)
    return NULL;

  recent_unlock_time = time(NULL) - RC_CLIENT_RECENT_UNLOCK_DELAY_SECONDS;
  for (subset = client->game->subsets; subset; subset = subset->next) {
    if (subset->active && subset->public_.num_achievements > 0) {
      const rc_client_achievement_info_t* start = subset->achievements;
      const rc_client_achievement_info_t* stop = start + subset->public_.num_achievements;
      if (after == NULL || (after >= start && after <= stop)) {
        /* found a subset containing the provided achievement. look for the next
         * achievement matching the requested bucket */
        uint32_t index = after ? (uint32_t)(after - start) + 1 : 0;
        do {
          if (index >= subset->public_.num_achievements) {
            /* done with this subset. find the next active subset with achievements */
            do {
              subset = subset->next;
              if (!subset)
                return NULL;
            } while (!subset->active || subset->public_.num_achievements == 0);

            index = 0;
          }

          /* found an achievement. check to see if it matches the requested bucket. */
          achievement_info = &subset->achievements[index];
          rc_client_update_achievement_display_information(client, achievement_info, recent_unlock_time);
          if (achievement_info->public_.bucket == bucket)
            return &achievement_info->public_;

          ++index;
        } while (1);
      }
    }
  }

  return NULL;
}


int rc_client_achievement_get_image_url(const rc_client_achievement_t* achievement, int state, char buffer[], size_t buffer_size)
{
  const int image_type = (state == RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED) ?
      RC_IMAGE_TYPE_ACHIEVEMENT : RC_IMAGE_TYPE_ACHIEVEMENT_LOCKED;

  if (!achievement || !achievement->badge_name[0])
    return rc_client_get_image_url(buffer, buffer_size, image_type, "00000");

  if (image_type == RC_IMAGE_TYPE_ACHIEVEMENT && achievement->badge_url) {
    const size_t len = strlen(achievement->badge_url);
    if (len >= buffer_size)
      return RC_INSUFFICIENT_BUFFER;

    memcpy(buffer, achievement->badge_url, len + 1);
    return RC_OK;
  }

  if (image_type == RC_IMAGE_TYPE_ACHIEVEMENT_LOCKED && achievement->badge_locked_url) {
    const size_t len = strlen(achievement->badge_locked_url);
    if (len >= buffer_size)
      return RC_INSUFFICIENT_BUFFER;

    memcpy(buffer, achievement->badge_locked_url, len + 1);
    return RC_OK;
  }

  return rc_client_get_image_url(buffer, buffer_size, image_type, achievement->badge_name);
}

typedef struct rc_client_award_achievement_callback_data_t
{
  uint32_t id;
  uint32_t retry_count;
  uint8_t hardcore;
  const char* game_hash;
  rc_clock_t unlock_time;
  rc_client_t* client;
  rc_client_scheduled_callback_data_t* scheduled_callback_data;
} rc_client_award_achievement_callback_data_t;

static int rc_client_is_award_achievement_pending(const rc_client_t* client, uint32_t achievement_id)
{
  /* assume lock already held */
  rc_client_scheduled_callback_data_t* scheduled_callback = client->state.scheduled_callbacks;
  for (; scheduled_callback; scheduled_callback = scheduled_callback->next)
  {
    if (scheduled_callback->callback == rc_client_award_achievement_retry)
    {
      rc_client_award_achievement_callback_data_t* ach_data =
        (rc_client_award_achievement_callback_data_t*)scheduled_callback->data;
      if (ach_data->id == achievement_id)
        return 1;
    }
  }

  return 0;
}

static void rc_client_award_achievement_server_call(rc_client_award_achievement_callback_data_t* ach_data);

static void rc_client_award_achievement_retry(rc_client_scheduled_callback_data_t* callback_data, rc_client_t* client, rc_clock_t now)
{
  rc_client_award_achievement_callback_data_t* ach_data =
    (rc_client_award_achievement_callback_data_t*)callback_data->data;

  (void)client;
  (void)now;

  rc_client_award_achievement_server_call(ach_data);
}

static void rc_client_award_achievement_callback(const rc_api_server_response_t* server_response, void* callback_data)
{
  rc_client_award_achievement_callback_data_t* ach_data =
      (rc_client_award_achievement_callback_data_t*)callback_data;
  rc_api_award_achievement_response_t award_achievement_response;

  int result = rc_api_process_award_achievement_server_response(&award_achievement_response, server_response);
  const char* error_message = rc_client_server_error_message(&result, server_response->http_status_code, &award_achievement_response.response);

  if (error_message) {
    if (award_achievement_response.response.error_message && !rc_client_should_retry(server_response)) {
      /* actual error from server */
      RC_CLIENT_LOG_ERR_FORMATTED(ach_data->client, "Error awarding achievement %u: %s", ach_data->id, error_message);
      rc_client_raise_server_error_event(ach_data->client, "award_achievement", ach_data->id, result, award_achievement_response.response.error_message);
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
      const uint32_t delay = (ach_data->retry_count > 8) ? 120 : (1 << (ach_data->retry_count - 2));
      RC_CLIENT_LOG_ERR_FORMATTED(ach_data->client, "Error awarding achievement %u: %s, retrying in %u seconds", ach_data->id, error_message, delay);

      if (!ach_data->scheduled_callback_data) {
        ach_data->scheduled_callback_data = (rc_client_scheduled_callback_data_t*)calloc(1, sizeof(*ach_data->scheduled_callback_data));
        if (!ach_data->scheduled_callback_data) {
          RC_CLIENT_LOG_ERR_FORMATTED(ach_data->client, "Failed to allocate scheduled callback data for reattempt to unlock achievement %u", ach_data->id);
          rc_client_raise_server_error_event(ach_data->client, "award_achievement", ach_data->id, RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY));
          return;
        }
        ach_data->scheduled_callback_data->callback = rc_client_award_achievement_retry;
        ach_data->scheduled_callback_data->data = ach_data;
        ach_data->scheduled_callback_data->related_id = ach_data->id;
      }

      ach_data->scheduled_callback_data->when =
          ach_data->client->callbacks.get_time_millisecs(ach_data->client) + delay * 1000;

      rc_client_schedule_callback(ach_data->client, ach_data->scheduled_callback_data);

      rc_client_update_disconnect_state(ach_data->client);
      return;
    }
  }
  else {
    ach_data->client->user.score = award_achievement_response.new_player_score;
    ach_data->client->user.score_casual = award_achievement_response.new_player_score_casual;

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
            ach_data->hardcore ? award_achievement_response.new_player_score : award_achievement_response.new_player_score_casual);
      }
      else {
        RC_CLIENT_LOG_INFO_FORMATTED(ach_data->client, "Achievement %u awarded, new score: %u",
            ach_data->id,
            ach_data->hardcore ? award_achievement_response.new_player_score : award_achievement_response.new_player_score_casual);
      }

      if (award_achievement_response.achievements_remaining == 0) {
        rc_client_subset_info_t* subset;
        for (subset = ach_data->client->game->subsets; subset; subset = subset->next) {
          if (subset->mastery == RC_CLIENT_MASTERY_STATE_NONE &&
              rc_client_subset_get_achievement_info(ach_data->client, subset, ach_data->id)) {
            if (subset->public_.id == ach_data->client->game->public_.id) {
              RC_CLIENT_LOG_INFO_FORMATTED(ach_data->client, "Game %u %s", ach_data->client->game->public_.id,
                ach_data->client->state.hardcore ? "mastered" : "completed");
              subset->mastery = RC_CLIENT_MASTERY_STATE_PENDING;
            }
            else {
              RC_CLIENT_LOG_INFO_FORMATTED(ach_data->client, "Subset %u %s", ach_data->client->game->public_.id,
                ach_data->client->state.hardcore ? "mastered" : "completed");

              subset->mastery = RC_CLIENT_MASTERY_STATE_PENDING;
            }
          }
        }
      }
    }
  }

  if (ach_data->retry_count)
    rc_client_update_disconnect_state(ach_data->client);

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

  if (ach_data->retry_count) {
    const rc_clock_t now = ach_data->client->callbacks.get_time_millisecs(ach_data->client);
    api_params.seconds_since_unlock = (uint32_t)((now - ach_data->unlock_time) / 1000);
  }

  result = rc_api_init_award_achievement_request_hosted(&request, &api_params, &ach_data->client->state.host);
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
    achievement->public_.unlock_time = achievement->unlock_time_hardcore = time(NULL);
    if (achievement->unlock_time_casual == 0)
      achievement->unlock_time_casual = achievement->unlock_time_hardcore;

    /* adjust score now - will get accurate score back from server */
    client->user.score += achievement->public_.points;
  }
  else {
    achievement->public_.unlock_time = achievement->unlock_time_casual = time(NULL);

    /* adjust score now - will get accurate score back from server */
    client->user.score_casual += achievement->public_.points;
  }

  achievement->public_.state = RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED;
  achievement->public_.unlocked |= (client->state.hardcore) ?
    RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH : RC_CLIENT_ACHIEVEMENT_UNLOCKED_CASUAL;

  rc_mutex_unlock(&client->state.mutex);

  if (achievement->public_.id >= RC_CLIENT_ACHIEVEMENT_WARNING_ID) {
    RC_CLIENT_LOG_INFO_FORMATTED(client, "Unlocked warning achievement %u: %s", achievement->public_.id, achievement->public_.title);
    return;
  }

  if (client->callbacks.can_submit_achievement_unlock &&
      !client->callbacks.can_submit_achievement_unlock(achievement->public_.id, client)) {
    RC_CLIENT_LOG_INFO_FORMATTED(client, "Achievement %u unlock blocked by client", achievement->public_.id);
    return;
  }

  /* can't unlock unpromoted achievements on the server */
  if (achievement->public_.category != RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED) {
    RC_CLIENT_LOG_INFO_FORMATTED(client, "Unlocked unpromoted achievement %u: %s", achievement->public_.id, achievement->public_.title);
    return;
  }

  /* don't actually unlock achievements when spectating */
  if (client->state.spectator_mode != RC_CLIENT_SPECTATOR_MODE_OFF) {
    RC_CLIENT_LOG_INFO_FORMATTED(client, "Spectated achievement %u: %s", achievement->public_.id, achievement->public_.title);
    return;
  }

  callback_data = (rc_client_award_achievement_callback_data_t*)calloc(1, sizeof(*callback_data));
  if (!callback_data) {
    RC_CLIENT_LOG_ERR_FORMATTED(client, "Failed to allocate callback data for unlocking achievement %u", achievement->public_.id);
    rc_client_raise_server_error_event(client, "award_achievement", achievement->public_.id, RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY));
    return;
  }
  callback_data->client = client;
  callback_data->id = achievement->public_.id;
  callback_data->hardcore = client->state.hardcore;
  callback_data->unlock_time = client->callbacks.get_time_millisecs(client);

  if (client->game) /* may be NULL if this gets called while unloading the game (from another thread - events are raised outside the lock) */
    callback_data->game_hash = client->game->public_.hash;

  RC_CLIENT_LOG_INFO_FORMATTED(client, "Awarding achievement %u: %s", achievement->public_.id, achievement->public_.title);
  rc_client_award_achievement_server_call(callback_data);
}

static void rc_client_subset_reset_achievements(rc_client_subset_info_t* subset)
{
  rc_client_achievement_info_t* achievement = subset->achievements;
  rc_client_achievement_info_t* stop = achievement + subset->public_.num_achievements;

  for (; achievement < stop; ++achievement) {
    rc_trigger_t* trigger = achievement->trigger;
    if (!trigger || achievement->public_.state != RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE)
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

static rc_client_leaderboard_info_t* rc_client_subset_get_leaderboard_info(const rc_client_subset_info_t* subset, uint32_t id)
{
  rc_client_leaderboard_info_t* leaderboard = subset->leaderboards;
  rc_client_leaderboard_info_t* stop = leaderboard + subset->public_.num_leaderboards;

  for (; leaderboard < stop; ++leaderboard) {
    if (leaderboard->public_.id == id)
      return leaderboard;
  }

  return NULL;
}

const rc_client_leaderboard_t* rc_client_get_leaderboard_info(const rc_client_t* client, uint32_t id)
{
  rc_client_subset_info_t* subset;

  if (!client)
    return NULL;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client) {
    if (client->state.external_client->get_leaderboard_info_v8)
      return client->state.external_client->get_leaderboard_info_v8(id);

    if (client->state.external_client->get_leaderboard_info)
      return rc_client_external_convert_v1_leaderboard(client, client->state.external_client->get_leaderboard_info(id));
  }
#endif

  if (!client->game)
    return NULL;

  for (subset = client->game->subsets; subset; subset = subset->next) {
    const rc_client_leaderboard_info_t* leaderboard = rc_client_subset_get_leaderboard_info(subset, id);
    if (leaderboard != NULL)
      return &leaderboard->public_;
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
  new_label_len = strlen(subset->public_.title) + strlen(label) + 4;
  new_label = (char*)rc_buffer_alloc(&game->buffer, new_label_len);
  snprintf(new_label, new_label_len, "%s - %s", subset->public_.title, label);

  *ptr = new_label;
  return new_label;
}

static uint8_t rc_client_get_leaderboard_bucket(const rc_client_leaderboard_info_t* leaderboard, int grouping)
{
  switch (leaderboard->public_.state) {
    case RC_CLIENT_LEADERBOARD_STATE_TRACKING:
      return (grouping == RC_CLIENT_LEADERBOARD_LIST_GROUPING_NONE) ?
        RC_CLIENT_LEADERBOARD_BUCKET_ALL : RC_CLIENT_LEADERBOARD_BUCKET_ACTIVE;

    case RC_CLIENT_LEADERBOARD_STATE_DISABLED:
      return RC_CLIENT_LEADERBOARD_BUCKET_UNSUPPORTED;

    default:
      if (grouping == RC_CLIENT_LEADERBOARD_LIST_GROUPING_NONE)
        return RC_CLIENT_LEADERBOARD_BUCKET_ALL;

      if (leaderboard->public_.category == RC_CLIENT_LEADERBOARD_CATEGORY_UNPROMOTED)
        return RC_CLIENT_LEADERBOARD_BUCKET_UNPROMOTED;

      return RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE;
  }
}

static int rc_client_is_subset_visible_in_leaderboard_list(const rc_client_subset_info_t* subset, const rc_client_leaderboard_list_params_t* params)
{
  if (!subset->active)
    return 0;

  return (params->subset_id == 0 || params->subset_id == subset->public_.id);
}

static int rc_client_is_leaderboard_visible_in_list(const rc_client_leaderboard_info_t* leaderboard, const rc_client_leaderboard_list_params_t* params)
{
  if (leaderboard->hidden)
    return 0;

  return (leaderboard->public_.category & params->category) != 0;
}

rc_client_leaderboard_list_t* rc_client_create_leaderboard_list(rc_client_t* client, int grouping)
{
  rc_client_leaderboard_list_params_t params;
  params.subset_id = 0;
  params.category = RC_CLIENT_LEADERBOARD_CATEGORY_PROMOTED;
  params.grouping = grouping;
  return rc_client_create_subset_leaderboard_list(client, &params);
}

rc_client_leaderboard_list_t* rc_client_create_subset_leaderboard_list(rc_client_t* client, const rc_client_leaderboard_list_params_t* params)
{
  rc_client_leaderboard_info_t* leaderboard;
  rc_client_leaderboard_info_t* stop;
  const rc_client_leaderboard_t** bucket_leaderboards;
  const rc_client_leaderboard_t** leaderboard_ptr;
  rc_client_leaderboard_bucket_t* bucket_ptr;
  rc_client_leaderboard_list_info_t* list;
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
    RC_CLIENT_LEADERBOARD_BUCKET_UNPROMOTED,
    RC_CLIENT_LEADERBOARD_BUCKET_UNSUPPORTED
  };

  if (!client)
    return (rc_client_leaderboard_list_t*)calloc(1, list_size);

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client) {
    if (client->state.external_client->create_subset_leaderboard_list)
      return (rc_client_leaderboard_list_t*)client->state.external_client->create_subset_leaderboard_list(params);

    if (client->state.external_client->create_leaderboard_list)
      return rc_client_external_convert_v1_leaderboard_list(client,
        (rc_client_leaderboard_list_t*)client->state.external_client->create_leaderboard_list(params->grouping));
  }
#endif

  if (!client->game)
    return (rc_client_leaderboard_list_t*)calloc(1, list_size);

  memset(&bucket_counts, 0, sizeof(bucket_counts));

  rc_mutex_lock(&client->state.mutex);

  subset = client->game->subsets;
  for (; subset; subset = subset->next) {
    if (!rc_client_is_subset_visible_in_leaderboard_list(subset, params))
      continue;

    num_subsets++;
    leaderboard = subset->leaderboards;
    stop = leaderboard + subset->public_.num_leaderboards;
    for (; leaderboard < stop; ++leaderboard) {
      if (!rc_client_is_leaderboard_visible_in_list(leaderboard, params))
        continue;

      leaderboard->bucket = rc_client_get_leaderboard_bucket(leaderboard, params->grouping);
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
        if (!rc_client_is_subset_visible_in_leaderboard_list(subset, params))
          continue;

        leaderboard = subset->leaderboards;
        stop = leaderboard + subset->public_.num_leaderboards;
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

  list = (rc_client_leaderboard_list_info_t*)malloc(list_size + buckets_size + num_leaderboards * sizeof(rc_client_leaderboard_t*));
  list->public_.buckets = bucket_ptr = (rc_client_leaderboard_bucket_t*)((uint8_t*)list + list_size);
  leaderboard_ptr = (const rc_client_leaderboard_t**)((uint8_t*)bucket_ptr + buckets_size);

  if (params->grouping == RC_CLIENT_LEADERBOARD_LIST_GROUPING_TRACKING) {
    for (i = 0; i < sizeof(shared_bucket_order) / sizeof(shared_bucket_order[0]); ++i) {
      bucket_type = shared_bucket_order[i];
      if (!bucket_counts[bucket_type])
        continue;

      bucket_leaderboards = leaderboard_ptr;
      for (subset = client->game->subsets; subset; subset = subset->next) {
        if (!rc_client_is_subset_visible_in_leaderboard_list(subset, params))
          continue;

        leaderboard = subset->leaderboards;
        stop = leaderboard + subset->public_.num_leaderboards;
        for (; leaderboard < stop; ++leaderboard) {
          if (leaderboard->bucket == bucket_type && rc_client_is_leaderboard_visible_in_list(leaderboard, params))
            *leaderboard_ptr++ = &leaderboard->public_;
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
    if (!rc_client_is_subset_visible_in_leaderboard_list(subset, params))
      continue;

    for (i = 0; i < sizeof(subset_bucket_order) / sizeof(subset_bucket_order[0]); ++i) {
      bucket_type = subset_bucket_order[i];
      if (!bucket_counts[bucket_type])
        continue;

      bucket_leaderboards = leaderboard_ptr;

      leaderboard = subset->leaderboards;
      stop = leaderboard + subset->public_.num_leaderboards;
      for (; leaderboard < stop; ++leaderboard) {
        if (leaderboard->bucket == bucket_type && rc_client_is_leaderboard_visible_in_list(leaderboard, params))
          *leaderboard_ptr++ = &leaderboard->public_;
      }

      if (leaderboard_ptr > bucket_leaderboards) {
        bucket_ptr->leaderboards = bucket_leaderboards;
        bucket_ptr->num_leaderboards = (uint32_t)(leaderboard_ptr - bucket_leaderboards);
        bucket_ptr->subset_id = (num_subsets > 1) ? subset->public_.id : 0;
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

  list->destroy_func = NULL;
  list->public_.num_buckets = (uint32_t)(bucket_ptr - list->public_.buckets);
  return &list->public_;
}

void rc_client_destroy_leaderboard_list(rc_client_leaderboard_list_t* list)
{
  rc_client_leaderboard_list_info_t* info = (rc_client_leaderboard_list_info_t*)list;
  if (info->destroy_func)
    info->destroy_func(info);
  else
    free(list);
}

int rc_client_has_leaderboards(rc_client_t* client)
{
  rc_client_subset_info_t* subset;
  int result;
  uint32_t i;

  if (!client)
    return 0;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->has_leaderboards)
    return client->state.external_client->has_leaderboards();
#endif

  if (!client->game)
    return 0;

  rc_mutex_lock(&client->state.mutex);

  result = 0;
  for (subset = client->game->subsets; subset; subset = subset->next)
  {
    if (!subset->active)
      continue;

    for (i = 0; i < subset->public_.num_leaderboards; ++i) {
      if (!subset->leaderboards[i].hidden) {
        result = 1;
        break;
      }
    }

    if (result)
      break;
  }

  rc_mutex_unlock(&client->state.mutex);

  return result;
}

void rc_client_allocate_leaderboard_tracker(rc_client_game_info_t* game, rc_client_leaderboard_info_t* leaderboard)
{
  rc_client_leaderboard_tracker_info_t* tracker;
  rc_client_leaderboard_tracker_info_t* available_tracker = NULL;

  for (tracker = game->leaderboard_trackers; tracker; tracker = tracker->next) {
    if (tracker->reference_count == 0) {
      if (available_tracker == NULL && tracker->pending_events == RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_NONE)
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
    leaderboard->public_.tracker_value = tracker->public_.display;
    return;
  }

  if (!available_tracker) {
    rc_client_leaderboard_tracker_info_t** next = &game->leaderboard_trackers;

    available_tracker = (rc_client_leaderboard_tracker_info_t*)rc_buffer_alloc(&game->buffer, sizeof(*available_tracker));
    memset(available_tracker, 0, sizeof(*available_tracker));
    available_tracker->public_.id = 1;

    for (tracker = *next; tracker; next = &tracker->next, tracker = *next)
      available_tracker->public_.id++;

    *next = available_tracker;
  }

  /* update the claimed tracker */
  available_tracker->reference_count = 1;
  available_tracker->value_djb2 = leaderboard->value_djb2;
  available_tracker->format = leaderboard->format;
  available_tracker->raw_value = leaderboard->value;
  available_tracker->pending_events = RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_SHOW;
  available_tracker->value_from_hits = (uint8_t)rc_value_from_hits(&leaderboard->lboard->value);
  leaderboard->tracker = available_tracker;
  leaderboard->public_.tracker_value = available_tracker->public_.display;
  game->pending_events |= RC_CLIENT_GAME_PENDING_EVENT_LEADERBOARD_TRACKER;
}

void rc_client_release_leaderboard_tracker(rc_client_game_info_t* game, rc_client_leaderboard_info_t* leaderboard)
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
  rc_clock_t submit_time;
  rc_client_t* client;
  rc_client_scheduled_callback_data_t* scheduled_callback_data;
} rc_client_submit_leaderboard_entry_callback_data_t;

static void rc_client_submit_leaderboard_entry_server_call(rc_client_submit_leaderboard_entry_callback_data_t* lboard_data);

static void rc_client_submit_leaderboard_entry_retry(rc_client_scheduled_callback_data_t* callback_data, rc_client_t* client, rc_clock_t now)
{
  rc_client_submit_leaderboard_entry_callback_data_t* lboard_data =
      (rc_client_submit_leaderboard_entry_callback_data_t*)callback_data->data;

  (void)client;
  (void)now;

  rc_client_submit_leaderboard_entry_server_call(lboard_data);
}

static void rc_client_raise_scoreboard_event(rc_client_submit_leaderboard_entry_callback_data_t* lboard_data,
    const rc_api_submit_lboard_entry_response_t* response)
{
  rc_client_leaderboard_scoreboard_t sboard;
  rc_client_event_t client_event;
  rc_client_subset_info_t* subset;
  rc_client_t* client = lboard_data->client;
  rc_client_leaderboard_info_t* leaderboard = NULL;

  if (!client || !client->game)
    return;

  for (subset = client->game->subsets; subset; subset = subset->next) {
    leaderboard = rc_client_subset_get_leaderboard_info(subset, lboard_data->id);
    if (leaderboard != NULL)
      break;
  }
  if (leaderboard == NULL) {
    RC_CLIENT_LOG_ERR_FORMATTED(client, "Trying to raise scoreboard for unknown leaderboard %u", lboard_data->id);
    return;
  }

  memset(&sboard, 0, sizeof(sboard));
  sboard.leaderboard_id = lboard_data->id;
  rc_format_value(sboard.submitted_score, sizeof(sboard.submitted_score), response->submitted_score, leaderboard->format);
  rc_format_value(sboard.best_score, sizeof(sboard.best_score), response->best_score, leaderboard->format);
  sboard.new_rank = response->new_rank;
  sboard.num_entries = response->num_entries;
  sboard.num_top_entries = response->num_top_entries;
  if (sboard.num_top_entries > 0) {
    sboard.top_entries = (rc_client_leaderboard_scoreboard_entry_t*)calloc(
      response->num_top_entries, sizeof(rc_client_leaderboard_scoreboard_entry_t));
    if (sboard.top_entries != NULL) {
      uint32_t i;
      for (i = 0; i < response->num_top_entries; i++) {
        sboard.top_entries[i].username = response->top_entries[i].username;
        sboard.top_entries[i].rank = response->top_entries[i].rank;
        rc_format_value(sboard.top_entries[i].score, sizeof(sboard.top_entries[i].score), response->top_entries[i].score,
            leaderboard->format);
      }
    }
  }

  memset(&client_event, 0, sizeof(client_event));
  client_event.type = RC_CLIENT_EVENT_LEADERBOARD_SCOREBOARD;
  client_event.leaderboard = &leaderboard->public_;
  client_event.leaderboard_scoreboard = &sboard;

  lboard_data->client->callbacks.event_handler(&client_event, lboard_data->client);

  if (sboard.top_entries != NULL) {
    free(sboard.top_entries);
  }
}

static void rc_client_submit_leaderboard_entry_callback(const rc_api_server_response_t* server_response, void* callback_data)
{
  rc_client_submit_leaderboard_entry_callback_data_t* lboard_data =
      (rc_client_submit_leaderboard_entry_callback_data_t*)callback_data;
  rc_api_submit_lboard_entry_response_t submit_lboard_entry_response;

  int result = rc_api_process_submit_lboard_entry_server_response(&submit_lboard_entry_response, server_response);
  const char* error_message = rc_client_server_error_message(&result, server_response->http_status_code, &submit_lboard_entry_response.response);

  if (error_message) {
    if (submit_lboard_entry_response.response.error_message && !rc_client_should_retry(server_response)) {
      /* actual error from server */
      RC_CLIENT_LOG_ERR_FORMATTED(lboard_data->client, "Error submitting leaderboard entry %u: %s", lboard_data->id, error_message);
      rc_client_raise_server_error_event(lboard_data->client, "submit_lboard_entry", lboard_data->id, result, submit_lboard_entry_response.response.error_message);
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
      const uint32_t delay = (lboard_data->retry_count > 8) ? 120 : (1 << (lboard_data->retry_count - 2));
      RC_CLIENT_LOG_ERR_FORMATTED(lboard_data->client, "Error submitting leaderboard entry %u: %s, retrying in %u seconds", lboard_data->id, error_message, delay);

      if (!lboard_data->scheduled_callback_data) {
        lboard_data->scheduled_callback_data = (rc_client_scheduled_callback_data_t*)calloc(1, sizeof(*lboard_data->scheduled_callback_data));
        if (!lboard_data->scheduled_callback_data) {
          RC_CLIENT_LOG_ERR_FORMATTED(lboard_data->client, "Failed to allocate scheduled callback data for reattempt to submit entry for leaderboard %u", lboard_data->id);
          rc_client_raise_server_error_event(lboard_data->client, "submit_lboard_entry", lboard_data->id, RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY));
          return;
        }
        lboard_data->scheduled_callback_data->callback = rc_client_submit_leaderboard_entry_retry;
        lboard_data->scheduled_callback_data->data = lboard_data;
        lboard_data->scheduled_callback_data->related_id = lboard_data->id;
      }

      lboard_data->scheduled_callback_data->when =
          lboard_data->client->callbacks.get_time_millisecs(lboard_data->client) + delay * 1000;

      rc_client_schedule_callback(lboard_data->client, lboard_data->scheduled_callback_data);

      rc_client_update_disconnect_state(lboard_data->client);
      return;
    }
  }
  else {
    /* raise event for scoreboard */
    if (lboard_data->retry_count < 2) {
      rc_client_raise_scoreboard_event(lboard_data, &submit_lboard_entry_response);
    }

    /* not currently doing anything with the response */
    if (lboard_data->retry_count) {
      RC_CLIENT_LOG_INFO_FORMATTED(lboard_data->client, "Leaderboard %u submission %d completed after %u attempts",
          lboard_data->id, lboard_data->score, lboard_data->retry_count);
    }
  }

  if (lboard_data->retry_count)
    rc_client_update_disconnect_state(lboard_data->client);

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

  if (lboard_data->retry_count) {
    const rc_clock_t now = lboard_data->client->callbacks.get_time_millisecs(lboard_data->client);
    api_params.seconds_since_completion = (uint32_t)((now - lboard_data->submit_time) / 1000);
  }

  result = rc_api_init_submit_lboard_entry_request_hosted(&request, &api_params, &lboard_data->client->state.host);
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

  if (!client->state.hardcore) {
    RC_CLIENT_LOG_INFO_FORMATTED(client, "Leaderboard %u entry submission not allowed in casual mode", leaderboard->public_.id);
    return;
  }

  if (client->callbacks.can_submit_leaderboard_entry &&
      !client->callbacks.can_submit_leaderboard_entry(leaderboard->public_.id, client)) {
    RC_CLIENT_LOG_INFO_FORMATTED(client, "Leaderboard %u entry submission blocked by client", leaderboard->public_.id);
    return;
  }

  /* don't actually submit leaderboard entries when spectating */
  if (client->state.spectator_mode != RC_CLIENT_SPECTATOR_MODE_OFF) {
    RC_CLIENT_LOG_INFO_FORMATTED(client, "Spectated %s (%d) for leaderboard %u: %s",
        leaderboard->public_.tracker_value, leaderboard->value, leaderboard->public_.id, leaderboard->public_.title);
    return;
  }

  callback_data = (rc_client_submit_leaderboard_entry_callback_data_t*)calloc(1, sizeof(*callback_data));
  if (!callback_data) {
    RC_CLIENT_LOG_ERR_FORMATTED(client, "Failed to allocate callback data for submitting entry for leaderboard %u", leaderboard->public_.id);
    rc_client_raise_server_error_event(client, "submit_lboard_entry", leaderboard->public_.id, RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY));
    return;
  }
  callback_data->client = client;
  callback_data->id = leaderboard->public_.id;
  callback_data->score = leaderboard->value;
  callback_data->game_hash = client->game->public_.hash;
  callback_data->submit_time = client->callbacks.get_time_millisecs(client);

  RC_CLIENT_LOG_INFO_FORMATTED(client, "Submitting %s (%d) for leaderboard %u: %s",
      leaderboard->public_.tracker_value, leaderboard->value, leaderboard->public_.id, leaderboard->public_.title);
  rc_client_submit_leaderboard_entry_server_call(callback_data);
}

static void rc_client_subset_reset_leaderboards(rc_client_game_info_t* game, rc_client_subset_info_t* subset)
{
  rc_client_leaderboard_info_t* leaderboard = subset->leaderboards;
  rc_client_leaderboard_info_t* stop = leaderboard + subset->public_.num_leaderboards;

  for (; leaderboard < stop; ++leaderboard) {
    rc_lboard_t* lboard = leaderboard->lboard;
    if (!lboard)
      continue;

    switch (leaderboard->public_.state) {
      case RC_CLIENT_LEADERBOARD_STATE_INACTIVE:
      case RC_CLIENT_LEADERBOARD_STATE_DISABLED:
        continue;

      case RC_CLIENT_LEADERBOARD_STATE_TRACKING:
        rc_client_release_leaderboard_tracker(game, leaderboard);
        /* fallthrough */ /* to default */
      default:
        leaderboard->public_.state = RC_CLIENT_LEADERBOARD_STATE_ACTIVE;
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

  result = rc_client_end_async(client, &lbinfo_callback_data->async_handle);
  if (result) {
    if (result != RC_CLIENT_ASYNC_DESTROYED) {
      RC_CLIENT_LOG_VERBOSE(client, "Fetch leaderbord entries aborted");
    }
    free(lbinfo_callback_data);
    return;
  }

  result = rc_api_process_fetch_leaderboard_info_server_response(&lbinfo_response, server_response);
  error_message = rc_client_server_error_message(&result, server_response->http_status_code, &lbinfo_response.response);
  if (error_message) {
    RC_CLIENT_LOG_ERR_FORMATTED(client, "Fetch leaderboard %u info failed: %s", lbinfo_callback_data->leaderboard_id, error_message);
    lbinfo_callback_data->callback(result, error_message, NULL, client, lbinfo_callback_data->callback_userdata);
  }
  else {
    rc_client_leaderboard_entry_list_info_t* info;
    const size_t list_size = sizeof(*info) + sizeof(rc_client_leaderboard_entry_t) * lbinfo_response.num_entries;
    size_t needed_size = list_size;
    uint32_t i;

    for (i = 0; i < lbinfo_response.num_entries; i++)
      needed_size += strlen(lbinfo_response.entries[i].username) + 1;

    info = (rc_client_leaderboard_entry_list_info_t*)malloc(needed_size);
    if (!info) {
      lbinfo_callback_data->callback(RC_OUT_OF_MEMORY, rc_error_str(RC_OUT_OF_MEMORY), NULL, client, lbinfo_callback_data->callback_userdata);
    }
    else {
      rc_client_leaderboard_entry_list_t* list = &info->public_;
      rc_client_leaderboard_entry_t* entry = list->entries = (rc_client_leaderboard_entry_t*)((uint8_t*)info + sizeof(*info));
      char* user = (char*)((uint8_t*)list + list_size);
      const rc_api_lboard_info_entry_t* lbentry = lbinfo_response.entries;
      const rc_api_lboard_info_entry_t* stop = lbentry + lbinfo_response.num_entries;
      const size_t logged_in_user_len = strlen(client->user.display_name) + 1;
      info->destroy_func = NULL;
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
      list->total_entries = lbinfo_response.total_entries;

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
  rc_client_async_handle_t* async_handle;
  rc_api_request_t request;
  int result;
  const char* error_message;

  result = rc_api_init_fetch_leaderboard_info_request_hosted(&request, lbinfo_request, &client->state.host);

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

  async_handle = &callback_data->async_handle;
  rc_client_begin_async(client, async_handle);
  client->callbacks.server_call(&request, rc_client_fetch_leaderboard_entries_callback, callback_data, client);
  rc_api_destroy_request(&request);

  return rc_client_async_handle_valid(client, async_handle) ? async_handle : NULL;
}

rc_client_async_handle_t* rc_client_begin_fetch_leaderboard_entries(rc_client_t* client, uint32_t leaderboard_id,
    uint32_t first_entry, uint32_t count, rc_client_fetch_leaderboard_entries_callback_t callback, void* callback_userdata)
{
  rc_api_fetch_leaderboard_info_request_t lbinfo_request;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->begin_fetch_leaderboard_entries)
    return client->state.external_client->begin_fetch_leaderboard_entries(client, leaderboard_id, first_entry, count, callback, callback_userdata);
#endif

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

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->begin_fetch_leaderboard_entries_around_user)
    return client->state.external_client->begin_fetch_leaderboard_entries_around_user(client, leaderboard_id, count, callback, callback_userdata);
#endif

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
  rc_client_leaderboard_entry_list_info_t* info = (rc_client_leaderboard_entry_list_info_t*)list;
  if (info->destroy_func)
    info->destroy_func(info);
  else
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

  int result = rc_api_process_ping_server_response(&response, server_response);
  const char* error_message = rc_client_server_error_message(&result, server_response->http_status_code, &response.response);
  if (error_message) {
    RC_CLIENT_LOG_WARN_FORMATTED(client, "Ping response error: %s", error_message);
  }

  rc_api_destroy_ping_response(&response);
}

static void rc_client_ping(rc_client_scheduled_callback_data_t* callback_data, rc_client_t* client, rc_clock_t now)
{
  rc_api_ping_request_t api_params;
  rc_api_request_t request;
  char buffer[256];
  int result;

  /* if no frames have been processed since the last ping, the emulator is idle. let the
   * server session expire. it will be resumed/restarted once frames start getting
   * processed again. */
  if (client->state.frames_processed != client->state.frames_at_last_ping) {
    client->state.frames_at_last_ping = client->state.frames_processed;

    memset(&api_params, 0, sizeof(api_params));
    api_params.username = client->user.username;
    api_params.api_token = client->user.token;
    api_params.game_id = client->game->public_.id;
    api_params.rich_presence = buffer;
    api_params.game_hash = client->game->public_.hash;
    api_params.hardcore = client->state.hardcore;

    if (!client->callbacks.rich_presence_override ||
        !client->callbacks.rich_presence_override(client, buffer, sizeof(buffer))) {
      rc_mutex_lock(&client->state.mutex);

      rc_runtime_get_richpresence(&client->game->runtime, buffer, sizeof(buffer),
          rc_client_read_modified_memory_helper, client, NULL);

      rc_mutex_unlock(&client->state.mutex);
    }

    /* there's a miniscule chance the game will be changed out while we're waiting for the lock.
     * if that happens, discard this ping. the new game will have scheduled its own ping.
     * don't reschedule this one. */
    if (!client->game || client->game->public_.id != api_params.game_id) {
      return;
    }

    result = rc_api_init_ping_request_hosted(&request, &api_params, &client->state.host);
    if (result != RC_OK) {
      RC_CLIENT_LOG_WARN_FORMATTED(client, "Error generating ping request: %s", rc_error_str(result));
    }
    else {
      client->callbacks.server_call(&request, rc_client_ping_callback, client, client);
    }
  }

  callback_data->when = now + 120 * 1000;
  rc_client_schedule_callback(client, callback_data);
}

void rc_client_schedule_ping(rc_client_t* client, rc_client_game_info_t* game)
{
  rc_client_scheduled_callback_data_t* callback_data = (rc_client_scheduled_callback_data_t*)
      rc_buffer_alloc(&game->buffer, sizeof(rc_client_scheduled_callback_data_t));

  memset(callback_data, 0, sizeof(*callback_data));
  callback_data->callback = rc_client_ping;
  callback_data->related_id = game->public_.id;
  callback_data->when = client->callbacks.get_time_millisecs(client) + 30 * 1000;
  rc_client_schedule_callback(client, callback_data);
}

void rc_client_unschedule_ping(rc_client_t* client, rc_client_game_info_t* game)
{
  rc_client_scheduled_callback_data_t** last = &client->state.scheduled_callbacks;
  rc_client_scheduled_callback_data_t* next;

  do {
    next = *last;
    if (!next)
      break;

    /* remove rich presence ping scheduled event for game */
    if (next->callback == rc_client_ping && next->related_id == game->public_.id) {
      *last = next->next;
      continue;
    }

    last = &next->next;
  } while (1);
}

int rc_client_has_rich_presence(rc_client_t* client)
{
  if (!client)
    return 0;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->has_rich_presence)
    return client->state.external_client->has_rich_presence();
#endif

  if (!client->game || !client->game->runtime.richpresence || !client->game->runtime.richpresence->richpresence)
    return 0;

  return 1;
}

size_t rc_client_get_rich_presence_message(rc_client_t* client, char buffer[], size_t buffer_size)
{
  int result;

  if (!client || !buffer)
    return 0;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->get_rich_presence_message)
    return client->state.external_client->get_rich_presence_message(buffer, buffer_size);
#endif

  if (!client->game)
    return 0;

  rc_mutex_lock(&client->state.mutex);

  result = rc_runtime_get_richpresence(&client->game->runtime, buffer, (unsigned)buffer_size,
      rc_client_read_modified_memory_helper, client, NULL);

  rc_mutex_unlock(&client->state.mutex);

  if (result == 0) {
    result = snprintf(buffer, buffer_size, "Playing %s", client->game->public_.title);
    /* snprintf will return the amount of space needed, we want to return the number of chars written */
    if ((size_t)result >= buffer_size)
      return (buffer_size - 1);
  }

  return result;
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

static void rc_client_invalidate_processing_memref(rc_client_t* client)
{
  /* if processing_memref is not set, this occurred following a pointer chain. ignore it. */
  if (!client->state.processing_memref)
    return;

  client->state.processing_memref->value.type = RC_VALUE_TYPE_NONE;

  rc_client_invalidate_memref_achievements(client->game, client, client->state.processing_memref);
  rc_client_invalidate_memref_leaderboards(client->game, client, client->state.processing_memref);

  client->state.processing_memref = NULL;
}

static uint32_t RC_CCONV rc_client_read_memory_validator(uint32_t address, uint8_t* buffer, uint32_t num_bytes, void* ud)
{
  rc_client_t* client = (rc_client_t*)ud;

  uint32_t num_read = client->callbacks.read_memory(address, buffer, num_bytes, client);
  if (num_read < num_bytes)
    rc_client_invalidate_processing_memref(client);

  return num_read;
}

static uint32_t RC_CCONV rc_client_read_modified_memory_helper(uint32_t address, uint8_t* buffer, uint32_t num_bytes, void* ud)
{
  rc_client_t* client = (rc_client_t*)ud;

  /* if the address is out of range, don't bother processing it. */
  if (address > client->game->max_valid_address)
    return 0;

  return client->callbacks.read_memory(address, buffer, num_bytes, client);
}

int rc_client_is_processing_required(rc_client_t* client)
{
  if (!client)
    return 0;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->is_processing_required)
    return client->state.external_client->is_processing_required();
#endif

  if (!client->game)
    return 0;

  if (client->game->runtime.trigger_count || client->game->runtime.lboard_count)
    return 1;

  return (client->game->runtime.richpresence && client->game->runtime.richpresence->richpresence);
}

static void rc_client_update_memref_values(rc_client_t* client, rc_eval_state_t* eval_state) {
  rc_memrefs_t* memrefs = client->game->runtime.memrefs;
  rc_memref_list_t* memref_list;
  rc_modified_memref_list_t* modified_memref_list;
  int invalidated_memref = 0;

  memref_list = &memrefs->memrefs;
  do {
    rc_memref_t* memref = memref_list->items;
    const rc_memref_t* memref_stop = memref + memref_list->count;
    uint32_t value;

    for (; memref < memref_stop; ++memref) {
      if (memref->value.type == RC_VALUE_TYPE_NONE)
        continue;

      /* if processing_memref is set, and the memory read fails, all dependent achievements will be disabled */
      client->state.processing_memref = memref;

      value = rc_read_memory(memref->address, memref->value.size, rc_client_read_memory_validator, client);

      if (client->state.processing_memref) {
        rc_update_memref_value(&memref->value, value);
      }
      else {
        /* if the peek function cleared the processing_memref, the memref was invalidated */
        invalidated_memref = 1;
      }
    }

    memref_list = memref_list->next;
  } while (memref_list);

  client->state.processing_memref = NULL;

  modified_memref_list = &memrefs->modified_memrefs;
  if (modified_memref_list->count) {
    do {
      rc_modified_memref_t* modified_memref = modified_memref_list->items;
      const rc_modified_memref_t* modified_memref_stop = modified_memref + modified_memref_list->count;

      for (; modified_memref < modified_memref_stop; ++modified_memref) {
        rc_update_memref_value(&modified_memref->memref.value,
            rc_get_modified_memref_value(modified_memref, eval_state));
      }

      modified_memref_list = modified_memref_list->next;
    } while (modified_memref_list);
  }

  if (invalidated_memref)
    rc_client_update_active_achievements(client->game);
}

static void rc_client_do_frame_process_achievements(rc_client_t* client, rc_client_subset_info_t* subset, rc_eval_state_t* eval_state)
{
  rc_client_achievement_info_t* achievement = subset->achievements;
  rc_client_achievement_info_t* stop = achievement + subset->public_.num_achievements;

  for (; achievement < stop; ++achievement) {
    rc_trigger_t* trigger = achievement->trigger;
    int old_state, new_state;
    uint32_t old_measured_value;

    if (!trigger || achievement->public_.state != RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE)
      continue;

    old_measured_value = trigger->measured_value;
    old_state = trigger->state;
    new_state = rc_test_trigger(trigger, eval_state);

    /* trigger->state doesn't actually change to RESET - RESET just serves as a notification.
     * we don't care about that particular notification, so look at the actual state. */
    if (new_state == RC_TRIGGER_STATE_RESET)
      new_state = trigger->state;

    /* if the measured value changed and the achievement hasn't triggered, show a progress indicator */
    if (trigger->measured_value != old_measured_value && old_measured_value != RC_MEASURED_UNKNOWN &&
        trigger->measured_value <= trigger->measured_target &&
        trigger->measured_target != 0 &&
        rc_trigger_state_active(new_state) && new_state != RC_TRIGGER_STATE_WAITING) {

      /* only show a popup for the achievement closest to triggering */
      float progress = (float)trigger->measured_value / (float)trigger->measured_target;

      if (trigger->measured_as_percent) {
        /* if reporting the measured value as a percentage, only show the popup if the percentage changes */
        const uint32_t old_percent = (uint32_t)(((unsigned long long)old_measured_value * 100) / trigger->measured_target);
        const uint32_t new_percent = (uint32_t)(((unsigned long long)trigger->measured_value * 100) / trigger->measured_target);
        if (old_percent == new_percent)
          progress = -1.0;
      }

      if (progress > client->game->progress_tracker.progress) {
        client->game->progress_tracker.progress = progress;
        client->game->progress_tracker.achievement = achievement;
        client->game->pending_events |= RC_CLIENT_GAME_PENDING_EVENT_PROGRESS_TRACKER;
        subset->pending_events |= RC_CLIENT_SUBSET_PENDING_EVENT_ACHIEVEMENT;
        achievement->pending_events |= RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_UPDATE;
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

static void rc_client_hide_progress_tracker(rc_client_t* client, rc_client_game_info_t* game)
{
  /* ASSERT: this should only be called if the mutex is held */

  if (game->progress_tracker.hide_callback &&
      game->progress_tracker.hide_callback->when &&
      game->progress_tracker.action == RC_CLIENT_PROGRESS_TRACKER_ACTION_NONE) {
    rc_client_reschedule_callback(client, game->progress_tracker.hide_callback, 0);
    game->progress_tracker.action = RC_CLIENT_PROGRESS_TRACKER_ACTION_HIDE;
    game->pending_events |= RC_CLIENT_GAME_PENDING_EVENT_PROGRESS_TRACKER;
  }
}

static void rc_client_progress_tracker_timer_elapsed(rc_client_scheduled_callback_data_t* callback_data, rc_client_t* client, rc_clock_t now)
{
  rc_client_event_t client_event;
  memset(&client_event, 0, sizeof(client_event));

  (void)callback_data;
  (void)now;

  rc_mutex_lock(&client->state.mutex);
  if (client->game->progress_tracker.action == RC_CLIENT_PROGRESS_TRACKER_ACTION_NONE) {
    client->game->progress_tracker.hide_callback->when = 0;
    client_event.type = RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE;
  }
  rc_mutex_unlock(&client->state.mutex);

  if (client_event.type)
    client->callbacks.event_handler(&client_event, client);
}

static void rc_client_do_frame_update_progress_tracker(rc_client_t* client, rc_client_game_info_t* game)
{
  /* ASSERT: this should only be called if the mutex is held */

  if (!game->progress_tracker.hide_callback) {
    game->progress_tracker.hide_callback = (rc_client_scheduled_callback_data_t*)
      rc_buffer_alloc(&game->buffer, sizeof(rc_client_scheduled_callback_data_t));
    memset(game->progress_tracker.hide_callback, 0, sizeof(rc_client_scheduled_callback_data_t));
    game->progress_tracker.hide_callback->callback = rc_client_progress_tracker_timer_elapsed;
  }

  if (game->progress_tracker.hide_callback->when == 0)
    game->progress_tracker.action = RC_CLIENT_PROGRESS_TRACKER_ACTION_SHOW;
  else
    game->progress_tracker.action = RC_CLIENT_PROGRESS_TRACKER_ACTION_UPDATE;

  rc_client_reschedule_callback(client, game->progress_tracker.hide_callback,
      client->callbacks.get_time_millisecs(client) + 2 * 1000);
}

static void rc_client_raise_progress_tracker_events(rc_client_t* client, rc_client_game_info_t* game)
{
  rc_client_event_t client_event;

  memset(&client_event, 0, sizeof(client_event));

  switch (game->progress_tracker.action) {
  case RC_CLIENT_PROGRESS_TRACKER_ACTION_SHOW:
    client_event.type = RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_SHOW;
    break;
  case RC_CLIENT_PROGRESS_TRACKER_ACTION_HIDE:
    client_event.type = RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_HIDE;
    break;
  default:
    client_event.type = RC_CLIENT_EVENT_ACHIEVEMENT_PROGRESS_INDICATOR_UPDATE;
    break;
  }
  game->progress_tracker.action = RC_CLIENT_PROGRESS_TRACKER_ACTION_NONE;

  client_event.achievement = &game->progress_tracker.achievement->public_;
  client->callbacks.event_handler(&client_event, client);
}

static void rc_client_raise_achievement_events(rc_client_t* client, rc_client_subset_info_t* subset)
{
  rc_client_achievement_info_t* achievement = subset->achievements;
  rc_client_achievement_info_t* stop = achievement + subset->public_.num_achievements;
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

    /* raise events */
    client_event.achievement = &achievement->public_;

    if (achievement->pending_events & RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_HIDE) {
      client_event.type = RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_HIDE;
      client->callbacks.event_handler(&client_event, client);
    }
    else if (achievement->pending_events & RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_SHOW) {
      client_event.type = RC_CLIENT_EVENT_ACHIEVEMENT_CHALLENGE_INDICATOR_SHOW;
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
  client_event.subset = &subset->public_;

  if (subset == client->game->subsets)
    client_event.type = RC_CLIENT_EVENT_GAME_COMPLETED;
  else
    client_event.type = RC_CLIENT_EVENT_SUBSET_COMPLETED;

  subset->mastery = RC_CLIENT_MASTERY_STATE_SHOWN;

  client->callbacks.event_handler(&client_event, client);
}

static void rc_client_do_frame_process_leaderboards(rc_client_t* client, rc_client_subset_info_t* subset, rc_eval_state_t* eval_state)
{
  rc_client_leaderboard_info_t* leaderboard = subset->leaderboards;
  rc_client_leaderboard_info_t* stop = leaderboard + subset->public_.num_leaderboards;

  for (; leaderboard < stop; ++leaderboard) {
    rc_lboard_t* lboard = leaderboard->lboard;
    int old_state, new_state;

    switch (leaderboard->public_.state) {
      case RC_CLIENT_LEADERBOARD_STATE_INACTIVE:
      case RC_CLIENT_LEADERBOARD_STATE_DISABLED:
        continue;

      default:
        if (!lboard)
          continue;

        break;
    }

    old_state = lboard->state;
    new_state = rc_evaluate_lboard(lboard, &leaderboard->value, eval_state);

    switch (new_state) {
      case RC_LBOARD_STATE_STARTED: /* leaderboard is running */
        if (old_state != RC_LBOARD_STATE_STARTED) {
          leaderboard->public_.state = RC_CLIENT_LEADERBOARD_STATE_TRACKING;
          leaderboard->pending_events |= RC_CLIENT_LEADERBOARD_PENDING_EVENT_STARTED;
          rc_client_allocate_leaderboard_tracker(client->game, leaderboard);
        }
        else {
          rc_client_update_leaderboard_tracker(client->game, leaderboard);
        }
        break;

      case RC_LBOARD_STATE_CANCELED:
        if (old_state != RC_LBOARD_STATE_CANCELED) {
          leaderboard->public_.state = RC_CLIENT_LEADERBOARD_STATE_ACTIVE;
          leaderboard->pending_events |= RC_CLIENT_LEADERBOARD_PENDING_EVENT_FAILED;
          rc_client_release_leaderboard_tracker(client->game, leaderboard);
        }
        break;

      case RC_LBOARD_STATE_TRIGGERED:
        if (old_state != RC_RUNTIME_EVENT_LBOARD_TRIGGERED) {
          leaderboard->public_.state = RC_CLIENT_LEADERBOARD_STATE_ACTIVE;
          leaderboard->pending_events |= RC_CLIENT_LEADERBOARD_PENDING_EVENT_SUBMITTED;

          if (old_state != RC_LBOARD_STATE_STARTED)
            rc_client_allocate_leaderboard_tracker(client->game, leaderboard);
          else
            rc_client_update_leaderboard_tracker(client->game, leaderboard);

          rc_client_release_leaderboard_tracker(client->game, leaderboard);
        }
        break;
    }

    if (leaderboard->pending_events)
      subset->pending_events |= RC_CLIENT_SUBSET_PENDING_EVENT_LEADERBOARD;
  }
}

static void rc_client_raise_leaderboard_tracker_events(rc_client_t* client, rc_client_game_info_t* game)
{
  rc_client_leaderboard_tracker_info_t* tracker = game->leaderboard_trackers;
  rc_client_event_t client_event;

  memset(&client_event, 0, sizeof(client_event));

  tracker = game->leaderboard_trackers;
  for (; tracker; tracker = tracker->next) {
    if (tracker->pending_events == RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_NONE)
      continue;

    client_event.leaderboard_tracker = &tracker->public_;

    /* update display text for new trackers or updated trackers */
    if (tracker->pending_events & (RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_SHOW | RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_UPDATE))
      rc_format_value(tracker->public_.display, sizeof(tracker->public_.display), tracker->raw_value, tracker->format);

    if (tracker->pending_events & RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_HIDE) {
      if (tracker->pending_events & RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_SHOW) {
        /* request to show and hide in the same frame - ignore the event */
      }
      else {
        client_event.type = RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE;
        client->callbacks.event_handler(&client_event, client);
      }
    }
    else if (tracker->pending_events & RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_SHOW) {
      client_event.type = RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW;
      client->callbacks.event_handler(&client_event, client);
    }
    else if (tracker->pending_events & RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_UPDATE) {
      client_event.type = RC_CLIENT_EVENT_LEADERBOARD_TRACKER_UPDATE;
      client->callbacks.event_handler(&client_event, client);
    }

    tracker->pending_events = RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_NONE;
  }
}

static void rc_client_raise_leaderboard_events(rc_client_t* client, rc_client_subset_info_t* subset)
{
  rc_client_leaderboard_info_t* leaderboard = subset->leaderboards;
  rc_client_leaderboard_info_t* leaderboard_stop = leaderboard + subset->public_.num_leaderboards;
  rc_client_event_t client_event;

  memset(&client_event, 0, sizeof(client_event));

  for (; leaderboard < leaderboard_stop; ++leaderboard) {
    if (leaderboard->pending_events == RC_CLIENT_LEADERBOARD_PENDING_EVENT_NONE)
      continue;

    client_event.leaderboard = &leaderboard->public_;

    if (leaderboard->pending_events & RC_CLIENT_LEADERBOARD_PENDING_EVENT_FAILED) {
      RC_CLIENT_LOG_VERBOSE_FORMATTED(client, "Leaderboard %u canceled: %s", leaderboard->public_.id, leaderboard->public_.title);
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
      RC_CLIENT_LOG_VERBOSE_FORMATTED(client, "Leaderboard %u started: %s", leaderboard->public_.id, leaderboard->public_.title);
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

void rc_client_raise_pending_events(rc_client_t* client, rc_client_game_info_t* game)
{
  rc_client_subset_info_t* subset;

  /* raise tracker events before leaderboard events so formatted values are updated for leaderboard events */
  if (game->pending_events & RC_CLIENT_GAME_PENDING_EVENT_LEADERBOARD_TRACKER)
    rc_client_raise_leaderboard_tracker_events(client, game);

  for (subset = game->subsets; subset; subset = subset->next)
    rc_client_subset_raise_pending_events(client, subset);

  /* raise progress tracker events after achievement events so formatted values are updated for tracker event */
  if (game->pending_events & RC_CLIENT_GAME_PENDING_EVENT_PROGRESS_TRACKER)
    rc_client_raise_progress_tracker_events(client, game);

  /* if any achievements were unlocked, resync the active achievements list */
  if (game->pending_events & RC_CLIENT_GAME_PENDING_EVENT_UPDATE_ACTIVE_ACHIEVEMENTS) {
    rc_mutex_lock(&client->state.mutex);
    rc_client_update_active_achievements(game);
    rc_mutex_unlock(&client->state.mutex);
  }

  game->pending_events = RC_CLIENT_GAME_PENDING_EVENT_NONE;
}

void rc_client_do_frame(rc_client_t* client)
{
  if (!client)
    return;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->do_frame) {
    client->state.external_client->do_frame();
    return;
  }
#endif

  if (client->game && !client->game->waiting_for_reset) {
    rc_runtime_richpresence_t* richpresence;
    rc_client_subset_info_t* subset;
    rc_eval_state_t eval_state;

    rc_init_eval_state(&eval_state, rc_client_read_modified_memory_helper, client);

    rc_mutex_lock(&client->state.mutex);

    rc_client_reset_pending_events(client);

    rc_client_update_memref_values(client, &eval_state);

    client->game->progress_tracker.progress = 0.0;
    for (subset = client->game->subsets; subset; subset = subset->next) {
      if (subset->active)
        rc_client_do_frame_process_achievements(client, subset, &eval_state);
    }
    if (client->game->pending_events & RC_CLIENT_GAME_PENDING_EVENT_PROGRESS_TRACKER)
      rc_client_do_frame_update_progress_tracker(client, client->game);

    if (client->state.hardcore || client->state.allow_leaderboards_in_casual) {
      for (subset = client->game->subsets; subset; subset = subset->next) {
        if (subset->active)
          rc_client_do_frame_process_leaderboards(client, subset, &eval_state);
      }
    }

    richpresence = client->game->runtime.richpresence;
    if (richpresence && richpresence->richpresence)
      rc_update_richpresence(richpresence->richpresence, &eval_state);

    rc_mutex_unlock(&client->state.mutex);

    rc_client_raise_pending_events(client, client->game);

    ++client->state.frames_processed;
  }

  /* we've processed a frame. if there's a pause delay in effect, process it */
  if (client->state.unpaused_frame_decay > 0) {
    client->state.unpaused_frame_decay--;

    if (client->state.unpaused_frame_decay == 0 &&
        client->state.required_unpaused_frames > RC_MINIMUM_UNPAUSED_FRAMES) {
      /* the full decay has elapsed and a penalty still exists.
       * lower the penalty and reset the decay counter */
      client->state.required_unpaused_frames >>= 1;

      if (client->state.required_unpaused_frames <= RC_MINIMUM_UNPAUSED_FRAMES)
        client->state.required_unpaused_frames = RC_MINIMUM_UNPAUSED_FRAMES;

      client->state.unpaused_frame_decay =
        client->state.required_unpaused_frames * (RC_PAUSE_DECAY_MULTIPLIER - 1) - 1;
    }
  }

  rc_client_idle(client);
}

void rc_client_idle(rc_client_t* client)
{
  rc_client_scheduled_callback_data_t* scheduled_callback;

  if (!client)
    return;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->idle) {
    client->state.external_client->idle();
    return;
  }
#endif

  scheduled_callback = client->state.scheduled_callbacks;
  if (scheduled_callback) {
    const rc_clock_t now = client->callbacks.get_time_millisecs(client);

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
          scheduled_callback->next = NULL;
        }
      }
      rc_mutex_unlock(&client->state.mutex);

      if (!scheduled_callback)
        break;

      scheduled_callback->callback(scheduled_callback, client, now);
    } while (1);
  }

  if (client->state.disconnect & ~RC_CLIENT_DISCONNECT_VISIBLE)
    rc_client_raise_disconnect_events(client);
}

void rc_client_schedule_callback(rc_client_t* client, rc_client_scheduled_callback_data_t* scheduled_callback)
{
  rc_client_scheduled_callback_data_t** last;
  rc_client_scheduled_callback_data_t* next;

  rc_mutex_lock(&client->state.mutex);

  last = &client->state.scheduled_callbacks;
  do {
    next = *last;
    if (!next || scheduled_callback->when < next->when) {
      scheduled_callback->next = next;
      *last = scheduled_callback;
      break;
    }

    last = &next->next;
  } while (1);

  rc_mutex_unlock(&client->state.mutex);
}

int rc_client_has_server_retry_scheduled_callback(rc_client_t* client)
{
  /* ASSERT: this should only be called if the mutex is held */

  rc_client_scheduled_callback_data_t* scheduled_callback = client->state.scheduled_callbacks;
  for (; scheduled_callback; scheduled_callback = scheduled_callback->next) {
    if (scheduled_callback->callback == rc_client_award_achievement_retry ||
      scheduled_callback->callback == rc_client_submit_leaderboard_entry_retry) {
      return 1;
    }
  }

  return 0;
}

static void rc_client_reschedule_callback(rc_client_t* client,
  rc_client_scheduled_callback_data_t* callback, rc_clock_t when)
{
  rc_client_scheduled_callback_data_t** last;
  rc_client_scheduled_callback_data_t* next;

  /* ASSERT: this should only be called if the mutex is held */

  callback->when = when;

  last = &client->state.scheduled_callbacks;
  do {
    next = *last;

    if (next == callback) {
      if (when == 0) {
        /* request to unschedule the callback */
        *last = next->next;
        next->next = NULL;
        break;
      }

      if (!next->next) {
         /* end of list, just append it */
         break;
      }

      if (when < next->next->when) {
        /* already in the correct place */
        break;
      }

      /* remove from current position - will insert later */
      *last = next->next;
      next->next = NULL;
      continue;
    }

    if (!next || (when < next->when && when != 0)) {
      /* insert here */
      callback->next = next;
      *last = callback;
      break;
    }

    last = &next->next;
  } while (1);
}

static void rc_client_reset_richpresence(rc_client_t* client)
{
  rc_runtime_richpresence_t* richpresence = client->game->runtime.richpresence;
  if (richpresence && richpresence->richpresence)
    rc_reset_richpresence(richpresence->richpresence);
}

static void rc_client_reset_variables(rc_client_t* client)
{
  if (client->game->runtime.richpresence && client->game->runtime.richpresence->richpresence)
    rc_reset_values(client->game->runtime.richpresence->richpresence->values);
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
  if (!client)
    return;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->reset) {
    client->state.external_client->reset();
    return;
  }
#endif

  if (!client->game)
    return;

  game_hash = rc_client_find_game_hash(client, client->game->public_.hash);
  if (game_hash && game_hash->game_id != client->game->public_.id) {
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

  rc_client_hide_progress_tracker(client, client->game);
  rc_client_reset_all(client);

  rc_mutex_unlock(&client->state.mutex);

  rc_client_raise_pending_events(client, client->game);
}

int rc_client_can_pause(rc_client_t* client, uint32_t* frames_remaining)
{
  if (!client)
    return 1;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->can_pause)
    return client->state.external_client->can_pause(frames_remaining);
#endif

  if (frames_remaining)
    *frames_remaining = 0;

  /* pause is always allowed in casual mode */
  if (!rc_client_get_hardcore_enabled(client))
    return 1;

  /* a full decay means we haven't processed any frames since the last time this was called. */
  if (client->state.unpaused_frame_decay == client->state.required_unpaused_frames * RC_PAUSE_DECAY_MULTIPLIER)
    return 1;

  /* if less than RC_MINIMUM_UNPAUSED_FRAMES have been processed, don't allow the pause */
  if (client->state.unpaused_frame_decay > client->state.required_unpaused_frames * (RC_PAUSE_DECAY_MULTIPLIER - 1)) {
    if (frames_remaining) {
      *frames_remaining = client->state.unpaused_frame_decay -
                          client->state.required_unpaused_frames * (RC_PAUSE_DECAY_MULTIPLIER - 1);
    }
    return 0;
  }

  /* we're going to allow the emulator to pause. calculate how many frames are needed before the next
   * pause will be allowed. */

  if (client->state.unpaused_frame_decay > 0) {
    /* The user has paused within the decay window. Require a longer
     * run of unpaused frames before allowing the next pause */
    if (client->state.required_unpaused_frames < 5 * 60) /* don't make delay longer then 5 seconds */
      client->state.required_unpaused_frames += RC_MINIMUM_UNPAUSED_FRAMES;
  }

  /* require multiple unpaused_frames windows to decay the penalty */
  client->state.unpaused_frame_decay = client->state.required_unpaused_frames * RC_PAUSE_DECAY_MULTIPLIER;

  return 1;
}

size_t rc_client_progress_size(rc_client_t* client)
{
  size_t result;

  if (!client)
    return 0;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->progress_size)
    return client->state.external_client->progress_size();
#endif

  if (!rc_client_is_game_loaded(client))
    return 0;

  rc_mutex_lock(&client->state.mutex);
  result = rc_runtime_progress_size(&client->game->runtime, NULL);
  rc_mutex_unlock(&client->state.mutex);

  return result;
}

int rc_client_serialize_progress(rc_client_t* client, uint8_t* buffer)
{
  return rc_client_serialize_progress_sized(client, buffer, 0xFFFFFFFF);
}

int rc_client_serialize_progress_sized(rc_client_t* client, uint8_t* buffer, size_t buffer_size)
{
  int result;

  if (!client)
    return RC_NO_GAME_LOADED;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->serialize_progress)
    return client->state.external_client->serialize_progress(buffer, buffer_size);
#endif

  if (!rc_client_is_game_loaded(client))
    return RC_NO_GAME_LOADED;

  if (!buffer)
    return RC_INVALID_STATE;

  rc_mutex_lock(&client->state.mutex);
  result = rc_runtime_serialize_progress_sized(buffer, (uint32_t)buffer_size, &client->game->runtime, NULL);
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
  achievement_stop = achievement + subset->public_.num_achievements;
  for (; achievement < achievement_stop; ++achievement) {
    rc_trigger_t* trigger = achievement->trigger;
    if (trigger && trigger->state == RC_TRIGGER_STATE_PRIMED &&
        achievement->public_.state == RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE) {
      achievement->pending_events |= RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_HIDE;
      subset->pending_events |= RC_CLIENT_SUBSET_PENDING_EVENT_ACHIEVEMENT;
    }
  }

  /* flag any visible trackers to be hidden */
  leaderboard = subset->leaderboards;
  leaderboard_stop = leaderboard + subset->public_.num_leaderboards;
  for (; leaderboard < leaderboard_stop; ++leaderboard) {
    rc_lboard_t* lboard = leaderboard->lboard;
    if (lboard && lboard->state == RC_LBOARD_STATE_STARTED &&
        leaderboard->public_.state == RC_CLIENT_LEADERBOARD_STATE_TRACKING) {
      leaderboard->pending_events |= RC_CLIENT_LEADERBOARD_PENDING_EVENT_FAILED;
      subset->pending_events |= RC_CLIENT_SUBSET_PENDING_EVENT_LEADERBOARD;
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
  achievement_stop = achievement + subset->public_.num_achievements;
  for (; achievement < achievement_stop; ++achievement) {
    rc_trigger_t* trigger = achievement->trigger;
    if (!trigger || achievement->public_.state != RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE)
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
  leaderboard_stop = leaderboard + subset->public_.num_leaderboards;
  for (; leaderboard < leaderboard_stop; ++leaderboard) {
    rc_lboard_t* lboard = leaderboard->lboard;
    if (!lboard ||
        leaderboard->public_.state == RC_CLIENT_LEADERBOARD_STATE_INACTIVE ||
        leaderboard->public_.state == RC_CLIENT_LEADERBOARD_STATE_DISABLED)
      continue;

    if (lboard->state == RC_LBOARD_STATE_STARTED) {
      leaderboard->value = (int)lboard->value.value.value;
      leaderboard->public_.state = RC_CLIENT_LEADERBOARD_STATE_TRACKING;

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
      leaderboard->public_.state = RC_CLIENT_LEADERBOARD_STATE_ACTIVE;
      rc_client_release_leaderboard_tracker(game, leaderboard);
    }
  }
}

int rc_client_deserialize_progress(rc_client_t* client, const uint8_t* serialized)
{
  return rc_client_deserialize_progress_sized(client, serialized, 0xFFFFFFFF);
}

int rc_client_deserialize_progress_sized(rc_client_t* client, const uint8_t* serialized, size_t serialized_size)
{
  rc_client_subset_info_t* subset;
  int result;

  if (!client)
    return RC_NO_GAME_LOADED;

#ifdef RC_CLIENT_SUPPORTS_EXTERNAL
  if (client->state.external_client && client->state.external_client->deserialize_progress)
    return client->state.external_client->deserialize_progress(serialized, serialized_size);
#endif

  if (!rc_client_is_game_loaded(client))
    return RC_NO_GAME_LOADED;

  rc_mutex_lock(&client->state.mutex);

  rc_client_reset_pending_events(client);

  for (subset = client->game->subsets; subset; subset = subset->next)
    rc_client_subset_before_deserialize_progress(subset);

  rc_client_hide_progress_tracker(client, client->game);

  if (!serialized) {
    rc_client_reset_all(client);
    result = RC_OK;
  }
  else {
    result = rc_runtime_deserialize_progress_sized(&client->game->runtime, serialized, (uint32_t)serialized_size, NULL);
  }

  for (subset = client->game->subsets; subset; subset = subset->next)
    rc_client_subset_after_deserialize_progress(client->game, subset);

  rc_mutex_unlock(&client->state.mutex);

  rc_client_raise_pending_events(client, client->game);

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
