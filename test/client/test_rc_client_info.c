#include "rc_client.h"

#include "../../src/runtime/rc_trigger.h"

#include "../../src/client/rc_client_game.h"
#include "../../src/client/rc_client_internal.h"
#include "../../src/util/rc_version.h"

#include "client_data.h"
#include "mock_client.h"

#include "../test_framework.h"

#if defined(_WIN32)
#include <Windows.h>
#elif defined(__unix__) && __STDC_VERSION__ >= 199309L
#include <unistd.h>
#else
#define RC_NO_SLEEP
#endif

/* ----- fetch hash library ----- */

static void test_fetch_hash_library_response(int result, const char* error_message,
  rc_client_hash_library_t* list, rc_client_t* client, void* callback_userdata)
{
  rc_client_callback_expect_success(result, error_message, client, callback_userdata);
  if (result != RC_OK)
    return;

  ASSERT_NUM_EQUALS(list->num_entries, 3);
  ASSERT_NUM_EQUALS(list->entries[0].game_id, 1);
  ASSERT_STR_EQUALS(list->entries[0].hash, "aabbccddeeff00112233445566778899");
  ASSERT_NUM_EQUALS(list->entries[1].game_id, 1);
  ASSERT_STR_EQUALS(list->entries[1].hash, "99aabbccddeeff001122334455667788");
  ASSERT_NUM_EQUALS(list->entries[2].game_id, 2);
  ASSERT_STR_EQUALS(list->entries[2].hash, "8899aabbccddeeff0011223344556677");

  rc_client_destroy_hash_library(list);
}

static void test_fetch_hash_library(void)
{
  g_client = mock_client_not_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=hashlibrary&c=1", "{\"Success\":true,\"MD5List\":{"
    "\"aabbccddeeff00112233445566778899\":1,"
    "\"99aabbccddeeff001122334455667788\":1,"
    "\"8899aabbccddeeff0011223344556677\":2"
    "}}");

  rc_client_begin_fetch_hash_library(g_client, 1, test_fetch_hash_library_response, g_callback_userdata);
  rc_client_destroy(g_client);
}

/* ----- fetch game titles ----- */

static void test_fetch_game_titles_response(int result, const char* error_message,
  rc_client_game_title_list_t* list, rc_client_t* client, void* callback_userdata)
{
  rc_client_callback_expect_success(result, error_message, client, callback_userdata);
  if (result != RC_OK)
    return;

  ASSERT_NUM_EQUALS(list->num_entries, 3);
  ASSERT_NUM_EQUALS(list->entries[0].game_id, 3);
  ASSERT_STR_EQUALS(list->entries[0].title, "Game Name 3");
  ASSERT_STR_EQUALS(list->entries[0].badge_name, "010003");
  ASSERT_STR_EQUALS(list->entries[0].badge_url, "https://media.retroachievements.org/Images/010003.png"); /* default media host */
  ASSERT_NUM_EQUALS(list->entries[1].game_id, 4);
  ASSERT_STR_EQUALS(list->entries[1].title, "Game Name 4");
  ASSERT_STR_EQUALS(list->entries[1].badge_name, "010004");
  ASSERT_STR_EQUALS(list->entries[1].badge_url, "http://media.retroachievements.org/Images/010004.png");
  ASSERT_NUM_EQUALS(list->entries[2].game_id, 7);
  ASSERT_STR_EQUALS(list->entries[2].title, "Game Name 7");
  ASSERT_STR_EQUALS(list->entries[2].badge_name, "010007");
  ASSERT_STR_EQUALS(list->entries[2].badge_url, "localhost/Images/010007.png");

  rc_client_destroy_game_title_list(list);
}

static void test_fetch_game_titles(void)
{
  const uint32_t game_ids[] = { 3, 4, 7 };
  g_client = mock_client_not_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=gameinfolist&g=3,4,7", "{\"Success\":true,\"Response\":["
    "{\"ID\": 3, \"Title\":\"Game Name 3\", \"ImageIcon\": \"/Images/010003.png\"}," /* ImageUrl explicitly not returned to test auto-populating logic */
    "{\"ID\": 4, \"Title\":\"Game Name 4\", \"ImageIcon\": \"/Images/010004.png\", \"ImageUrl\": \"http:\\/\\/media.retroachievements.org\\/Images\\/010004.png\"},"
    "{\"ID\": 7, \"Title\":\"Game Name 7\", \"ImageIcon\": \"/Images/010007.png\", \"ImageUrl\": \"localhost\\/Images\\/010007.png\"}"
    "]}");

  rc_client_begin_fetch_game_titles(g_client, game_ids, 3, test_fetch_game_titles_response, g_callback_userdata);
  rc_client_destroy(g_client);
}

