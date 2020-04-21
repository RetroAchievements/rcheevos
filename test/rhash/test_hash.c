#include "rhash.h"

#include "../test_framework.h"
#include "data.h"

#include <stdlib.h>


typedef struct mock_file
{
  const char* path;
  uint8_t* data;
  size_t size;
  size_t pos;
} mock_file;

static mock_file mock_file_instance;

static void* mock_file_open(const char* path)
{
  if (strcmp(path, mock_file_instance.path) == 0)
    return &mock_file_instance;

  return NULL;
}

static void mock_file_seek(void* file_handle, size_t offset, int origin)
{
  mock_file* file = (mock_file*)file_handle;
  switch (origin)
  {
    case SEEK_SET:
      file->pos = offset;
      break;
    case SEEK_CUR:
      file->pos += offset;
      break;
    case SEEK_END:
      file->pos = file->size - offset;
      break;
  }
}

static size_t mock_file_tell(void* file_handle)
{
  mock_file* file = (mock_file*)file_handle;
  return file->pos;
}

static size_t mock_file_read(void* file_handle, void* buffer, size_t count)
{
  mock_file* file = (mock_file*)file_handle;
  size_t remaining = file->size - file->pos;
  if (count > remaining)
    count = remaining;

  if (count > 0)
  {
    memcpy(buffer, &file->data[file->pos], count);
    file->pos += count;
  }

  return count;
}

static void mock_file_close(void* file_handle)
{
}

static void init_mock_filereader()
{
  struct rc_hash_filereader reader;
  reader.open = mock_file_open;
  reader.seek = mock_file_seek;
  reader.tell = mock_file_tell;
  reader.read = mock_file_read;
  reader.close = mock_file_close;

  rc_hash_init_custom_filereader(&reader);
}

static int hash_mock_file(const char* filename, char hash[33], int console_id, uint8_t* buffer, size_t buffer_size)
{
  mock_file_instance.path = filename;
  mock_file_instance.data = buffer;
  mock_file_instance.size = buffer_size;
  mock_file_instance.pos = 0;

  return rc_hash_generate_from_file(hash, console_id, filename);
}

static void iterate_mock_file(struct rc_hash_iterator *iterator, const char* filename, uint8_t* buffer, size_t buffer_size)
{
  mock_file_instance.path = filename;
  mock_file_instance.data = buffer;
  mock_file_instance.size = buffer_size;
  mock_file_instance.pos = 0;

  rc_hash_initialize_iterator(iterator, filename, NULL, 0);
}

/* ========================================================================= */

static void test_hash_full_file(int console_id, const char* filename, size_t size, const char* expected_md5)
{
  uint8_t* image = generate_generic_file(size);
  char hash_buffer[33], hash_file[33], hash_iterator[33];

  /* test full buffer hash */
  int result_buffer = rc_hash_generate_from_buffer(hash_buffer, console_id, image, size);

  /* test full file hash */
  int result_file = hash_mock_file(filename, hash_file, console_id, image, size);

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  iterate_mock_file(&iterator, filename, image, size);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_buffer, 1);
  ASSERT_STR_EQUALS(hash_buffer, expected_md5);

  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

/* ========================================================================= */

static void test_hash_nes_32k()
{
  size_t image_size;
  uint8_t* image = generate_nes_file(32, 0, &image_size);
  char hash[33];
  int result = rc_hash_generate_from_buffer(hash, RC_CONSOLE_NINTENDO, image, image_size);
  free(image);

  ASSERT_NUM_EQUALS(result, 1);
  ASSERT_STR_EQUALS(hash, "6a2305a2b6675a97ff792709be1ca857");
  ASSERT_NUM_EQUALS(image_size, 32768);
}

