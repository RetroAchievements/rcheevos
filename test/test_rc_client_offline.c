#include "rc_client.h"
#include "../src/rc_client_internal.h"
#include "../src/rc_client_offline.h"

#include "test_framework.h"

#include <string.h>
#include <stdlib.h>

TEST_FRAMEWORK_DECLARATIONS()

/* ---- Mock storage callbacks ---- */

#define MOCK_STORAGE_MAX_FILES 16
#define MOCK_STORAGE_MAX_SIZE (512 * 1024)

typedef struct {
  char filename[128];
  uint8_t data[MOCK_STORAGE_MAX_SIZE];
  uint32_t size;
} mock_storage_file_t;

static mock_storage_file_t g_mock_files[MOCK_STORAGE_MAX_FILES];
static int g_mock_file_count = 0;

static void mock_storage_reset(void) {
  g_mock_file_count = 0;
  memset(g_mock_files, 0, sizeof(g_mock_files));
}

static mock_storage_file_t* mock_storage_find(const char* filename) {
  int i;
  for (i = 0; i < g_mock_file_count; i++) {
    if (strcmp(g_mock_files[i].filename, filename) == 0)
      return &g_mock_files[i];
  }
  return NULL;
}

static int RC_CCONV mock_write_storage(const char* filename, const uint8_t* data,
    uint32_t data_size, rc_client_t* client) {
  mock_storage_file_t* file = mock_storage_find(filename);
  (void)client;

  if (!file) {
    if (g_mock_file_count >= MOCK_STORAGE_MAX_FILES)
      return -1;
    file = &g_mock_files[g_mock_file_count++];
    strncpy(file->filename, filename, sizeof(file->filename) - 1);
  }

  if (data_size > MOCK_STORAGE_MAX_SIZE)
    return -1;

  memcpy(file->data, data, data_size);
  file->size = data_size;
  return (int)data_size;
}

static int RC_CCONV mock_read_storage(const char* filename, uint8_t* buffer,
    uint32_t buffer_size, rc_client_t* client) {
  mock_storage_file_t* file = mock_storage_find(filename);
  uint32_t to_read;
  (void)client;

  if (!file)
    return 0; /* file not found */

  to_read = file->size;
  if (to_read > buffer_size)
    to_read = buffer_size;

  memcpy(buffer, file->data, to_read);
  return (int)to_read;
}

/* ---- Mock time callback ---- */

static rc_clock_t g_mock_time = 1000000;

static rc_clock_t RC_CCONV mock_get_time(const rc_client_t* client) {
  (void)client;
  return g_mock_time;
}

/* dummy callbacks for rc_client_create */
static uint32_t RC_CCONV mock_read_memory(uint32_t address, uint8_t* buffer, uint32_t num_bytes, rc_client_t* client) {
  (void)address; (void)buffer; (void)num_bytes; (void)client;
  return 0;
}

static void RC_CCONV mock_server_call(const rc_api_request_t* request, rc_client_server_callback_t callback,
    void* callback_data, rc_client_t* client) {
  (void)request; (void)callback; (void)callback_data; (void)client;
  /* do nothing - we don't actually make server calls in these tests */
}

/* ---- Helper to create a minimal client ---- */

static rc_client_t* create_test_client(void) {
  rc_client_t* client = rc_client_create(mock_read_memory, mock_server_call);
  rc_client_set_storage_callbacks(client, mock_read_storage, mock_write_storage);
  rc_client_set_get_time_millisecs_function(client, mock_get_time);
  return client;
}

/* ---- CRC32 tests ---- */

static void test_crc32_empty(void) {
  uint32_t crc = rc_offline_crc32(NULL, 0);
  /* CRC32 of empty data should be 0 */
  ASSERT_NUM_EQUALS(crc, 0);
}

static void test_crc32_known_value(void) {
  const uint8_t data[] = "Hello, World!";
  uint32_t crc = rc_offline_crc32(data, 13);
  /* known CRC32 for "Hello, World!" */
  ASSERT_NUM_EQUALS(crc, 0xEC4AC3D0);
}

