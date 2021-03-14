#ifndef RC_API_H
#define RC_API_H

#include "rc_error.h"

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rc_api_buffer_t {
  char* write;
  char* end;
  struct rc_api_buffer_t* next;
  char data[256]; /* actual size of data[] may be larger than 256 bytes for buffers allocated in the next chain */
}
rc_api_buffer_t;

typedef struct rc_api_request_t {
  const char* url;
  const char* post_data;

  rc_api_buffer_t buffer;
}
rc_api_request_t;

typedef struct rc_api_response_t {
  int succeeded;
  const char* error_message;

  rc_api_buffer_t buffer;
}
rc_api_response_t;

void rc_api_destroy_request(rc_api_request_t* request);

/* ===== General Functions ===== */
void rc_api_set_host(const char* hostname);

/* --- Fetch Image --- */

typedef struct rc_api_fetch_image_request_t {
  const char* image_name;
  int image_type;
}
rc_api_fetch_image_request_t;

#define RC_IMAGE_TYPE_GAME 1
#define RC_IMAGE_TYPE_ACHIEVEMENT 2
#define RC_IMAGE_TYPE_ACHIEVEMENT_LOCKED 3
#define RC_IMAGE_TYPE_USER 4

int rc_api_init_fetch_image_request(rc_api_request_t* request, const rc_api_fetch_image_request_t* api_params);

/* ===== User Functions ===== */
/* --- Login --- */

typedef struct rc_api_login_request_t {
  const char* username;
  const char* api_token;
  const char* password;
}
rc_api_login_request_t;

typedef struct rc_api_login_response_t {
  const char* username;
  const char* api_token;
  unsigned score;
  unsigned num_unread_messages;

  rc_api_response_t response;
}
rc_api_login_response_t;

int rc_api_init_login_request(rc_api_request_t* request, const rc_api_login_request_t* api_params);
int rc_api_process_login_response(rc_api_login_response_t* response, const char* server_response);
void rc_api_destroy_login_response(rc_api_login_response_t* response);

/* --- Start Session --- */

typedef struct rc_api_start_session_request_t {
  const char* username;
  const char* api_token;
  unsigned game_id;
}
rc_api_start_session_request_t;

typedef struct rc_api_start_session_response_t {
  rc_api_response_t response;
}
rc_api_start_session_response_t;

int rc_api_init_start_session_request(rc_api_request_t* request, const rc_api_start_session_request_t* api_params);
int rc_api_process_start_session_response(rc_api_start_session_response_t* response, const char* server_response);
void rc_api_destroy_start_session_response(rc_api_start_session_response_t* response);

/* --- Fetch User Unlocks --- */

typedef struct rc_api_fetch_user_unlocks_request_t {
  const char* username;
  const char* api_token;
  unsigned game_id;
  int hardcore;
}
rc_api_fetch_user_unlocks_request_t;

typedef struct rc_api_fetch_user_unlocks_response_t {
  unsigned* achievement_ids;
  unsigned num_achievement_ids;

  rc_api_response_t response;
}
rc_api_fetch_user_unlocks_response_t;

int rc_api_init_fetch_user_unlocks_request(rc_api_request_t* request, const rc_api_fetch_user_unlocks_request_t* api_params);
int rc_api_process_fetch_user_unlocks_response(rc_api_fetch_user_unlocks_response_t* response, const char* server_response);
void rc_api_destroy_fetch_user_unlocks_response(rc_api_fetch_user_unlocks_response_t* response);

/* ===== Runtime Functions ===== */
/* --- Resolve Hash --- */

typedef struct rc_api_resolve_hash_request_t {
  const char* username;
  const char* api_token;
  const char* game_hash;
}
rc_api_resolve_hash_request_t;

typedef struct rc_api_resolve_hash_response_t {
  unsigned game_id;

  rc_api_response_t response;
}
rc_api_resolve_hash_response_t;

int rc_api_init_resolve_hash_request(rc_api_request_t* request, const rc_api_resolve_hash_request_t* api_params);
int rc_api_process_resolve_hash_response(rc_api_resolve_hash_response_t* response, const char* server_response);
void rc_api_destroy_resolve_hash_response(rc_api_resolve_hash_response_t* response);

