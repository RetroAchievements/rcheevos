#include "rc_client_offline.h"
#include "rc_api_user.h"

#include <string.h>
#include <time.h>

/* ---- CRC32 (standard polynomial 0xEDB88320) ---- */

static uint32_t rc_offline_crc32_table[256];
static int rc_offline_crc32_table_initialized = 0;

void rc_offline_crc32_init_table(void) {
  uint32_t i, j, crc;
  if (rc_offline_crc32_table_initialized)
    return;
  for (i = 0; i < 256; i++) {
    crc = i;
    for (j = 0; j < 8; j++) {
      if (crc & 1)
        crc = (crc >> 1) ^ 0xEDB88320u;
      else
        crc >>= 1;
    }
    rc_offline_crc32_table[i] = crc;
  }
  rc_offline_crc32_table_initialized = 1;
}

uint32_t rc_offline_crc32(const uint8_t* data, uint32_t size) {
  uint32_t crc = 0xFFFFFFFFu;
  uint32_t i;
  if (!data || size == 0)
    return 0x00000000u;
  rc_offline_crc32_init_table();
  for (i = 0; i < size; i++)
    crc = rc_offline_crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
  return crc ^ 0xFFFFFFFFu;
}

/* ---- Helper: build filename into a static buffer ---- */

static void rc_offline_build_filename(char* buffer, size_t buffer_size,
    const char* username, const char* game_hash, const char* suffix) {
  if (game_hash)
    snprintf(buffer, buffer_size, "%s_%s_%s", username, game_hash, suffix);
  else
    snprintf(buffer, buffer_size, "%s_%s", username, suffix);
}

/* ---- Credential Cache ---- */

#define RC_CREDENTIAL_CACHE_MAGIC   0x52434352  /* "RCCR" */
#define RC_CREDENTIAL_CACHE_VERSION 1

typedef struct rc_credential_cache_t {
  uint32_t magic;
  uint16_t version;
  uint16_t reserved;
  uint32_t score;
  uint32_t score_softcore;
  char username[64];
  char token[64];
  char display_name[64];
  char avatar_url[256];
  uint32_t crc32;
} rc_credential_cache_t;  /* 460 bytes */

void rc_client_offline_cache_credentials(rc_client_t* client, const char* username,
    const char* token, const char* display_name, uint32_t score,
    uint32_t score_softcore, const char* avatar_url) {
  rc_credential_cache_t cache;
  char filename[128];

  if (!client->callbacks.write_storage)
    return;

  memset(&cache, 0, sizeof(cache));
  cache.magic = RC_CREDENTIAL_CACHE_MAGIC;
  cache.version = RC_CREDENTIAL_CACHE_VERSION;
  cache.score = score;
  cache.score_softcore = score_softcore;

  if (username)
    strncpy(cache.username, username, sizeof(cache.username) - 1);
  if (token)
    strncpy(cache.token, token, sizeof(cache.token) - 1);
  if (display_name)
    strncpy(cache.display_name, display_name, sizeof(cache.display_name) - 1);
  if (avatar_url)
    strncpy(cache.avatar_url, avatar_url, sizeof(cache.avatar_url) - 1);

  /* CRC covers everything except the crc32 field itself */
  cache.crc32 = rc_offline_crc32((const uint8_t*)&cache,
      (uint32_t)(sizeof(cache) - sizeof(cache.crc32)));

  rc_offline_build_filename(filename, sizeof(filename), username, NULL, "credentials.bin");
  client->callbacks.write_storage(filename, (const uint8_t*)&cache,
      (uint32_t)sizeof(cache), client);
}

