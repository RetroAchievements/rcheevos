#include "rc_client.h"

#include "../../src/runtime/rc_format.h"
#include "../../src/runtime/rc_trigger.h"

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

/* ----- user ----- */

static void test_get_user_game_summary(void)
{
  rc_client_user_game_summary_t summary;

  g_client = mock_client_logged_in();
  rc_client_set_unpromoted_enabled(g_client, 1);
  mock_client_load_game(patchdata_exhaustive, unlock_6_8h_and_9);

  rc_client_get_user_game_summary(g_client, &summary);
  ASSERT_NUM_EQUALS(summary.num_promoted_achievements, 7);
  ASSERT_NUM_EQUALS(summary.num_unpromoted_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 1);

  ASSERT_NUM_EQUALS(summary.points_available, 35);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 5);

  ASSERT_NUM_EQUALS(summary.beaten_time, 0);
  ASSERT_NUM_EQUALS(summary.completed_time, 0);

  rc_client_destroy(g_client);
}

static void test_get_user_game_summary_casual(void)
{
  rc_client_user_game_summary_t summary;

  g_client = mock_client_logged_in();
  rc_client_set_unpromoted_enabled(g_client, 1);
  mock_client_load_game(patchdata_exhaustive, unlock_6_8h_and_9);
  rc_client_set_hardcore_enabled(g_client, 0);

  rc_client_get_user_game_summary(g_client, &summary);
  ASSERT_NUM_EQUALS(summary.num_promoted_achievements, 7);
  ASSERT_NUM_EQUALS(summary.num_unpromoted_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 3);

  ASSERT_NUM_EQUALS(summary.points_available, 35);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 15);

  ASSERT_NUM_EQUALS(summary.beaten_time, 0);
  ASSERT_NUM_EQUALS(summary.completed_time, 0);

  rc_client_destroy(g_client);
}

static void test_get_user_game_summary_encore_mode(void)
{
  rc_client_user_game_summary_t summary;

  g_client = mock_client_logged_in();
  rc_client_set_unpromoted_enabled(g_client, 1);
  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2", patchdata_exhaustive);
  mock_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING, unlock_6_8h_and_9);

  rc_client_set_encore_mode_enabled(g_client, 1);
  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_success, g_callback_userdata);

  rc_client_get_user_game_summary(g_client, &summary);
  ASSERT_NUM_EQUALS(summary.num_promoted_achievements, 7);
  ASSERT_NUM_EQUALS(summary.num_unpromoted_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 1);

  ASSERT_NUM_EQUALS(summary.points_available, 35);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 5);

  ASSERT_NUM_EQUALS(summary.beaten_time, 0);
  ASSERT_NUM_EQUALS(summary.completed_time, 0);

  rc_client_destroy(g_client);
}

static void test_get_user_game_summary_with_unsupported_and_unpromoted(void)
{
  rc_client_user_game_summary_t summary;

  g_client = mock_client_logged_in();
  rc_client_set_unpromoted_enabled(g_client, 1);
  mock_client_load_game(patchdata_unpromoted_unsupported, no_unlocks);

  rc_client_get_user_game_summary(g_client, &summary);
  ASSERT_NUM_EQUALS(summary.num_promoted_achievements, 2);
  ASSERT_NUM_EQUALS(summary.num_unpromoted_achievements, 1);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 1);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 0);

  ASSERT_NUM_EQUALS(summary.points_available, 7);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 0);

  ASSERT_NUM_EQUALS(summary.beaten_time, 0);
  ASSERT_NUM_EQUALS(summary.completed_time, 0);

  rc_client_destroy(g_client);
}

static void test_get_user_game_summary_with_unsupported_unlocks(void)
{
  rc_client_user_game_summary_t summary;

  g_client = mock_client_logged_in();
  rc_client_set_unpromoted_enabled(g_client, 1);
  mock_client_load_game(patchdata_unpromoted_unsupported, unlock_5501_5502_and_5503);

  /* unlocked unsupported achievement should be counted in both unlocked and unsuppored buckets */
  rc_client_get_user_game_summary(g_client, &summary);
  ASSERT_NUM_EQUALS(summary.num_promoted_achievements, 2);
  ASSERT_NUM_EQUALS(summary.num_unpromoted_achievements, 1);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 1);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 2);

  ASSERT_NUM_EQUALS(summary.points_available, 7);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 7);

  ASSERT_NUM_EQUALS(summary.beaten_time, 0);
  ASSERT_NUM_EQUALS(summary.completed_time, 1234567999);

  rc_client_destroy(g_client);
}

static void test_get_user_game_summary_with_unpromoted_off(void)
{
  rc_client_user_game_summary_t summary;

  g_client = mock_client_logged_in();
  rc_client_set_unpromoted_enabled(g_client, 0);
  mock_client_load_game(patchdata_unpromoted_unsupported, no_unlocks);

  /* unpromoted achievements are not copied from the patch data to the runtime if unpromoted is off */
  rc_client_get_user_game_summary(g_client, &summary);
  ASSERT_NUM_EQUALS(summary.num_promoted_achievements, 2);
  ASSERT_NUM_EQUALS(summary.num_unpromoted_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 1);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 0);

  ASSERT_NUM_EQUALS(summary.points_available, 7);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 0);

  ASSERT_NUM_EQUALS(summary.beaten_time, 0);
  ASSERT_NUM_EQUALS(summary.completed_time, 0);

  rc_client_destroy(g_client);
}

static void test_get_user_game_summary_no_achievements(void)
{
  rc_client_user_game_summary_t summary;

  g_client = mock_client_logged_in();
  rc_client_set_unpromoted_enabled(g_client, 1);
  mock_client_load_game(patchdata_empty, no_unlocks);

  rc_client_get_user_game_summary(g_client, &summary);
  ASSERT_NUM_EQUALS(summary.num_promoted_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unpromoted_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 0);

  ASSERT_NUM_EQUALS(summary.points_available, 0);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 0);

  ASSERT_NUM_EQUALS(summary.beaten_time, 0);
  ASSERT_NUM_EQUALS(summary.completed_time, 0);

  rc_client_destroy(g_client);
}

static void test_get_user_game_summary_unknown_game(void)
{
  rc_client_user_game_summary_t summary;

  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2", patchdata_not_found);
  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_unknown_game, g_callback_userdata);

  rc_client_get_user_game_summary(g_client, &summary);
  ASSERT_NUM_EQUALS(summary.num_promoted_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unpromoted_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 0);

  ASSERT_NUM_EQUALS(summary.points_available, 0);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 0);

  rc_client_destroy(g_client);
}

static void test_get_user_game_summary_progress_incomplete(void)
{
  rc_client_user_game_summary_t summary;

  g_client = mock_client_logged_in();
  rc_client_set_unpromoted_enabled(g_client, 1);
  mock_client_load_game(patchdata_exhaustive_typed, unlock_8);

  rc_client_get_user_game_summary(g_client, &summary);
  ASSERT_NUM_EQUALS(summary.num_promoted_achievements, 7);
  ASSERT_NUM_EQUALS(summary.num_unpromoted_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 1);

  ASSERT_NUM_EQUALS(summary.points_available, 35);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 5);

  ASSERT_NUM_EQUALS(summary.beaten_time, 0);
  ASSERT_NUM_EQUALS(summary.completed_time, 0);

  rc_client_destroy(g_client);
}

static void test_get_user_game_summary_progress_progression_no_win(void)
{
  rc_client_user_game_summary_t summary;
  /* 6 and 8 are progression, 9 is win condition */
  const char* unlock_5_6_and_8 = "{\"Success\":true,\"HardcoreUnlocks\":["
      "{\"ID\":5,\"When\":1234568890},"
      "{\"ID\":6,\"When\":1234567999},"
      "{\"ID\":8,\"When\":1234567895}"
    "]}";

  g_client = mock_client_logged_in();
  rc_client_set_unpromoted_enabled(g_client, 1);
  mock_client_load_game(patchdata_exhaustive_typed, unlock_5_6_and_8);

  rc_client_get_user_game_summary(g_client, &summary);
  ASSERT_NUM_EQUALS(summary.num_promoted_achievements, 7);
  ASSERT_NUM_EQUALS(summary.num_unpromoted_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 3);

  ASSERT_NUM_EQUALS(summary.points_available, 35);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 15);

  ASSERT_NUM_EQUALS(summary.beaten_time, 0);
  ASSERT_NUM_EQUALS(summary.completed_time, 0);

  rc_client_destroy(g_client);
}

static void test_get_user_game_summary_progress_win_only(void)
{
  rc_client_user_game_summary_t summary;
  /* 6 and 8 are progression, 9 is win condition */
  const char* unlock_5_and_9 = "{\"Success\":true,\"HardcoreUnlocks\":["
      "{\"ID\":5,\"When\":1234568890},"
      "{\"ID\":9,\"When\":1234567999}"
    "]}";

  g_client = mock_client_logged_in();
  rc_client_set_unpromoted_enabled(g_client, 1);
  mock_client_load_game(patchdata_exhaustive_typed, unlock_5_and_9);

  rc_client_get_user_game_summary(g_client, &summary);
  ASSERT_NUM_EQUALS(summary.num_promoted_achievements, 7);
  ASSERT_NUM_EQUALS(summary.num_unpromoted_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 2);

  ASSERT_NUM_EQUALS(summary.points_available, 35);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 10);

  ASSERT_NUM_EQUALS(summary.beaten_time, 0);
  ASSERT_NUM_EQUALS(summary.completed_time, 0);

  rc_client_destroy(g_client);
}

static void test_get_user_game_summary_beat(void)
{
  rc_client_user_game_summary_t summary;
  /* 6 and 8 are progression, 9 is win condition */
  const char* unlocks = "{\"Success\":true,\"HardcoreUnlocks\":["
      "{\"ID\":5,\"When\":1234568890},"
      "{\"ID\":6,\"When\":1234567999},"
      "{\"ID\":8,\"When\":1234567890},"
      "{\"ID\":9,\"When\":1234568765}"
    "]}";

  g_client = mock_client_logged_in();
  rc_client_set_unpromoted_enabled(g_client, 1);
  mock_client_load_game(patchdata_exhaustive_typed, unlocks);

  rc_client_get_user_game_summary(g_client, &summary);
  ASSERT_NUM_EQUALS(summary.num_promoted_achievements, 7);
  ASSERT_NUM_EQUALS(summary.num_unpromoted_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 4);

  ASSERT_NUM_EQUALS(summary.points_available, 35);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 20);

  ASSERT_NUM_EQUALS(summary.beaten_time, 1234568765);
  ASSERT_NUM_EQUALS(summary.completed_time, 0);

  rc_client_destroy(g_client);
}

static void test_get_user_game_summary_mastery(void)
{
  rc_client_user_game_summary_t summary;
  const char* unlocks = "{\"Success\":true,\"HardcoreUnlocks\":["
    "{\"ID\":5,\"When\":1234568890},"
    "{\"ID\":6,\"When\":1234567999},"
    "{\"ID\":7,\"When\":1234569123},"
    "{\"ID\":8,\"When\":1234567890},"
    "{\"ID\":9,\"When\":1234568765},"
    "{\"ID\":70,\"When\":1234568901},"
    "{\"ID\":71,\"When\":1234566789}"
    "]}";

  g_client = mock_client_logged_in();
  rc_client_set_unpromoted_enabled(g_client, 1);
  mock_client_load_game(patchdata_exhaustive_typed, unlocks);

  rc_client_get_user_game_summary(g_client, &summary);
  ASSERT_NUM_EQUALS(summary.num_promoted_achievements, 7);
  ASSERT_NUM_EQUALS(summary.num_unpromoted_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 7);

  ASSERT_NUM_EQUALS(summary.points_available, 35);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 35);

  ASSERT_NUM_EQUALS(summary.beaten_time, 1234568765);
  ASSERT_NUM_EQUALS(summary.completed_time, 1234569123);

  rc_client_destroy(g_client);
}

