#ifndef RC_HASH_INTERNAL_H
#define RC_HASH_INTERNAL_H

#include "rc_hash.h"

RC_BEGIN_C_DECLS

void rc_hash_verbose(const rc_hash_callbacks_t* callbacks, const char* message);
void rc_hash_verbose_formatted(const rc_hash_callbacks_t* callbacks, const char* format, ...);

int rc_hash_error(const rc_hash_callbacks_t*, const char* message);
int rc_hash_error_formatted(const rc_hash_callbacks_t* callbacks, const char* format, ...);

int64_t rc_file_size(const char* path);

RC_END_C_DECLS

#endif /* RC_HASH_INTERNAL_H */
