#ifndef RC_HASH_INTERNAL_H
#define RC_HASH_INTERNAL_H

#include "rc_hash.h"

RC_BEGIN_C_DECLS

void rc_hash_verbose(const rc_hash_callbacks_t* callbacks, const char* message);
void rc_hash_verbose_formatted(const rc_hash_callbacks_t* callbacks, const char* format, ...);

int rc_hash_error(const rc_hash_callbacks_t*, const char* message);
int rc_hash_error_formatted(const rc_hash_callbacks_t* callbacks, const char* format, ...);

int64_t rc_file_size(const char* path);

typedef void (RC_CCONV* rc_hash_iterator_ext_handler_t)(rc_hash_iterator_t* iterator, const char* path, int data);
typedef struct rc_hash_iterator_ext_handler_entry_t {
  char ext[8];
  rc_hash_iterator_ext_handler_t handler;
  int data;
} rc_hash_iterator_ext_handler_entry_t;

const rc_hash_iterator_ext_handler_entry_t* rc_hash_get_iterator_ext_handlers(size_t* num_handlers);

RC_END_C_DECLS

#endif /* RC_HASH_INTERNAL_H */