int rc_client_offline_load_credentials(rc_client_t* client, const char* username) {
  rc_credential_cache_t cache;
  char filename[128];
  int bytes_read;
  uint32_t expected_crc;

  if (!client->callbacks.read_storage)
    return 0;

  rc_offline_build_filename(filename, sizeof(filename), username, NULL, "credentials.bin");
  bytes_read = client->callbacks.read_storage(filename, (uint8_t*)&cache,
      (uint32_t)sizeof(cache), client);

  if (bytes_read != (int)sizeof(cache))
    return 0;

  if (cache.magic != RC_CREDENTIAL_CACHE_MAGIC || cache.version != RC_CREDENTIAL_CACHE_VERSION)
    return 0;

  expected_crc = rc_offline_crc32((const uint8_t*)&cache,
      (uint32_t)(sizeof(cache) - sizeof(cache.crc32)));
  if (cache.crc32 != expected_crc) {
    RC_CLIENT_LOG_WARN(client, "Credential cache CRC mismatch, ignoring");
    return 0;
  }

  /* Populate client->user from cache.
   * Strings are copied into the client's buffer so they persist. */
  {
    char* buf_username;
    char* buf_token;
    char* buf_display_name;
    char* buf_avatar_url;

    rc_mutex_lock(&client->state.mutex);

    buf_username = rc_buffer_strcpy(&client->state.buffer, cache.username);
    buf_token = rc_buffer_strcpy(&client->state.buffer, cache.token);
    buf_display_name = rc_buffer_strcpy(&client->state.buffer, cache.display_name);
    buf_avatar_url = rc_buffer_strcpy(&client->state.buffer, cache.avatar_url);

    client->user.username = buf_username;
    client->user.token = buf_token;
    client->user.display_name = buf_display_name;
    client->user.avatar_url = buf_avatar_url;
    client->user.score = cache.score;
    client->user.score_softcore = cache.score_softcore;
    client->user.num_unread_messages = 0;

    client->state.user = RC_CLIENT_USER_STATE_LOGGED_IN;
    client->state.offline = 1;

    rc_mutex_unlock(&client->state.mutex);
  }

  RC_CLIENT_LOG_INFO(client, "Loaded credentials from cache, entering offline mode");
  return 1;
}

/* ---- Game Definition Cache ---- */

void rc_client_offline_cache_game_data(rc_client_t* client, const char* username,
    const char* game_hash, const uint8_t* response_body, uint32_t response_size) {
  rc_offline_cache_header_t header;
  uint8_t* buffer;
  uint32_t total_size;
  char filename[128];

  if (!client->callbacks.write_storage || !response_body || response_size == 0)
    return;

  total_size = (uint32_t)sizeof(header) + response_size;
  /* Allocate a single buffer for header + data */
  buffer = (uint8_t*)malloc(total_size);
  if (!buffer)
    return;

  memset(&header, 0, sizeof(header));
  header.magic = RC_OFFLINE_CACHE_MAGIC;
  header.version = RC_OFFLINE_CACHE_VERSION;
  header.timestamp = (uint64_t)time(NULL);
  header.data_size = response_size;
  header.crc32 = rc_offline_crc32(response_body, response_size);

  memcpy(buffer, &header, sizeof(header));
  memcpy(buffer + sizeof(header), response_body, response_size);

  rc_offline_build_filename(filename, sizeof(filename), username, game_hash, "patch.bin");
  client->callbacks.write_storage(filename, buffer, total_size, client);

  free(buffer);
}

int rc_client_offline_load_game_data(rc_client_t* client, const char* username,
    const char* game_hash, uint8_t* buffer, uint32_t buffer_size) {
  rc_offline_cache_header_t header;
  char filename[128];
  int bytes_read;
  uint32_t expected_crc;

  if (!client->callbacks.read_storage)
    return 0;

  rc_offline_build_filename(filename, sizeof(filename), username, game_hash, "patch.bin");
  bytes_read = client->callbacks.read_storage(filename, buffer, buffer_size, client);

  if (bytes_read <= (int)sizeof(header))
    return 0;

  memcpy(&header, buffer, sizeof(header));

  if (header.magic != RC_OFFLINE_CACHE_MAGIC || header.version != RC_OFFLINE_CACHE_VERSION)
    return 0;

  if ((uint32_t)bytes_read < sizeof(header) + header.data_size)
    return 0;

  expected_crc = rc_offline_crc32(buffer + sizeof(header), header.data_size);
  if (header.crc32 != expected_crc) {
    RC_CLIENT_LOG_WARN(client, "Game data cache CRC mismatch, ignoring");
    return 0;
  }

  /* Return just the payload size (caller gets data at buffer + sizeof(header)) */
  return (int)header.data_size;
}

/* ---- Unlock Status Cache ---- */