static void test_get_user_game_summary_warning(void)
{
  rc_client_user_game_summary_t summary;

  g_client = mock_client_logged_in();
  mock_client_load_game(patchdata_warning, no_unlocks);

  rc_client_get_user_game_summary(g_client, &summary);
  ASSERT_NUM_EQUALS(summary.num_promoted_achievements, 2);
  ASSERT_NUM_EQUALS(summary.num_unpromoted_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unsupported_achievements, 0);
  ASSERT_NUM_EQUALS(summary.num_unlocked_achievements, 0);

  ASSERT_NUM_EQUALS(summary.points_available, 7);
  ASSERT_NUM_EQUALS(summary.points_unlocked, 0);

  ASSERT_NUM_EQUALS(summary.beaten_time, 0);
  ASSERT_NUM_EQUALS(summary.completed_time, 0);

  rc_client_destroy(g_client);
}

/* ----- get game image ----- */

static void test_game_get_image_url(void)
{
  char buffer[256];
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);

  ASSERT_NUM_EQUALS(rc_client_game_get_image_url(rc_client_get_game_info(g_client), buffer, sizeof(buffer)), RC_OK);
  ASSERT_STR_EQUALS(buffer, "http://server/Images/112233.png");

  ASSERT_NUM_EQUALS(rc_client_game_get_image_url(rc_client_get_game_info(g_client), buffer, 10), RC_INSUFFICIENT_BUFFER);

  rc_client_destroy(g_client);
}

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

/* ----- subset ----- */

static void test_load_subset(void)
{
  rc_client_achievement_info_t* achievement;
  rc_client_leaderboard_info_t* leaderboard;
  rc_client_subset_info_t* subset_info;
  const rc_client_subset_t* subset;
  g_client = mock_client_logged_in();

  reset_mock_api_handlers();
  mock_api_response("r=achievementsets&u=Username&t=ApiToken&m=0123456789ABCDEF&v=2", patchdata_subset);
  mock_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=1&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING, "{\"Success\":true}");

  rc_client_begin_load_game(g_client, "0123456789ABCDEF", rc_client_callback_expect_success, g_callback_userdata);

  ASSERT_PTR_NULL(g_client->state.load);
  ASSERT_PTR_NOT_NULL(g_client->game);
  if (g_client->game) {
    ASSERT_PTR_EQUALS(rc_client_get_game_info(g_client), &g_client->game->public_);

    ASSERT_NUM_EQUALS(g_client->game->public_.id, 1234);
    ASSERT_NUM_EQUALS(g_client->game->public_.console_id, 17);
    ASSERT_STR_EQUALS(g_client->game->public_.title, "Sample Game");
    ASSERT_STR_EQUALS(g_client->game->public_.hash, "0123456789ABCDEF");
    ASSERT_STR_EQUALS(g_client->game->public_.badge_name, "112233");
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_achievements, 7);
    ASSERT_NUM_EQUALS(g_client->game->subsets->public_.num_leaderboards, 7);
  }

  subset = rc_client_get_subset_info(g_client, 2345);
  ASSERT_PTR_NOT_NULL(subset);
  if (subset) {
    subset_info = g_client->game->subsets->next;
    ASSERT_PTR_EQUALS(subset, &subset_info->public_);

    ASSERT_NUM_EQUALS(subset->id, 2345);
    ASSERT_STR_EQUALS(subset->title, "Bonus");
    ASSERT_STR_EQUALS(subset->badge_name, "112234");
    ASSERT_STR_EQUALS(subset->badge_url, "http://server/Images/112234.png");
    ASSERT_NUM_EQUALS(subset->num_achievements, 3);
    ASSERT_NUM_EQUALS(subset->num_leaderboards, 2);

    achievement = &subset_info->achievements[0];
    ASSERT_NUM_EQUALS(achievement->public_.id, 5501);
    ASSERT_STR_EQUALS(achievement->public_.title, "Achievement 5501");
    ASSERT_STR_EQUALS(achievement->public_.description, "Desc 5501");
    ASSERT_STR_EQUALS(achievement->public_.badge_name, "005501");
    ASSERT_STR_EQUALS(achievement->public_.badge_url, "https://media.retroachievements.org/Badge/005501.png");
    ASSERT_STR_EQUALS(achievement->public_.badge_locked_url, "https://media.retroachievements.org/Badge/005501_lock.png");
    ASSERT_NUM_EQUALS(achievement->public_.points, 5);
    ASSERT_NUM_EQUALS(achievement->public_.unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->public_.state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->public_.category, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED);
    ASSERT_STR_EQUALS(achievement->author, "User1");
    ASSERT_NUM_EQUALS(achievement->created_time, 1367266583);
    ASSERT_NUM_EQUALS(achievement->updated_time, 1376929305);
    ASSERT_PTR_NOT_NULL(achievement->trigger);

    achievement = &subset_info->achievements[1];
    ASSERT_NUM_EQUALS(achievement->public_.id, 5502);
    ASSERT_STR_EQUALS(achievement->public_.title, "Achievement 5502");
    ASSERT_STR_EQUALS(achievement->public_.description, "Desc 5502");
    ASSERT_STR_EQUALS(achievement->public_.badge_name, "005502");
    ASSERT_NUM_EQUALS(achievement->public_.points, 5);
    ASSERT_NUM_EQUALS(achievement->public_.unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->public_.state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->public_.category, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED);
    ASSERT_PTR_NOT_NULL(achievement->trigger);

    achievement = &subset_info->achievements[2];
    ASSERT_NUM_EQUALS(achievement->public_.id, 5503);
    ASSERT_STR_EQUALS(achievement->public_.title, "Achievement 5503");
    ASSERT_STR_EQUALS(achievement->public_.description, "Desc 5503");
    ASSERT_STR_EQUALS(achievement->public_.badge_name, "005503");
    ASSERT_NUM_EQUALS(achievement->public_.points, 5);
    ASSERT_NUM_EQUALS(achievement->public_.unlock_time, 0);
    ASSERT_NUM_EQUALS(achievement->public_.state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(achievement->public_.category, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED);
    ASSERT_PTR_NOT_NULL(achievement->trigger);

    leaderboard = &subset_info->leaderboards[0];
    ASSERT_NUM_EQUALS(leaderboard->public_.id, 81);
    ASSERT_STR_EQUALS(leaderboard->public_.title, "Leaderboard 81");
    ASSERT_STR_EQUALS(leaderboard->public_.description, "Desc 81");
    ASSERT_NUM_EQUALS(leaderboard->public_.state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(leaderboard->format, RC_FORMAT_SCORE);
    ASSERT_PTR_NOT_NULL(leaderboard->lboard);
    ASSERT_NUM_NOT_EQUALS(leaderboard->value_djb2, 0);
    ASSERT_PTR_NULL(leaderboard->tracker);

    leaderboard = &subset_info->leaderboards[1];
    ASSERT_NUM_EQUALS(leaderboard->public_.id, 82);
    ASSERT_STR_EQUALS(leaderboard->public_.title, "Leaderboard 82");
    ASSERT_STR_EQUALS(leaderboard->public_.description, "Desc 82");
    ASSERT_NUM_EQUALS(leaderboard->public_.state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(leaderboard->format, RC_FORMAT_SCORE);
    ASSERT_PTR_NOT_NULL(leaderboard->lboard);
    ASSERT_NUM_NOT_EQUALS(leaderboard->value_djb2, 0);
    ASSERT_PTR_NULL(leaderboard->tracker);
  }

  rc_client_destroy(g_client);
}

static void test_subset_list(void)
{
  rc_client_subset_list_t* list;
  const rc_client_subset_t* subset;

  g_client = mock_client_game_loaded(patchdata_subset, no_unlocks);

  list = rc_client_create_subset_list(g_client);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_subsets, 2);

    subset = list->subsets[0];
    ASSERT_NUM_EQUALS(subset->id, 1111);
    ASSERT_STR_EQUALS(subset->title, "Sample Game");
    ASSERT_STR_EQUALS(subset->badge_name, "112233");
    ASSERT_NUM_EQUALS(subset->num_achievements, 7);
    ASSERT_NUM_EQUALS(subset->num_leaderboards, 7);
    ASSERT_STR_EQUALS(subset->badge_url, "http://server/Images/112233.png");

    subset = list->subsets[1];
    ASSERT_NUM_EQUALS(subset->id, 2345);
    ASSERT_STR_EQUALS(subset->title, "Bonus");
    ASSERT_STR_EQUALS(subset->badge_name, "112234");
    ASSERT_NUM_EQUALS(subset->num_achievements, 3);
    ASSERT_NUM_EQUALS(subset->num_leaderboards, 2);
    ASSERT_STR_EQUALS(subset->badge_url, "http://server/Images/112234.png");

    rc_client_destroy_subset_list(list);
  }

  rc_client_unload_game(g_client);
  ASSERT_PTR_NULL(g_client->game);

  list = rc_client_create_subset_list(g_client);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_subsets, 0);
    rc_client_destroy_subset_list(list);
  }

  rc_client_destroy(g_client);
}

/* ----- achievement list ----- */

