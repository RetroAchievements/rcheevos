#include "rc_api.h"
#include "rc_api_common.h"

#include "../rcheevos/rc_compat.h"
#include "../rhash/md5.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* --- Resolve Hash --- */

int rc_api_init_resolve_hash_request(rc_api_request_t* request, const rc_api_resolve_hash_request_t* api_params)
{
  rc_api_url_builder_t builder;

  rc_buf_init(&request->buffer);
  rc_api_url_build_dorequest(&builder, &request->buffer, "gameid", api_params->username);
  rc_url_builder_append_str_param(&builder, "m", api_params->game_hash);
  request->url = rc_url_builder_finalize(&builder);

  if (builder.result != RC_OK)
    return builder.result;

  rc_url_builder_init(&builder, &request->buffer, 48);
  rc_url_builder_append_str_param(&builder, "t", api_params->api_token);
  request->post_data = rc_url_builder_finalize(&builder);

  return builder.result;
}

int rc_api_process_resolve_hash_response(rc_api_resolve_hash_response_t* response, const char* server_response)
{
  int result;
  rc_json_field_t fields[] = {
    {"Success"},
    {"Error"},
    {"GameID"},
  };

  memset(response, 0, sizeof(*response));
  rc_buf_init(&response->response.buffer);

  result = rc_json_parse_response(&response->response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
  if (result != RC_OK)
    return result;

  rc_json_get_required_unum(&response->game_id, &response->response, &fields[2], "GameID");
  return RC_OK;
}

void rc_api_destroy_resolve_hash_response(rc_api_resolve_hash_response_t* response)
{
  rc_buf_destroy(&response->response.buffer);
}

/* --- Award Achievement --- */

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
    {"AchievementID"}
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

  rc_json_get_optional_unum(&response->new_player_score, &fields[2], "Score", 0);
  rc_json_get_optional_unum(&response->awarded_achievement_id, &fields[3], "AchievementID", 0);

  return RC_OK;
}

void rc_api_destroy_award_achievement_response(rc_api_award_achievement_response_t* response)
{
  rc_buf_destroy(&response->response.buffer);
}

/* --- Submit Leaderboard Entry --- */

int rc_api_init_submit_lboard_entry_request(rc_api_request_t* request, const rc_api_submit_lboard_entry_request_t* api_params)
{
    rc_api_url_builder_t builder;
    char signature[96];
    char checksum[33];
    md5_state_t md5;
    md5_byte_t digest[16];

    rc_buf_init(&request->buffer);
    rc_api_url_build_dorequest(&builder, &request->buffer, "submitlbentry", api_params->username);
    rc_url_builder_append_num_param(&builder, "i", api_params->leaderboard_id);
    rc_url_builder_append_signed_num_param(&builder, "s", api_params->score);
    if (api_params->game_hash && *api_params->game_hash)
      rc_url_builder_append_str_param(&builder, "m", api_params->game_hash);
    request->url = rc_url_builder_finalize(&builder);

    if (builder.result != RC_OK)
      return builder.result;

    /* Evaluate the signature. */
    snprintf(signature, sizeof(signature), "%u%s%u", api_params->leaderboard_id, api_params->username, api_params->leaderboard_id);
    md5_init(&md5);
    md5_append(&md5, (unsigned char*)signature, (int)strlen(signature));
    md5_finish(&md5, digest);
    snprintf(checksum, sizeof(checksum), "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
      digest[0], digest[1], digest[2], digest[3], digest[4], digest[5], digest[6], digest[7],
      digest[8], digest[9], digest[10], digest[11], digest[12], digest[13], digest[14], digest[15]
    );

    rc_url_builder_init(&builder, &request->buffer, 48);
    rc_url_builder_append_str_param(&builder, "t", api_params->api_token);
    rc_url_builder_append_str_param(&builder, "v", checksum);
    request->post_data = rc_url_builder_finalize(&builder);

    return builder.result;
}