static void test_hash_nes_32k_with_header()
{
  size_t image_size;
  uint8_t* image = generate_nes_file(32, 1, &image_size);
  char hash[33];
  int result = rc_hash_generate_from_buffer(hash, RC_CONSOLE_NINTENDO, image, image_size);
  free(image);

  /* NOTE: expectation is that this hash matches the hash in test_hash_nes_32k */
  ASSERT_NUM_EQUALS(result, 1);
  ASSERT_STR_EQUALS(hash, "6a2305a2b6675a97ff792709be1ca857");
  ASSERT_NUM_EQUALS(image_size, 32768 + 16);
}

static void test_hash_nes_256k()
{
  size_t image_size;
  uint8_t* image = generate_nes_file(256, 0, &image_size);
  char hash[33];
  int result = rc_hash_generate_from_buffer(hash, RC_CONSOLE_NINTENDO, image, image_size);
  free(image);

  ASSERT_NUM_EQUALS(result, 1);
  ASSERT_STR_EQUALS(hash, "545d527301b8ae148153988d6c4fcb84");
  ASSERT_NUM_EQUALS(image_size, 262144);
}

static void test_hash_fds_two_sides()
{
  size_t image_size;
  uint8_t* image = generate_fds_file(2, 0, &image_size);
  char hash[33];
  int result = rc_hash_generate_from_buffer(hash, RC_CONSOLE_NINTENDO, image, image_size);
  free(image);

  ASSERT_NUM_EQUALS(result, 1);
  ASSERT_STR_EQUALS(hash, "fd770d4d34c00760fabda6ad294a8f0b");
  ASSERT_NUM_EQUALS(image_size, 65500 * 2);
}

static void test_hash_fds_two_sides_with_header()
{
  size_t image_size;
  uint8_t* image = generate_fds_file(2, 1, &image_size);
  char hash[33];
  int result = rc_hash_generate_from_buffer(hash, RC_CONSOLE_NINTENDO, image, image_size);
  free(image);

  /* NOTE: expectation is that this hash matches the hash in test_hash_fds_two_sides */
  ASSERT_NUM_EQUALS(result, 1);
  ASSERT_STR_EQUALS(hash, "fd770d4d34c00760fabda6ad294a8f0b");
  ASSERT_NUM_EQUALS(image_size, 65500 * 2 + 16);
}

static void test_hash_nes_file_32k()
{
  size_t image_size;
  uint8_t* image = generate_nes_file(32, 0, &image_size);
  char hash[33];
  int result = hash_mock_file("test.nes", hash, RC_CONSOLE_NINTENDO, image, image_size);
  free(image);

  ASSERT_NUM_EQUALS(result, 1);
  ASSERT_STR_EQUALS(hash, "6a2305a2b6675a97ff792709be1ca857");
  ASSERT_NUM_EQUALS(image_size, 32768);
}

static void test_hash_nes_iterator_32k()
{
    size_t image_size;
    uint8_t* image = generate_nes_file(32, 0, &image_size);
    char hash1[33], hash2[33];
    int result1, result2;
    struct rc_hash_iterator iterator;
    iterate_mock_file(&iterator, "test.nes", image, image_size);
    result1 = rc_hash_iterate(hash1, &iterator);
    result2 = rc_hash_iterate(hash2, &iterator);
    rc_hash_destroy_iterator(&iterator);
    free(image);

    ASSERT_NUM_EQUALS(result1, 1);
    ASSERT_STR_EQUALS(hash1, "6a2305a2b6675a97ff792709be1ca857");

    ASSERT_NUM_EQUALS(result2, 0);
    ASSERT_STR_EQUALS(hash2, "");
}

static void test_hash_nes_file_iterator_32k()
{
  size_t image_size;
  uint8_t* image = generate_nes_file(32, 0, &image_size);
  char hash1[33], hash2[33];
  int result1, result2;
  struct rc_hash_iterator iterator;
  rc_hash_initialize_iterator(&iterator, "test.nes", image, image_size);
  result1 = rc_hash_iterate(hash1, &iterator);
  result2 = rc_hash_iterate(hash2, &iterator);
  rc_hash_destroy_iterator(&iterator);
  free(image);

  ASSERT_NUM_EQUALS(result1, 1);
  ASSERT_STR_EQUALS(hash1, "6a2305a2b6675a97ff792709be1ca857");

  ASSERT_NUM_EQUALS(result2, 0);
  ASSERT_STR_EQUALS(hash2, "");
}