static void test_achievement_list_simple(void)
{
  rc_client_achievement_list_t* list;
  const rc_client_achievement_t** iter;
  const rc_client_achievement_t* achievement;

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 2);

    iter = list->buckets[0].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5501);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5502);

    rc_client_destroy_achievement_list(list);
  }

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_UNPROMOTED, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 0);
    rc_client_destroy_achievement_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_achievement_list_simple_with_unlocks(void)
{
  rc_client_achievement_list_t* list;
  const rc_client_achievement_t** iter;
  const rc_client_achievement_t* achievement;

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, unlock_5501h_and_5502);

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    /* in hardcore mode, 5501 should be unlocked, but 5502 will be locked */
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);

    iter = list->buckets[0].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5502);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_CASUAL);
    iter = list->buckets[1].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5501);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);

    rc_client_destroy_achievement_list(list);
  }

  rc_client_set_hardcore_enabled(g_client, 0);

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    /* in casual mode, both should be unlocked */
    ASSERT_NUM_EQUALS(list->num_buckets, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 2);

    iter = list->buckets[0].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5501);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5502);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_CASUAL);

    rc_client_destroy_achievement_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_achievement_list_simple_with_unlocks_encore_mode(void)
{
  rc_client_achievement_list_t* list;
  const rc_client_achievement_t** iter;
  const rc_client_achievement_t* achievement;

  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_logged_in();
  rc_client_set_encore_mode_enabled(g_client, 1);
  mock_client_load_game(patchdata_2ach_1lbd, unlock_5501h_and_5502);

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    /* in hardcore mode, 5501 should be unlocked, but both will appear locked due to encore mode */
    ASSERT_NUM_EQUALS(list->num_buckets, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 2);

    iter = list->buckets[0].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5501);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5502);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_CASUAL);

    rc_client_destroy_achievement_list(list);
  }

  rc_client_set_hardcore_enabled(g_client, 0);
  mock_api_response("r=startsession&u=Username&t=ApiToken&g=1234&h=0&m=0123456789ABCDEF&l=" RCHEEVOS_VERSION_STRING, unlock_5501h_and_5502);

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    /* in casual mode, both should be unlocked, but will appear locked due to encore mode */
    ASSERT_NUM_EQUALS(list->num_buckets, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 2);

    iter = list->buckets[0].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5501);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5502);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_CASUAL);

    rc_client_destroy_achievement_list(list);
  }

  /* unlock 5501, should appear unlocked again */
  mock_memory(memory, sizeof(memory));
  rc_client_do_frame(g_client);
  memory[1] = 3;
  memory[2] = 7;
  mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=5501&h=0&m=0123456789ABCDEF&v=7f066800cd3962efeb1f479e5671b59c",
    "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":5,\"AchievementsRemaining\":6}");

  reset_mock_events();
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 1);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_ACHIEVEMENT_TRIGGERED, 5501));

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);

    achievement = list->buckets[0].achievements[0];
    ASSERT_NUM_EQUALS(achievement->id, 5502);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_CASUAL);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);

    achievement = list->buckets[1].achievements[0];
    ASSERT_NUM_EQUALS(achievement->id, 5501);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);

    rc_client_destroy_achievement_list(list);
  }

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Recently Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);

    achievement = list->buckets[0].achievements[0];
    ASSERT_NUM_EQUALS(achievement->id, 5501);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);

    achievement = list->buckets[1].achievements[0];
    ASSERT_NUM_EQUALS(achievement->id, 5502);
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_CASUAL);

    rc_client_destroy_achievement_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_achievement_list_simple_with_unpromoted_and_unsupported(void)
{
  rc_client_achievement_list_t* list;

  g_client = mock_client_logged_in();
  rc_client_set_unpromoted_enabled(g_client, 1);
  mock_client_load_game(patchdata_unpromoted_unsupported, no_unlocks);

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 5501);
    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unsupported");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5503);

    rc_client_destroy_achievement_list(list);
  }

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_UNPROMOTED, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNPROMOTED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Unpromoted");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 5502);

    rc_client_destroy_achievement_list(list);
  }

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED_AND_UNPROMOTED, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 3);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 5501);
    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNPROMOTED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unpromoted");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5502);
    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Unsupported");
    ASSERT_NUM_EQUALS(list->buckets[2].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[0]->id, 5503);

    rc_client_destroy_achievement_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_achievement_list_simple_with_unpromoted_off(void)
{
  rc_client_achievement_list_t* list;

  g_client = mock_client_logged_in();
  rc_client_set_unpromoted_enabled(g_client, 0);
  mock_client_load_game(patchdata_unpromoted_unsupported, no_unlocks);

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 5501);
    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unsupported");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5503);

    rc_client_destroy_achievement_list(list);
  }

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_UNPROMOTED, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 0);
    rc_client_destroy_achievement_list(list);
  }

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED_AND_UNPROMOTED, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 5501);
    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unsupported");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5503);

    rc_client_destroy_achievement_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_achievement_list_buckets(void)
{
  rc_client_achievement_list_t* list;
  const rc_client_achievement_t** iter;
  const rc_client_achievement_t* achievement;

  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, unlock_8);
  mock_memory(memory, sizeof(memory));

  rc_client_do_frame(g_client); /* advance achievements out of waiting state */
  reset_mock_events();

  mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=5&h=1&m=0123456789ABCDEF&v=732f8e30e9c1eb08948dda098c305d8b",
      "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":5,\"AchievementsRemaining\":6}");

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 6);
    iter = list->buckets[0].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 6);
    ASSERT_STR_EQUALS(achievement->measured_progress, "");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 0.0);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 7);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 9);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 70);
    ASSERT_STR_EQUALS(achievement->measured_progress, "");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 0.0);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 71);
    ASSERT_STR_EQUALS(achievement->measured_progress, "");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 0.0);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 8);

    rc_client_destroy_achievement_list(list);
  }

  memory[5] = 5; /* trigger achievement 5 */
  memory[6] = 2; /* start measuring achievement 6 */
  memory[1] = 1; /* begin challenge achievement 7 */
  memory[0x11] = 100; /* start measuring achievements 70 and 71 */
  rc_client_do_frame(g_client);
  reset_mock_events();

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 4);

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Active Challenges");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 7);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Recently Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[2].num_achievements, 4);
    iter = list->buckets[2].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 6);
    ASSERT_STR_EQUALS(achievement->measured_progress, "2/6");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 33.333333);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 9);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 70);
    ASSERT_STR_EQUALS(achievement->measured_progress, "25,600/100,000");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 25.6);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 71);
    ASSERT_STR_EQUALS(achievement->measured_progress, "25%");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 25.6);

    ASSERT_NUM_EQUALS(list->buckets[3].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[3].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[3].label, "Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[3].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[3].achievements[0]->id, 8);

    rc_client_destroy_achievement_list(list);
  }

  /* also check mapping to lock state */
  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 5);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 6);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[1]->id, 7);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[2]->id, 9);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[3]->id, 70);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[4]->id, 71);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 2);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[1]->id, 8);

    rc_client_destroy_achievement_list(list);
  }

  /* recently unlocked achievement no longer recent */
  ((rc_client_achievement_t*)rc_client_get_achievement_info(g_client, 5))->unlock_time -= 15 * 60;
  memory[6] = 5; /* almost there achievement 6 */
  memory[1] = 0; /* stop challenge achievement 7 */
  rc_client_do_frame(g_client);
  reset_mock_events();

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 3);

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Almost There");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 6);
    ASSERT_STR_EQUALS(list->buckets[0].achievements[0]->measured_progress, "5/6");
    ASSERT_FLOAT_EQUALS(list->buckets[0].achievements[0] ->measured_percent, 83.333333);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 4);
    iter = list->buckets[1].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 7);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 9);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 70);
    ASSERT_STR_EQUALS(achievement->measured_progress, "25,600/100,000");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 25.6);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 71);
    ASSERT_STR_EQUALS(achievement->measured_progress, "25%");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 25.6);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[2].num_achievements, 2);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[0]->id, 5);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[1]->id, 8);

    rc_client_destroy_achievement_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_achievement_list_buckets_progress_sort(void)
{
  rc_client_achievement_list_t* list;
  const rc_client_achievement_t** iter;
  const rc_client_achievement_t* achievement;

  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, no_unlocks);
  mock_memory(memory, sizeof(memory));

  rc_client_do_frame(g_client); /* advance achievements out of waiting state */
  reset_mock_events();

  g_client->game->subsets->achievements[0].trigger->measured_target = 100;
  g_client->game->subsets->achievements[0].trigger->measured_value = 86;
  g_client->game->subsets->achievements[1].trigger->measured_target = 100;
  g_client->game->subsets->achievements[1].trigger->measured_value = 85;
  g_client->game->subsets->achievements[2].trigger->measured_target = 1000;
  g_client->game->subsets->achievements[2].trigger->measured_value = 855;
  g_client->game->subsets->achievements[3].trigger->measured_target = 100;
  g_client->game->subsets->achievements[3].trigger->measured_value = 87;
  g_client->game->subsets->achievements[4].trigger->measured_target = 100;
  g_client->game->subsets->achievements[4].trigger->measured_value = 85;
  g_client->game->subsets->achievements[5].trigger->measured_target = 100;
  g_client->game->subsets->achievements[5].trigger->measured_value = 75;

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Almost There");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 5);
    iter = list->buckets[0].achievements;
    achievement = *iter++;
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 87.0f);
    ASSERT_STR_EQUALS(achievement->measured_progress, "87/100");
    achievement = *iter++;
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 86.0f);
    ASSERT_STR_EQUALS(achievement->measured_progress, "86/100");
    achievement = *iter++;
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 85.5f);
    ASSERT_STR_EQUALS(achievement->measured_progress, "855/1,000");
    achievement = *iter++;
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 85.0f);
    ASSERT_STR_EQUALS(achievement->measured_progress, "85/100");
    ASSERT_NUM_EQUALS(achievement->id, 6);
    achievement = *iter++;
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 85.0f);
    ASSERT_STR_EQUALS(achievement->measured_progress, "85/100");
    ASSERT_NUM_EQUALS(achievement->id, 9);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 2);
    ASSERT_FLOAT_EQUALS(list->buckets[1].achievements[0]->measured_percent, 75.0f);
    ASSERT_STR_EQUALS(list->buckets[1].achievements[0]->measured_progress, "75/100");
    ASSERT_FLOAT_EQUALS(list->buckets[1].achievements[1]->measured_percent, 0.0f);
    ASSERT_STR_EQUALS(list->buckets[1].achievements[1]->measured_progress, "");

    rc_client_destroy_achievement_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_achievement_list_buckets_progress_sort_big_ids(void)
{
  rc_client_achievement_list_t* list;
  const rc_client_achievement_t** iter;
  const rc_client_achievement_t* achievement;

  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_big_ids, no_unlocks);
  mock_memory(memory, sizeof(memory));

  rc_client_do_frame(g_client); /* advance achievements out of waiting state */
  reset_mock_events();

  g_client->game->subsets->achievements[0].trigger->measured_target = 100;
  g_client->game->subsets->achievements[0].trigger->measured_value = 86;
  g_client->game->subsets->achievements[1].trigger->measured_target = 100;
  g_client->game->subsets->achievements[1].trigger->measured_value = 85;
  g_client->game->subsets->achievements[2].trigger->measured_target = 100;
  g_client->game->subsets->achievements[2].trigger->measured_value = 85;

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Almost There");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 3);
    iter = list->buckets[0].achievements;
    achievement = *iter++;
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 86.0f);
    ASSERT_STR_EQUALS(achievement->measured_progress, "86/100");
    achievement = *iter++;
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 85.0f);
    ASSERT_STR_EQUALS(achievement->measured_progress, "85/100");
    ASSERT_NUM_EQUALS(achievement->id, 6);
    achievement = *iter++;
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 85.0f);
    ASSERT_STR_EQUALS(achievement->measured_progress, "85/100");
    ASSERT_NUM_EQUALS(achievement->id, 4294967295UL);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_FLOAT_EQUALS(list->buckets[1].achievements[0]->measured_percent, 0.0f);
    ASSERT_STR_EQUALS(list->buckets[1].achievements[0]->measured_progress, "");

    rc_client_destroy_achievement_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_achievement_list_buckets_with_unsynced(void)
{
  rc_client_achievement_list_t* list;
  rc_client_achievement_t* achievement;
  const char* unlock_request_params = "r=awardachievement&u=Username&t=ApiToken&a=5&h=1&m=0123456789ABCDEF&v=732f8e30e9c1eb08948dda098c305d8b";
  const char* unlock_request_params2 = "r=awardachievement&u=Username&t=ApiToken&a=5&h=1&m=0123456789ABCDEF&o=2&v=3cca3f9a9cfd6c0d5b6a17b9bb539070";

  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_exhaustive, unlock_8);
  mock_client_enable_async(g_client);
  mock_memory(memory, sizeof(memory));

  /* discard the queued ping to make finding the retry easier */
  g_client->state.scheduled_callbacks = NULL;

  rc_client_do_frame(g_client); /* advance achievements out of waiting state */
  reset_mock_events();

  memory[5] = 5; /* trigger achievement 5 */
  memory[1] = 1; /* begin challenge achievement 7 */
  rc_client_do_frame(g_client);
  reset_mock_events();

  /* first failure will immediately requeue the request */
  async_api_error(unlock_request_params, response_503, 503);
  assert_api_pending(unlock_request_params);
  ASSERT_PTR_NULL(g_client->state.scheduled_callbacks);
  rc_client_idle(g_client);

  /* second failure will queue it */
  async_api_error(unlock_request_params, response_503, 503);
  assert_api_call_count(unlock_request_params, 0);
  ASSERT_PTR_NOT_NULL(g_client->state.scheduled_callbacks);
  rc_client_idle(g_client);
  reset_mock_events();

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  if (list)
  {
    ASSERT_NUM_EQUALS(list->num_buckets, 4);

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Active Challenges");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 7);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Recently Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[2].num_achievements, 4);

    ASSERT_NUM_EQUALS(list->buckets[3].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[3].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[3].label, "Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[3].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[3].achievements[0]->id, 8);

    rc_client_destroy_achievement_list(list);
  }

  /* adjust unlock time on achievement 5 so it's no longer recent */
  achievement = (rc_client_achievement_t*)rc_client_get_achievement_info(g_client, 5);
  achievement->unlock_time -= 15 * 60;

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  if (list)
  {
    ASSERT_NUM_EQUALS(list->num_buckets, 4);

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Active Challenges");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 7);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNSYNCED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unlocks Not Synced to Server");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[2].num_achievements, 4);

    ASSERT_NUM_EQUALS(list->buckets[3].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[3].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[3].label, "Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[3].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[3].achievements[0]->id, 8);

    rc_client_destroy_achievement_list(list);
  }

  /* allow unlock request to succeed */
  g_now += 2 * 1000;
  rc_client_idle(g_client);
  assert_api_pending(unlock_request_params2);
  async_api_response(unlock_request_params2, "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":8,\"AchievementsRemaining\":11}");

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  if (list)
  {
    ASSERT_NUM_EQUALS(list->num_buckets, 3);

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Active Challenges");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 7);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 4);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[2].num_achievements, 2);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[0]->id, 5);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[1]->id, 8);

    rc_client_destroy_achievement_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_achievement_list_subset_with_unpromoted_and_unsupported(void)
{
  rc_client_achievement_list_t* list;

  g_client = mock_client_logged_in();
  rc_client_set_unpromoted_enabled(g_client, 1);
  mock_client_load_game(patchdata_subset_unpromoted_unsupported, no_unlocks);

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 3);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 1111);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Sample Game - Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 6);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 5);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[1]->id, 6);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[2]->id, 7);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[3]->id, 9);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[4]->id, 70);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[5]->id, 71);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Bonus - Locked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5501);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Bonus - Unsupported");
    ASSERT_NUM_EQUALS(list->buckets[2].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[0]->id, 5503);

    rc_client_destroy_achievement_list(list);
  }

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_UNPROMOTED, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNPROMOTED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 1111);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Sample Game - Unpromoted");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 8);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNPROMOTED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Bonus - Unpromoted");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5502);

    rc_client_destroy_achievement_list(list);
  }

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED_AND_UNPROMOTED, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_LOCK_STATE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 5);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 1111);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Sample Game - Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 6);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 5);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[1]->id, 6);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[2]->id, 7);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[3]->id, 9);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[4]->id, 70);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[5]->id, 71);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNPROMOTED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 1111);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Sample Game - Unpromoted");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 8);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Bonus - Locked");
    ASSERT_NUM_EQUALS(list->buckets[2].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[0]->id, 5501);

    ASSERT_NUM_EQUALS(list->buckets[3].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNPROMOTED);
    ASSERT_NUM_EQUALS(list->buckets[3].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[3].label, "Bonus - Unpromoted");
    ASSERT_NUM_EQUALS(list->buckets[3].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[3].achievements[0]->id, 5502);

    ASSERT_NUM_EQUALS(list->buckets[4].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED);
    ASSERT_NUM_EQUALS(list->buckets[4].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[4].label, "Bonus - Unsupported");
    ASSERT_NUM_EQUALS(list->buckets[4].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[4].achievements[0]->id, 5503);

    rc_client_destroy_achievement_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_achievement_list_subset_buckets(void)
{
  rc_client_achievement_list_t* list;
  const rc_client_achievement_t** iter;
  const rc_client_achievement_t* achievement;

  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_subset, unlock_8_and_5502);
  mock_memory(memory, sizeof(memory));

  rc_client_do_frame(g_client); /* advance achievements out of waiting state */
  reset_mock_events();

  mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=5&h=1&m=0123456789ABCDEF&v=732f8e30e9c1eb08948dda098c305d8b",
      "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":5,\"AchievementsRemaining\":6}");
  mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=5501&h=1&m=0123456789ABCDEF&v=9b9bdf5501eb6289a6655affbcc695e6",
      "{\"Success\":true,\"Score\":5437,\"SoftcoreScore\":777,\"AchievementID\":5,\"AchievementsRemaining\":6}");

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 4);

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 1111);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Sample Game - Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 6);
    iter = list->buckets[0].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 5);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 6);
    ASSERT_STR_EQUALS(achievement->measured_progress, "");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 0.0);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 7);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 9);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 70);
    ASSERT_STR_EQUALS(achievement->measured_progress, "");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 0.0);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 71);
    ASSERT_STR_EQUALS(achievement->measured_progress, "");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 0.0);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 1111);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Sample Game - Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 8);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Bonus - Locked");
    ASSERT_NUM_EQUALS(list->buckets[2].num_achievements, 2);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[0]->id, 5501);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[1]->id, 5503);

    ASSERT_NUM_EQUALS(list->buckets[3].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[3].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[3].label, "Bonus - Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[3].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[3].achievements[0]->id, 5502);

    rc_client_destroy_achievement_list(list);
  }

  memory[5] = 5; /* trigger achievement 5 */
  memory[6] = 2; /* start measuring achievement 6 */
  memory[1] = 1; /* begin challenge achievement 7 */
  memory[0x11] = 100; /* start measuring achievements 70 and 71 */
  memory[0x17] = 7; /* trigger achievement 5501 */
  rc_client_do_frame(g_client);
  reset_mock_events();

  /* set the unlock time for achievement 5 back one second to ensure consistent sorting */
  ((rc_client_achievement_t*)rc_client_get_achievement_info(g_client, 5))->unlock_time--;

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 6);

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_ACTIVE_CHALLENGE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Active Challenges");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 7);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Recently Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 2);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5501);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[1]->id, 5);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 1111);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Sample Game - Locked");
    ASSERT_NUM_EQUALS(list->buckets[2].num_achievements, 4);
    iter = list->buckets[2].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 6);
    ASSERT_STR_EQUALS(achievement->measured_progress, "2/6");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 33.333333);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 9);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 70);
    ASSERT_STR_EQUALS(achievement->measured_progress, "25,600/100,000");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 25.6);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 71);
    ASSERT_STR_EQUALS(achievement->measured_progress, "25%");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 25.6);

    ASSERT_NUM_EQUALS(list->buckets[3].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[3].subset_id, 1111);
    ASSERT_STR_EQUALS(list->buckets[3].label, "Sample Game - Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[3].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[3].achievements[0]->id, 8);

    ASSERT_NUM_EQUALS(list->buckets[4].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[4].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[4].label, "Bonus - Locked");
    ASSERT_NUM_EQUALS(list->buckets[4].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[4].achievements[0]->id, 5503);

    ASSERT_NUM_EQUALS(list->buckets[5].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[5].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[5].label, "Bonus - Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[5].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[5].achievements[0]->id, 5502);

    rc_client_destroy_achievement_list(list);
  }

  /* recently unlocked achievements no longer recent */
  ((rc_client_achievement_t*)rc_client_get_achievement_info(g_client, 5))->unlock_time -= 15 * 60;
  ((rc_client_achievement_t*)rc_client_get_achievement_info(g_client, 5501))->unlock_time -= 15 * 60;
  memory[6] = 5; /* almost there achievement 6 */
  memory[1] = 0; /* stop challenge achievement 7 */
  rc_client_do_frame(g_client);
  reset_mock_events();

  list = rc_client_create_achievement_list(g_client, RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED, RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 5);

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_ALMOST_THERE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Almost There");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 6);
    ASSERT_STR_EQUALS(list->buckets[0].achievements[0]->measured_progress, "5/6");
    ASSERT_FLOAT_EQUALS(list->buckets[0].achievements[0] ->measured_percent, 83.333333);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 1111);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Sample Game - Locked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 4);
    iter = list->buckets[1].achievements;
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 7);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 9);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 70);
    ASSERT_STR_EQUALS(achievement->measured_progress, "25,600/100,000");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 25.6);
    achievement = *iter++;
    ASSERT_NUM_EQUALS(achievement->id, 71);
    ASSERT_STR_EQUALS(achievement->measured_progress, "25%");
    ASSERT_FLOAT_EQUALS(achievement->measured_percent, 25.6);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 1111);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Sample Game - Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[2].num_achievements, 2);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[0]->id, 5);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[1]->id, 8);

    ASSERT_NUM_EQUALS(list->buckets[3].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[3].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[3].label, "Bonus - Locked");
    ASSERT_NUM_EQUALS(list->buckets[3].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[3].achievements[0]->id, 5503);

    ASSERT_NUM_EQUALS(list->buckets[4].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[4].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[4].label, "Bonus - Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[4].num_achievements, 2);
    ASSERT_NUM_EQUALS(list->buckets[4].achievements[0]->id, 5501);
    ASSERT_NUM_EQUALS(list->buckets[4].achievements[1]->id, 5502);

    rc_client_destroy_achievement_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_achievement_list_single_subset_buckets(void)
{
  rc_client_achievement_list_t* list;
  rc_client_achievement_list_params_t params;

  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_subset, unlock_8_and_5502);
  mock_memory(memory, sizeof(memory));

  rc_client_do_frame(g_client); /* advance achievements out of waiting state */
  reset_mock_events();

  mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=5&h=1&m=0123456789ABCDEF&v=732f8e30e9c1eb08948dda098c305d8b",
    "{\"Success\":true,\"Score\":5432,\"SoftcoreScore\":777,\"AchievementID\":5,\"AchievementsRemaining\":6}");
  mock_api_response("r=awardachievement&u=Username&t=ApiToken&a=5501&h=1&m=0123456789ABCDEF&v=9b9bdf5501eb6289a6655affbcc695e6",
    "{\"Success\":true,\"Score\":5437,\"SoftcoreScore\":777,\"AchievementID\":5,\"AchievementsRemaining\":6}");

  memset(&params, 0, sizeof(params));
  params.subset_id = 2345;
  params.category = RC_CLIENT_ACHIEVEMENT_CATEGORY_PROMOTED;
  params.grouping = RC_CLIENT_ACHIEVEMENT_LIST_GROUPING_PROGRESS;

  list = rc_client_create_subset_achievement_list(g_client, &params);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 5501);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[1]->id, 5503);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5502);

    rc_client_destroy_achievement_list(list);
  }

  memory[5] = 5; /* trigger achievement 5 */
  memory[6] = 2; /* start measuring achievement 6 */
  memory[1] = 1; /* begin challenge achievement 7 */
  memory[0x11] = 100; /* start measuring achievements 70 and 71 */
  memory[0x17] = 7; /* trigger achievement 5501 */
  rc_client_do_frame(g_client);
  reset_mock_events();

  /* set the unlock time for achievement 5 back one second to ensure consistent sorting */
  ((rc_client_achievement_t*)rc_client_get_achievement_info(g_client, 5))->unlock_time--;

  list = rc_client_create_subset_achievement_list(g_client, &params);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 3);

    /* achievement 7 is an active challenge, but not part of the selected subset */

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_RECENTLY_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Recently Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 5501);
    /* achievement 5 also recently unlocked, but not part of the selected subset */

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5503);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[2].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[2].achievements[0]->id, 5502);

    rc_client_destroy_achievement_list(list);
  }

  /* recently unlocked achievements no longer recent */
  ((rc_client_achievement_t*)rc_client_get_achievement_info(g_client, 5))->unlock_time -= 15 * 60;
  ((rc_client_achievement_t*)rc_client_get_achievement_info(g_client, 5501))->unlock_time -= 15 * 60;
  memory[6] = 5; /* almost there achievement 6 */
  memory[1] = 0; /* stop challenge achievement 7 */
  rc_client_do_frame(g_client);
  reset_mock_events();

  list = rc_client_create_subset_achievement_list(g_client, &params);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);

    /* achievement 6 is "almost there", but not part of the subset */

    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Locked");
    ASSERT_NUM_EQUALS(list->buckets[0].num_achievements, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].achievements[0]->id, 5503);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unlocked");
    ASSERT_NUM_EQUALS(list->buckets[1].num_achievements, 2);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[0]->id, 5501);
    ASSERT_NUM_EQUALS(list->buckets[1].achievements[1]->id, 5502);

    rc_client_destroy_achievement_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_get_next_achievement_first(void)
{
  const rc_client_achievement_t* achievement;
  g_client = mock_client_game_loaded(patchdata_subset, unlock_6_8h_and_9);

  achievement = rc_client_get_next_achievement_info(g_client, NULL, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->id, 5); /* 5 is the first locked achievement */

  achievement = rc_client_get_next_achievement_info(g_client, NULL, RC_CLIENT_ACHIEVEMENT_BUCKET_UNLOCKED);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->id, 8); /* 8 is the first unlocked achievement since 6 only has a casual unlock */

  achievement = rc_client_get_next_achievement_info(g_client, NULL, RC_CLIENT_ACHIEVEMENT_BUCKET_UNSUPPORTED);
  ASSERT_PTR_NULL(achievement); /* all achievements in this set should be supported */

  rc_client_destroy(g_client);
}

