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

  /* NES */
  TEST(test_hash_nes_32k);
  TEST(test_hash_nes_32k_with_header);
  TEST(test_hash_nes_256k);
  TEST(test_hash_fds_two_sides);
  TEST(test_hash_fds_two_sides_with_header);

  TEST(test_hash_nes_file_32k);
  TEST(test_hash_nes_file_iterator_32k);
  TEST(test_hash_nes_iterator_32k);

  TEST_SUITE_END();
}
