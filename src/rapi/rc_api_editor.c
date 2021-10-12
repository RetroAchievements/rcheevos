#include "rc_api_editor.h"
#include "rc_api_common.h"

#include <stdlib.h>
#include <string.h>

/* --- Fetch Code Notes --- */

int rc_api_init_fetch_code_notes_request(rc_api_request_t* request, const rc_api_fetch_code_notes_request_t* api_params) {
  rc_api_url_builder_t builder;

  rc_api_url_build_dorequest_url(request);

  if (api_params->game_id == 0)
    return RC_INVALID_STATE;

  rc_url_builder_init(&builder, &request->buffer, 48);
  rc_url_builder_append_str_param(&builder, "r", "codenotes2");
  rc_url_builder_append_unum_param(&builder, "g", api_params->game_id);

  request->post_data = rc_url_builder_finalize(&builder);

  return builder.result;
}

int rc_api_process_fetch_code_notes_response(rc_api_fetch_code_notes_response_t* response, const char* server_response) {
  rc_json_field_t iterator;
  rc_api_code_note_t* note;
  const char* address_str;
  const char* last_author = "";
  size_t last_author_len = 0;
  size_t len;
  int result;

  rc_json_field_t fields[] = {
    {"Success"},
    {"Error"},
    {"CodeNotes"}
  };

  rc_json_field_t note_fields[] = {
    {"Address"},
    {"User"},
    {"Note"}
  };

  memset(response, 0, sizeof(*response));
  rc_buf_init(&response->response.buffer);

  result = rc_json_parse_response(&response->response, server_response, fields, sizeof(fields) / sizeof(fields[0]));
  if (result != RC_OK || !response->response.succeeded)
    return result;

  if (!rc_json_get_required_array(&response->num_notes, &iterator, &response->response, &fields[2], "CodeNotes"))
    return RC_MISSING_VALUE;

  if (response->num_notes) {
    response->notes = (rc_api_code_note_t*)rc_buf_alloc(&response->response.buffer, response->num_notes * sizeof(rc_api_code_note_t));
    if (!response->notes)
      return RC_OUT_OF_MEMORY;

    note = response->notes;
    while (rc_json_get_array_entry_object(note_fields, sizeof(note_fields) / sizeof(note_fields[0]), &iterator)) {
      /* an empty note represents a record that was deleted on the server */
      /* a note set to '' also represents a deleted note (remnant of a bug) */
      /* NOTE: len will include the quotes */
      len = note_fields[2].value_end - note_fields[2].value_start;
      if (len == 2 || (len == 4 && note_fields[2].value_start[1] == '\'' && note_fields[2].value_start[2] == '\'')) {
        --response->num_notes;
        continue;
      }

      if (!rc_json_get_required_string(&address_str, &response->response, &note_fields[0], "Address"))
        return RC_MISSING_VALUE;
      note->address = (unsigned)strtol(address_str, NULL, 16);
      if (!rc_json_get_required_string(&note->note, &response->response, &note_fields[2], "Note"))
        return RC_MISSING_VALUE;

      len = note_fields[1].value_end - note_fields[1].value_start;
      if (len == last_author_len && memcmp(note_fields[1].value_start, last_author, len) == 0) {
        note->author = last_author;
      }
      else {
        if (!rc_json_get_required_string(&note->author, &response->response, &note_fields[1], "User"))
          return RC_MISSING_VALUE;

        last_author = note->author;
        last_author_len = len;
      }

      ++note;
    }
  }

  return RC_OK;
}

void rc_api_destroy_fetch_code_notes_response(rc_api_fetch_code_notes_response_t* response) {
  rc_buf_destroy(&response->response.buffer);
}