static void test_get_next_achievement_sequence(void)
{
  const rc_client_achievement_t* achievement;
  g_client = mock_client_game_loaded(patchdata_subset, unlock_6_8h_and_9);

  achievement = rc_client_get_next_achievement_info(g_client, NULL, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->id, 5); /* 5 is the first locked achievement */

  achievement = rc_client_get_next_achievement_info(g_client, achievement, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->id, 6);

  achievement = rc_client_get_next_achievement_info(g_client, achievement, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->id, 7);

  achievement = rc_client_get_next_achievement_info(g_client, achievement, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->id, 9); /* 8 was unlocked and is skipped */

  achievement = rc_client_get_next_achievement_info(g_client, achievement, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->id, 70);

  achievement = rc_client_get_next_achievement_info(g_client, achievement, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->id, 71);

  achievement = rc_client_get_next_achievement_info(g_client, achievement, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->id, 5501); /* done with core set, start iterating subset */

  achievement = rc_client_get_next_achievement_info(g_client, achievement, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->id, 5502);

  achievement = rc_client_get_next_achievement_info(g_client, achievement, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
  ASSERT_PTR_NOT_NULL(achievement);
  ASSERT_NUM_EQUALS(achievement->id, 5503);

  achievement = rc_client_get_next_achievement_info(g_client, achievement, RC_CLIENT_ACHIEVEMENT_BUCKET_LOCKED);
  ASSERT_PTR_NULL(achievement); /* done iterating subset */

  rc_client_destroy(g_client);
}

static void test_achievement_get_image_url(void)
{
  char buffer[256];
  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);

  ASSERT_NUM_EQUALS(rc_client_achievement_get_image_url(rc_client_get_achievement_info(g_client, 5501),
      RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED, buffer, sizeof(buffer)), RC_OK);
  ASSERT_STR_EQUALS(buffer, "https://media.retroachievements.org/Badge/00234.png");

  ASSERT_NUM_EQUALS(rc_client_achievement_get_image_url(rc_client_get_achievement_info(g_client, 5501),
      RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE, buffer, sizeof(buffer)), RC_OK);
  ASSERT_STR_EQUALS(buffer, "https://media.retroachievements.org/Badge/00234_lock.png");

  ASSERT_NUM_EQUALS(rc_client_achievement_get_image_url(rc_client_get_achievement_info(g_client, 5501),
      RC_CLIENT_ACHIEVEMENT_STATE_DISABLED, buffer, sizeof(buffer)), RC_OK);
  ASSERT_STR_EQUALS(buffer, "https://media.retroachievements.org/Badge/00234_lock.png");

  ASSERT_NUM_EQUALS(rc_client_achievement_get_image_url(rc_client_get_achievement_info(g_client, 5501),
      RC_CLIENT_ACHIEVEMENT_STATE_INACTIVE, buffer, sizeof(buffer)), RC_OK);
  ASSERT_STR_EQUALS(buffer, "https://media.retroachievements.org/Badge/00234_lock.png");

  ASSERT_NUM_EQUALS(rc_client_achievement_get_image_url(rc_client_get_achievement_info(g_client, 5501),
    RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED, buffer, 10), RC_INSUFFICIENT_BUFFER);
  ASSERT_NUM_EQUALS(rc_client_achievement_get_image_url(rc_client_get_achievement_info(g_client, 5501),
    RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE, buffer, 10), RC_INSUFFICIENT_BUFFER);

  rc_client_destroy(g_client);
}