void rc_client_offline_cache_unlocks(rc_client_t* client, const char* username,
    const char* game_hash, const rc_api_start_session_response_t* response) {
  rc_offline_cache_header_t header;
  uint8_t* buffer;
  uint8_t* ptr;
  uint32_t total_entries, data_size, total_size, i;
  char filename[128];

  if (!client->callbacks.write_storage)
    return;

  total_entries = response->num_hardcore_unlocks + response->num_unlocks;
  data_size = total_entries * RC_OFFLINE_UNLOCK_ENTRY_SIZE;
  total_size = (uint32_t)sizeof(header) + data_size;

  buffer = (uint8_t*)malloc(total_size);
  if (!buffer)
    return;

  ptr = buffer + sizeof(header);

  /* Write hardcore unlocks */
  for (i = 0; i < response->num_hardcore_unlocks; i++) {
    uint32_t id = response->hardcore_unlocks[i].achievement_id;
    uint64_t when = (uint64_t)response->hardcore_unlocks[i].when;
    memcpy(ptr, &id, 4); ptr += 4;
    memcpy(ptr, &when, 8); ptr += 8;
    *ptr = 1; ptr++;  /* hardcore = 1 */
    memset(ptr, 0, 3); ptr += 3;
  }

  /* Write softcore unlocks */
  for (i = 0; i < response->num_unlocks; i++) {
    uint32_t id = response->unlocks[i].achievement_id;
    uint64_t when = (uint64_t)response->unlocks[i].when;
    memcpy(ptr, &id, 4); ptr += 4;
    memcpy(ptr, &when, 8); ptr += 8;
    *ptr = 0; ptr++;  /* hardcore = 0 */
    memset(ptr, 0, 3); ptr += 3;
  }

  memset(&header, 0, sizeof(header));
  header.magic = RC_OFFLINE_CACHE_MAGIC;
  header.version = RC_OFFLINE_CACHE_VERSION;
  header.timestamp = (uint64_t)time(NULL);
  header.data_size = data_size;
  header.crc32 = rc_offline_crc32(buffer + sizeof(header), data_size);

  memcpy(buffer, &header, sizeof(header));

  rc_offline_build_filename(filename, sizeof(filename), username, game_hash, "unlocks.bin");
  client->callbacks.write_storage(filename, buffer, total_size, client);

  free(buffer);
}

int rc_client_offline_load_unlocks(rc_client_t* client, const char* username,
    const char* game_hash, uint8_t* buffer, uint32_t buffer_size) {
  rc_offline_cache_header_t header;
  char filename[128];
  int bytes_read;
  uint32_t expected_crc;

  if (!client->callbacks.read_storage)
    return 0;

  rc_offline_build_filename(filename, sizeof(filename), username, game_hash, "unlocks.bin");
  bytes_read = client->callbacks.read_storage(filename, buffer, buffer_size, client);

  if (bytes_read <= (int)sizeof(header))
    return 0;

  memcpy(&header, buffer, sizeof(header));

  if (header.magic != RC_OFFLINE_CACHE_MAGIC || header.version != RC_OFFLINE_CACHE_VERSION)
    return 0;

  if ((uint32_t)bytes_read < sizeof(header) + header.data_size)
    return 0;

  expected_crc = rc_offline_crc32(buffer + sizeof(header), header.data_size);
  if (header.crc32 != expected_crc) {
    RC_CLIENT_LOG_WARN(client, "Unlock cache CRC mismatch, ignoring");
    return 0;
  }

  return (int)header.data_size;
}

/* ---- Persistent Unlock Queue ---- */