/* ----- fetch game list ----- */

static void test_fetch_game_list_response(int result, const char* error_message,
  rc_client_game_list_t* list, rc_client_t* client, void* callback_userdata)
{
  rc_client_callback_expect_success(result, error_message, client, callback_userdata);
  if (result != RC_OK)
    return;

  ASSERT_NUM_EQUALS(list->num_entries, 2);
  ASSERT_NUM_EQUALS(list->entries[0].id, 3);
  ASSERT_STR_EQUALS(list->entries[0].name, "Game Name 3");
  ASSERT_STR_EQUALS(list->entries[0].image_name, "010003");
  ASSERT_STR_EQUALS(list->entries[0].image_url, "https://media.retroachievements.org/Images/010003.png");
  ASSERT_NUM_EQUALS(list->entries[0].num_achievements, 10);
  ASSERT_NUM_EQUALS(list->entries[0].num_leaderboards, 2);
  ASSERT_NUM_EQUALS(list->entries[0].points, 50);
  ASSERT_NUM_EQUALS(list->entries[0].num_supported_hashes, 2);
  ASSERT_STR_EQUALS(list->entries[0].supported_hashes[0], "00112233445566778899aabbccddeeff");
  ASSERT_STR_EQUALS(list->entries[0].supported_hashes[1], "112233445566778899aabbccddeeff00");
  ASSERT_NUM_EQUALS(list->entries[0].num_unsupported_hashes, 1);
  ASSERT_STR_EQUALS(list->entries[0].unsupported_hashes[0], "2233445566778899aabbccddeeff0011");

  ASSERT_NUM_EQUALS(list->entries[1].id, 4);
  ASSERT_STR_EQUALS(list->entries[1].name, "Game Name 4");
  ASSERT_STR_EQUALS(list->entries[1].image_name, "010004");
  ASSERT_STR_EQUALS(list->entries[1].image_url, "http://media.retroachievements.org/Images/010004.png");
  ASSERT_NUM_EQUALS(list->entries[1].num_achievements, 20);
  ASSERT_NUM_EQUALS(list->entries[1].num_leaderboards, 0);
  ASSERT_NUM_EQUALS(list->entries[1].points, 100);
  ASSERT_NUM_EQUALS(list->entries[1].num_supported_hashes, 1);
  ASSERT_STR_EQUALS(list->entries[1].supported_hashes[0], "33445566778899aabbccddeeff001122");
  ASSERT_NUM_EQUALS(list->entries[1].num_unsupported_hashes, 0);
  ASSERT_PTR_NULL(list->entries[1].unsupported_hashes);

  rc_client_destroy_game_list(list);
}

static void test_fetch_game_list(void)
{
  g_client = mock_client_not_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=systemgames&s=17", "{\"Success\":true,\"Response\":["
    "{\"ID\": 3, \"Title\":\"Game Name 3\", \"ImageIcon\": \"\\/Images\\/010003.png\","
      "\"ImageUrl\": \"https:\\/\\/media.retroachievements.org\\/Images\\/010003.png\","
      "\"NumAchievements\":10,\"NumLeaderboards\":2,\"Points\":50,"
      "\"SupportedHashes\":[\"00112233445566778899aabbccddeeff\",\"112233445566778899aabbccddeeff00\"],"
      "\"UnsupportedHashes\":[\"2233445566778899aabbccddeeff0011\"]},"
    "{\"ID\": 4, \"Title\":\"Game Name 4\", \"ImageIcon\": \"\\/Images\\/010004.png\","
      "\"ImageUrl\": \"http:\\/\\/media.retroachievements.org\\/Images\\/010004.png\","
      "\"NumAchievements\":20,\"NumLeaderboards\":0,\"Points\":100,"
      "\"SupportedHashes\":[\"33445566778899aabbccddeeff001122\"]}"
    "]}");

  rc_client_begin_fetch_game_list(g_client, 17, test_fetch_game_list_response, g_callback_userdata);
  rc_client_destroy(g_client);
}

