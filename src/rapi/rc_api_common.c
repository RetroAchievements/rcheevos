#include "rc_api.h"
#include "rc_api_common.h"

#include "rc_compat.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RETROACHIEVEMENTS_HOST "https://retroachievements.org"
static char* g_host = NULL;

static int rc_json_parse_object(const char** json_ptr, rc_json_field_t* fields, size_t field_count);
static int rc_json_parse_array(const char** json_ptr, rc_json_field_t* field);

static int rc_json_parse_field(const char** json_ptr, rc_json_field_t* field)
{
  int result;

  field->value_start = *json_ptr;

  switch (**json_ptr)
  {
    case '"': /* quoted string */
      ++(*json_ptr);
      while (**json_ptr != '"') {
        if (**json_ptr == '\\')
          ++(*json_ptr);

        if (**json_ptr == '\0')
          return RC_INVALID_JSON;

        ++(*json_ptr);
      }
      ++(*json_ptr);
      break;

    case '-':
    case '+': /* signed number */
      ++(*json_ptr);
      /* fallthrough to number */
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9': /* number */
      do {
        ++(*json_ptr);
      } while (**json_ptr >= '0' && **json_ptr <= '9');
      if (**json_ptr == '.') {
        do {
          ++(*json_ptr);
        } while (**json_ptr >= '0' && **json_ptr <= '9');
      }
      break;

    case '[': /* array */
      result = rc_json_parse_array(json_ptr, field);
      if (result != RC_OK)
          return result;

      break;

    case '{': /* object */
      result = rc_json_parse_object(json_ptr, NULL, 0);
      if (result != RC_OK)
        return result;

      break;

    default: /* non-quoted text [true,false,null] */
      if (!isalpha(**json_ptr))
        return RC_INVALID_JSON;

      do {
        ++(*json_ptr);
      } while (isalnum(**json_ptr));
      break;
  }

  field->value_end = *json_ptr;
  return RC_OK;
}

static int rc_json_parse_array(const char** json_ptr, rc_json_field_t* field)
{
  rc_json_field_t unused_field;
  const char* json = *json_ptr;
  int result;

  if (*json != '[')
    return RC_INVALID_JSON;
  ++json;

  field->array_size = 0;
  if (*json != ']') {
    do
    {
      while (isspace(*json))
        ++json;

      result = rc_json_parse_field(&json, &unused_field);
      if (result != RC_OK)
        return result;

      ++field->array_size;

      while (isspace(*json))
        ++json;

      if (*json != ',')
        break;

      ++json;
    } while (1);

    if (*json != ']')
      return RC_INVALID_JSON;
  }

  *json_ptr = ++json;
  return RC_OK;
}

static int rc_json_parse_object(const char** json_ptr, rc_json_field_t* fields, size_t field_count)
{
  rc_json_field_t non_matching_field;
  rc_json_field_t* field;
  const char* json = *json_ptr;
  const char* key_start;
  size_t key_len;
  size_t i;

  for (i = 0; i < field_count; ++i)
    fields[i].value_start = fields[i].value_end = NULL;

  if (*json != '{')
    return RC_INVALID_JSON;
  ++json;

  do
  {
    while (isspace(*json))
      ++json;

    if (*json != '"')
      return RC_INVALID_JSON;

    key_start = ++json;
    while (*json != '"') {
      if (!*json)
        return RC_INVALID_JSON;
      ++json;
    }
    key_len = json - key_start;
    ++json;

    while (isspace(*json))
      ++json;

    if (*json != ':')
      return RC_INVALID_JSON;

    ++json;

    while (isspace(*json))
      ++json;

    field = &non_matching_field;
    for (i = 0; i < field_count; ++i) {
      if (!fields[i].value_start && strncmp(fields[i].name, key_start, key_len) == 0 && fields[i].name[key_len] == '\0') {
        field = &fields[i];
        break;
      }
    }

    if (rc_json_parse_field(&json, field) < 0)
      return RC_INVALID_JSON;

    while (isspace(*json))
      ++json;

    if (*json != ',')
      break;

    ++json;
  } while (1);

  if (*json != '}')
    return RC_INVALID_JSON;

  *json_ptr = ++json;
  return RC_OK;
}

