#ifndef RAPI_H
#define RAPI_H

#include "rerror.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct rc_api_buffer_t {
  char* write;
  char* end;
  struct rc_api_buffer_t* next;
  char data[256];
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

/* === Login === */

typedef struct rc_api_login_request_t {
  const char* username;
  const char* api_token;
  const char* password;
}
rc_api_login_request_t;

typedef struct rc_api_login_response_t {
  const char* username;
  const char* api_token;
  unsigned score;
  unsigned num_unread_messages;

  rc_api_response_t response;
}
rc_api_login_response_t;

int rc_api_init_login_request(rc_api_request_t* request, const rc_api_login_request_t* login);
int rc_api_process_login_response(rc_api_login_response_t* response, const char* server_response);
void rc_api_destroy_login_response(rc_api_login_response_t* response);

#ifdef __cplusplus
}
#endif

#endif /* RAPI_H */
