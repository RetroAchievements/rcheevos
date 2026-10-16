#ifndef RC_CLIENT_GAME_H
#define RC_CLIENT_GAME_H

#include "rc_client.h"
#include "rc_runtime.h"
#include "rc_util.h"

#include "rc_client_do_frame.h"

#include <stdint.h>

RC_BEGIN_C_DECLS

struct rc_lboard_t;  /* rc_lboard.h */
struct rc_trigger_t; /* rc_trigger.h */

/*****************************************************************************\
| Achievements                                                                |
\*****************************************************************************/

enum {
  RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_NONE = 0,
  RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_TRIGGERED = (1 << 1),
  RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_SHOW = (1 << 2),
  RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_CHALLENGE_INDICATOR_HIDE = (1 << 3),
  RC_CLIENT_ACHIEVEMENT_PENDING_EVENT_UPDATE = (1 << 4) /* not a real event, just triggers update */
};

#define RC_CLIENT_ACHIEVEMENT_WARNING_ID 101000001

typedef struct rc_client_achievement_info_t {
  rc_client_achievement_t public_;

  struct rc_trigger_t* trigger;
  uint8_t md5[16];

  time_t unlock_time_hardcore;
  time_t unlock_time_casual;

  uint8_t pending_events;

  const char* author;
  time_t created_time;
  time_t updated_time;
} rc_client_achievement_info_t;

struct rc_client_achievement_list_info_t;
typedef void (RC_CCONV* rc_client_destroy_achievement_list_func_t)(struct rc_client_achievement_list_info_t* list);

typedef struct rc_client_achievement_list_info_t {
  rc_client_achievement_list_t public_;
  rc_client_destroy_achievement_list_func_t destroy_func;
} rc_client_achievement_list_info_t;

enum {
  RC_CLIENT_PROGRESS_TRACKER_ACTION_NONE,
  RC_CLIENT_PROGRESS_TRACKER_ACTION_SHOW,
  RC_CLIENT_PROGRESS_TRACKER_ACTION_UPDATE,
  RC_CLIENT_PROGRESS_TRACKER_ACTION_HIDE
};

typedef struct rc_client_progress_tracker_t {
  rc_client_achievement_info_t* achievement;
  float progress;

  rc_client_scheduled_callback_data_t* hide_callback;
  uint8_t action;
} rc_client_progress_tracker_t;

/*****************************************************************************\
| Leaderboard Trackers                                                        |
\*****************************************************************************/

enum {
  RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_NONE = 0,
  RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_UPDATE = (1 << 1),
  RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_SHOW = (1 << 2),
  RC_CLIENT_LEADERBOARD_TRACKER_PENDING_EVENT_HIDE = (1 << 3)
};

typedef struct rc_client_leaderboard_tracker_info_t {
  rc_client_leaderboard_tracker_t public_;
  struct rc_client_leaderboard_tracker_info_t* next;
  int32_t raw_value;

  uint32_t value_djb2;

  uint8_t format;
  uint8_t pending_events;
  uint8_t reference_count;
  uint8_t value_from_hits;
} rc_client_leaderboard_tracker_info_t;

/*****************************************************************************\
| Leaderboards                                                                |
\*****************************************************************************/

enum {
  RC_CLIENT_LEADERBOARD_PENDING_EVENT_NONE = 0,
  RC_CLIENT_LEADERBOARD_PENDING_EVENT_STARTED = (1 << 1),
  RC_CLIENT_LEADERBOARD_PENDING_EVENT_FAILED = (1 << 2),
  RC_CLIENT_LEADERBOARD_PENDING_EVENT_SUBMITTED = (1 << 3)
};

typedef struct rc_client_leaderboard_info_t {
  rc_client_leaderboard_t public_;

  struct rc_lboard_t* lboard;
  uint8_t md5[16];

  rc_client_leaderboard_tracker_info_t* tracker;

  uint32_t value_djb2;
  int32_t value;

  uint8_t format;
  uint8_t pending_events;
  uint8_t bucket;
  uint8_t hidden;
} rc_client_leaderboard_info_t;

struct rc_client_leaderboard_list_info_t;
typedef void (RC_CCONV* rc_client_destroy_leaderboard_list_func_t)(struct rc_client_leaderboard_list_info_t* list);

typedef struct rc_client_leaderboard_list_info_t {
  rc_client_leaderboard_list_t public_;
  rc_client_destroy_leaderboard_list_func_t destroy_func;
} rc_client_leaderboard_list_info_t;