/* ----- leaderboards ----- */

static void test_leaderboard_list_simple(void)
{
  rc_client_leaderboard_list_t* list;
  const rc_client_leaderboard_t** iter;
  const rc_client_leaderboard_t* leaderboard;
  uint8_t memory[16] = { 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0 };

  g_client = mock_client_logged_in();
  mock_memory(memory, sizeof(memory));
  mock_client_load_game(patchdata_exhaustive, no_unlocks);

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_NONE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_ALL);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "All");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 7);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 45);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 48);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 51);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 52);

    rc_client_destroy_leaderboard_list(list);
  }

  memory[0x0A] = 1; /* start 45,46,47 */
  rc_client_do_frame(g_client);

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_NONE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_ALL);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "All");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 7);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 45);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 48);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 51);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 52);

    rc_client_destroy_leaderboard_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_leaderboard_list_simple_with_unsupported(void)
{
  rc_client_leaderboard_list_t* list;
  const rc_client_leaderboard_t** iter;
  const rc_client_leaderboard_t* leaderboard;
  uint8_t memory[16] = { 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0 };

  g_client = mock_client_logged_in();
  mock_memory(memory, 0x0E); /* 0x0E address is now invalid (44,45,46,47,48)*/
  mock_client_load_game(patchdata_exhaustive, no_unlocks);

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_NONE);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_ALL);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "All");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 2);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 51);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 52);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_UNSUPPORTED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unsupported");
    ASSERT_NUM_EQUALS(list->buckets[1].num_leaderboards, 5);

    iter = list->buckets[1].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 45);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 48);

    rc_client_destroy_leaderboard_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_leaderboard_list_buckets(void)
{
  rc_client_leaderboard_list_t* list;
  const rc_client_leaderboard_t** iter;
  const rc_client_leaderboard_t* leaderboard;
  uint8_t memory[16] = { 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0 };

  g_client = mock_client_logged_in();
  mock_memory(memory, sizeof(memory));
  mock_client_load_game(patchdata_exhaustive, no_unlocks);

  rc_client_do_frame(g_client);

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_TRACKING);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Inactive");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 7);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 45);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 48);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 51);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 52);

    rc_client_destroy_leaderboard_list(list);
  }

  memory[0x0A] = 1; /* start 45,46,47 */
  rc_client_do_frame(g_client);

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_TRACKING);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_ACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Active");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 3);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 45);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Inactive");
    ASSERT_NUM_EQUALS(list->buckets[1].num_leaderboards, 4);

    iter = list->buckets[1].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 48);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 51);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 52);

    rc_client_destroy_leaderboard_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_leaderboard_list_buckets_with_unsupported(void)
{
  rc_client_leaderboard_list_t* list;
  const rc_client_leaderboard_t** iter;
  const rc_client_leaderboard_t* leaderboard;
  uint8_t memory[16] = { 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0 };

  g_client = mock_client_logged_in();
  mock_memory(memory, 0x0E); /* 0x0E address is now invalid (44,45,46,47,48)*/
  mock_client_load_game(patchdata_exhaustive, no_unlocks);

  rc_client_do_frame(g_client);

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_TRACKING);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Inactive");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 2);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 51);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 52);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_UNSUPPORTED);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Unsupported");
    ASSERT_NUM_EQUALS(list->buckets[1].num_leaderboards, 5);

    iter = list->buckets[1].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 45);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 48);

    rc_client_destroy_leaderboard_list(list);
  }

  memory[0x0B] = 3; /* start 52 */
  rc_client_do_frame(g_client);

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_TRACKING);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 3);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_ACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Active");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 1);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 52);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Inactive");
    ASSERT_NUM_EQUALS(list->buckets[1].num_leaderboards, 1);

    iter = list->buckets[1].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 51);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_UNSUPPORTED);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Unsupported");
    ASSERT_NUM_EQUALS(list->buckets[2].num_leaderboards, 5);

    iter = list->buckets[2].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 45);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 48);

    rc_client_destroy_leaderboard_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_leaderboard_list_subset(void)
{
  rc_client_leaderboard_list_t* list;
  const rc_client_leaderboard_t** iter;
  const rc_client_leaderboard_t* leaderboard;
  uint8_t memory[16] = { 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0 };

  g_client = mock_client_logged_in();
  mock_memory(memory, sizeof(memory));
  mock_client_load_game(patchdata_subset, no_unlocks);

  rc_client_do_frame(g_client);

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_TRACKING);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 1111);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Sample Game - Inactive");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 7);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 45);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 48);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 51);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 52);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Bonus - Inactive");
    ASSERT_NUM_EQUALS(list->buckets[1].num_leaderboards, 2);

    iter = list->buckets[1].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 81);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 82);

    rc_client_destroy_leaderboard_list(list);
  }

  memory[0x0A] = 1; /* start 45,46,47 */
  memory[0x08] = 2; /* start 82 */
  rc_client_do_frame(g_client);

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_TRACKING);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 3);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_ACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Active");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 4);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 45);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 82);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 1111);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Sample Game - Inactive");
    ASSERT_NUM_EQUALS(list->buckets[1].num_leaderboards, 4);

    iter = list->buckets[1].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 48);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 51);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 52);

    ASSERT_NUM_EQUALS(list->buckets[2].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[2].subset_id, 2345);
    ASSERT_STR_EQUALS(list->buckets[2].label, "Bonus - Inactive");
    ASSERT_NUM_EQUALS(list->buckets[2].num_leaderboards, 1);

    iter = list->buckets[2].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 81);

    rc_client_destroy_leaderboard_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_leaderboard_list_hidden(void)
{
  rc_client_leaderboard_list_t* list;
  const rc_client_leaderboard_t** iter;
  const rc_client_leaderboard_t* leaderboard;
  uint8_t memory[16] = { 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0 };

  g_client = mock_client_logged_in();
  mock_memory(memory, sizeof(memory));
  mock_client_load_game(patchdata_leaderboards_hidden, no_unlocks);

  rc_client_do_frame(g_client);

  /* hidden leaderboards (45+48) should not appear in list */

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_TRACKING);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Inactive");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 3);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);

    rc_client_destroy_leaderboard_list(list);
  }

  memory[0x0A] = 1; /* start 45,46,47 */
  rc_client_do_frame(g_client);

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_TRACKING);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_ACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Active");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 2);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Inactive");
    ASSERT_NUM_EQUALS(list->buckets[1].num_leaderboards, 1);

    iter = list->buckets[1].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);

    rc_client_destroy_leaderboard_list(list);
  }

  rc_client_destroy(g_client);
}

static void test_leaderboard_list_unpromoted(void)
{
  rc_client_leaderboard_list_t* list;
  const rc_client_leaderboard_t** iter;
  const rc_client_leaderboard_t* leaderboard;
  uint8_t memory[16] = { 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0 };

  g_client = mock_client_logged_in();
  mock_memory(memory, sizeof(memory));
  mock_client_load_game(patchdata_leaderboards_unpromoted, no_unlocks);

  rc_client_do_frame(g_client);

  /* unpromoted leaderboards (45+48) should not appear in list */

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_TRACKING);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 1);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Inactive");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 3);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);

    rc_client_destroy_leaderboard_list(list);
  }

  memory[0x0A] = 1; /* start 45,46,47 */
  rc_client_do_frame(g_client);

  list = rc_client_create_leaderboard_list(g_client, RC_CLIENT_LEADERBOARD_LIST_GROUPING_TRACKING);
  ASSERT_PTR_NOT_NULL(list);
  if (list) {
    ASSERT_NUM_EQUALS(list->num_buckets, 2);
    ASSERT_NUM_EQUALS(list->buckets[0].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_ACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[0].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[0].label, "Active");
    ASSERT_NUM_EQUALS(list->buckets[0].num_leaderboards, 2);

    iter = list->buckets[0].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 46);
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 47);

    ASSERT_NUM_EQUALS(list->buckets[1].bucket_type, RC_CLIENT_LEADERBOARD_BUCKET_INACTIVE);
    ASSERT_NUM_EQUALS(list->buckets[1].subset_id, 0);
    ASSERT_STR_EQUALS(list->buckets[1].label, "Inactive");
    ASSERT_NUM_EQUALS(list->buckets[1].num_leaderboards, 1);

    iter = list->buckets[1].leaderboards;
    leaderboard = *iter++;
    ASSERT_NUM_EQUALS(leaderboard->id, 44);

    rc_client_destroy_leaderboard_list(list);
  }

  rc_client_destroy(g_client);
}

