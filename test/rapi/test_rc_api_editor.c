#include "rc_api_editor.h"

#include "../test_framework.h"
#include "rc_compat.h"

#define DOREQUEST_URL "https://retroachievements.org/dorequest.php"

static void test_init_fetch_code_notes_request()
{
  rc_api_fetch_code_notes_request_t fetch_code_notes_request;
  rc_api_request_t request;

  memset(&fetch_code_notes_request, 0, sizeof(fetch_code_notes_request));
  fetch_code_notes_request.game_id = 1234;

  ASSERT_NUM_EQUALS(rc_api_init_fetch_code_notes_request(&request, &fetch_code_notes_request), RC_OK);
  ASSERT_STR_EQUALS(request.url, DOREQUEST_URL);
  ASSERT_STR_EQUALS(request.post_data, "r=codenotes2&g=1234");

  rc_api_destroy_request(&request);
}

static void test_init_fetch_code_notes_request_no_game_id()
{
  rc_api_fetch_code_notes_request_t fetch_code_notes_request;
  rc_api_request_t request;

  memset(&fetch_code_notes_request, 0, sizeof(fetch_code_notes_request));

  ASSERT_NUM_EQUALS(rc_api_init_fetch_code_notes_request(&request, &fetch_code_notes_request), RC_INVALID_STATE);

  rc_api_destroy_request(&request);
}

static void test_init_fetch_code_notes_response_empty_array()
{
  rc_api_fetch_code_notes_response_t fetch_code_notes_response;
  const char* server_response = "{\"Success\":true,\"CodeNotes\":[]}";
  memset(&fetch_code_notes_response, 0, sizeof(fetch_code_notes_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_code_notes_response(&fetch_code_notes_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_code_notes_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_code_notes_response.response.error_message);
  ASSERT_PTR_NULL(fetch_code_notes_response.notes);
  ASSERT_NUM_EQUALS(fetch_code_notes_response.num_notes, 0);

  rc_api_destroy_fetch_code_notes_response(&fetch_code_notes_response);
}

static void test_init_fetch_code_notes_response_one_item()
{
  rc_api_fetch_code_notes_response_t fetch_code_notes_response;
  const char* server_response = "{\"Success\":true,\"CodeNotes\":["
      "{\"User\":\"User\",\"Address\":\"0x001234\",\"Note\":\"01=true\"}"
      "]}";
  memset(&fetch_code_notes_response, 0, sizeof(fetch_code_notes_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_code_notes_response(&fetch_code_notes_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_code_notes_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_code_notes_response.response.error_message);
  ASSERT_NUM_EQUALS(fetch_code_notes_response.num_notes, 1);
  ASSERT_PTR_NOT_NULL(fetch_code_notes_response.notes);
  ASSERT_NUM_EQUALS(fetch_code_notes_response.notes[0].address, 0x1234);
  ASSERT_STR_EQUALS(fetch_code_notes_response.notes[0].author, "User");
  ASSERT_STR_EQUALS(fetch_code_notes_response.notes[0].note, "01=true");

  rc_api_destroy_fetch_code_notes_response(&fetch_code_notes_response);
}

static void test_init_fetch_code_notes_response_several_items()
{
  rc_api_fetch_code_notes_response_t fetch_code_notes_response;
  const char* server_response = "{\"Success\":true,\"CodeNotes\":["
      "{\"User\":\"User\",\"Address\":\"0x001234\",\"Note\":\"01=true\"},"
      "{\"User\":\"User\",\"Address\":\"0x002000\",\"Note\":\"Happy\"},"
      "{\"User\":\"User2\",\"Address\":\"0x002002\",\"Note\":\"Sad\"},"
      "{\"User\":\"User\",\"Address\":\"0x002ABC\",\"Note\":\"Banana\\n0=a\"}"
      "]}";
  memset(&fetch_code_notes_response, 0, sizeof(fetch_code_notes_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_code_notes_response(&fetch_code_notes_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_code_notes_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_code_notes_response.response.error_message);
  ASSERT_NUM_EQUALS(fetch_code_notes_response.num_notes, 4);
  ASSERT_PTR_NOT_NULL(fetch_code_notes_response.notes);

  ASSERT_NUM_EQUALS(fetch_code_notes_response.notes[0].address, 0x1234);
  ASSERT_STR_EQUALS(fetch_code_notes_response.notes[0].author, "User");
  ASSERT_STR_EQUALS(fetch_code_notes_response.notes[0].note, "01=true");

  ASSERT_NUM_EQUALS(fetch_code_notes_response.notes[1].address, 0x2000);
  ASSERT_STR_EQUALS(fetch_code_notes_response.notes[1].author, "User");
  ASSERT_STR_EQUALS(fetch_code_notes_response.notes[1].note, "Happy");

  ASSERT_NUM_EQUALS(fetch_code_notes_response.notes[2].address, 0x2002);
  ASSERT_STR_EQUALS(fetch_code_notes_response.notes[2].author, "User2");
  ASSERT_STR_EQUALS(fetch_code_notes_response.notes[2].note, "Sad");

  ASSERT_NUM_EQUALS(fetch_code_notes_response.notes[3].address, 0x2ABC);
  ASSERT_STR_EQUALS(fetch_code_notes_response.notes[3].author, "User");
  ASSERT_STR_EQUALS(fetch_code_notes_response.notes[3].note, "Banana\n0=a");

  rc_api_destroy_fetch_code_notes_response(&fetch_code_notes_response);
}

static void test_init_fetch_code_notes_response_deleted_items()
{
  rc_api_fetch_code_notes_response_t fetch_code_notes_response;
  const char* server_response = "{\"Success\":true,\"CodeNotes\":["
      "{\"User\":\"User\",\"Address\":\"0x001234\",\"Note\":\"\"},"
      "{\"User\":\"User\",\"Address\":\"0x002000\",\"Note\":\"Happy\"},"
      "{\"User\":\"User2\",\"Address\":\"0x002002\",\"Note\":\"''\"},"
      "{\"User\":\"User\",\"Address\":\"0x002ABC\",\"Note\":\"\"}"
      "]}";
  memset(&fetch_code_notes_response, 0, sizeof(fetch_code_notes_response));

  ASSERT_NUM_EQUALS(rc_api_process_fetch_code_notes_response(&fetch_code_notes_response, server_response), RC_OK);
  ASSERT_NUM_EQUALS(fetch_code_notes_response.response.succeeded, 1);
  ASSERT_PTR_NULL(fetch_code_notes_response.response.error_message);
  ASSERT_NUM_EQUALS(fetch_code_notes_response.num_notes, 1);
  ASSERT_PTR_NOT_NULL(fetch_code_notes_response.notes);

  ASSERT_NUM_EQUALS(fetch_code_notes_response.notes[0].address, 0x2000);
  ASSERT_STR_EQUALS(fetch_code_notes_response.notes[0].author, "User");
  ASSERT_STR_EQUALS(fetch_code_notes_response.notes[0].note, "Happy");

  rc_api_destroy_fetch_code_notes_response(&fetch_code_notes_response);
}

void test_rapi_editor(void) {
  TEST_SUITE_BEGIN();

  /* fetch code notes */
  TEST(test_init_fetch_code_notes_request);
  TEST(test_init_fetch_code_notes_request_no_game_id);

  TEST(test_init_fetch_code_notes_response_empty_array);
  TEST(test_init_fetch_code_notes_response_one_item);
  TEST(test_init_fetch_code_notes_response_several_items);
  TEST(test_init_fetch_code_notes_response_deleted_items);

  TEST_SUITE_END();
}
