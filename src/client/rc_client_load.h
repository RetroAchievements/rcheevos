#ifndef RC_CLIENT_LOAD_H
#define RC_CLIENT_LOAD_H

#include "rc_client.h" /* rc_client_callback */
#include "rc_hash.h"   /* rc_hash_iterator_t */
#include "rc_export.h"

#include "rc_client_async.h" /* rc_client_async_handle_t */

#include <stdint.h>

RC_BEGIN_C_DECLS

/*****************************************************************************\
| Enums                                                                       |
\*****************************************************************************/

/*****************************************************************************\
| Structures                                                                  |
\*****************************************************************************/

struct rc_api_start_session_response_t; /* rc_api_user.h */
struct rc_client_t;                     /* rc_client.h */
struct rc_client_game_info_t;           /* rc_client_game.h */
struct rc_client_game_hash_t;           /* rc_client_game.h */
struct rc_client_subset_info_t;         /* rc_client_game.h */
struct rc_client_pending_media_t;       /* rc_client_game.h */

typedef struct rc_client_load_state_t
{
  struct rc_client_t* client;
  rc_client_callback_t callback;
  void* callback_userdata;

  struct rc_client_game_info_t* game;
  struct rc_client_subset_info_t* subset;
  struct rc_client_game_hash_t* hash;

#ifdef RC_CLIENT_SUPPORTS_HASH
  rc_hash_iterator_t hash_iterator;
  struct rc_client_game_hash_t* tried_hashes[4];
#endif
  struct rc_client_pending_media_t* pending_media;

  struct rc_api_start_session_response_t* start_session_response;

  rc_client_async_handle_t async_handle;

  uint8_t progress;
  uint8_t outstanding_requests;
#ifdef RC_CLIENT_SUPPORTS_HASH
  uint8_t hash_console_id;
#endif
} rc_client_load_state_t;

/*****************************************************************************\
| Functions                                                                   |
\*****************************************************************************/

void rc_client_begin_fetch_game_sets(struct rc_client_load_state_t* load_state);

void rc_client_load_error(struct rc_client_load_state_t* load_state, int result, const char* error_message);

RC_END_C_DECLS

#endif /* RC_CLIENT_USER_H */