int rc_json_parse_response(rc_api_response_t* response, const char* json, rc_json_field_t* fields, size_t field_count)
{
#ifndef NDEBUG
  if (field_count < 2)
    return RC_INVALID_STATE;
  if (strcmp(fields[0].name, "Success") != 0)
    return RC_INVALID_STATE;
  if (strcmp(fields[1].name, "Error") != 0)
    return RC_INVALID_STATE;
#endif

  if (*json == '{')
  {
    int result = rc_json_parse_object(&json, fields, field_count);

    rc_json_get_string(&response->error_message, &response->buffer, &fields[1], "Error");
    rc_json_get_bool(&response->succeeded, &fields[0], "Success");

    return result;
  }

  if (*json) {
    const char* end = json;
    while (*end && *end != '\n' && end - json < 200)
      ++end;

    if (end > json && end[-1] == '\r')
      --end;

    if (end > json) {
      char* dst = rc_buf_reserve(&response->buffer, (end - json) + 1);
      response->error_message = dst;
      memcpy(dst, json, end - json);
      dst += (end - json);
      *dst++ = '\0';
      rc_buf_consume(&response->buffer, response->error_message, dst);
    }
  }

  response->succeeded = 0;
  return RC_INVALID_JSON;
}

static int rc_json_missing_field(rc_api_response_t* response, const rc_json_field_t* field)
{
  const char* not_found = " not found in response";
  const size_t not_found_len = strlen(not_found);
  const size_t field_len = strlen(field->name);

  char* write = rc_buf_reserve(&response->buffer, field_len + not_found_len + 1);
  if (write) {
    response->error_message = write;
    memcpy(write, field->name, field_len);
    write += field_len;
    memcpy(write, not_found, not_found_len + 1);
    write += not_found_len + 1;
    rc_buf_consume(&response->buffer, response->error_message, write);
  }

  response->succeeded = 0;
  return 0;
}

int rc_json_get_required_object(rc_json_field_t* fields, size_t field_count, rc_api_response_t* response, rc_json_field_t* field, const char* field_name)
{
  const char* json = field->value_start;

  if (!json)
    return rc_json_missing_field(response, field);

  return (rc_json_parse_object(&json, fields, field_count) == RC_OK);
}

int rc_json_get_required_array(int* num_entries, rc_json_field_t* iterator, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name)
{
  if (!field->value_start || *field->value_start != '[')
    return rc_json_missing_field(response, field);

  memcpy(iterator, field, sizeof(*iterator));
  ++iterator->value_start; /* skip [ */

  *num_entries = field->array_size;
  return 1;
}

int rc_json_get_array_entry_object(rc_json_field_t* fields, size_t field_count, rc_json_field_t* iterator)
{
  if (!iterator->array_size)
    return 0;

  while (isspace(*iterator->value_start))
    ++iterator->value_start;

  rc_json_parse_object(&iterator->value_start, fields, field_count);

  while (isspace(*iterator->value_start))
    ++iterator->value_start;

  ++iterator->value_start; /* skip , or ] */

  --iterator->array_size;
  return 1;
}

int rc_json_get_string(const char** out, rc_api_buffer_t* buffer, const rc_json_field_t* field, const char* field_name)
{
  const char* src = field->value_start;
  size_t len = field->value_end - field->value_start;
  char* dst;

#ifndef NDEBUG
  if (strcmp(field->name, field_name) != 0)
    return 0;
#endif

  if (!src) {
    *out = NULL;
    return 0;
  }

  *out = dst = rc_buf_reserve(buffer, len);

  if (*src == '\"') {
    ++src;
    while (*src != '\"') {
      if (*src == '\\')
        ++src;

      *dst++ = *src++;
    }
  } else {
    memcpy(dst, src, len);
    dst += len;
  }

  *dst++ = '\0';
  rc_buf_consume(buffer, *out, dst);
  return 1;
}

void rc_json_get_optional_string(const char** out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name, const char* default_value)
{
  if (!rc_json_get_string(out, &response->buffer, field, field_name))
    *out = default_value;
}

int rc_json_get_required_string(const char** out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name)
{
  if (rc_json_get_string(out, &response->buffer, field, field_name))
    return 1;

  return rc_json_missing_field(response, field);
}

int rc_json_get_num(int* out, const rc_json_field_t* field, const char* field_name)
{
  const char* src = field->value_start;
  int value = 0;
  int negative = 0;

#ifndef NDEBUG
  if (strcmp(field->name, field_name) != 0)
    return 0;
#endif

  if (!src) {
    *out = 0;
    return 0;
  }

  /* assert: string is valid number per rc_json_parse_field */
  if (*src == '-') {
    negative = 1;
    ++src;
  } else if (*src == '+') {
    ++src;
  } else if (*src < '0' || *src > '9') {
    *out = 0;
    return 0;
  }

  while (src < field->value_end && *src != '.') {
    value *= 10;
    value += *src - '0';
    ++src;
  }

  if (negative)
    *out = -value;
  else
    *out = value;

  return 1;
}

void rc_json_get_optional_num(int* out, const rc_json_field_t* field, const char* field_name, int default_value)
{
  if (!rc_json_get_num(out, field, field_name))
    *out = default_value;
}

int rc_json_get_required_num(int* out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name)
{
  if (rc_json_get_num(out, field, field_name))
    return 1;

  return rc_json_missing_field(response, field);
}

