#ifndef RC_HASH_INTERNAL_H
#define RC_HASH_INTERNAL_H

#include "rc_hash.h"

RC_BEGIN_C_DECLS

void rc_hash_verbose(const rc_hash_callbacks_t* callbacks, const char* message);
void rc_hash_verbose_formatted(const rc_hash_callbacks_t* callbacks, const char* format, ...);

int rc_hash_error(const rc_hash_callbacks_t*, const char* message);
int rc_hash_error_formatted(const rc_hash_callbacks_t* callbacks, const char* format, ...);

typedef void (RC_CCONV* rc_hash_iterator_ext_handler_t)(rc_hash_iterator_t* iterator, const char* path, int data);
typedef struct rc_hash_iterator_ext_handler_entry_t {
  char ext[8];
  rc_hash_iterator_ext_handler_t handler;
  int data;
} rc_hash_iterator_ext_handler_entry_t;

const rc_hash_iterator_ext_handler_entry_t* rc_hash_get_iterator_ext_handlers(size_t* num_handlers);

const char* rc_path_get_filename(const char* path);
int rc_path_compare_extension(const char* path, const char* ext);

typedef struct rc_hash_cdrom_track_t {
  void* file_handle;        /* the file handle for reading the track data */
  const rc_hash_filereader_t* file_reader; /* functions to perform raw file I/O */
  int64_t file_track_offset;/* the offset of the track data within the file */
  int sector_size;          /* the size of each sector in the track data */
  int sector_header_size;   /* the offset to the raw data within a sector block */
  int raw_data_size;        /* the amount of raw data within a sector block */
  int track_first_sector;   /* the first absolute sector associated to the track (includes pregap) */
  int track_pregap_sectors; /* the number of pregap sectors */
#ifndef NDEBUG
  uint32_t track_id;        /* the index of the track */
#endif
} rc_hash_cdrom_track_t;

RC_END_C_DECLS

#endif /* RC_HASH_INTERNAL_H */