static const char* lbinfo_4401_top_10 = "{\"Success\":true,\"LeaderboardData\":{\"LBID\":4401,\"GameID\":1234,"
    "\"LowerIsBetter\":1,\"LBTitle\":\"Leaderboard1\",\"LBDesc\":\"Desc1\",\"LBFormat\":\"SCORE\","
    "\"LBMem\":\"STA:0xH000C=1::CAN:0xH000D=1::SUB:0xH000D=2::VAL:0x 000E\",\"LBAuthor\":null,"
    "\"LBCreated\":\"2013-10-20 22:12:21\",\"LBUpdated\":\"2021-06-14 08:18:19\",\"TotalEntries\":78,"
    "\"Entries\":["
      "{\"User\":\"PlayerG\",\"Score\":3524,\"Rank\":1,\"Index\":1,\"DateSubmitted\":1615654895},"
      "{\"User\":\"PlayerB\",\"Score\":3645,\"Rank\":2,\"Index\":2,\"DateSubmitted\":1615634566},"
      "{\"User\":\"DisplayName\",\"Score\":3754,\"Rank\":3,\"Index\":3,\"DateSubmitted\":1615234553},"
      "{\"User\":\"PlayerC\",\"Score\":3811,\"Rank\":4,\"Index\":4,\"DateSubmitted\":1615653844},"
      "{\"User\":\"PlayerF\",\"Score\":3811,\"Rank\":4,\"Index\":5,\"DateSubmitted\":1615623878},"
      "{\"User\":\"PlayerA\",\"Score\":3811,\"Rank\":4,\"Index\":6,\"DateSubmitted\":1615653284},"
      "{\"User\":\"PlayerI\",\"Score\":3902,\"Rank\":7,\"Index\":7,\"DateSubmitted\":1615632174},"
      "{\"User\":\"PlayerE\",\"Score\":3956,\"Rank\":8,\"Index\":8,\"DateSubmitted\":1616384834},"
      "{\"User\":\"PlayerD\",\"Score\":3985,\"Rank\":9,\"Index\":9,\"DateSubmitted\":1615238383},"
      "{\"User\":\"PlayerH\",\"Score\":4012,\"Rank\":10,\"Index\":10,\"DateSubmitted\":1615638984}"
    "]"
  "}}";

static const char* lbinfo_4401_top_10_no_user = "{\"Success\":true,\"LeaderboardData\":{\"LBID\":4401,\"GameID\":1234,"
    "\"LowerIsBetter\":1,\"LBTitle\":\"Leaderboard1\",\"LBDesc\":\"Desc1\",\"LBFormat\":\"SCORE\","
    "\"LBMem\":\"STA:0xH000C=1::CAN:0xH000D=1::SUB:0xH000D=2::VAL:0x 000E\",\"LBAuthor\":null,"
    "\"LBCreated\":\"2013-10-20 22:12:21\",\"LBUpdated\":\"2021-06-14 08:18:19\",\"TotalEntries\":78,"
    "\"Entries\":["
      "{\"User\":\"PlayerG\",\"Score\":3524,\"Rank\":1,\"Index\":1,\"DateSubmitted\":1615654895},"
      "{\"User\":\"PlayerB\",\"Score\":3645,\"Rank\":2,\"Index\":2,\"DateSubmitted\":1615634566},"
      "{\"User\":\"PlayerJ\",\"Score\":3754,\"Rank\":3,\"Index\":3,\"DateSubmitted\":1615234553},"
      "{\"User\":\"PlayerC\",\"Score\":3811,\"Rank\":4,\"Index\":4,\"DateSubmitted\":1615653844},"
      "{\"User\":\"PlayerF\",\"Score\":3811,\"Rank\":4,\"Index\":5,\"DateSubmitted\":1615623878},"
      "{\"User\":\"PlayerA\",\"Score\":3811,\"Rank\":4,\"Index\":6,\"DateSubmitted\":1615653284},"
      "{\"User\":\"PlayerI\",\"Score\":3902,\"Rank\":7,\"Index\":7,\"DateSubmitted\":1615632174},"
      "{\"User\":\"PlayerE\",\"Score\":3956,\"Rank\":8,\"Index\":8,\"DateSubmitted\":1616384834},"
      "{\"User\":\"PlayerD\",\"Score\":3985,\"Rank\":9,\"Index\":9,\"DateSubmitted\":1615238383},"
      "{\"User\":\"PlayerH\",\"Score\":4012,\"Rank\":10,\"Index\":10,\"DateSubmitted\":1615638984}"
    "]"
  "}}";

static const char* lbinfo_4401_near_user = "{\"Success\":true,\"LeaderboardData\":{\"LBID\":4401,\"GameID\":1234,"
    "\"LowerIsBetter\":1,\"LBTitle\":\"Leaderboard1\",\"LBDesc\":\"Desc1\",\"LBFormat\":\"SCORE\","
    "\"LBMem\":\"STA:0xH000C=1::CAN:0xH000D=1::SUB:0xH000D=2::VAL:0x 000E\",\"LBAuthor\":null,"
    "\"LBCreated\":\"2013-10-20 22:12:21\",\"LBUpdated\":\"2021-06-14 08:18:19\",\"TotalEntries\":78,"
    "\"Entries\":["
      "{\"User\":\"PlayerG\",\"Score\":3524,\"Rank\":17,\"Index\":17,\"DateSubmitted\":1615654895},"
      "{\"User\":\"PlayerB\",\"Score\":3645,\"Rank\":18,\"Index\":18,\"DateSubmitted\":1615634566},"
      "{\"User\":\"PlayerC\",\"Score\":3811,\"Rank\":19,\"Index\":19,\"DateSubmitted\":1615653844},"
      "{\"User\":\"PlayerF\",\"Score\":3811,\"Rank\":19,\"Index\":20,\"DateSubmitted\":1615623878},"
      "{\"User\":\"DisplayName\",\"Score\":3811,\"Rank\":19,\"Index\":21,\"DateSubmitted\":1615234553},"
      "{\"User\":\"PlayerA\",\"Score\":3811,\"Rank\":19,\"Index\":22,\"DateSubmitted\":1615653284},"
      "{\"User\":\"PlayerI\",\"Score\":3902,\"Rank\":23,\"Index\":23,\"DateSubmitted\":1615632174},"
      "{\"User\":\"PlayerE\",\"Score\":3956,\"Rank\":24,\"Index\":24,\"DateSubmitted\":1616384834},"
      "{\"User\":\"PlayerD\",\"Score\":3985,\"Rank\":25,\"Index\":25,\"DateSubmitted\":1615238383},"
      "{\"User\":\"PlayerH\",\"Score\":4012,\"Rank\":26,\"Index\":26,\"DateSubmitted\":1615638984}"
    "]"
  "}}";

static rc_client_leaderboard_entry_list_t* g_leaderboard_entries = NULL;
static void rc_client_callback_expect_leaderboard_entry_list(int result, const char* error_message, rc_client_leaderboard_entry_list_t* list, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_OK);
  ASSERT_PTR_NULL(error_message);
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);

  ASSERT_PTR_NOT_NULL(list);
  g_leaderboard_entries = list;
}

static void test_fetch_leaderboard_entries(void)
{
  rc_client_leaderboard_entry_t* entry;
  char url[256];

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);
  g_leaderboard_entries = NULL;

  mock_api_response("r=lbinfo&i=4401&c=10", lbinfo_4401_top_10);

  rc_client_begin_fetch_leaderboard_entries(g_client, 4401, 1, 10,
      rc_client_callback_expect_leaderboard_entry_list, g_callback_userdata);
  ASSERT_PTR_NOT_NULL(g_leaderboard_entries);

  ASSERT_NUM_EQUALS(g_leaderboard_entries->num_entries, 10);
  ASSERT_NUM_EQUALS(g_leaderboard_entries->total_entries, 78);

  entry = g_leaderboard_entries->entries;
  ASSERT_STR_EQUALS(entry->user, "PlayerG");
  ASSERT_STR_EQUALS(entry->display, "003524");
  ASSERT_NUM_EQUALS(entry->index, 1);
  ASSERT_NUM_EQUALS(entry->rank, 1);
  ASSERT_NUM_EQUALS(entry->submitted, 1615654895);

  ASSERT_NUM_EQUALS(rc_client_leaderboard_entry_get_user_image_url(entry, url, sizeof(url)), RC_OK);
  ASSERT_STR_EQUALS(url, "https://media.retroachievements.org/UserPic/PlayerG.png");

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerB");
  ASSERT_STR_EQUALS(entry->display, "003645");
  ASSERT_NUM_EQUALS(entry->index, 2);
  ASSERT_NUM_EQUALS(entry->rank, 2);
  ASSERT_NUM_EQUALS(entry->submitted, 1615634566);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "DisplayName");
  ASSERT_STR_EQUALS(entry->display, "003754");
  ASSERT_NUM_EQUALS(entry->index, 3);
  ASSERT_NUM_EQUALS(entry->rank, 3);
  ASSERT_NUM_EQUALS(entry->submitted, 1615234553);

  ASSERT_NUM_EQUALS(rc_client_leaderboard_entry_get_user_image_url(entry, url, sizeof(url)), RC_OK);
  ASSERT_STR_EQUALS(url, "https://media.retroachievements.org/UserPic/DisplayName.png");

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerC");
  ASSERT_STR_EQUALS(entry->display, "003811");
  ASSERT_NUM_EQUALS(entry->index, 4);
  ASSERT_NUM_EQUALS(entry->rank, 4);
  ASSERT_NUM_EQUALS(entry->submitted, 1615653844);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerF");
  ASSERT_STR_EQUALS(entry->display, "003811");
  ASSERT_NUM_EQUALS(entry->index, 5);
  ASSERT_NUM_EQUALS(entry->rank, 4);
  ASSERT_NUM_EQUALS(entry->submitted, 1615623878);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerA");
  ASSERT_STR_EQUALS(entry->display, "003811");
  ASSERT_NUM_EQUALS(entry->index, 6);
  ASSERT_NUM_EQUALS(entry->rank, 4);
  ASSERT_NUM_EQUALS(entry->submitted, 1615653284);

  ASSERT_NUM_EQUALS(rc_client_leaderboard_entry_get_user_image_url(entry, url, sizeof(url)), RC_OK);
  ASSERT_STR_EQUALS(url, "https://media.retroachievements.org/UserPic/PlayerA.png");

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerI");
  ASSERT_STR_EQUALS(entry->display, "003902");
  ASSERT_NUM_EQUALS(entry->index, 7);
  ASSERT_NUM_EQUALS(entry->rank, 7);
  ASSERT_NUM_EQUALS(entry->submitted, 1615632174);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerE");
  ASSERT_STR_EQUALS(entry->display, "003956");
  ASSERT_NUM_EQUALS(entry->index, 8);
  ASSERT_NUM_EQUALS(entry->rank, 8);
  ASSERT_NUM_EQUALS(entry->submitted, 1616384834);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerD");
  ASSERT_STR_EQUALS(entry->display, "003985");
  ASSERT_NUM_EQUALS(entry->index, 9);
  ASSERT_NUM_EQUALS(entry->rank, 9);
  ASSERT_NUM_EQUALS(entry->submitted, 1615238383);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerH");
  ASSERT_STR_EQUALS(entry->display, "004012");
  ASSERT_NUM_EQUALS(entry->index, 10);
  ASSERT_NUM_EQUALS(entry->rank, 10);
  ASSERT_NUM_EQUALS(entry->submitted, 1615638984);

  ASSERT_NUM_EQUALS(g_leaderboard_entries->user_index, 2);

  rc_client_destroy_leaderboard_entry_list(g_leaderboard_entries);
  rc_client_destroy(g_client);
}

static void test_fetch_leaderboard_entries_no_user(void)
{
  rc_client_leaderboard_entry_t* entry;

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);
  g_leaderboard_entries = NULL;

  mock_api_response("r=lbinfo&i=4401&c=10", lbinfo_4401_top_10_no_user);

  rc_client_begin_fetch_leaderboard_entries(g_client, 4401, 1, 10,
      rc_client_callback_expect_leaderboard_entry_list, g_callback_userdata);
  ASSERT_PTR_NOT_NULL(g_leaderboard_entries);

  ASSERT_NUM_EQUALS(g_leaderboard_entries->num_entries, 10);

  entry = g_leaderboard_entries->entries;
  ASSERT_STR_EQUALS(entry->user, "PlayerG");
  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerB");
  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerJ");
  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerC");
  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerF");
  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerA");
  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerI");
  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerE");
  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerD");
  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerH");

  ASSERT_NUM_EQUALS(g_leaderboard_entries->user_index, -1);

  rc_client_destroy_leaderboard_entry_list(g_leaderboard_entries);
  rc_client_destroy(g_client);
}