int rc_json_get_bool(int* out, const rc_json_field_t* field, const char* field_name)
{
  const char* src = field->value_start;

#ifndef NDEBUG
  if (strcmp(field->name, field_name) != 0)
    return 0;
#endif

  if (src) {
    const size_t len = field->value_end - field->value_start;
    if (len == 4 && memcmp(src, "true", 4) == 0) {
      *out = 1;
      return 1;
    } else if (len == 5 && memcmp(src, "false", 5) == 0) {
      *out = 0;
      return 1;
    } else if (len == 1) {
      *out = (*src != '0');
      return 1;
    }
  }

  *out = 0;
  return 0;
}

void rc_json_get_optional_bool(int* out, const rc_json_field_t* field, const char* field_name, int default_value)
{
  if (!rc_json_get_bool(out, field, field_name))
    *out = default_value;
}

int rc_json_get_required_bool(int* out, rc_api_response_t* response, const rc_json_field_t* field, const char* field_name)
{
  if (rc_json_get_bool(out, field, field_name))
    return 1;

  return rc_json_missing_field(response, field);
}

void rc_api_destroy_request(rc_api_request_t* request)
{
  rc_buf_destroy(&request->buffer);
}

void rc_buf_init(rc_api_buffer_t* buffer)
{
  buffer->write = &buffer->data[0];
  buffer->end = &buffer->data[sizeof(buffer->data)];
  buffer->next = NULL;
}

void rc_buf_destroy(rc_api_buffer_t* buffer)
{
  /* first buffer is not allocated */
  buffer = buffer->next;

  /* deallocate any additional buffers */
  while (buffer) {
    rc_api_buffer_t* next = buffer->next;
    free(buffer);
    buffer = next;
  }
}

char* rc_buf_reserve(rc_api_buffer_t* buffer, size_t amount)
{
  size_t remaining;
  do {
    remaining = buffer->end - buffer->write;
    if (remaining >= amount)
      return buffer->write;

    if (!buffer->next) {
      /* allocate a chunk of memory that is a multiple of 256-bytes. casting it to an rc_api_buffer_t will
       * effectively unbound the data field, so use write and end pointers to track how data is being used.
       */
      const size_t buffer_prefix_size = sizeof(rc_api_buffer_t) - sizeof(buffer->data);
      const size_t alloc_size = (amount + buffer_prefix_size + 0xFF) & ~0xFF;
      buffer->next = (rc_api_buffer_t*)malloc(alloc_size);
      if (!buffer->next)
        return NULL;

      buffer->next->write = buffer->next->data;
      buffer->next->end = buffer->next->write + (alloc_size - buffer_prefix_size);
      buffer->next->next = NULL;
    }

    buffer = buffer->next;
  } while (1);
}

void rc_buf_consume(rc_api_buffer_t* buffer, const char* start, char* end)
{
  do {
    if (buffer->write == start) {
      size_t offset = (end - buffer->data);
      offset = (offset + 7) & ~7;
      buffer->write = &buffer->data[offset];
      break;
    }

    buffer = buffer->next;
  } while (buffer);
}

void* rc_buf_alloc(rc_api_buffer_t* buffer, size_t amount)
{
  char* ptr = rc_buf_reserve(buffer, amount);
  rc_buf_consume(buffer, ptr, ptr + amount);
  return (void*)ptr;
}

void rc_url_builder_init(rc_api_url_builder_t* builder, rc_api_buffer_t* buffer, size_t estimated_size)
{
  memset(builder, 0, sizeof(*builder));
  builder->buffer = buffer;
  builder->write = builder->start = rc_buf_reserve(buffer, estimated_size);
  builder->end = builder->start + estimated_size;
}

const char* rc_url_builder_finalize(rc_api_url_builder_t* builder)
{
  rc_url_builder_append(builder, "", 1);

  if (builder->result != RC_OK)
    return NULL;

  rc_buf_consume(builder->buffer, builder->start, builder->write);
  return builder->start;
}

static int rc_url_builder_reserve(rc_api_url_builder_t* builder, size_t amount) {

  if (builder->result == RC_OK) {
    size_t remaining = builder->end - builder->write;
    if (remaining < amount) {
      const size_t used = builder->write - builder->start;
      const size_t current_size = builder->end - builder->start;
      const size_t buffer_prefix_size = sizeof(rc_api_buffer_t) - sizeof(builder->buffer->data);
      char* new_start;
      size_t new_size = (current_size < 256) ? 256 : current_size * 2;
      do {
        remaining = new_size - used;
        if (remaining >= amount)
          break;

        new_size *= 2;
      } while (1);

      /* rc_buf_reserve will align to 256 bytes after including the buffer prefix. attempt to account for that */
      if ((remaining - amount) > buffer_prefix_size)
        new_size -= buffer_prefix_size;

      new_start = rc_buf_reserve(builder->buffer, new_size);
      if (!new_start) {
        builder->result = RC_OUT_OF_MEMORY;
        return RC_OUT_OF_MEMORY;
      }

      if (new_start != builder->start) {
        memcpy(new_start, builder->start, used);
        builder->start = new_start;
        builder->write = new_start + used;
      }

      builder->end = builder->start + new_size;
    }
  }

  return builder->result;
}

