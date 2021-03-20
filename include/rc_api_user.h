#ifndef RC_API_USER_H
#define RC_API_USER_H

#include "rc_api_request.h"

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif /* RC_API_H */