static void test_crc32_deterministic(void) {
  const uint8_t data[] = {0x01, 0x02, 0x03, 0x04};
  uint32_t crc1 = rc_offline_crc32(data, 4);
  uint32_t crc2 = rc_offline_crc32(data, 4);
  ASSERT_NUM_EQUALS(crc1, crc2);
}

/* ---- Credential cache tests ---- */

static void test_credential_cache_roundtrip(void) {
  rc_client_t* client = create_test_client();
  mock_storage_reset();

  rc_client_offline_cache_credentials(client, "TestUser", "abc123token",
      "TestDisplay", 1000, 500, "http://avatar.png");

  /* verify file was written */
  ASSERT_TRUE(mock_storage_find("TestUser_credentials.bin") != NULL);

  /* clear client user data */
  memset(&client->user, 0, sizeof(client->user));

  /* load credentials */
  ASSERT_NUM_EQUALS(rc_client_offline_load_credentials(client, "TestUser"), 1);
  ASSERT_STR_EQUALS(client->user.username, "TestUser");
  ASSERT_STR_EQUALS(client->user.token, "abc123token");
  ASSERT_STR_EQUALS(client->user.display_name, "TestDisplay");
  ASSERT_NUM_EQUALS(client->user.score, 1000);
  ASSERT_NUM_EQUALS(client->user.score_softcore, 500);
  ASSERT_NUM_EQUALS(client->state.offline, 1);
  ASSERT_NUM_EQUALS(client->state.user, RC_CLIENT_USER_STATE_LOGGED_IN);

  rc_client_destroy(client);
}

static void test_credential_cache_missing_file(void) {
  rc_client_t* client = create_test_client();
  mock_storage_reset();

  ASSERT_NUM_EQUALS(rc_client_offline_load_credentials(client, "NonExistentUser"), 0);

  rc_client_destroy(client);
}

static void test_credential_cache_corrupted(void) {
  rc_client_t* client = create_test_client();
  mock_storage_reset();

  /* write valid credentials first */
  rc_client_offline_cache_credentials(client, "TestUser", "token",
      "Display", 100, 50, "http://avatar.png");

  /* corrupt the file */
  {
    mock_storage_file_t* file = mock_storage_find("TestUser_credentials.bin");
    ASSERT_TRUE(file != NULL);
    file->data[10] ^= 0xFF; /* flip some bits */
  }

  /* load should fail due to CRC mismatch */
  ASSERT_NUM_EQUALS(rc_client_offline_load_credentials(client, "TestUser"), 0);

  rc_client_destroy(client);
}

static void test_credential_cache_no_callbacks(void) {
  rc_client_t* client = rc_client_create(mock_read_memory, mock_server_call);
  /* don't set storage callbacks */

  /* should be a no-op, not crash */
  rc_client_offline_cache_credentials(client, "TestUser", "token",
      "Display", 100, 50, "http://avatar.png");

  ASSERT_NUM_EQUALS(rc_client_offline_load_credentials(client, "TestUser"), 0);

  rc_client_destroy(client);
}

/* ---- Game data cache tests ---- */

static void test_game_data_cache_roundtrip(void) {
  rc_client_t* client = create_test_client();
  uint8_t read_buffer[4096];
  int payload_size;
  const char* test_json = "{\"Success\":true,\"GameId\":1234}";
  mock_storage_reset();

  /* populate username for filename generation */
  client->user.username = "TestUser";

  rc_client_offline_cache_game_data(client, "TestUser", "abc123hash",
      (const uint8_t*)test_json, (uint32_t)strlen(test_json));

  ASSERT_TRUE(mock_storage_find("TestUser_abc123hash_patch.bin") != NULL);

  payload_size = rc_client_offline_load_game_data(client, "TestUser", "abc123hash",
      read_buffer, sizeof(read_buffer));

  ASSERT_NUM_EQUALS(payload_size, (int)strlen(test_json));

  /* compare the payload (after the header) */
  {
    rc_offline_cache_header_t header;
    memcpy(&header, read_buffer, sizeof(header));
    ASSERT_NUM_EQUALS(memcmp(read_buffer + sizeof(header), test_json, strlen(test_json)), 0);
  }

  rc_client_destroy(client);
}