void rc_url_builder_append_encoded_str(rc_api_url_builder_t* builder, const char* str) {
  static const char hex[] = "0123456789abcdef";
  const char* start = str;
  size_t len = 0;
  for (;;) {
    const char c = *str++;
    switch (c) {
      case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': case 'i': case 'j':
      case 'k': case 'l': case 'm': case 'n': case 'o': case 'p': case 'q': case 'r': case 's': case 't':
      case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
      case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': case 'I': case 'J':
      case 'K': case 'L': case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T':
      case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
      case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
      case '-': case '_': case '.': case '~':
        len++;
        continue;

      case '\0':
        if (len)
          rc_url_builder_append(builder, start, len);

        return;

      default:
        if (rc_url_builder_reserve(builder, len + 3) != RC_OK)
          return;

        if (len) {
          memcpy(builder->write, start, len);
          builder->write += len;
        }

        if (c == ' ') {
          *builder->write++ = '+';
        } else {
          *builder->write++ = '%';
          *builder->write++ = hex[c >> 4];
          *builder->write++ = hex[c & 0x0F];
        }
        break;
    }

    start = str;
    len = 0;
  }
}

void rc_url_builder_append(rc_api_url_builder_t* builder, const char* data, size_t len)
{
  if (rc_url_builder_reserve(builder, len) == RC_OK) {
    memcpy(builder->write, data, len);
    builder->write += len;
  }
}

static int rc_url_builder_append_param_equals(rc_api_url_builder_t* builder, const char* param)
{
  size_t param_len = strlen(param);

  size_t remaining = (builder->end - builder->write);
  if (rc_url_builder_reserve(builder, param_len + 2) == RC_OK) {
    if (builder->write > builder->start) {
      if (builder->write[-1] != '?')
        *builder->write++ = '&';
    }

    memcpy(builder->write, param, param_len);
    builder->write += param_len;
    *builder->write++ = '=';
  }

  return builder->result;
}

void rc_url_builder_append_num_param(rc_api_url_builder_t* builder, const char* param, unsigned value)
{
  if (rc_url_builder_append_param_equals(builder, param) == RC_OK) {
    char num[16];
    int chars = sprintf(num, "%u", value);
    rc_url_builder_append(builder, num, chars);
  }
}

void rc_url_builder_append_signed_num_param(rc_api_url_builder_t* builder, const char* param, int value)
{
  if (rc_url_builder_append_param_equals(builder, param) == RC_OK) {
    char num[16];
    int chars = sprintf(num, "%d", value);
    rc_url_builder_append(builder, num, chars);
  }
}

void rc_url_builder_append_str_param(rc_api_url_builder_t* builder, const char* param, const char* value)
{
  rc_url_builder_append_param_equals(builder, param);
  rc_url_builder_append_encoded_str(builder, value);
}

void rc_api_set_host(const char* hostname)
{
  if (g_host != NULL)
    free(g_host);

  if (hostname != NULL)
  {
    if (strstr(hostname, "://"))
    {
      g_host = strdup(hostname);
    }
    else
    {
      const size_t hostname_len = strlen(hostname);
      g_host = (char*)malloc(hostname_len + 7 + 1);
      memcpy(g_host, "http://", 7);
      memcpy(&g_host[7], hostname, hostname_len + 1);
    }
  }
  else
  {
    g_host = NULL;
  }
}

void rc_api_url_build_dorequest(rc_api_url_builder_t* builder, rc_api_buffer_t* buffer, const char* api, const char* username)
{
  #define DOREQUEST_ENDPOINT "/dorequest.php"
  const size_t endpoint_len = sizeof(DOREQUEST_ENDPOINT) - 1;
  const size_t host_len = (g_host ? strlen(g_host) : sizeof(RETROACHIEVEMENTS_HOST) - 1);
  const size_t base_url_len = host_len + endpoint_len;

  rc_url_builder_init(builder, buffer, base_url_len + 32);

  if (g_host)
    rc_url_builder_append(builder, g_host, host_len);
  else
    rc_url_builder_append(builder, RETROACHIEVEMENTS_HOST, host_len);

  rc_url_builder_append(builder, DOREQUEST_ENDPOINT, endpoint_len);

  *builder->write++ = '?';
  rc_url_builder_append_str_param(builder, "r", api);
  rc_url_builder_append_str_param(builder, "u", username);

  #undef DOREQUEST_ENDPOINT
}
