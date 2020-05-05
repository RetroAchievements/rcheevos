#include "rapi.h"
#include "rapi_common.h"

#include <string.h>

int rc_api_init_login_request(rc_api_request_t* request, const rc_api_login_request_t* login)
{
  rc_api_url_builder_t builder;

  rc_buf_init(&request->buffer);
  rc_api_url_build_dorequest(&builder, &request->buffer, "login", login->username);
  request->url = rc_url_builder_finalize(&builder);

  if (builder.result != RC_OK)
    return builder.result;

  rc_url_builder_init(&builder, &request->buffer, 64);

  if (login->password && login->password[0])
    rc_url_builder_append_str_param(&builder, "p", login->password);
  else
    rc_url_builder_append_str_param(&builder, "t", login->api_token);

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
    {"Messages"},
  };

  memset(response, 0, sizeof(*response));
  rc_buf_init(&response->response.buffer);

  result = rc_json_parse_response(server_response, fields, sizeof(fields) / sizeof(fields[0]));
  if (rc_json_get_string(&response->response.error_message, &response->response.buffer, &fields[1], "Error"))
    return result;
  if (result != RC_OK)
    return result;
  if (rc_json_get_bool(&response->response.succeeded, &fields[0], "Success") && !response->response.succeeded)
    return RC_OK;

  if (!rc_json_get_required_string(&response->username, &response->response, &fields[2], "User"))
    return RC_MISSING_VALUE;
  if (!rc_json_get_required_string(&response->api_token, &response->response, &fields[3], "Token"))
    return RC_MISSING_VALUE;

  rc_json_get_optional_num(&response->score, &fields[4], "Score", 0);
  rc_json_get_optional_num(&response->num_unread_messages, &fields[5], "Messages", 0);

  return RC_OK;
}

void rc_api_destroy_login_response(rc_api_login_response_t* response)
{
  rc_buf_destroy(&response->response.buffer);
}
