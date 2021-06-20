#ifndef RC_API_INFO_H
#define RC_API_INFO_H

#include "rc_api_request.h"

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- Fetch Leaderboard Info --- */

/**
 * API parameters for a fetch leaderboard info request.
 */
typedef struct rc_api_fetch_leaderboard_info_request_t {
  /* The username of the player */
  const char* username;
  /* The API token from the login request */
  const char* api_token;
  /* The unique identifier of the leaderboard */
  unsigned leaderboard_id;
  /* The index of the first entry to retrieve  */
  unsigned first_entry;
  /* The number of entries to retrieve */
  unsigned count;
}
rc_api_fetch_leaderboard_info_request_t;

/* A leaderboard info entry */
typedef struct rc_api_lboard_info_entry_t {
  /* The user associated to the entry */
  const char* username;
  /* The rank of the entry */
  unsigned rank;
  /* The value of the entry */
  int score;
  /* When the entry was submitted */
  time_t submitted;
}
rc_api_lboard_info_entry_t;

/**
 * Response data for a fetch leaderboard info request.
 */
typedef struct rc_api_fetch_leaderboard_info_response_t {
  /* The unique identifier of the leaderboard */
  unsigned id;
  /* The format to pass to rc_format_value to format the leaderboard value */
  int format;
  /* If non-zero, indicates that lower scores appear first */
  int lower_is_better;
  /* The title of the leaderboard */
  const char* title;
  /* The description of the leaderboard */
  const char* description;
  /* The definition of the leaderboard to be passed to rc_runtime_activate_lboard */
  const char* definition;
  /* The unique identifier of the game to which the leaderboard is associated */
  unsigned game_id;
  /* The author of the leaderboard */
  const char* author;
  /* When the leaderboard was first uploaded to the server */
  time_t created;
  /* When the leaderboard was last modified on the server */
  time_t updated;

  /* An array of requested entries */
  rc_api_lboard_info_entry_t* entries;
  /* The number of items in the entries array */
  unsigned num_entries;

  /* Common server-provided response information */
  rc_api_response_t response;
}
rc_api_fetch_leaderboard_info_response_t;

int rc_api_init_fetch_leaderboard_info_request(rc_api_request_t* request, const rc_api_fetch_leaderboard_info_request_t* api_params);
int rc_api_process_fetch_leaderboard_info_response(rc_api_fetch_leaderboard_info_response_t* response, const char* server_response);
void rc_api_destroy_fetch_leaderboard_info_response(rc_api_fetch_leaderboard_info_response_t* response);

#ifdef __cplusplus
}
#endif

#endif /* RC_API_INFO_H */