static void test_fetch_leaderboard_entries_around_user(void)
{
  rc_client_leaderboard_entry_t* entry;

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);
  g_leaderboard_entries = NULL;

  mock_api_response("r=lbinfo&i=4401&u=Username&c=10", lbinfo_4401_near_user);

  rc_client_begin_fetch_leaderboard_entries_around_user(g_client, 4401, 10,
      rc_client_callback_expect_leaderboard_entry_list, g_callback_userdata);
  ASSERT_PTR_NOT_NULL(g_leaderboard_entries);

  ASSERT_NUM_EQUALS(g_leaderboard_entries->num_entries, 10);
  ASSERT_NUM_EQUALS(g_leaderboard_entries->total_entries, 78);

  entry = g_leaderboard_entries->entries;
  ASSERT_STR_EQUALS(entry->user, "PlayerG");
  ASSERT_STR_EQUALS(entry->display, "003524");
  ASSERT_NUM_EQUALS(entry->index, 17);
  ASSERT_NUM_EQUALS(entry->rank, 17);
  ASSERT_NUM_EQUALS(entry->submitted, 1615654895);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerB");
  ASSERT_STR_EQUALS(entry->display, "003645");
  ASSERT_NUM_EQUALS(entry->index, 18);
  ASSERT_NUM_EQUALS(entry->rank, 18);
  ASSERT_NUM_EQUALS(entry->submitted, 1615634566);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerC");
  ASSERT_STR_EQUALS(entry->display, "003811");
  ASSERT_NUM_EQUALS(entry->index, 19);
  ASSERT_NUM_EQUALS(entry->rank, 19);
  ASSERT_NUM_EQUALS(entry->submitted, 1615653844);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerF");
  ASSERT_STR_EQUALS(entry->display, "003811");
  ASSERT_NUM_EQUALS(entry->index, 20);
  ASSERT_NUM_EQUALS(entry->rank, 19);
  ASSERT_NUM_EQUALS(entry->submitted, 1615623878);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "DisplayName");
  ASSERT_STR_EQUALS(entry->display, "003811");
  ASSERT_NUM_EQUALS(entry->index, 21);
  ASSERT_NUM_EQUALS(entry->rank, 19);
  ASSERT_NUM_EQUALS(entry->submitted, 1615234553);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerA");
  ASSERT_STR_EQUALS(entry->display, "003811");
  ASSERT_NUM_EQUALS(entry->index, 22);
  ASSERT_NUM_EQUALS(entry->rank, 19);
  ASSERT_NUM_EQUALS(entry->submitted, 1615653284);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerI");
  ASSERT_STR_EQUALS(entry->display, "003902");
  ASSERT_NUM_EQUALS(entry->index, 23);
  ASSERT_NUM_EQUALS(entry->rank, 23);
  ASSERT_NUM_EQUALS(entry->submitted, 1615632174);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerE");
  ASSERT_STR_EQUALS(entry->display, "003956");
  ASSERT_NUM_EQUALS(entry->index, 24);
  ASSERT_NUM_EQUALS(entry->rank, 24);
  ASSERT_NUM_EQUALS(entry->submitted, 1616384834);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerD");
  ASSERT_STR_EQUALS(entry->display, "003985");
  ASSERT_NUM_EQUALS(entry->index, 25);
  ASSERT_NUM_EQUALS(entry->rank, 25);
  ASSERT_NUM_EQUALS(entry->submitted, 1615238383);

  ++entry;
  ASSERT_STR_EQUALS(entry->user, "PlayerH");
  ASSERT_STR_EQUALS(entry->display, "004012");
  ASSERT_NUM_EQUALS(entry->index, 26);
  ASSERT_NUM_EQUALS(entry->rank, 26);
  ASSERT_NUM_EQUALS(entry->submitted, 1615638984);

  ASSERT_NUM_EQUALS(g_leaderboard_entries->user_index, 4);

  rc_client_destroy_leaderboard_entry_list(g_leaderboard_entries);
  rc_client_destroy(g_client);
}

static void rc_client_callback_expect_leaderboard_entry_list_login_required(int result, const char* error_message,
    rc_client_leaderboard_entry_list_t* list, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_LOGIN_REQUIRED);
  ASSERT_STR_EQUALS(error_message, "Login required");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
  ASSERT_PTR_NULL(list);
}

static void test_fetch_leaderboard_entries_around_user_not_logged_in(void)
{
  g_client = mock_client_not_logged_in();
  g_leaderboard_entries = NULL;

  mock_api_response("r=lbinfo&i=4401&u=Username&c=10", lbinfo_4401_near_user);

  rc_client_begin_fetch_leaderboard_entries_around_user(g_client, 4401, 10,
      rc_client_callback_expect_leaderboard_entry_list_login_required, g_callback_userdata);
  ASSERT_PTR_NULL(g_leaderboard_entries);

  assert_api_not_called("r=lbinfo&i=4401&u=Username&c=10");

  rc_client_destroy(g_client);
}

static void rc_client_callback_expect_leaderboard_uncalled(int result, const char* error_message,
  rc_client_leaderboard_entry_list_t* list, rc_client_t* client, void* callback_userdata)
{
  (void)result;
  (void)error_message;
  (void)list;
  (void)client;
  (void)callback_userdata;

  ASSERT_FAIL("Callback should not have been called.")
}

static void test_fetch_leaderboard_entries_async_aborted(void)
{
  rc_client_async_handle_t* handle;

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);
  mock_client_enable_async(g_client);

  g_leaderboard_entries = NULL;

  handle = rc_client_begin_fetch_leaderboard_entries(g_client, 4401, 1, 10,
    rc_client_callback_expect_leaderboard_uncalled, g_callback_userdata);

  rc_client_abort_async(g_client, handle);

  async_api_response("r=lbinfo&i=4401&c=10", lbinfo_4401_top_10);
  ASSERT_PTR_NULL(g_leaderboard_entries);

  rc_client_destroy(g_client);
}

static void rc_client_callback_expect_leaderboard_internet_not_available(int result, const char* error_message,
    rc_client_leaderboard_entry_list_t* list, rc_client_t* client, void* callback_userdata)
{
  ASSERT_NUM_EQUALS(result, RC_NO_RESPONSE);
  ASSERT_STR_EQUALS(error_message, "Internet not available.");
  ASSERT_PTR_EQUALS(client, g_client);
  ASSERT_PTR_EQUALS(callback_userdata, g_callback_userdata);
  ASSERT_PTR_NULL(list);
}

static void test_fetch_leaderboard_entries_client_error(void)
{
  rc_client_async_handle_t* handle;

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, no_unlocks);

  g_leaderboard_entries = NULL;

  mock_api_error("r=lbinfo&i=4401&c=10", "Internet not available.", RC_API_SERVER_RESPONSE_CLIENT_ERROR);

  handle = rc_client_begin_fetch_leaderboard_entries(g_client, 4401, 1, 10,
      rc_client_callback_expect_leaderboard_internet_not_available, g_callback_userdata);

  ASSERT_PTR_NULL(handle);
  ASSERT_PTR_NULL(g_leaderboard_entries);

  rc_client_destroy(g_client);
}

static void test_has_leaderboards(void)
{
  g_client = mock_client_logged_in();
  mock_client_load_game(patchdata_leaderboard_only, no_unlocks);

  ASSERT_NUM_EQUALS(rc_client_has_leaderboards(g_client), 1);

  rc_client_destroy(g_client);
}

static void test_has_leaderboards_none(void)
{
  g_client = mock_client_logged_in();
  mock_client_load_game(patchdata_2ach_0lbd, no_unlocks);

  ASSERT_NUM_EQUALS(rc_client_has_leaderboards(g_client), 0);

  rc_client_destroy(g_client);
}

static void test_has_leaderboards_with_hidden(void)
{
  g_client = mock_client_logged_in();
  mock_client_load_game(patchdata_leaderboards_hidden, no_unlocks);

  ASSERT_NUM_EQUALS(rc_client_has_leaderboards(g_client), 1);

  rc_client_destroy(g_client);
}

static void test_has_leaderboards_only_hidden(void)
{
  g_client = mock_client_logged_in();
  mock_client_load_game(patchdata_leaderboards_only_hidden, no_unlocks);

  ASSERT_NUM_EQUALS(rc_client_has_leaderboards(g_client), 0);

  rc_client_destroy(g_client);
}

static void test_map_leaderboard_format(void)
{
  int i;

  for (i = 0; i < 30; ++i) {
    switch (i) {
      case RC_FORMAT_SECONDS:
      case RC_FORMAT_CENTISECS:
      case RC_FORMAT_MINUTES:
      case RC_FORMAT_SECONDS_AS_MINUTES:
      case RC_FORMAT_FRAMES:
        ASSERT_NUM_EQUALS(rc_client_map_leaderboard_format(i), RC_CLIENT_LEADERBOARD_FORMAT_TIME);
        break;

      case RC_FORMAT_SCORE:
        ASSERT_NUM_EQUALS(rc_client_map_leaderboard_format(i), RC_CLIENT_LEADERBOARD_FORMAT_SCORE);
        break;

      default:
        ASSERT_NUM_EQUALS(rc_client_map_leaderboard_format(i), RC_CLIENT_LEADERBOARD_FORMAT_VALUE);
        break;
    }
  }
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

/* ----- settings ----- */

static void test_set_hardcore_disable(void)
{
  const rc_client_achievement_t* achievement;
  const rc_client_leaderboard_t* leaderboard;
  const rc_trigger_t* trigger;

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, unlock_5501h_and_5502);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);

  achievement = rc_client_get_achievement_info(g_client, 5501);
  ASSERT_PTR_NOT_NULL(achievement);
  trigger = ((rc_client_achievement_info_t*)achievement)->trigger;
  ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_TRIGGERED);

  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  trigger = ((rc_client_achievement_info_t*)achievement)->trigger;
  ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_CASUAL);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_WAITING);

  ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, 1); /* only 5502 should be active*/

  leaderboard = rc_client_get_leaderboard_info(g_client, 4401);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
  ASSERT_NUM_EQUALS(g_client->game->runtime.lboard_count, 1);

  rc_client_set_hardcore_enabled(g_client, 0);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 0);
  ASSERT_NUM_EQUALS(g_client->game->waiting_for_reset, 0);

  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  trigger = ((rc_client_achievement_info_t*)achievement)->trigger;

  ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_CASUAL);
  ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
  ASSERT_NUM_EQUALS(trigger->state, RC_TRIGGER_STATE_TRIGGERED);
  ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, 0); /* 5502 should not be active*/

  leaderboard = rc_client_get_leaderboard_info(g_client, 4401);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_INACTIVE);
  ASSERT_NUM_EQUALS(g_client->game->runtime.lboard_count, 0);

  rc_client_destroy(g_client);
}

static void test_set_hardcore_disable_active_tracker(void)
{
  const rc_client_leaderboard_t* leaderboard;
  rc_client_event_t* event;
  uint8_t memory[64];
  memset(memory, 0, sizeof(memory));

  g_client = mock_client_game_loaded(patchdata_2ach_1lbd, unlock_5501h_and_5502);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  mock_memory(memory, sizeof(memory));

  rc_client_do_frame(g_client);

  memory[0x0C] = 1;
  memory[0x0E] = 25;
  reset_mock_events();
  rc_client_do_frame(g_client);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 2);

  event = find_event(RC_CLIENT_EVENT_LEADERBOARD_STARTED, 4401);
  ASSERT_PTR_NOT_NULL(event);

  event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_SHOW, 1);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);
  ASSERT_STR_EQUALS(event->leaderboard_tracker->display, "000025");

  leaderboard = rc_client_get_leaderboard_info(g_client, 4401);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_TRACKING);

  reset_mock_events();
  rc_client_set_hardcore_enabled(g_client, 0);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 0);
  ASSERT_NUM_EQUALS(g_client->game->waiting_for_reset, 0);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 1);

  leaderboard = rc_client_get_leaderboard_info(g_client, 4401);
  ASSERT_PTR_NOT_NULL(leaderboard);
  ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_INACTIVE);

  event = find_event(RC_CLIENT_EVENT_LEADERBOARD_TRACKER_HIDE, 1);
  ASSERT_PTR_NOT_NULL(event);
  ASSERT_NUM_EQUALS(event->leaderboard_tracker->id, 1);

  rc_client_destroy(g_client);
}

