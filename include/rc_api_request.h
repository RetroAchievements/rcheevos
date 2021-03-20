#ifndef RC_API_REQUEST_H
#define RC_API_REQUEST_H

#include "rc_error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rc_api_buffer_t {
  char* write;
  char* end;
  struct rc_api_buffer_t* next;
  char data[256]; /* actual size of data[] may be larger than 256 bytes for buffers allocated in the next chain */
}
rc_api_buffer_t;

typedef struct rc_api_request_t {
  const char* url;
  const char* post_data;

  rc_api_buffer_t buffer;
}
rc_api_request_t;

typedef struct rc_api_response_t {
  int succeeded;
  const char* error_message;

  rc_api_buffer_t buffer;
}
rc_api_response_t;

void rc_api_destroy_request(rc_api_request_t* request);

void rc_api_set_host(const char* hostname);
void rc_api_set_image_host(const char* hostname);

#ifdef __cplusplus
}
#endif

#endif /* RC_API_REQUEST_H */
