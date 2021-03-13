#ifndef RC_API_H
#define RC_API_H

#include "rc_error.h"

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

/* ===== Runtime Functions ===== */
/* --- Resolve Hash --- */

typedef struct rc_api_resolve_hash_request_t
{
  const char* username;
  const char* api_token;
  const char* game_hash;
}
rc_api_resolve_hash_request_t;

typedef struct rc_api_resolve_hash_response_t
{
  unsigned game_id;

  rc_api_response_t response;
}
rc_api_resolve_hash_response_t;

int rc_api_init_resolve_hash_request(rc_api_request_t* request, const rc_api_resolve_hash_request_t* api_params);
int rc_api_process_resolve_hash_response(rc_api_resolve_hash_response_t* response, const char* server_response);
void rc_api_destroy_resolve_hash_response(rc_api_resolve_hash_response_t* response);

/* --- Award Achievement --- */

typedef struct rc_api_award_achievement_request_t
{
  const char* username;
  const char* api_token;
  unsigned achievement_id;
  int hardcore;
  const char* game_hash;
}
rc_api_award_achievement_request_t;

typedef struct rc_api_award_achievement_response_t
{
  unsigned awarded_achievement_id;
  unsigned new_player_score;

  rc_api_response_t response;
}
rc_api_award_achievement_response_t;

int rc_api_init_award_achievement_request(rc_api_request_t* request, const rc_api_award_achievement_request_t* api_params);
int rc_api_process_award_achievement_response(rc_api_award_achievement_response_t* response, const char* server_response);
void rc_api_destroy_award_achievement_response(rc_api_award_achievement_response_t* response);

/* --- Submit Leaderboard Entry --- */

typedef struct rc_api_submit_lboard_entry_request_t
{
  const char* username;
  const char* api_token;
  unsigned leaderboard_id;
  int score;
  const char* game_hash;
}
rc_api_submit_lboard_entry_request_t;

typedef struct rc_api_lboard_entry_t
{
  const char* username;
  unsigned rank;
  int score;
}
rc_api_lboard_entry_t;

typedef struct rc_api_submit_lboard_entry_response_t
{
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