void test_hash(void) {
  TEST_SUITE_BEGIN();

  init_mock_filereader();

  /* Apple II */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_APPLE_II, "test.dsk", 143360, "88be638f4d78b4072109e55f13e8a0ac");

  /* Atari 2600 */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_ATARI_2600, "test.bin", 2048, "02c3f2fa186388ba8eede9147fb431c4");

  /* Atari 7800 - includes 128-byte header */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_ATARI_7800, "test.a78", 16384 + 128, "f063cca169b2e49afc339a253a9abadb");

  /* Atari Jaguar */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_ATARI_JAGUAR, "test.jag", 0x400000, "a247ec8a8c42e18fcb80702dfadac14b");

  /* Colecovision */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_COLECOVISION, "test.col", 16384, "455f07d8500f3fabc54906737866167f");

  /* Gameboy */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_GAMEBOY, "test.gb", 131072, "a0f425b23200568132ba76b2405e3933");

  /* Gameboy Color */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_GAMEBOY_COLOR, "test.gbc", 2097152, "cf86acf519625a25a17b1246975e90ae");

  /* Gameboy Advance */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_GAMEBOY_COLOR, "test.gba", 4194304, "a247ec8a8c42e18fcb80702dfadac14b");

  /* Game Gear */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_GAME_GEAR, "test.gg", 524288, "68f0f13b598e0b66461bc578375c3888");

  /* Intellivision */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_INTELLIVISION, "test.bin", 8192, "ce1127f881b40ce6a67ecefba50e2835");

  /* Master System */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_MASTER_SYSTEM, "test.sms", 131072, "a0f425b23200568132ba76b2405e3933");

  /* Mega Drive */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_MEGA_DRIVE, "test.md", 1048576, "da9461b3b0f74becc3ccf6c2a094c516");

  /* Neo Geo Pocket */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_NEOGEO_POCKET, "test.ngc", 2097152, "cf86acf519625a25a17b1246975e90ae");

  /* NES */
  TEST(test_hash_nes_32k);
  TEST(test_hash_nes_32k_with_header);
  TEST(test_hash_nes_256k);
  TEST(test_hash_fds_two_sides);
  TEST(test_hash_fds_two_sides_with_header);

  TEST(test_hash_nes_file_32k);
  TEST(test_hash_nes_file_iterator_32k);
  TEST(test_hash_nes_iterator_32k);

  /* Nintendo 64 */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_NINTENDO_64, "test.n64", 16777216, "d7a0af7f7e89aca1ca75d9c07ce1860f");

  /* Oric (no fixed file size) */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_ORIC, "test.tap", 18119, "953a2baa3232c63286aeae36b2172cef");

  /* PC-8800 */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_PC8800, "test.d88", 348288, "8cca4121bf87200f45e91b905a9f5afd");

  /* Pokemon Mini */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_POKEMON_MINI, "test.min", 524288, "68f0f13b598e0b66461bc578375c3888");

  /* Sega 32X */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_SEGA_32X, "test.bin", 3145728, "07d733f252896ec41b4fd521fe610e2c");

  /* SG-1000 */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_SG1000, "test.sg", 32768, "6a2305a2b6675a97ff792709be1ca857");

  /* Vectrex */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_SG1000, "test.vec", 4096, "572686c3a073162e4ec6eff86e6f6e3a");

  /* VirtualBoy */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_SG1000, "test.vb", 524288, "68f0f13b598e0b66461bc578375c3888");

  /* WonderSwan */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_WONDERSWAN, "test.ws", 524288, "68f0f13b598e0b66461bc578375c3888");
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_WONDERSWAN, "test.wsc", 4194304, "a247ec8a8c42e18fcb80702dfadac14b");

  TEST_SUITE_END();
}
