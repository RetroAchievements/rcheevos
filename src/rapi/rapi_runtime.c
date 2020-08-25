#include "rapi.h"
#include "rapi_common.h"

#include <string.h>

int rc_api_init_award_achievement_request(rc_api_request_t* request, const rc_api_award_achievement_request_t* api_params)
{
  rc_api_url_builder_t builder;

  rc_buf_init(&request->buffer);
  rc_api_url_build_dorequest(&builder, &request->buffer, "awardachievement", api_params->username);
  rc_url_builder_append_num_param(&builder, "a", api_params->achievement_id);
  rc_url_builder_append_num_param(&builder, "h", api_params->hardcore ? 1 : 0);
  if (api_params->game_hash && *api_params->game_hash)
    rc_url_builder_append_str_param(&builder, "m", api_params->game_hash);
  request->url = rc_url_builder_finalize(&builder);

  if (builder.result != RC_OK)
    return builder.result;

  rc_url_builder_init(&builder, &request->buffer, 48);
  rc_url_builder_append_str_param(&builder, "t", api_params->api_token);
  request->post_data = rc_url_builder_finalize(&builder);

  return builder.result;
}

int rc_api_process_award_achievement_response(rc_api_award_achievement_response_t* response, const char* server_response)
{
  int result;
  rc_json_field_t fields[] = {
    {"Success"},
    {"Error"},
    {"Score"},
    {"AchievementID"},
  };

  memset(response, 0, sizeof(*response));
  rc_buf_init(&response->response.buffer);

  result = rc_json_parse_response(&response->response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
  if (result != RC_OK)
    return result;

  if (!response->response.succeeded) {
    if (response->response.error_message &&
        memcmp(response->response.error_message, "User already has", 16) == 0) {
      /* not really an error, the achievement is unlocked, just not by the current call.
       *  hardcore:     User already has hardcore and regular achievements awarded.
       *  non-hardcore: User already has this achievement awarded.
       */
      response->response.succeeded = 1;
    } else {
      return result;
    }
  }

  rc_json_get_optional_num(&response->new_player_score, &fields[2], "Score", 0);
  rc_json_get_optional_num(&response->awarded_achievement_id, &fields[3], "AchievementID", 0);

  return RC_OK;
}

void rc_api_destroy_award_achievement_response(rc_api_award_achievement_response_t* response)
{
  rc_buf_destroy(&response->response.buffer);
}