static void test_game_data_cache_missing(void) {
  rc_client_t* client = create_test_client();
  uint8_t read_buffer[4096];
  mock_storage_reset();

  int payload_size = rc_client_offline_load_game_data(client, "TestUser", "nonexistent",
      read_buffer, sizeof(read_buffer));

  ASSERT_NUM_EQUALS(payload_size, 0);

  rc_client_destroy(client);
}

/* ---- Queue tests ---- */

static void test_queue_append_and_load(void) {
  rc_client_t* client = create_test_client();
  rc_offline_queue_entry_t entry;
  rc_offline_queue_entry_t loaded[RC_OFFLINE_QUEUE_MAX_ENTRIES];
  int count;
  mock_storage_reset();

  client->user.username = "TestUser";

  memset(&entry, 0, sizeof(entry));
  entry.achievement_id = 12345;
  entry.unlock_unix_time = 1700000000;
  entry.hardcore = 0;
  entry.entry_type = 0;
  strncpy(entry.game_hash, "abcdef1234567890", sizeof(entry.game_hash) - 1);

  rc_client_offline_queue_append(client, &entry);

  count = rc_client_offline_queue_load(client, loaded, RC_OFFLINE_QUEUE_MAX_ENTRIES);
  ASSERT_NUM_EQUALS(count, 1);
  ASSERT_NUM_EQUALS(loaded[0].achievement_id, 12345);
  ASSERT_NUM_EQUALS(loaded[0].unlock_unix_time, 1700000000);
  ASSERT_NUM_EQUALS(loaded[0].hardcore, 0);
  ASSERT_STR_EQUALS(loaded[0].game_hash, "abcdef1234567890");

  rc_client_destroy(client);
}

static void test_queue_append_multiple(void) {
  rc_client_t* client = create_test_client();
  rc_offline_queue_entry_t entry;
  rc_offline_queue_entry_t loaded[RC_OFFLINE_QUEUE_MAX_ENTRIES];
  int count;
  mock_storage_reset();

  client->user.username = "TestUser";

  memset(&entry, 0, sizeof(entry));
  entry.entry_type = 0;
  strncpy(entry.game_hash, "hash1", sizeof(entry.game_hash) - 1);

  entry.achievement_id = 100;
  entry.unlock_unix_time = 1700000000;
  rc_client_offline_queue_append(client, &entry);

  entry.achievement_id = 200;
  entry.unlock_unix_time = 1700000100;
  rc_client_offline_queue_append(client, &entry);

  entry.achievement_id = 300;
  entry.unlock_unix_time = 1700000200;
  rc_client_offline_queue_append(client, &entry);

  count = rc_client_offline_queue_load(client, loaded, RC_OFFLINE_QUEUE_MAX_ENTRIES);
  ASSERT_NUM_EQUALS(count, 3);
  ASSERT_NUM_EQUALS(loaded[0].achievement_id, 100);
  ASSERT_NUM_EQUALS(loaded[1].achievement_id, 200);
  ASSERT_NUM_EQUALS(loaded[2].achievement_id, 300);

  rc_client_destroy(client);
}

static void test_queue_clear(void) {
  rc_client_t* client = create_test_client();
  rc_offline_queue_entry_t entry;
  rc_offline_queue_entry_t loaded[RC_OFFLINE_QUEUE_MAX_ENTRIES];
  int count;
  mock_storage_reset();

  client->user.username = "TestUser";

  memset(&entry, 0, sizeof(entry));
  entry.achievement_id = 100;
  entry.unlock_unix_time = 1700000000;
  rc_client_offline_queue_append(client, &entry);

  rc_client_offline_queue_clear(client);

  count = rc_client_offline_queue_load(client, loaded, RC_OFFLINE_QUEUE_MAX_ENTRIES);
  ASSERT_NUM_EQUALS(count, 0);

  rc_client_destroy(client);
}