/* --- Fetch Game Data --- */

typedef struct rc_api_fetch_game_data_request_t {
  const char* username;
  const char* api_token;
  unsigned game_id;
}
rc_api_fetch_game_data_request_t;

typedef struct rc_api_leaderboard_definition_t {
  unsigned id;
  int format;
  const char* title;
  const char* description;
  const char* definition;
}
rc_api_leaderboard_definition_t;

typedef struct rc_api_achievement_definition_t {
  unsigned id;
  unsigned points;
  unsigned category;
  const char* title;
  const char* description;
  const char* definition;
  const char* author;
  const char* badge_name;
  time_t created;
  time_t updated;
}
rc_api_achievement_definition_t;

#define RC_ACHIEVEMENT_CATEGORY_CORE 3
#define RC_ACHIEVEMENT_CATEGORY_UNOFFICIAL 5

typedef struct rc_api_fetch_game_data_response_t {
  unsigned id;
  unsigned console_id;
  const char* title;
  const char* image_name;
  const char* rich_presence_script;

  rc_api_achievement_definition_t* achievements;
  unsigned num_achievements;

  rc_api_leaderboard_definition_t* leaderboards;
  unsigned num_leaderboards;

  rc_api_response_t response;
}
rc_api_fetch_game_data_response_t;

int rc_api_init_fetch_game_data_request(rc_api_request_t* request, const rc_api_fetch_game_data_request_t* api_params);
int rc_api_process_fetch_game_data_response(rc_api_fetch_game_data_response_t* response, const char* server_response);
void rc_api_destroy_fetch_game_data_response(rc_api_fetch_game_data_response_t* response);

/* --- Ping --- */

typedef struct rc_api_ping_request_t {
  const char* username;
  const char* api_token;
  unsigned game_id;
  const char* rich_presence;
}
rc_api_ping_request_t;

typedef struct rc_api_ping_response_t {
  rc_api_response_t response;
}
rc_api_ping_response_t;

int rc_api_init_ping_request(rc_api_request_t* request, const rc_api_ping_request_t* api_params);
int rc_api_process_ping_response(rc_api_ping_response_t* response, const char* server_response);
void rc_api_destroy_ping_response(rc_api_ping_response_t* response);

/* --- Award Achievement --- */

typedef struct rc_api_award_achievement_request_t {
  const char* username;
  const char* api_token;
  unsigned achievement_id;
  int hardcore;
  const char* game_hash;
}
rc_api_award_achievement_request_t;

typedef struct rc_api_award_achievement_response_t {
  unsigned awarded_achievement_id;
  unsigned new_player_score;

  rc_api_response_t response;
}
rc_api_award_achievement_response_t;

int rc_api_init_award_achievement_request(rc_api_request_t* request, const rc_api_award_achievement_request_t* api_params);
int rc_api_process_award_achievement_response(rc_api_award_achievement_response_t* response, const char* server_response);
void rc_api_destroy_award_achievement_response(rc_api_award_achievement_response_t* response);

/* --- Submit Leaderboard Entry --- */

typedef struct rc_api_submit_lboard_entry_request_t {
  const char* username;
  const char* api_token;
  unsigned leaderboard_id;
  int score;
  const char* game_hash;
}
rc_api_submit_lboard_entry_request_t;

typedef struct rc_api_lboard_entry_t {
  const char* username;
  unsigned rank;
  int score;
}
rc_api_lboard_entry_t;

typedef struct rc_api_submit_lboard_entry_response_t {
  int submitted_score;
  int best_score;
  unsigned new_rank;
  unsigned num_entries;

  rc_api_lboard_entry_t* top_entries;
  unsigned num_top_entries;

  rc_api_response_t response;
}
rc_api_submit_lboard_entry_response_t;

int rc_api_init_submit_lboard_entry_request(rc_api_request_t* request, const rc_api_submit_lboard_entry_request_t* api_params);
int rc_api_process_submit_lboard_entry_response(rc_api_submit_lboard_entry_response_t* response, const char* server_response);
void rc_api_destroy_submit_lboard_entry_response(rc_api_submit_lboard_entry_response_t* response);

#ifdef __cplusplus
}
#endif

#endif /* RC_API_H */
