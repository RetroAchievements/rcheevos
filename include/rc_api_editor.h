#ifndef RC_API_EDITOR_H
#define RC_API_EDITOR_H

#include "rc_api_request.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --- Fetch Code Notes --- */

/**
 * API parameters for a fetch code notes request.
 */
typedef struct rc_api_fetch_code_notes_request_t {
  /* The unique identifier of the game */
  unsigned game_id;
}
rc_api_fetch_code_notes_request_t;

/* A code note definiton */
typedef struct rc_api_code_note_t {
  /* The address the note is associated to */
  unsigned address;
  /* The name of the use who last updated the note */
  const char* author;
  /* The contents of the note */
  const char* note;
} rc_api_code_note_t;

/**
 * Response data for a fetch code notes request.
 */
typedef struct rc_api_fetch_code_notes_response_t {
  /* An array of code notes for the game */
  rc_api_code_note_t* notes;
  /* The number of items in the notes array */
  unsigned num_notes;

  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_fetch_code_notes_response_t;

int rc_api_init_fetch_code_notes_request(rc_api_request_t* request, const rc_api_fetch_code_notes_request_t* api_params);
int rc_api_process_fetch_code_notes_response(rc_api_fetch_code_notes_response_t* response, const char* server_response);
void rc_api_destroy_fetch_code_notes_response(rc_api_fetch_code_notes_response_t* response);

/* --- Update Code Note --- */

/**
 * API parameters for an update code note request.
 */
typedef struct rc_api_update_code_note_request_t {
  /* The username of the developer */
  const char* username;
  /* The API token from the login request */
  const char* api_token;
  /* The unique identifier of the game */
  unsigned game_id;
  /* The address the note is associated to */
  unsigned address;
  /* The contents of the note (NULL or empty to delete a note) */
  const char* note;
}
rc_api_update_code_note_request_t;

/**
 * Response data for an update code note request.
 */
typedef struct rc_api_update_code_note_response_t {
  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_update_code_note_response_t;

int rc_api_init_update_code_note_request(rc_api_request_t* request, const rc_api_update_code_note_request_t* api_params);
int rc_api_process_update_code_note_response(rc_api_update_code_note_response_t* response, const char* server_response);
void rc_api_destroy_update_code_note_response(rc_api_update_code_note_response_t* response);

/* --- Update Achievement --- */

/**
 * API parameters for an update achievement request.
 */
typedef struct rc_api_update_achievement_request_t {
  /* The username of the developer */
  const char* username;
  /* The API token from the login request */
  const char* api_token;
  /* The unique identifier of the achievement (0 to create a new achievement) */
  unsigned achievement_id;
  /* The unique identifier of the game */
  unsigned game_id;
  /* The name of the achievement */
  const char* title;
  /* The description of the achievement */
  const char* description;
  /* The badge name for the achievement */
  const char* badge;
  /* The serialized trigger for the achievement */
  const char* trigger;
  /* The number of points the achievement is worth */
  unsigned points;
  /* The category of the achievement */
  unsigned category;
}
rc_api_update_achievement_request_t;

/**
 * Response data for an update achievement request.
 */
typedef struct rc_api_update_achievement_response_t {
  /* The unique identifier of the achievement */
  unsigned achievement_id;

  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_update_achievement_response_t;

int rc_api_init_update_achievement_request(rc_api_request_t* request, const rc_api_update_achievement_request_t* api_params);
int rc_api_process_update_achievement_response(rc_api_update_achievement_response_t* response, const char* server_response);
void rc_api_destroy_update_achievement_response(rc_api_update_achievement_response_t* response);

#ifdef __cplusplus
}
#endif

#endif /* RC_EDITOR_H */
