#include "rapi.h"

#include "../test_framework.h"

#define DOREQUEST_URL "http://retroachievements.org/dorequest.php"

static void test_init_award_achievement_request_hardcore()
{
  rc_api_award_achievement_request_t award_achievement_request;
  rc_api_request_t request;

  memset(&award_achievement_request, 0, sizeof(award_achievement_request));
  award_achievement_request.username = "Username";
  award_achievement_request.api_token = "API_TOKEN";
  award_achievement_request.achievement_id = 1234;
  award_achievement_request.hardcore = 1;
  award_achievement_request.game_hash = "ABCDEF0123456789";

  ASSERT_NUM_EQUALS(rc_api_init_award_achievement_request(&request, &award_achievement_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL "?r=awardachievement&u=Username&a=1234&h=1&m=ABCDEF0123456789");
  ASSERT_STR_EQUALS(request.post_data, "t=API_TOKEN");

  rc_api_destroy_request(&request);
}

static void test_init_award_achievement_request_non_hardcore()
{
  rc_api_award_achievement_request_t award_achievement_request;
  rc_api_request_t request;

  memset(&award_achievement_request, 0, sizeof(award_achievement_request));
  award_achievement_request.username = "Username";
  award_achievement_request.api_token = "API_TOKEN";
  award_achievement_request.achievement_id = 5432;
  award_achievement_request.hardcore = 0;
  award_achievement_request.game_hash = "ABABCBCBDEDEFFFF";

  ASSERT_NUM_EQUALS(rc_api_init_award_achievement_request(&request, &award_achievement_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL "?r=awardachievement&u=Username&a=5432&h=0&m=ABABCBCBDEDEFFFF");
  ASSERT_STR_EQUALS(request.post_data, "t=API_TOKEN");

  rc_api_destroy_request(&request);
}

static void test_init_award_achievement_request_no_hash()
{
  rc_api_award_achievement_request_t award_achievement_request;
  rc_api_request_t request;

  memset(&award_achievement_request, 0, sizeof(award_achievement_request));
  award_achievement_request.username = "Username";
  award_achievement_request.api_token = "API_TOKEN";
  award_achievement_request.achievement_id = 1234;
  award_achievement_request.hardcore = 1;

  ASSERT_NUM_EQUALS(rc_api_init_award_achievement_request(&request, &award_achievement_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL "?r=awardachievement&u=Username&a=1234&h=1");
  ASSERT_STR_EQUALS(request.post_data, "t=API_TOKEN");

  rc_api_destroy_request(&request);
}

static void test_process_award_achievement_response_success()
{
  rc_api_award_achievement_response_t award_achievement_response;
  const char* server_response = "{\"Success\":true,\"Score\":119102,\"AchievementID\":56481}";

  memset(&award_achievement_response, 0, sizeof(award_achievement_response));

  ASSERT_NUM_EQUALS(rc_api_process_award_achievement_response(&award_achievement_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(award_achievement_response.response.succeeded, 1);
  ASSERT_PTR_NULL(award_achievement_response.response.error_message);
  ASSERT_NUM_EQUALS(award_achievement_response.new_player_score, 119102);
  ASSERT_NUM_EQUALS(award_achievement_response.awarded_achievement_id, 56481);

  rc_api_destroy_award_achievement_response(&award_achievement_response);
}

static void test_process_award_achievement_response_hardcore_already_unlocked()
{
  rc_api_award_achievement_response_t award_achievement_response;
  const char* server_response = "{\"Success\":false,\"Error\":\"User already has hardcore and regular achievements awarded.\",\"Score\":119210,\"AchievementID\":56494}";

  memset(&award_achievement_response, 0, sizeof(award_achievement_response));

  ASSERT_NUM_EQUALS(rc_api_process_award_achievement_response(&award_achievement_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(award_achievement_response.response.succeeded, 1);
  ASSERT_STR_EQUALS(award_achievement_response.response.error_message, "User already has hardcore and regular achievements awarded.");
  ASSERT_NUM_EQUALS(award_achievement_response.new_player_score, 119210);
  ASSERT_NUM_EQUALS(award_achievement_response.awarded_achievement_id, 56494);

  rc_api_destroy_award_achievement_response(&award_achievement_response);
}

static void test_process_award_achievement_response_non_hardcore_already_unlocked()
{
  rc_api_award_achievement_response_t award_achievement_response;
  const char* server_response = "{\"Success\":false,\"Error\":\"User already has this achievement awarded.\",\"Score\":119210,\"AchievementID\":56494}";

  memset(&award_achievement_response, 0, sizeof(award_achievement_response));

  ASSERT_NUM_EQUALS(rc_api_process_award_achievement_response(&award_achievement_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(award_achievement_response.response.succeeded, 1);
  ASSERT_STR_EQUALS(award_achievement_response.response.error_message, "User already has this achievement awarded.");
  ASSERT_NUM_EQUALS(award_achievement_response.new_player_score, 119210);
  ASSERT_NUM_EQUALS(award_achievement_response.awarded_achievement_id, 56494);

  rc_api_destroy_award_achievement_response(&award_achievement_response);
}

static void test_process_award_achievement_response_generic_failure()
{
  rc_api_award_achievement_response_t award_achievement_response;
  const char* server_response = "{\"Success\":false}";

  memset(&award_achievement_response, 0, sizeof(award_achievement_response));

  ASSERT_NUM_EQUALS(rc_api_process_award_achievement_response(&award_achievement_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(award_achievement_response.response.succeeded, 0);
  ASSERT_PTR_NULL(award_achievement_response.response.error_message);
  ASSERT_NUM_EQUALS(award_achievement_response.new_player_score, 0);
  ASSERT_NUM_EQUALS(award_achievement_response.awarded_achievement_id, 0);

  rc_api_destroy_award_achievement_response(&award_achievement_response);
}

static void test_process_award_achievement_response_empty()
{
  rc_api_award_achievement_response_t award_achievement_response;
  const char* server_response = "";

  memset(&award_achievement_response, 0, sizeof(award_achievement_response));

  ASSERT_NUM_EQUALS(rc_api_process_award_achievement_response(&award_achievement_response, server_response), RC_INVALID_JSON);
  ASSERT_NUM_EQUALS(award_achievement_response.response.succeeded, 0);
  ASSERT_PTR_NULL(award_achievement_response.response.error_message);
  ASSERT_NUM_EQUALS(award_achievement_response.new_player_score, 0);
  ASSERT_NUM_EQUALS(award_achievement_response.awarded_achievement_id, 0);

  rc_api_destroy_award_achievement_response(&award_achievement_response);
}

static void test_process_award_achievement_response_text()
{
  rc_api_award_achievement_response_t award_achievement_response;
  const char* server_response = "You do not have access to that resource";

  memset(&award_achievement_response, 0, sizeof(award_achievement_response));

  ASSERT_NUM_EQUALS(rc_api_process_award_achievement_response(&award_achievement_response, server_response), RC_INVALID_JSON);
  ASSERT_NUM_EQUALS(award_achievement_response.response.succeeded, 0);
  ASSERT_STR_EQUALS(award_achievement_response.response.error_message, "You do not have access to that resource");
  ASSERT_NUM_EQUALS(award_achievement_response.new_player_score, 0);
  ASSERT_NUM_EQUALS(award_achievement_response.awarded_achievement_id, 0);

  rc_api_destroy_award_achievement_response(&award_achievement_response);
}

static void test_process_award_achievement_response_no_fields()
{
  rc_api_award_achievement_response_t award_achievement_response;
  const char* server_response = "{\"Success\":true}";

  memset(&award_achievement_response, 0, sizeof(award_achievement_response));

  ASSERT_NUM_EQUALS(rc_api_process_award_achievement_response(&award_achievement_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(award_achievement_response.response.succeeded, 1);
  ASSERT_PTR_NULL(award_achievement_response.response.error_message);
  ASSERT_NUM_EQUALS(award_achievement_response.new_player_score, 0);
  ASSERT_NUM_EQUALS(award_achievement_response.awarded_achievement_id, 0);

  rc_api_destroy_award_achievement_response(&award_achievement_response);
}

void test_rapi_runtime(void) {
  TEST_SUITE_BEGIN();

  /* awardachievement */
  TEST(test_init_award_achievement_request_hardcore);
  TEST(test_init_award_achievement_request_non_hardcore);
  TEST(test_init_award_achievement_request_no_hash);

  TEST(test_process_award_achievement_response_success);
  TEST(test_process_award_achievement_response_hardcore_already_unlocked);
  TEST(test_process_award_achievement_response_non_hardcore_already_unlocked);
  TEST(test_process_award_achievement_response_generic_failure);
  TEST(test_process_award_achievement_response_empty);
  TEST(test_process_award_achievement_response_text);
  TEST(test_process_award_achievement_response_no_fields);

  TEST_SUITE_END();
}