static void test_set_hardcore_enable(void)
{
  const rc_client_achievement_t* achievement;
  const rc_client_leaderboard_t* leaderboard;

  g_client = mock_client_logged_in();
  rc_client_set_hardcore_enabled(g_client, 0);
  mock_client_load_game_casual(patchdata_2ach_1lbd, unlock_5501h_and_5502);

  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 0);

  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_CASUAL);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, 0); /* 5502 should not be active*/
  }

  leaderboard = rc_client_get_leaderboard_info(g_client, 4401);
  ASSERT_PTR_NOT_NULL(leaderboard);
  if (leaderboard) {
    ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_INACTIVE);
    ASSERT_NUM_EQUALS(g_client->game->runtime.lboard_count, 0);
  }

  /* when enabling hardcore, flag waiting_for_reset. this will prevent processing until rc_client_reset is called */
  reset_mock_events();
  rc_client_set_hardcore_enabled(g_client, 1);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(g_client->game->waiting_for_reset, 1);
  ASSERT_PTR_NOT_NULL(find_event(RC_CLIENT_EVENT_RESET, 0));

  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_CASUAL);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, 1); /* 5502 should be active*/
  }

  leaderboard = rc_client_get_leaderboard_info(g_client, 4401);
  ASSERT_PTR_NOT_NULL(leaderboard);
  if (leaderboard) {
    ASSERT_NUM_EQUALS(leaderboard->state, RC_CLIENT_LEADERBOARD_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(g_client->game->runtime.lboard_count, 1);
  }

  /* resetting clears waiting_for_reset */
  rc_client_reset(g_client);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(g_client->game->waiting_for_reset, 0);

  /* hardcore already enabled, attempting to set it again shouldn't flag waiting_for_reset */
  rc_client_set_hardcore_enabled(g_client, 1);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(g_client->game->waiting_for_reset, 0);

  rc_client_destroy(g_client);
}

static void test_set_hardcore_enable_no_game_loaded(void)
{
  g_client = mock_client_logged_in();
  rc_client_set_hardcore_enabled(g_client, 0);

  /* enabling hardcore before a game is loaded just toggles the flag  */
  reset_mock_events();
  rc_client_set_hardcore_enabled(g_client, 1);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(get_pending_event_count(), 0);

  rc_client_destroy(g_client);
}

static void test_set_hardcore_enable_encore_mode(void)
{
  const rc_client_achievement_t* achievement;
  rc_client_achievement_info_t* achievement_info;

  g_client = mock_client_logged_in();
  rc_client_set_encore_mode_enabled(g_client, 1);
  mock_client_load_game(patchdata_2ach_1lbd, unlock_5501h_and_5502);

  ASSERT_NUM_EQUALS(rc_client_get_encore_mode_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, 2);

  g_client->game->runtime.triggers[0].trigger->state = RC_TRIGGER_STATE_ACTIVE;
  g_client->game->runtime.triggers[1].trigger->state = RC_TRIGGER_STATE_ACTIVE;

  achievement = rc_client_get_achievement_info(g_client, 5501);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH); /* unlock information still tracked */
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);     /* but achievement remains active */
    ASSERT_NUM_EQUALS(g_client->game->runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_ACTIVE);
  }
  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_CASUAL);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(g_client->game->runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_ACTIVE);
  }

  /* toggle hardcore mode should retain active achievements */
  rc_client_set_hardcore_enabled(g_client, 0);
  ASSERT_NUM_EQUALS(rc_client_get_encore_mode_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 0);
  ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, 2);

  achievement = rc_client_get_achievement_info(g_client, 5501);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(g_client->game->runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_ACTIVE);
  }
  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_CASUAL);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(g_client->game->runtime.triggers[1].trigger->state, RC_TRIGGER_STATE_ACTIVE);
  }

  /* toggle hardcore mode should retain active achievements */
  rc_client_set_hardcore_enabled(g_client, 1);
  ASSERT_NUM_EQUALS(rc_client_get_encore_mode_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, 2);

  /* trigger an achievement */
  achievement_info = (rc_client_achievement_info_t*)rc_client_get_achievement_info(g_client, 5501);
  achievement_info->public_.state = RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED;
  g_client->game->runtime.triggers[0].trigger->state = RC_TRIGGER_STATE_TRIGGERED;

  /* toggle hardcore mode should retain active achievements */
  rc_client_set_hardcore_enabled(g_client, 0);
  ASSERT_NUM_EQUALS(rc_client_get_encore_mode_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 0);
  ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, 1); /* only one active now */

  achievement = rc_client_get_achievement_info(g_client, 5501);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
  }
  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_CASUAL);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
    ASSERT_NUM_EQUALS(g_client->game->runtime.triggers[0].trigger->state, RC_TRIGGER_STATE_ACTIVE);
    ASSERT_PTR_EQUALS(((rc_client_achievement_info_t*)achievement)->trigger, g_client->game->runtime.triggers[0].trigger);
  }

  /* toggle hardcore mode should retain active achievements */
  rc_client_set_hardcore_enabled(g_client, 1);
  ASSERT_NUM_EQUALS(rc_client_get_encore_mode_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(rc_client_get_hardcore_enabled(g_client), 1);
  ASSERT_NUM_EQUALS(g_client->game->runtime.trigger_count, 1);

  rc_client_destroy(g_client);
}

static void test_set_encore_mode_enable(void)
{
  const rc_client_achievement_t* achievement;

  g_client = mock_client_logged_in();
  rc_client_set_encore_mode_enabled(g_client, 1);
  mock_client_load_game(patchdata_2ach_1lbd, unlock_5501h_and_5502);

  ASSERT_NUM_EQUALS(rc_client_get_encore_mode_enabled(g_client), 1);

  achievement = rc_client_get_achievement_info(g_client, 5501);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH); /* unlock information still tracked */
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);     /* but achievement remains active */
  }
  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_CASUAL);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  }

  /* toggle encore mode with a game loaded has no effect */
  rc_client_set_encore_mode_enabled(g_client, 0);
  ASSERT_NUM_EQUALS(rc_client_get_encore_mode_enabled(g_client), 0);

  achievement = rc_client_get_achievement_info(g_client, 5501);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  }
  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_CASUAL);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  }

  rc_client_destroy(g_client);
}

static void test_set_encore_mode_disable(void)
{
  const rc_client_achievement_t* achievement;

  g_client = mock_client_logged_in();
  rc_client_set_encore_mode_enabled(g_client, 1);
  rc_client_set_encore_mode_enabled(g_client, 0);
  mock_client_load_game(patchdata_2ach_1lbd, unlock_5501h_and_5502);

  ASSERT_NUM_EQUALS(rc_client_get_encore_mode_enabled(g_client), 0);

  achievement = rc_client_get_achievement_info(g_client, 5501);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
  }
  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_CASUAL);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  }

  /* toggle encore mode with a game loaded has no effect */
  rc_client_set_encore_mode_enabled(g_client, 1);
  ASSERT_NUM_EQUALS(rc_client_get_encore_mode_enabled(g_client), 1);

  achievement = rc_client_get_achievement_info(g_client, 5501);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_BOTH);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_UNLOCKED);
  }
  achievement = rc_client_get_achievement_info(g_client, 5502);
  ASSERT_PTR_NOT_NULL(achievement);
  if (achievement) {
    ASSERT_NUM_EQUALS(achievement->unlocked, RC_CLIENT_ACHIEVEMENT_UNLOCKED_CASUAL);
    ASSERT_NUM_EQUALS(achievement->state, RC_CLIENT_ACHIEVEMENT_STATE_ACTIVE);
  }

  rc_client_destroy(g_client);
}

static void test_get_user_agent_clause(void)
{
  char expected_clause[] = "rcheevos/" RCHEEVOS_VERSION_STRING;
  char buffer[64];

  g_client = mock_client_not_logged_in();
  ASSERT_NUM_EQUALS(rc_client_get_user_agent_clause(g_client, buffer, sizeof(buffer)), sizeof(expected_clause) - 1);
  ASSERT_STR_EQUALS(buffer, expected_clause);

  /* snprintf will return the number of characters it wants, even if the buffer is too small,
   * but will only fill as much of the buffer is available */
  ASSERT_NUM_EQUALS(rc_client_get_user_agent_clause(g_client, buffer, 8), sizeof(expected_clause) - 1);
  ASSERT_STR_EQUALS(buffer, "rcheevo");

  rc_client_destroy(g_client);
}

static void test_version(void)
{
  ASSERT_STR_EQUALS(rc_version_string(), RCHEEVOS_VERSION_STRING);
  ASSERT_NUM_EQUALS(rc_version(), RCHEEVOS_VERSION);
}

/* ----- harness ----- */

void test_client(void) {
  TEST_SUITE_BEGIN();

  /* user */
  TEST(test_get_user_game_summary);
  TEST(test_get_user_game_summary_casual);
  TEST(test_get_user_game_summary_encore_mode);
  TEST(test_get_user_game_summary_with_unsupported_and_unpromoted);
  TEST(test_get_user_game_summary_with_unsupported_unlocks);
  TEST(test_get_user_game_summary_with_unpromoted_off);
  TEST(test_get_user_game_summary_no_achievements);
  TEST(test_get_user_game_summary_unknown_game);
  TEST(test_get_user_game_summary_progress_incomplete);
  TEST(test_get_user_game_summary_progress_progression_no_win);
  TEST(test_get_user_game_summary_progress_win_only);
  TEST(test_get_user_game_summary_beat);
  TEST(test_get_user_game_summary_mastery);
  TEST(test_get_user_game_summary_warning);

  /* game */
  TEST(test_game_get_image_url);

  /* hash library */
  TEST(test_fetch_hash_library);

  /* game titles */
  TEST(test_fetch_game_titles);

  /* game list */
  TEST(test_fetch_game_list);

  /* all user progress */
  TEST(test_fetch_all_user_progress);

  /* subset */
  TEST(test_load_subset);
  TEST(test_subset_list);

  /* achievements */
  TEST(test_achievement_list_simple);
  TEST(test_achievement_list_simple_with_unlocks);
  TEST(test_achievement_list_simple_with_unlocks_encore_mode);
  TEST(test_achievement_list_simple_with_unpromoted_and_unsupported);
  TEST(test_achievement_list_simple_with_unpromoted_off);
  TEST(test_achievement_list_buckets);
  TEST(test_achievement_list_buckets_progress_sort);
  TEST(test_achievement_list_buckets_progress_sort_big_ids);
  TEST(test_achievement_list_buckets_with_unsynced);
  TEST(test_achievement_list_subset_with_unpromoted_and_unsupported);
  TEST(test_achievement_list_subset_buckets);
  TEST(test_achievement_list_single_subset_buckets);

  TEST(test_get_next_achievement_first);
  TEST(test_get_next_achievement_sequence);

  TEST(test_achievement_get_image_url);

  /* leaderboards */
  TEST(test_leaderboard_list_simple);
  TEST(test_leaderboard_list_simple_with_unsupported);
  TEST(test_leaderboard_list_buckets);
  TEST(test_leaderboard_list_buckets_with_unsupported);
  TEST(test_leaderboard_list_subset);
  TEST(test_leaderboard_list_hidden);
  TEST(test_leaderboard_list_unpromoted);

  TEST(test_fetch_leaderboard_entries);
  TEST(test_fetch_leaderboard_entries_no_user);
  TEST(test_fetch_leaderboard_entries_around_user);
  TEST(test_fetch_leaderboard_entries_around_user_not_logged_in);
  TEST(test_fetch_leaderboard_entries_async_aborted);
  TEST(test_fetch_leaderboard_entries_client_error);

  TEST(test_has_leaderboards);
  TEST(test_has_leaderboards_none);
  TEST(test_has_leaderboards_with_hidden);
  TEST(test_has_leaderboards_only_hidden);

  TEST(test_map_leaderboard_format);

  /* clock */
  TEST(test_clock_get_now_millisecs);

  /* settings */
  TEST(test_set_hardcore_disable);
  TEST(test_set_hardcore_disable_active_tracker);
  TEST(test_set_hardcore_enable);
  TEST(test_set_hardcore_enable_no_game_loaded);
  TEST(test_set_hardcore_enable_encore_mode);
  TEST(test_set_encore_mode_enable);
  TEST(test_set_encore_mode_disable);

  TEST(test_get_user_agent_clause);
  TEST(test_version);

  TEST_SUITE_END();
}