struct rc_client_leaderboard_entry_list_info_t;
typedef void (RC_CCONV* rc_client_destroy_leaderboard_entry_list_func_t)(struct rc_client_leaderboard_entry_list_info_t* list);

typedef struct rc_client_leaderboard_entry_list_info_t {
  rc_client_leaderboard_entry_list_t public_;
  rc_client_destroy_leaderboard_entry_list_func_t destroy_func;
} rc_client_leaderboard_entry_list_info_t;

uint8_t rc_client_map_leaderboard_format(int format);

/*****************************************************************************\
| Subsets                                                                     |
\*****************************************************************************/

enum {
  RC_CLIENT_SUBSET_PENDING_EVENT_NONE = 0,
  RC_CLIENT_SUBSET_PENDING_EVENT_ACHIEVEMENT = (1 << 1),
  RC_CLIENT_SUBSET_PENDING_EVENT_LEADERBOARD = (1 << 2)
};

typedef struct rc_client_subset_info_t {
  rc_client_subset_t public_;

  rc_client_achievement_info_t* achievements;
  rc_client_leaderboard_info_t* leaderboards;

  struct rc_client_subset_info_t* next;

  const char* all_label;
  const char* inactive_label;
  const char* locked_label;
  const char* unlocked_label;
  const char* unpromoted_label;
  const char* unsupported_label;

  uint8_t active;
  uint8_t mastery;
  uint8_t pending_events;
} rc_client_subset_info_t;

struct rc_client_subset_list_info_t;
typedef void (RC_CCONV* rc_client_destroy_subset_list_func_t)(struct rc_client_subset_list_info_t* list);

typedef struct rc_client_subset_list_info_t {
  rc_client_subset_list_t public_;
  rc_client_destroy_subset_list_func_t destroy_func;
} rc_client_subset_list_info_t;

const rc_client_achievement_t* rc_client_subset_get_achievement_info(rc_client_t* client, rc_client_subset_info_t* subset, uint32_t id);
const rc_client_leaderboard_info_t* rc_client_subset_get_leaderboard_info(const rc_client_subset_info_t* subset, uint32_t id);

/*****************************************************************************\
| Game                                                                        |
\*****************************************************************************/

typedef struct rc_client_game_hash_t {
  char hash[33];
  uint8_t is_unknown;
  uint32_t game_id;
  struct rc_client_game_hash_t* next;
} rc_client_game_hash_t;

rc_client_game_hash_t* rc_client_find_game_hash(rc_client_t* client, const char* hash);

typedef struct rc_client_media_hash_t {
  rc_client_game_hash_t* game_hash;
  struct rc_client_media_hash_t* next;
  uint32_t path_djb2;
} rc_client_media_hash_t;

enum {
  RC_CLIENT_GAME_PENDING_EVENT_NONE = 0,
  RC_CLIENT_GAME_PENDING_EVENT_LEADERBOARD_TRACKER = (1 << 1),
  RC_CLIENT_GAME_PENDING_EVENT_UPDATE_ACTIVE_ACHIEVEMENTS = (1 << 2),
  RC_CLIENT_GAME_PENDING_EVENT_PROGRESS_TRACKER = (1 << 3)
};

typedef struct rc_client_game_info_t {
  rc_client_game_t public_;
  rc_client_leaderboard_tracker_info_t* leaderboard_trackers;
  rc_client_progress_tracker_t progress_tracker;

  rc_client_subset_info_t* subsets;

  rc_client_media_hash_t* media_hash;

  rc_runtime_t runtime;

  uint32_t max_valid_address;

  uint8_t waiting_for_reset;
  uint8_t pending_events;

  rc_buffer_t buffer;
} rc_client_game_info_t;

/*****************************************************************************\
| Functions                                                                   |
\*****************************************************************************/

void rc_client_toggle_hardcore_achievements(rc_client_game_info_t* game, rc_client_t* client, uint8_t active_bit);

void rc_client_update_active_achievements(rc_client_game_info_t* game);
void rc_client_update_active_leaderboards(rc_client_game_info_t* game);
void rc_client_update_achievement_display_information(rc_client_t* client, rc_client_achievement_info_t* achievement, time_t recent_unlock_time);

void rc_client_activate_achievements(struct rc_client_game_info_t* game, rc_client_t* client);
void rc_client_activate_leaderboards(struct rc_client_game_info_t* game, rc_client_t* client);
void rc_client_deactivate_leaderboards(rc_client_game_info_t* game, rc_client_t* client);

void rc_client_add_game_hash(rc_client_t* client, const char* hash, uint32_t game_id);

RC_END_C_DECLS

#endif /* RC_CLIENT_GAME_H */
