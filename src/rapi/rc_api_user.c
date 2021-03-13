#include "rc_api.h"
#include "rc_api_common.h"

#include <string.h>

/* --- Login --- */

int rc_api_init_login_request(rc_api_request_t* request, const rc_api_login_request_t* api_params)
{
  rc_api_url_builder_t builder;

  rc_buf_init(&request->buffer);
  rc_api_url_build_dorequest(&builder, &request->buffer, "login", api_params->username);
  request->url = rc_url_builder_finalize(&builder);

  if (builder.result != RC_OK)
    return builder.result;

  rc_url_builder_init(&builder, &request->buffer, 48);

  if (api_params->password && api_params->password[0])
    rc_url_builder_append_str_param(&builder, "p", api_params->password);
  else
    rc_url_builder_append_str_param(&builder, "t", api_params->api_token);

  request->post_data = rc_url_builder_finalize(&builder);

  return builder.result;
}

int rc_api_process_login_response(rc_api_login_response_t* response, const char* server_response)
{
  int result;
  rc_json_field_t fields[] = {
    {"Success"},
    {"Error"},
    {"User"},
    {"Token"},
    {"Score"},
    {"Messages"}
  };

  memset(response, 0, sizeof(*response));
  rc_buf_init(&response->response.buffer);

  result = rc_json_parse_response(&response->response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
  if (result != RC_OK || !response->response.succeeded)
    return result;

  if (!rc_json_get_required_string(&response->username, &response->response, &fields[2], "User"))
    return RC_MISSING_VALUE;
  if (!rc_json_get_required_string(&response->api_token, &response->response, &fields[3], "Token"))
    return RC_MISSING_VALUE;

  rc_json_get_optional_unum(&response->score, &fields[4], "Score", 0);
  rc_json_get_optional_unum(&response->num_unread_messages, &fields[5], "Messages", 0);

  return RC_OK;
}

void rc_api_destroy_login_response(rc_api_login_response_t* response)
{
  rc_buf_destroy(&response->response.buffer);
}

/* --- Start Session --- */

int rc_api_init_start_session_request(rc_api_request_t* request, const rc_api_start_session_request_t* api_params)
{
  rc_api_url_builder_t builder;

  rc_buf_init(&request->buffer);
  rc_api_url_build_dorequest(&builder, &request->buffer, "postactivity", api_params->username);

  /* activity type enum (only 3 is used )
    *  1 = earned achievement - handled by awardachievement
    *  2 = logged in - handled by login
    *  3 = started playing
    *  4 = uploaded achievement - handled by uploadachievement
    *  5 = modified achievement - handled by uploadachievement
    */
  rc_url_builder_append_unum_param(&builder, "a", 3);
  rc_url_builder_append_unum_param(&builder, "m", api_params->game_id);
  request->url = rc_url_builder_finalize(&builder);

  if (builder.result != RC_OK)
    return builder.result;

  rc_url_builder_init(&builder, &request->buffer, 48);
  rc_url_builder_append_str_param(&builder, "t", api_params->api_token);
  request->post_data = rc_url_builder_finalize(&builder);

  return builder.result;
}

int rc_api_process_start_session_response(rc_api_start_session_response_t* response, const char* server_response)
{
  rc_json_field_t fields[] = {
    {"Success"},
    {"Error"},
  };

  memset(response, 0, sizeof(*response));
  rc_buf_init(&response->response.buffer);

  return rc_json_parse_response(&response->response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
}

void rc_api_destroy_start_session_response(rc_api_start_session_response_t* response)
{
  rc_buf_destroy(&response->response.buffer);
}
