#ifndef RC_HASH_INTERNAL_H
#define RC_HASH_INTERNAL_H

#include "rc_hash.h"
#include "md5.h"

RC_BEGIN_C_DECLS

/* hash.c */

void rc_hash_verbose(const rc_hash_callbacks_t* callbacks, const char* message);
void rc_hash_verbose_formatted(const rc_hash_callbacks_t* callbacks, const char* format, ...);

int rc_hash_error(const rc_hash_callbacks_t*, const char* message);
int rc_hash_error_formatted(const rc_hash_callbacks_t* callbacks, const char* format, ...);

void* rc_file_open(const rc_hash_iterator_t* iterator, const char* path);
void rc_file_seek(const rc_hash_iterator_t* iterator, void* file_handle, int64_t offset, int origin);
int64_t rc_file_tell(const rc_hash_iterator_t* iterator, void* file_handle);
size_t rc_file_read(const rc_hash_iterator_t* iterator, void* file_handle, void* buffer, int requested_bytes);
void rc_file_close(const rc_hash_iterator_t* iterator, void* file_handle);
int64_t rc_file_size(const rc_hash_iterator_t* iterator, const char* path);


void rc_hash_iterator_verbose(const rc_hash_iterator_t* iterator, const char* message);
void rc_hash_iterator_verbose_formatted(const rc_hash_iterator_t* iterator, const char* format, ...);
int rc_hash_iterator_error(const rc_hash_iterator_t* iterator, const char* message);
int rc_hash_iterator_error_formatted(const rc_hash_iterator_t* iterator, const char* format, ...);


int rc_hash_finalize(const rc_hash_iterator_t* iterator, md5_state_t* md5, char hash[33]);


const char* rc_path_get_filename(const char* path);
int rc_path_compare_extension(const char* path, const char* ext);


typedef void (RC_CCONV* rc_hash_iterator_ext_handler_t)(rc_hash_iterator_t* iterator, const char* path, int data);
typedef struct rc_hash_iterator_ext_handler_entry_t {
  char ext[8];
  rc_hash_iterator_ext_handler_t handler;
  int data;
} rc_hash_iterator_ext_handler_entry_t;

const rc_hash_iterator_ext_handler_entry_t* rc_hash_get_iterator_ext_handlers(size_t* num_handlers);


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

/* hash_zip.c */
int rc_hash_ms_dos(char hash[33], const rc_hash_iterator_t* iterator);

RC_END_C_DECLS

#endif /* RC_HASH_INTERNAL_H */