int rc_api_process_submit_lboard_entry_response(rc_api_submit_lboard_entry_response_t* response, const char* server_response)
{
    rc_api_lboard_entry_t* entry;
    rc_json_field_t iterator;
    const char* str;
    int result;

    rc_json_field_t fields[] = {
      {"Success"},
      {"Error"},
      {"Response"} /* nested object */
    };

    rc_json_field_t response_fields[] = {
      {"Score"},
      {"BestScore"},
      {"RankInfo"}, /* nested object */
      {"TopEntries"} /* array */
      /* unused fields
      {"LBData"}, / * array * /
      {"ScoreFormatted"},
      {"TopEntriesFriends"}, / * array * /
       * unused fields */
    };

    /* unused fields
    rc_json_field_t lbdata_fields[] = {
      {"Format"},
      {"LeaderboardID"},
      {"GameID"},
      {"Title"},
      {"LowerIsBetter"}
    };
     * unused fields */

    rc_json_field_t entry_fields[] = {
      {"User"},
      {"Rank"},
      {"Score"}
      /* unused fields
      { "DateSumitted" },
       * unused fields */
    };

    rc_json_field_t rank_info_fields[] = {
      {"Rank"},
      {"NumEntries"}
      /* unused fields
      {"LowerIsBetter"},
      {"UserRank"},
       * unused fields */
    };

    memset(response, 0, sizeof(*response));
    rc_buf_init(&response->response.buffer);

    result = rc_json_parse_response(&response->response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
    if (result != RC_OK)
      return result;

    if (!rc_json_get_required_object(response_fields, sizeof(response_fields) / sizeof(response_fields[0]), &response->response, &fields[2], "Response"))
      return RC_MISSING_VALUE;
    if (!rc_json_get_required_num(&response->submitted_score, &response->response, &response_fields[0], "Score"))
      return RC_MISSING_VALUE;
    if (!rc_json_get_required_num(&response->best_score, &response->response, &response_fields[1], "BestScore"))
      return RC_MISSING_VALUE;

    if (!rc_json_get_required_object(rank_info_fields, sizeof(rank_info_fields) / sizeof(rank_info_fields[0]), &response->response, &response_fields[2], "RankInfo"))
      return RC_MISSING_VALUE;
    if (!rc_json_get_required_unum(&response->new_rank, &response->response, &rank_info_fields[0], "Rank"))
      return RC_MISSING_VALUE;
    if (!rc_json_get_required_string(&str, &response->response, &rank_info_fields[1], "NumEntries"))
      return RC_MISSING_VALUE;
    response->num_entries = (unsigned)atoi(str);

    if (!rc_json_get_required_array(&response->num_top_entries, &iterator, &response->response, &response_fields[3], "TopEntries"))
      return RC_MISSING_VALUE;

    if (response->num_top_entries) {
      response->top_entries = (rc_api_lboard_entry_t*)rc_buf_alloc(&response->response.buffer, response->num_top_entries * sizeof(rc_api_lboard_entry_t));
      if (!response->top_entries)
        return RC_OUT_OF_MEMORY;

      entry = response->top_entries;
      while (rc_json_get_array_entry_object(entry_fields, sizeof(entry_fields) / sizeof(entry_fields[0]), &iterator)) {
        if (!rc_json_get_required_string(&entry->username, &response->response, &entry_fields[0], "User"))
          return RC_MISSING_VALUE;

        if (!rc_json_get_required_unum(&entry->rank, &response->response, &entry_fields[1], "Rank"))
          return RC_MISSING_VALUE;

        if (!rc_json_get_required_num(&entry->score, &response->response, &entry_fields[2], "Score"))
          return RC_MISSING_VALUE;

        ++entry;
      }
    }

    return RC_OK;
}

void rc_api_destroy_submit_lboard_entry_response(rc_api_submit_lboard_entry_response_t* response)
{
    rc_buf_destroy(&response->response.buffer);
}