void rc_client_offline_queue_append(rc_client_t* client,
    const rc_offline_queue_entry_t* entry) {
  rc_offline_queue_header_t header;
  rc_offline_queue_entry_t entries[RC_OFFLINE_QUEUE_MAX_ENTRIES];
  char filename[128];
  uint8_t* buffer;
  uint32_t max_read_size, total_size;
  int bytes_read;
  uint16_t count = 0;

  if (!client->callbacks.write_storage || !client->callbacks.read_storage)
    return;

  rc_offline_build_filename(filename, sizeof(filename),
      client->user.username, NULL, "queue.bin");

  /* Read the full existing queue in a single call (bounded by the maximum possible size). */
  max_read_size = (uint32_t)sizeof(header) +
      RC_OFFLINE_QUEUE_MAX_ENTRIES * (uint32_t)sizeof(rc_offline_queue_entry_t);
  buffer = (uint8_t*)malloc(max_read_size);
  if (buffer) {
    bytes_read = client->callbacks.read_storage(filename, buffer, max_read_size, client);
    if (bytes_read >= (int)sizeof(header)) {
      memcpy(&header, buffer, sizeof(header));
      if (header.magic == RC_OFFLINE_QUEUE_MAGIC &&
          header.version == RC_OFFLINE_QUEUE_VERSION &&
          header.entry_count <= RC_OFFLINE_QUEUE_MAX_ENTRIES &&
          bytes_read >= (int)(sizeof(header) + header.entry_count * sizeof(rc_offline_queue_entry_t))) {
        uint32_t expected_crc = rc_offline_crc32(buffer + sizeof(header),
            header.entry_count * (uint32_t)sizeof(rc_offline_queue_entry_t));
        if (header.crc32 == expected_crc) {
          count = header.entry_count;
          memcpy(entries, buffer + sizeof(header), count * sizeof(rc_offline_queue_entry_t));
        }
      }
    }
    free(buffer);
  }

  if (count >= RC_OFFLINE_QUEUE_MAX_ENTRIES) {
    RC_CLIENT_LOG_WARN(client, "Offline queue is full, dropping achievement unlock");
    return;
  }

  /* Append new entry */
  memcpy(&entries[count], entry, sizeof(rc_offline_queue_entry_t));
  count++;

  /* Write updated queue */
  total_size = (uint32_t)sizeof(header) + count * (uint32_t)sizeof(rc_offline_queue_entry_t);
  buffer = (uint8_t*)malloc(total_size);
  if (!buffer)
    return;

  memset(&header, 0, sizeof(header));
  header.magic = RC_OFFLINE_QUEUE_MAGIC;
  header.version = RC_OFFLINE_QUEUE_VERSION;
  header.entry_count = count;
  header.crc32 = rc_offline_crc32((const uint8_t*)entries,
      count * (uint32_t)sizeof(rc_offline_queue_entry_t));

  memcpy(buffer, &header, sizeof(header));
  memcpy(buffer + sizeof(header), entries,
      count * sizeof(rc_offline_queue_entry_t));

  client->callbacks.write_storage(filename, buffer, total_size, client);
  free(buffer);

  RC_CLIENT_LOG_INFO_FORMATTED(client, "Queued offline unlock for achievement %u", entry->achievement_id);
}

int rc_client_offline_queue_load(rc_client_t* client,
    rc_offline_queue_entry_t* entries, uint32_t max_entries) {
  rc_offline_queue_header_t header;
  char filename[128];
  uint8_t* buffer;
  int bytes_read;
  uint32_t total_size, expected_crc;

  if (!client->callbacks.read_storage)
    return 0;

  rc_offline_build_filename(filename, sizeof(filename),
      client->user.username, NULL, "queue.bin");

  /* Read header first */
  bytes_read = client->callbacks.read_storage(filename, (uint8_t*)&header,
      (uint32_t)sizeof(header), client);
  if (bytes_read != (int)sizeof(header))
    return 0;

  if (header.magic != RC_OFFLINE_QUEUE_MAGIC || header.version != RC_OFFLINE_QUEUE_VERSION)
    return 0;

  if (header.entry_count == 0)
    return 0;

  if (header.entry_count > RC_OFFLINE_QUEUE_MAX_ENTRIES)
    header.entry_count = RC_OFFLINE_QUEUE_MAX_ENTRIES;

  if (header.entry_count > max_entries)
    header.entry_count = (uint16_t)max_entries;

  /* Read full file */
  total_size = (uint32_t)sizeof(header) + header.entry_count * (uint32_t)sizeof(rc_offline_queue_entry_t);
  buffer = (uint8_t*)malloc(total_size);
  if (!buffer)
    return 0;

  bytes_read = client->callbacks.read_storage(filename, buffer, total_size, client);
  if (bytes_read != (int)total_size) {
    free(buffer);
    return 0;
  }

  expected_crc = rc_offline_crc32(buffer + sizeof(header),
      header.entry_count * (uint32_t)sizeof(rc_offline_queue_entry_t));
  if (header.crc32 != expected_crc) {
    RC_CLIENT_LOG_WARN(client, "Queue file CRC mismatch, ignoring");
    free(buffer);
    return 0;
  }

  memcpy(entries, buffer + sizeof(header),
      header.entry_count * sizeof(rc_offline_queue_entry_t));
  free(buffer);

  return (int)header.entry_count;
}

void rc_client_offline_queue_clear(rc_client_t* client) {
  rc_offline_queue_header_t header;
  char filename[128];

  if (!client->callbacks.write_storage)
    return;

  rc_offline_build_filename(filename, sizeof(filename),
      client->user.username, NULL, "queue.bin");

  /* Write an empty queue */
  memset(&header, 0, sizeof(header));
  header.magic = RC_OFFLINE_QUEUE_MAGIC;
  header.version = RC_OFFLINE_QUEUE_VERSION;
  header.entry_count = 0;
  header.crc32 = 0; /* CRC of zero bytes is 0x00000000 */

  client->callbacks.write_storage(filename, (const uint8_t*)&header,
      (uint32_t)sizeof(header), client);
}