/* ----- all user progress ----- */

static void test_fetch_all_user_progress_response(int result, const char* error_message,
  rc_client_all_user_progress_t* list, rc_client_t* client, void* callback_userdata)
{
  rc_client_callback_expect_success(result, error_message, client, callback_userdata);
  if (result != RC_OK)
    return;

  ASSERT_NUM_EQUALS(list->num_entries, 4);
  ASSERT_NUM_EQUALS(list->entries[0].game_id, 10);
  ASSERT_NUM_EQUALS(list->entries[0].num_achievements, 11);
  ASSERT_NUM_EQUALS(list->entries[0].num_unlocked_achievements, 0);
  ASSERT_NUM_EQUALS(list->entries[0].num_unlocked_achievements_hardcore, 0);
  ASSERT_NUM_EQUALS(list->entries[1].game_id, 20);
  ASSERT_NUM_EQUALS(list->entries[1].num_achievements, 21);
  ASSERT_NUM_EQUALS(list->entries[1].num_unlocked_achievements, 22);
  ASSERT_NUM_EQUALS(list->entries[1].num_unlocked_achievements_hardcore, 0);
  ASSERT_NUM_EQUALS(list->entries[2].game_id, 30);
  ASSERT_NUM_EQUALS(list->entries[2].num_achievements, 31);
  ASSERT_NUM_EQUALS(list->entries[2].num_unlocked_achievements, 32);
  ASSERT_NUM_EQUALS(list->entries[2].num_unlocked_achievements_hardcore, 33);
  ASSERT_NUM_EQUALS(list->entries[3].game_id, 40);
  ASSERT_NUM_EQUALS(list->entries[3].num_achievements, 41);
  ASSERT_NUM_EQUALS(list->entries[3].num_unlocked_achievements, 0);
  ASSERT_NUM_EQUALS(list->entries[3].num_unlocked_achievements_hardcore, 43);

  rc_client_destroy_all_user_progress(list);
}

static void test_fetch_all_user_progress(void)
{
  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=allprogress&u=Username&t=ApiToken&c=1", "{\"Success\":true,\"Response\":{\"10\":{\"Achievements\":11},"
      "\"20\":{\"Achievements\":21,\"Unlocked\":22},"
      "\"30\":{\"Achievements\":31,\"Unlocked\":32,\"UnlockedHardcore\":33},"
      "\"40\":{\"Achievements\":41,\"UnlockedHardcore\":43}}}");

  rc_client_begin_fetch_all_user_progress(g_client, 1, test_fetch_all_user_progress_response, g_callback_userdata);
  rc_client_destroy(g_client);
}

/* ----- clock ----- */

static void test_clock_get_now_millisecs(void)
{
  rc_client_t* client = rc_client_create(NULL, NULL);
  rc_get_time_millisecs_func_t get_millisecs = client->callbacks.get_time_millisecs;

#ifdef RC_NO_SLEEP
  rc_clock_t time1;
  ASSERT_PTR_NOT_NULL(get_millisecs);
  time1 = get_millisecs(client);
  ASSERT_NUM_NOT_EQUALS(time1, 0);
#else
  rc_clock_t time1, time2, diff;
  time_t first = time(NULL), now;

  do {
    ASSERT_PTR_NOT_NULL(get_millisecs);
    time1 = get_millisecs(client);
    ASSERT_NUM_NOT_EQUALS(time1, 0);

#if defined(_WIN32)
    Sleep(50);
#else
    usleep(50000);
#endif

    time2 = get_millisecs(client);
    ASSERT_NUM_NOT_EQUALS(time2, 0);
    diff = time2 - time1;

    ASSERT_NUM_GREATER(diff, 49);
    if (diff < 100)
      break;

    now = time(NULL);
    if (now - first >= 3) {
      ASSERT_FAIL("could not get a 50ms sleep interval within 3 seconds");
      break;
    }

  } while (1);
#endif

  rc_client_destroy(client);
}

/* ----- harness ----- */

void test_client_info(void) {
  TEST_SUITE_BEGIN();

  /* hash library */
  TEST(test_fetch_hash_library);

  /* game titles */
  TEST(test_fetch_game_titles);

  /* game list */
  TEST(test_fetch_game_list);

  /* all user progress */
  TEST(test_fetch_all_user_progress);

  TEST_SUITE_END();
}
