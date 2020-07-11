#include "rhash.h"

#include "../test_framework.h"
#include "data.h"
#include "mock_filereader.h"

#include <stdlib.h>

static int hash_mock_file(const char* filename, char hash[33], int console_id, const uint8_t* buffer, size_t buffer_size)
{
  mock_file(0, filename, buffer, buffer_size);

  return rc_hash_generate_from_file(hash, console_id, filename);
}

static void iterate_mock_file(struct rc_hash_iterator *iterator, const char* filename, const uint8_t* buffer, size_t buffer_size)
{
  mock_file(0, filename, buffer, buffer_size);

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

static void test_hash_m3u(int console_id, const char* filename, size_t size, const char* expected_md5)
{
  uint8_t* image = generate_generic_file(size);
  char hash_file[33], hash_iterator[33];
  const char* m3u_filename = "test.m3u";

  mock_file(0, filename, image, size);
  mock_file(1, m3u_filename, (uint8_t*)filename, strlen(filename));

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, console_id, m3u_filename);

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, m3u_filename, NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_filename(int console_id, const char* path, const char* expected_md5)
{
    char hash_file[33], hash_iterator[33];

    /* test file hash */
    int result_file = rc_hash_generate_from_file(hash_file, console_id, path);

    /* test file identification from iterator */
    int result_iterator;
    struct rc_hash_iterator iterator;

    rc_hash_initialize_iterator(&iterator, path, NULL, 0);
    result_iterator = rc_hash_iterate(hash_iterator, &iterator);
    rc_hash_destroy_iterator(&iterator);

    /* validation */
    ASSERT_NUM_EQUALS(result_file, 1);
    ASSERT_STR_EQUALS(hash_file, expected_md5);

    ASSERT_NUM_EQUALS(result_iterator, 1);
    ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

/* ========================================================================= */

static void test_hash_3do_bin()
{
  size_t image_size;
  uint8_t* image = generate_3do_bin(1, 123456, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "9b2266b8f5abed9c12cce780750e88d6";

  mock_file(0, "game.bin", image, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_3DO, "game.bin");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  mock_file_size(0, 45678901); /* must be > 32MB for iterator to consider CD formats for bin */
  rc_hash_initialize_iterator(&iterator, "game.bin", NULL, 0);
  mock_file_size(0, image_size); /* change it back before doing the hashing */

  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_3do_cue()
{
  size_t image_size;
  uint8_t* image = generate_3do_bin(1, 9347, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "257d1d19365a864266b236214dbea29c";

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_3DO, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_3do_iso()
{
  size_t image_size;
  uint8_t* image = generate_3do_bin(1, 9347, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "257d1d19365a864266b236214dbea29c";

  mock_file(0, "game.iso", image, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_3DO, "game.iso");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.iso", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_3do_invalid_header()
{
  /* this is meant to simulate attempting to open a non-3DO CD. TODO: generate PSX CD */
  size_t image_size;
  uint8_t* image = generate_3do_bin(1, 12, &image_size);
  char hash_file[33];

  /* make the header not match */
  image[3] = 0x34;

  mock_file(0, "game.bin", image, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_3DO, "game.bin");

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 0);
}

static void test_hash_3do_launchme_case_insensitive()
{
  /* main executable for "Captain Quazar" is "launchme" */
  /* main executable for "Rise of the Robots" is "launchMe" */
  /* main executable for "Road Rash" is "LaunchMe" */
  /* main executable for "Sewer Shark" is "Launchme" */
  size_t image_size;
  uint8_t* image = generate_3do_bin(1, 6543, &image_size);
  char hash_file[33];
  const char* expected_md5 = "59622882e3261237e8a1e396825ae4f5";

  memcpy(&image[2048 + 0x14 + 0x48 + 0x20], "launchme", 8);
  mock_file(0, "game.bin", image, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_3DO, "game.bin");

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);
}

static void test_hash_3do_no_launchme()
{
  /* this case should not happen */
  size_t image_size;
  uint8_t* image = generate_3do_bin(1, 6543, &image_size);
  char hash_file[33];

  memcpy(&image[2048 + 0x14 + 0x48 + 0x20], "filename", 8);
  mock_file(0, "game.bin", image, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_3DO, "game.bin");

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 0);
}

static void test_hash_3do_long_directory()
{
  /* root directory for "Dragon's Lair" uses more than one sector */
  size_t image_size;
  uint8_t* image = generate_3do_bin(3, 6543, &image_size);
  char hash_file[33];
  const char* expected_md5 = "8979e876ae502e0f79218f7ff7bd8c2a";

  mock_file(0, "game.bin", image, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_3DO, "game.bin");

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);
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

/* ========================================================================= */

static void test_hash_pce_cd()
{
  size_t image_size;
  uint8_t* image = generate_pce_cd_bin(72, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "6565819195a49323e080e7539b54f251";

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PC_ENGINE, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_pce_cd_invalid_header()
{
  size_t image_size;
  uint8_t* image = generate_pce_cd_bin(72, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "bf619eac0cdf3f68d496ea9344137e8b"; /* Sega CD hash */

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  /* make the header not match */
  image[2048 + 0x24] = 0x34;

  /* test file hash (won't match) */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PC_ENGINE, "game.cue");

  /* test file identification from iterator (won't match PC-FX; will fallback to Sega CD) */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 0);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

/* ========================================================================= */

static void test_hash_pcfx()
{
  size_t image_size;
  uint8_t* image = generate_pcfx_bin(72, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "0a03af66559b8529c50c4e7788379598";

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PCFX, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_pcfx_invalid_header()
{
  size_t image_size;
  uint8_t* image = generate_pcfx_bin(72, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "ae2af724fcd27ffca04ed2dd4ac83e28"; /* Sega CD hash */

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);

  /* make the header not match */
  image[12] = 0x34;

  /* test file hash (won't match) */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PC_ENGINE, "game.cue");

  /* test file identification from iterator (won't match PC-FX; will fallback to Sega CD) */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 0);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_pcfx_pce_cd()
{
  /* Battle Heat is formatted as a PC-Engine CD */
  size_t image_size;
  uint8_t* image = generate_pce_cd_bin(72, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "6565819195a49323e080e7539b54f251";

  mock_file(0, "game.bin", image, image_size);
  mock_file(1, "game.cue", (uint8_t*)"game.bin", 8);
  mock_file(2, "game2.bin", image, image_size); /* PC-Engine CD check only applies to track 2 */

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PCFX, "game.cue");

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, "game.cue", NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}
/* ========================================================================= */

static void assert_valid_m3u(const char* disc_filename, const char* m3u_filename, const char* m3u_contents)
{
  const size_t size = 131072;
  uint8_t* image = generate_generic_file(size);
  char hash_file[33], hash_iterator[33];
  const char* expected_md5 = "a0f425b23200568132ba76b2405e3933";

  mock_file(0, disc_filename, image, size);
  mock_file(1, m3u_filename, (uint8_t*)m3u_contents, strlen(m3u_contents));

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PC8800, m3u_filename);

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, m3u_filename, NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, expected_md5);

  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, expected_md5);
}

static void test_hash_m3u_with_comments()
{
  assert_valid_m3u("test.d88", "test.m3u", 
      "#EXTM3U\r\n\r\n#EXTBYT:131072\r\ntest.d88\r\n");
}

static void test_hash_m3u_empty()
{
  char hash_file[33], hash_iterator[33];
  const char* m3u_filename = "test.m3u";
  const char* m3u_contents = "#EXTM3U\r\n\r\n#EXTBYT:131072\r\n";

  mock_file(0, m3u_filename, (uint8_t*)m3u_contents, strlen(m3u_contents));

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_PC8800, m3u_filename);

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, m3u_filename, NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* validation */
  ASSERT_NUM_EQUALS(result_file, 0);
  ASSERT_NUM_EQUALS(result_iterator, 0);
}

static void test_hash_m3u_trailing_whitespace()
{
  assert_valid_m3u("test.d88", "test.m3u", 
      "#EXTM3U  \r\n  \r\n#EXTBYT:131072  \r\ntest.d88  \t  \r\n");
}

static void test_hash_m3u_line_ending()
{
  assert_valid_m3u("test.d88", "test.m3u", 
      "#EXTM3U\n\n#EXTBYT:131072\ntest.d88\n");
}

static void test_hash_m3u_extension_case()
{
  assert_valid_m3u("test.D88", "test.M3U", 
      "#EXTM3U\r\n\r\n#EXTBYT:131072\r\ntest.D88\r\n");
}

static void test_hash_m3u_relative_path()
{
  assert_valid_m3u("folder1/folder2/test.d88", "folder1/test.m3u", 
      "#EXTM3U\r\n\r\n#EXTBYT:131072\r\nfolder2/test.d88");
}

static void test_hash_m3u_absolute_path(const char* absolute_path)
{
  char m3u_contents[128] = "#EXTM3U\r\n\r\n#EXTBYT:131072\r\n";
  strcat(m3u_contents, absolute_path);

  assert_valid_m3u(absolute_path, "relative/test.m3u", m3u_contents);
}

static void test_hash_file_without_ext()
{
  size_t image_size;
  uint8_t* image = generate_nes_file(32, 1, &image_size);
  char hash_file[33], hash_iterator[33];
  const char* filename = "test";

  mock_file(0, filename, image, image_size);

  /* test file hash */
  int result_file = rc_hash_generate_from_file(hash_file, RC_CONSOLE_NINTENDO, filename);

  /* test file identification from iterator */
  int result_iterator;
  struct rc_hash_iterator iterator;

  rc_hash_initialize_iterator(&iterator, filename, NULL, 0);
  result_iterator = rc_hash_iterate(hash_iterator, &iterator);
  rc_hash_destroy_iterator(&iterator);

  /* cleanup */
  free(image);

  /* validation */

  /* specifying a console will use the appropriate hasher */
  ASSERT_NUM_EQUALS(result_file, 1);
  ASSERT_STR_EQUALS(hash_file, "6a2305a2b6675a97ff792709be1ca857");

  /* no extension will use the default full file iterator, so hash should include header */
  ASSERT_NUM_EQUALS(result_iterator, 1);
  ASSERT_STR_EQUALS(hash_iterator, "64b131c5c7fec32985d9c99700babb7e");
}

/* ========================================================================= */

void test_hash(void) {
  TEST_SUITE_BEGIN();

  init_mock_filereader();
  init_mock_cdreader();

  /* 3DO */
  TEST(test_hash_3do_bin);
  TEST(test_hash_3do_cue);
  TEST(test_hash_3do_iso);
  TEST(test_hash_3do_invalid_header);
  TEST(test_hash_3do_launchme_case_insensitive);
  TEST(test_hash_3do_no_launchme);
  TEST(test_hash_3do_long_directory);

  /* Apple II */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_APPLE_II, "test.dsk", 143360, "88be638f4d78b4072109e55f13e8a0ac");

  /* Arcade */
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "game.zip", "c8d46d341bea4fd5bff866a65ff8aea9");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "game.7z", "c8d46d341bea4fd5bff866a65ff8aea9");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/game.zip", "c8d46d341bea4fd5bff866a65ff8aea9");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "\\game.zip", "c8d46d341bea4fd5bff866a65ff8aea9");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "roms\\game.zip", "c8d46d341bea4fd5bff866a65ff8aea9");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "C:\\roms\\game.zip", "c8d46d341bea4fd5bff866a65ff8aea9");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/roms/game.zip", "c8d46d341bea4fd5bff866a65ff8aea9");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/games/game.zip", "c8d46d341bea4fd5bff866a65ff8aea9");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/roms/game.7z", "c8d46d341bea4fd5bff866a65ff8aea9");

  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/nes_game.zip", "9b7aad36b365712fc93728088de4c209");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/nes/game.zip", "9b7aad36b365712fc93728088de4c209");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "C:\\roms\\nes\\game.zip", "9b7aad36b365712fc93728088de4c209");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "nes\\game.zip", "9b7aad36b365712fc93728088de4c209");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/snes/game.zip", "c8d46d341bea4fd5bff866a65ff8aea9");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/nes2/game.zip", "c8d46d341bea4fd5bff866a65ff8aea9");

  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/coleco/game.zip", "c546f63ae7de98add4b9f221a4749260");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/msx/game.zip", "59ab85f6b56324fd81b4e324b804c29f");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/pce/game.zip", "c414a783f3983bbe2e9e01d9d5320c7e");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/sgx/game.zip", "db545ab29694bfda1010317d4bac83b8");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/tg16/game.zip", "8b6c5c2e54915be2cdba63973862e143");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/fds/game.zip", "c0c135a97e8c577cfdf9204823ff211f");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/gamegear/game.zip", "f6f471e952b8103032b723f57bdbe767");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/sms/game.zip", "43f35f575dead94dd2f42f9caf69fe5a");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/megadriv/game.zip", "f99d0aaf12ba3eb6ced9878c76692c63");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/sg1000/game.zip", "e8f6c711c4371f09537b4f2a7a304d6c");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/spectrum/game.zip", "a5f62157b2617bd728c4b1bc885c29e9");
  TEST_PARAMS3(test_hash_filename, RC_CONSOLE_ARCADE, "/home/user/ngp/game.zip", "d4133b74c4e57274ca514e27a370dcb6");

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

  /* MSX */
  TEST_PARAMS4(test_hash_full_file, RC_CONSOLE_MSX, "test.dsk", 737280, "0e73fe94e5f2e2d8216926eae512b7a6");
  TEST_PARAMS4(test_hash_m3u, RC_CONSOLE_MSX, "test.dsk", 737280, "0e73fe94e5f2e2d8216926eae512b7a6");

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
  TEST_PARAMS4(test_hash_m3u, RC_CONSOLE_PC8800, "test.d88", 348288, "8cca4121bf87200f45e91b905a9f5afd");

  /* PC Engine CD */
  TEST(test_hash_pce_cd);
  TEST(test_hash_pce_cd_invalid_header);

  /* PC-FX */
  TEST(test_hash_pcfx);
  TEST(test_hash_pcfx_invalid_header);
  TEST(test_hash_pcfx_pce_cd);

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

  /* m3u support */
  TEST(test_hash_m3u_with_comments);
  TEST(test_hash_m3u_empty);
  TEST(test_hash_m3u_trailing_whitespace);
  TEST(test_hash_m3u_line_ending);
  TEST(test_hash_m3u_extension_case);
  TEST(test_hash_m3u_relative_path);
  TEST_PARAMS1(test_hash_m3u_absolute_path, "/absolute/test.d88");
  TEST_PARAMS1(test_hash_m3u_absolute_path, "\\absolute\\test.d88");
  TEST_PARAMS1(test_hash_m3u_absolute_path, "C:\\absolute\\test.d88");
  TEST_PARAMS1(test_hash_m3u_absolute_path, "\\\\server\\absolute\\test.d88");
  TEST_PARAMS1(test_hash_m3u_absolute_path, "samba:/absolute/test.d88");

  /* other */
  TEST(test_hash_file_without_ext);

  TEST_SUITE_END();
}