static void test_queue_corrupted(void) {
  rc_client_t* client = create_test_client();
  rc_offline_queue_entry_t entry;
  rc_offline_queue_entry_t loaded[RC_OFFLINE_QUEUE_MAX_ENTRIES];
  int count;
  mock_storage_reset();

  client->user.username = "TestUser";

  memset(&entry, 0, sizeof(entry));
  entry.achievement_id = 100;
  entry.unlock_unix_time = 1700000000;
  rc_client_offline_queue_append(client, &entry);

  /* corrupt the queue file */
  {
    mock_storage_file_t* file = mock_storage_find("TestUser_queue.bin");
    ASSERT_TRUE(file != NULL);
    file->data[20] ^= 0xFF;
  }

  count = rc_client_offline_queue_load(client, loaded, RC_OFFLINE_QUEUE_MAX_ENTRIES);
  ASSERT_NUM_EQUALS(count, 0);

  rc_client_destroy(client);
}

static void test_queue_empty_load(void) {
  rc_client_t* client = create_test_client();
  rc_offline_queue_entry_t loaded[RC_OFFLINE_QUEUE_MAX_ENTRIES];
  int count;
  mock_storage_reset();

  client->user.username = "TestUser";

  count = rc_client_offline_queue_load(client, loaded, RC_OFFLINE_QUEUE_MAX_ENTRIES);
  ASSERT_NUM_EQUALS(count, 0);

  rc_client_destroy(client);
}

static void test_queue_no_callbacks(void) {
  rc_client_t* client = rc_client_create(mock_read_memory, mock_server_call);
  rc_offline_queue_entry_t entry;
  rc_offline_queue_entry_t loaded[RC_OFFLINE_QUEUE_MAX_ENTRIES];
  int count;

  client->user.username = "TestUser";

  memset(&entry, 0, sizeof(entry));
  entry.achievement_id = 100;

  /* should not crash */
  rc_client_offline_queue_append(client, &entry);

  count = rc_client_offline_queue_load(client, loaded, RC_OFFLINE_QUEUE_MAX_ENTRIES);
  ASSERT_NUM_EQUALS(count, 0);

  rc_client_destroy(client);
}

/* ---- Offline state tests ---- */

static void test_get_offline_default(void) {
  rc_client_t* client = create_test_client();

  ASSERT_NUM_EQUALS(rc_client_get_offline(client), 0);

  rc_client_destroy(client);
}

static void test_get_offline_after_credential_load(void) {
  rc_client_t* client = create_test_client();
  mock_storage_reset();

  rc_client_offline_cache_credentials(client, "TestUser", "token",
      "Display", 100, 50, "http://avatar.png");

  /* loading credentials should set offline flag */
  rc_client_offline_load_credentials(client, "TestUser");
  ASSERT_NUM_EQUALS(rc_client_get_offline(client), 1);

  rc_client_destroy(client);
}

static void test_get_offline_null_client(void) {
  ASSERT_NUM_EQUALS(rc_client_get_offline(NULL), 0);
}

/* ---- Test runner ---- */

void test_rc_client_offline(void) {
  TEST_SUITE_BEGIN();

  /* crc32 */
  TEST(test_crc32_empty);
  TEST(test_crc32_known_value);
  TEST(test_crc32_deterministic);

  /* credential cache */
  TEST(test_credential_cache_roundtrip);
  TEST(test_credential_cache_missing_file);
  TEST(test_credential_cache_corrupted);
  TEST(test_credential_cache_no_callbacks);

  /* game data cache */
  TEST(test_game_data_cache_roundtrip);
  TEST(test_game_data_cache_missing);

  /* queue */
  TEST(test_queue_append_and_load);
  TEST(test_queue_append_multiple);
  TEST(test_queue_clear);
  TEST(test_queue_corrupted);
  TEST(test_queue_empty_load);
  TEST(test_queue_no_callbacks);

  /* offline state */
  TEST(test_get_offline_default);
  TEST(test_get_offline_after_credential_load);
  TEST(test_get_offline_null_client);

  TEST_SUITE_END();
}

int main(int argc, char* argv[]) {
  (void)argc;
  (void)argv;

  TEST_FRAMEWORK_INIT();

  test_rc_client_offline();

  TEST_FRAMEWORK_SHUTDOWN();

  return TEST_FRAMEWORK_PASSED() ? 0 : 1;
}
