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
  /* the address the note is associated to */
  unsigned address;
  /* the name of the use who last updated the note */
  const char* author;
  /* the contents of the note */
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
  /* the address the note is associated to */
  unsigned address;
  /* the contents of the note (NULL or empty to delete a note) */
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

#ifdef __cplusplus
}
#endif

#endif /* RC_EDITOR_H */
