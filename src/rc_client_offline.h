#ifndef RC_CLIENT_OFFLINE_H
#define RC_CLIENT_OFFLINE_H

#include "rc_client_internal.h"

RC_BEGIN_C_DECLS

/* ---- File format constants ---- */
#define RC_OFFLINE_UNLOCK_ENTRY_SIZE 16  /* on-disk unlock entry: id(4) + when(8) + hardcore(1) + reserved(3) */

/* ---- Queue file format constants ---- */
#define RC_OFFLINE_QUEUE_MAGIC       0x52435051  /* "RCPQ" */
#define RC_OFFLINE_QUEUE_VERSION     1
#define RC_OFFLINE_QUEUE_MAX_ENTRIES 256

#define RC_OFFLINE_CACHE_MAGIC       0x52434346  /* "RCCF" */
#define RC_OFFLINE_CACHE_VERSION     1

/* ---- Queue entry ---- */
typedef struct rc_offline_queue_entry_t {
  uint32_t achievement_id;
  uint32_t unlock_unix_time;
  uint32_t retry_count;
  uint8_t  hardcore;
  uint8_t  entry_type;     /* 0 = achievement, 1 = leaderboard (reserved) */
  uint8_t  reserved1[2];
  char     game_hash[33];  /* null-terminated MD5 hex */
  uint8_t  reserved2[7];
} rc_offline_queue_entry_t;  /* 56 bytes */

/* ---- Queue file header ---- */
typedef struct rc_offline_queue_header_t {
  uint32_t magic;
  uint16_t version;
  uint16_t entry_count;
  uint32_t crc32;
  uint32_t reserved;
} rc_offline_queue_header_t;  /* 16 bytes */

/* ---- Cache file header ---- */
typedef struct rc_offline_cache_header_t {
  uint32_t magic;
  uint16_t version;
  uint16_t reserved1;
  uint64_t timestamp;
  uint32_t data_size;
  uint32_t crc32;
} rc_offline_cache_header_t;  /* 22 bytes, padded to 24 */

/* ---- Credential cache ---- */
void rc_client_offline_cache_credentials(rc_client_t* client, const char* username,
    const char* token, const char* display_name, uint32_t score,
    uint32_t score_softcore, const char* avatar_url);
int rc_client_offline_load_credentials(rc_client_t* client, const char* username);

/* ---- Game definition cache ---- */
void rc_client_offline_cache_game_data(rc_client_t* client, const char* username,
    const char* game_hash, const uint8_t* response_body, uint32_t response_size);
int rc_client_offline_load_game_data(rc_client_t* client, const char* username,
    const char* game_hash, uint8_t* buffer, uint32_t buffer_size);

/* ---- Unlock status cache ---- */
struct rc_api_start_session_response_t;
void rc_client_offline_cache_unlocks(rc_client_t* client, const char* username,
    const char* game_hash, const struct rc_api_start_session_response_t* response);
int rc_client_offline_load_unlocks(rc_client_t* client, const char* username,
    const char* game_hash, uint8_t* buffer, uint32_t buffer_size);

/* ---- Persistent unlock queue ---- */
void rc_client_offline_queue_append(rc_client_t* client,
    const rc_offline_queue_entry_t* entry);
void rc_client_offline_queue_save_pending(rc_client_t* client);
int rc_client_offline_queue_load(rc_client_t* client,
    rc_offline_queue_entry_t* entries, uint32_t max_entries);
void rc_client_offline_queue_clear(rc_client_t* client);

/* ---- Utilities ---- */
void rc_offline_crc32_init_table(void); /* call once from single-threaded context */
uint32_t rc_offline_crc32(const uint8_t* data, uint32_t size);

RC_END_C_DECLS

#endif /* RC_CLIENT_OFFLINE_H */
