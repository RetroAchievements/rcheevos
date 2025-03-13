#include "rc_hash.h"

#include "rc_hash_internal.h"

#include "../rc_compat.h"

#include "aes.h"
#include "md5.h"

#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <share.h>
#endif

/* arbitrary limit to prevent allocating and hashing large files */
#define MAX_BUFFER_SIZE 64 * 1024 * 1024

const char* rc_path_get_filename(const char* path);
static int rc_hash_whole_file(char hash[33], const rc_hash_iterator_t* iterator);
static int rc_hash_from_file(char hash[33], uint32_t console_id, const rc_hash_iterator_t* iterator);

/* ===================================================== */

static rc_hash_message_callback g_error_message_callback = NULL;
rc_hash_message_callback g_verbose_message_callback = NULL;

static void rc_hash_dispatch_message_va(const rc_hash_message_callback callback, const char* format, va_list args)
{
  char buffer[1024];

#ifdef __STDC_SECURE_LIB__
  vsprintf_s(buffer, sizeof(buffer), format, args);
#elif __STDC_VERSION__ >= 199901L /* vsnprintf requires c99 */
  vsnprintf(buffer, sizeof(buffer), format, args);
#else /* c89 doesn't have a size-limited vsprintf function - assume the buffer is large enough */
  vsprintf(buffer, format, args);
#endif

  callback(buffer);
}

void rc_hash_init_error_message_callback(rc_hash_message_callback callback)
{
  g_error_message_callback = callback;
}

static rc_hash_message_callback rc_hash_get_error_message_callback(const rc_hash_callbacks_t* callbacks)
{
  if (callbacks && callbacks->error_message)
    return callbacks->error_message;

  if (g_error_message_callback)
    return g_error_message_callback;

  if (callbacks && callbacks->verbose_message)
    return callbacks->verbose_message;

  if (g_verbose_message_callback)
    return g_verbose_message_callback;

  return NULL;
}

int rc_hash_error(const rc_hash_callbacks_t* callbacks, const char* message)
{
  rc_hash_message_callback message_callback = rc_hash_get_error_message_callback(callbacks);

  if (message_callback)
    message_callback(message);

  return 0;
}

int rc_hash_error_formatted(const rc_hash_callbacks_t* callbacks, const char* format, ...)
{
  rc_hash_message_callback message_callback = rc_hash_get_error_message_callback(callbacks);

  if (message_callback) {
    va_list args;
    va_start(args, format);
    rc_hash_dispatch_message_va(message_callback, format, args);
    va_end(args);
  }

  return 0;
}

int rc_hash_iterator_error(const rc_hash_iterator_t* iterator, const char* message)
{
  rc_hash_message_callback message_callback = rc_hash_get_error_message_callback(&iterator->callbacks);

  if (message_callback)
    message_callback(message);

  return 0;
}

int rc_hash_iterator_error_formatted(const rc_hash_iterator_t* iterator, const char* format, ...)
{
  rc_hash_message_callback message_callback = rc_hash_get_error_message_callback(&iterator->callbacks);

  if (message_callback) {
    va_list args;
    va_start(args, format);
    rc_hash_dispatch_message_va(message_callback, format, args);
    va_end(args);
  }

  return 0;
}

void rc_hash_init_verbose_message_callback(rc_hash_message_callback callback)
{
  g_verbose_message_callback = callback;
}

void rc_hash_verbose(const rc_hash_callbacks_t* callbacks, const char* message)
{
  if (callbacks->verbose_message)
    callbacks->verbose_message(message);
  else if (g_verbose_message_callback)
    g_verbose_message_callback(message);
}

void rc_hash_verbose_formatted(const rc_hash_callbacks_t* callbacks, const char* format, ...)
{
  if (callbacks && callbacks->verbose_message) {
    va_list args;
    va_start(args, format);
    rc_hash_dispatch_message_va(callbacks->verbose_message, format, args);
    va_end(args);
  }
  else if (g_verbose_message_callback) {
    va_list args;
    va_start(args, format);
    rc_hash_dispatch_message_va(g_verbose_message_callback, format, args);
    va_end(args);
  }
}

void rc_hash_iterator_verbose(const rc_hash_iterator_t* iterator, const char* message)
{
  rc_hash_verbose(&iterator->callbacks, message);
}

void rc_hash_iterator_verbose_formatted(const rc_hash_iterator_t* iterator, const char* format, ...)
{
  if (iterator->callbacks.verbose_message) {
    va_list args;
    va_start(args, format);
    rc_hash_dispatch_message_va(iterator->callbacks.verbose_message, format, args);
    va_end(args);
  }
  else if (g_verbose_message_callback) {
    va_list args;
    va_start(args, format);
    rc_hash_dispatch_message_va(g_verbose_message_callback, format, args);
    va_end(args);
  }
}

/* ===================================================== */

static struct rc_hash_filereader g_filereader_funcs;
static struct rc_hash_filereader* g_filereader = NULL;

#if defined(WINVER) && WINVER >= 0x0500
static void* filereader_open(const char* path)
{
  /* Windows requires using wchar APIs for Unicode paths */
  /* Note that MultiByteToWideChar will only be defined for >= Windows 2000 */
  wchar_t* wpath;
  int wpath_length;
  FILE* fp;

  /* Calculate wpath length from path */
  wpath_length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
  if (wpath_length == 0) /* 0 indicates error (this is likely from invalid UTF-8) */
    return NULL;

  wpath = (wchar_t*)malloc(wpath_length * sizeof(wchar_t));
  if (!wpath)
    return NULL;

  if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, wpath_length) == 0)
  {
    free(wpath);
    return NULL;
  }

 #if defined(__STDC_SECURE_LIB__)
  /* have to use _SH_DENYNO because some cores lock the file while its loaded */
  fp = _wfsopen(wpath, L"rb", _SH_DENYNO);
 #else
  fp = _wfopen(wpath, L"rb");
 #endif

  free(wpath);
  return fp;
}
#else /* !WINVER >= 0x0500 */
static void* filereader_open(const char* path)
{
 #if defined(__STDC_SECURE_LIB__)
  #if defined(WINVER)
   /* have to use _SH_DENYNO because some cores lock the file while its loaded */
   return _fsopen(path, "rb", _SH_DENYNO);
  #else /* !WINVER */
   FILE *fp;
   fopen_s(&fp, path, "rb");
   return fp;
  #endif
 #else /* !__STDC_SECURE_LIB__ */
  return fopen(path, "rb");
 #endif
}
#endif /* WINVER >= 0x0500 */

static void filereader_seek(void* file_handle, int64_t offset, int origin)
{
#if defined(_WIN32)
  _fseeki64((FILE*)file_handle, offset, origin);
#elif defined(_LARGEFILE64_SOURCE)
  fseeko64((FILE*)file_handle, offset, origin);
#else
  fseek((FILE*)file_handle, offset, origin);
#endif
}

static int64_t filereader_tell(void* file_handle)
{
#if defined(_WIN32)
  return _ftelli64((FILE*)file_handle);
#elif defined(_LARGEFILE64_SOURCE)
  return ftello64((FILE*)file_handle);
#else
  return ftell((FILE*)file_handle);
#endif
}

static size_t filereader_read(void* file_handle, void* buffer, size_t requested_bytes)
{
  return fread(buffer, 1, requested_bytes, (FILE*)file_handle);
}

static void filereader_close(void* file_handle)
{
  fclose((FILE*)file_handle);
}

/* for unit tests - normally would call rc_hash_init_custom_filereader(NULL) */
void rc_hash_reset_filereader(void)
{
  g_filereader = NULL;
}

void rc_hash_init_custom_filereader(struct rc_hash_filereader* reader)
{
  /* initialize with defaults first */
  g_filereader_funcs.open = filereader_open;
  g_filereader_funcs.seek = filereader_seek;
  g_filereader_funcs.tell = filereader_tell;
  g_filereader_funcs.read = filereader_read;
  g_filereader_funcs.close = filereader_close;

  /* hook up any provided custom handlers */
  if (reader) {
    if (reader->open)
      g_filereader_funcs.open = reader->open;

    if (reader->seek)
      g_filereader_funcs.seek = reader->seek;

    if (reader->tell)
      g_filereader_funcs.tell = reader->tell;

    if (reader->read)
      g_filereader_funcs.read = reader->read;

    if (reader->close)
      g_filereader_funcs.close = reader->close;
  }

  g_filereader = &g_filereader_funcs;
}

void* rc_file_open(const rc_hash_iterator_t* iterator, const char* path)
{
  void* handle = NULL;

  if (!iterator->callbacks.filereader.open) {
    rc_hash_iterator_error(iterator, "No callback registered for opening files");
  } else {
    handle = iterator->callbacks.filereader.open(path);
    if (handle)
      rc_hash_iterator_verbose_formatted(iterator, "Opened %s", rc_path_get_filename(path));
  }

  return handle;
}

void rc_file_seek(const rc_hash_iterator_t* iterator, void* file_handle, int64_t offset, int origin)
{
  if (iterator->callbacks.filereader.seek)
    iterator->callbacks.filereader.seek(file_handle, offset, origin);
}

int64_t rc_file_tell(const rc_hash_iterator_t* iterator, void* file_handle)
{
  return iterator->callbacks.filereader.tell ? iterator->callbacks.filereader.tell(file_handle) : 0;
}

size_t rc_file_read(const rc_hash_iterator_t* iterator, void* file_handle, void* buffer, int requested_bytes)
{
  return iterator->callbacks.filereader.read ? iterator->callbacks.filereader.read(file_handle, buffer, requested_bytes) : 0;
}

void rc_file_close(const rc_hash_iterator_t* iterator, void* file_handle)
{
  if (iterator->callbacks.filereader.close)
    iterator->callbacks.filereader.close(file_handle);
}

int64_t rc_file_size(const rc_hash_iterator_t* iterator, const char* path)
{
  int64_t size = 0;

  /* don't use rc_file_open to avoid log statements */
  if (!iterator->callbacks.filereader.open) {
    rc_hash_iterator_error(iterator, "No callback registered for opening files");
  } else {
    void* handle = iterator->callbacks.filereader.open(path);
    if (handle) {
      rc_file_seek(iterator, handle, 0, SEEK_END);
      size = rc_file_tell(iterator, handle);
      rc_file_close(iterator, handle);
    }
  }

  return size;
}

/* ===================================================== */

static struct rc_hash_cdreader g_cdreader_funcs;
static struct rc_hash_cdreader* g_cdreader = NULL;

void rc_hash_init_custom_cdreader(struct rc_hash_cdreader* reader)
{
  if (reader)
  {
    memcpy(&g_cdreader_funcs, reader, sizeof(g_cdreader_funcs));
    g_cdreader = &g_cdreader_funcs;
  }
  else
  {
    g_cdreader = NULL;
  }
}

static void* rc_cd_open_track(const rc_hash_iterator_t* iterator, uint32_t track)
{
  if (iterator->callbacks.cdreader.open_track_filereader)
    return iterator->callbacks.cdreader.open_track_filereader(iterator->path, track, &iterator->callbacks.filereader);

  if (iterator->callbacks.cdreader.open_track)
    return iterator->callbacks.cdreader.open_track(iterator->path, track);

  if (g_cdreader && g_cdreader->open_track)
    return g_cdreader->open_track(iterator->path, track);

  rc_hash_iterator_error(iterator, "no hook registered for cdreader_open_track");
  return NULL;
}

static size_t rc_cd_read_sector(const rc_hash_iterator_t* iterator, void* track_handle, uint32_t sector, void* buffer, size_t requested_bytes)
{
  if (iterator->callbacks.cdreader.read_sector)
    return iterator->callbacks.cdreader.read_sector(track_handle, sector, buffer, requested_bytes);

  if (g_cdreader && g_cdreader->read_sector)
    return g_cdreader->read_sector(track_handle, sector, buffer, requested_bytes);

  rc_hash_iterator_error(iterator, "no hook registered for cdreader_read_sector");
  return 0;
}

static uint32_t rc_cd_first_track_sector(const rc_hash_iterator_t* iterator, void* track_handle)
{
  if (iterator->callbacks.cdreader.first_track_sector)
    return iterator->callbacks.cdreader.first_track_sector(track_handle);

  if (g_cdreader && g_cdreader->first_track_sector)
    return g_cdreader->first_track_sector(track_handle);

  rc_hash_iterator_error(iterator, "no hook registered for cdreader_first_track_sector");
  return 0;
}

static void rc_cd_close_track(const rc_hash_iterator_t* iterator, void* track_handle)
{
  if (iterator->callbacks.cdreader.close_track)
    iterator->callbacks.cdreader.close_track(track_handle);
  else if (g_cdreader && g_cdreader->close_track)
    g_cdreader->close_track(track_handle);
  else
    rc_hash_iterator_error(iterator, "no hook registered for cdreader_close_track");
}

static uint32_t rc_cd_find_file_sector(const rc_hash_iterator_t* iterator, void* track_handle, const char* path, uint32_t* size)
{
  uint8_t buffer[2048], *tmp;
  int sector;
  uint32_t num_sectors = 0;
  size_t filename_length;
  const char* slash;

  if (!track_handle)
    return 0;

  /* we start at the root. don't need to explicitly find it */
  if (*path == '\\')
    ++path;

  filename_length = strlen(path);
  slash = strrchr(path, '\\');
  if (slash)
  {
    /* find the directory record for the first part of the path */
    memcpy(buffer, path, slash - path);
    buffer[slash - path] = '\0';

    sector = rc_cd_find_file_sector(iterator, track_handle, (const char *)buffer, NULL);
    if (!sector)
      return 0;

    ++slash;
    filename_length -= (slash - path);
    path = slash;
  }
  else
  {
    uint32_t logical_block_size;

    /* find the cd information */
    if (!rc_cd_read_sector(iterator, track_handle, rc_cd_first_track_sector(iterator, track_handle) + 16, buffer, 256))
      return 0;

    /* the directory_record starts at 156, the sector containing the table of contents is 2 bytes into that.
     * https://www.cdroller.com/htm/readdata.html
     */
    sector = buffer[156 + 2] | (buffer[156 + 3] << 8) | (buffer[156 + 4] << 16);

    /* if the table of contents spans more than one sector, it's length of section will exceed it's logical block size */
    logical_block_size = (buffer[128] | (buffer[128 + 1] << 8)); /* logical block size */
    if (logical_block_size == 0) {
      num_sectors = 1;
    } else {
      num_sectors = (buffer[156 + 10] | (buffer[156 + 11] << 8) | (buffer[156 + 12] << 16) | (buffer[156 + 13] << 24)); /* length of section */
      num_sectors /= logical_block_size;
    }
  }

  /* fetch and process the directory record */
  if (!rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer)))
    return 0;

  tmp = buffer;
  do
  {
    if (tmp >= buffer + sizeof(buffer) || !*tmp)
    {
      /* end of this path table block. if the path table spans multiple sectors, keep scanning */
      if (num_sectors > 1)
      {
        --num_sectors;
        if (rc_cd_read_sector(iterator, track_handle, ++sector, buffer, sizeof(buffer)))
        {
          tmp = buffer;
          continue;
        }
      }
      break;
    }

    /* filename is 33 bytes into the record and the format is "FILENAME;version" or "DIRECTORY" */
    if ((tmp[32] == filename_length || tmp[33 + filename_length] == ';') &&
        strncasecmp((const char*)(tmp + 33), path, filename_length) == 0)
    {
      sector = tmp[2] | (tmp[3] << 8) | (tmp[4] << 16);

      rc_hash_iterator_verbose_formatted(iterator, "Found %s at sector %d", path, sector);

      if (size)
        *size = tmp[10] | (tmp[11] << 8) | (tmp[12] << 16) | (tmp[13] << 24);

      return sector;
    }

    /* the first byte of the record is the length of the record */
    tmp += *tmp;
  } while (1);

  return 0;
}

/* ===================================================== */

static rc_hash_3ds_get_cia_normal_key_func _3ds_get_cia_normal_key_func = NULL;
static rc_hash_3ds_get_ncch_normal_keys_func _3ds_get_ncch_normal_keys_func = NULL;

void rc_hash_init_3ds_get_cia_normal_key_func(rc_hash_3ds_get_cia_normal_key_func func)
{
  _3ds_get_cia_normal_key_func = func;
}

void rc_hash_init_3ds_get_ncch_normal_keys_func(rc_hash_3ds_get_ncch_normal_keys_func func)
{
  _3ds_get_ncch_normal_keys_func = func;
}

/* ===================================================== */

const char* rc_path_get_filename(const char* path)
{
  const char* ptr = path + strlen(path);
  do
  {
    if (ptr[-1] == '/' || ptr[-1] == '\\')
      break;

    --ptr;
  } while (ptr > path);

  return ptr;
}

static const char* rc_path_get_extension(const char* path)
{
  const char* ptr = path + strlen(path);
  do
  {
    if (ptr[-1] == '.')
      return ptr;

    --ptr;
  } while (ptr > path);

  return path + strlen(path);
}

int rc_path_compare_extension(const char* path, const char* ext)
{
  size_t path_len = strlen(path);
  size_t ext_len = strlen(ext);
  const char* ptr = path + path_len - ext_len;
  if (ptr[-1] != '.')
    return 0;

  if (memcmp(ptr, ext, ext_len) == 0)
    return 1;

  do
  {
    if (tolower(*ptr) != *ext)
      return 0;

    ++ext;
    ++ptr;
  } while (*ptr);

  return 1;
}

/* ===================================================== */

static void rc_hash_byteswap16(uint8_t* buffer, const uint8_t* stop)
{
  uint32_t* ptr = (uint32_t*)buffer;
  const uint32_t* stop32 = (const uint32_t*)stop;
  while (ptr < stop32)
  {
    uint32_t temp = *ptr;
    temp = (temp & 0xFF00FF00) >> 8 |
           (temp & 0x00FF00FF) << 8;
    *ptr++ = temp;
  }
}

static void rc_hash_byteswap32(uint8_t* buffer, const uint8_t* stop)
{
  uint32_t* ptr = (uint32_t*)buffer;
  const uint32_t* stop32 = (const uint32_t*)stop;
  while (ptr < stop32)
  {
    uint32_t temp = *ptr;
    temp = (temp & 0xFF000000) >> 24 |
           (temp & 0x00FF0000) >> 8 |
           (temp & 0x0000FF00) << 8 |
           (temp & 0x000000FF) << 24;
    *ptr++ = temp;
  }
}

int rc_hash_finalize(const rc_hash_iterator_t* iterator, md5_state_t* md5, char hash[33])
{
  md5_byte_t digest[16];

  md5_finish(md5, digest);

  /* NOTE: sizeof(hash) is 4 because it's still treated like a pointer, despite specifying a size */
  snprintf(hash, 33, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
    digest[0], digest[1], digest[2], digest[3], digest[4], digest[5], digest[6], digest[7],
    digest[8], digest[9], digest[10], digest[11], digest[12], digest[13], digest[14], digest[15]
  );

  rc_hash_iterator_verbose_formatted(iterator, "Generated hash %s", hash);

  return 1;
}

static int rc_hash_buffer(char hash[33], const uint8_t* buffer, size_t buffer_size, const rc_hash_iterator_t* iterator)
{
  md5_state_t md5;

  if (buffer_size > MAX_BUFFER_SIZE)
    buffer_size = MAX_BUFFER_SIZE;

  md5_init(&md5);

  md5_append(&md5, buffer, (int)buffer_size);

  rc_hash_iterator_verbose_formatted(iterator, "Hashing %u byte buffer", (unsigned)buffer_size);

  return rc_hash_finalize(iterator, &md5, hash);
}

static int rc_hash_iterator_buffer(char hash[33], const rc_hash_iterator_t* iterator)
{
  return rc_hash_buffer(hash, iterator->buffer, iterator->buffer_size, iterator);
}

static int rc_hash_unheadered_iterator_buffer(char hash[33], const rc_hash_iterator_t* iterator, size_t header_size)
{
  return rc_hash_buffer(hash, iterator->buffer + header_size, iterator->buffer_size - header_size, iterator);
}

static int rc_hash_cd_file(md5_state_t* md5, const rc_hash_iterator_t* iterator, void* track_handle, uint32_t sector, const char* name, uint32_t size, const char* description)
{
  uint8_t buffer[2048];
  size_t num_read;

  if ((num_read = rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer))) < sizeof(buffer))
    return rc_hash_iterator_error_formatted(iterator, "Could not read %s", description);

  if (size > MAX_BUFFER_SIZE)
    size = MAX_BUFFER_SIZE;

  if (name)
    rc_hash_iterator_verbose_formatted(iterator, "Hashing %s title (%u bytes) and contents (%u bytes) ", name, (unsigned)strlen(name), size);
  else
    rc_hash_iterator_verbose_formatted(iterator, "Hashing %s contents (%u bytes @ sector %u)", description, size, sector);

  if (size < (unsigned)num_read) /* we read a whole sector - only hash the part containing file data */
    num_read = (size_t)size;

  do
  {
    md5_append(md5, buffer, (int)num_read);

    if (size <= (unsigned)num_read)
      break;
    size -= (unsigned)num_read;

    ++sector;
    if (size >= sizeof(buffer))
      num_read = rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer));
    else
      num_read = rc_cd_read_sector(iterator, track_handle, sector, buffer, size);
  } while (num_read > 0);

  return 1;
}

static int rc_hash_3do(char hash[33], const rc_hash_iterator_t* iterator)
{
  uint8_t buffer[2048];
  const uint8_t operafs_identifier[7] = { 0x01, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x01 };
  void* track_handle;
  md5_state_t md5;
  int sector;
  int block_size, block_location;
  int offset, stop;
  size_t size = 0;

  track_handle = rc_cd_open_track(iterator, 1);
  if (!track_handle)
    return rc_hash_iterator_error(iterator, "Could not open track");

  /* the Opera filesystem stores the volume information in the first 132 bytes of sector 0
   * https://github.com/barbeque/3dodump/blob/master/OperaFS-Format.md
   */
  rc_cd_read_sector(iterator, track_handle, 0, buffer, 132);

  if (memcmp(buffer, operafs_identifier, sizeof(operafs_identifier)) == 0)
  {
    rc_hash_iterator_verbose_formatted(iterator, "Found 3DO CD, title=%.32s", &buffer[0x28]);

    /* include the volume header in the hash */
    md5_init(&md5);
    md5_append(&md5, buffer, 132);

    /* the block size is at offset 0x4C (assume 0x4C is always 0) */
    block_size = buffer[0x4D] * 65536 + buffer[0x4E] * 256 + buffer[0x4F];

    /* the root directory block location is at offset 0x64 (and duplicated several
     * times, but we just look at the primary record) (assume 0x64 is always 0)*/
    block_location = buffer[0x65] * 65536 + buffer[0x66] * 256 + buffer[0x67];

    /* multiply the block index by the block size to get the real address */
    block_location *= block_size;

    /* convert that to a sector and read it */
    sector = block_location / 2048;

    do
    {
      rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer));

      /* offset to start of entries is at offset 0x10 (assume 0x10 and 0x11 are always 0) */
      offset = buffer[0x12] * 256 + buffer[0x13];

      /* offset to end of entries is at offset 0x0C (assume 0x0C is always 0) */
      stop = buffer[0x0D] * 65536 + buffer[0x0E] * 256 + buffer[0x0F];

      while (offset < stop)
      {
        if (buffer[offset + 0x03] == 0x02) /* file */
        {
          if (strcasecmp((const char*)&buffer[offset + 0x20], "LaunchMe") == 0)
          {
            /* the block size is at offset 0x0C (assume 0x0C is always 0) */
            block_size = buffer[offset + 0x0D] * 65536 + buffer[offset + 0x0E] * 256 + buffer[offset + 0x0F];

            /* the block location is at offset 0x44 (assume 0x44 is always 0) */
            block_location = buffer[offset + 0x45] * 65536 + buffer[offset + 0x46] * 256 + buffer[offset + 0x47];
            block_location *= block_size;

            /* the file size is at offset 0x10 (assume 0x10 is always 0) */
            size = (size_t)buffer[offset + 0x11] * 65536 + buffer[offset + 0x12] * 256 + buffer[offset + 0x13];

            rc_hash_iterator_verbose_formatted(iterator, "Hashing header (%u bytes) and %.32s (%u bytes) ", 132, &buffer[offset + 0x20], (unsigned)size);

            break;
          }
        }

        /* the number of extra copies of the file is at offset 0x40 (assume 0x40-0x42 are always 0) */
        offset += 0x48 + buffer[offset + 0x43] * 4;
      }

      if (size != 0)
        break;

      /* did not find the file, see if the directory listing is continued in another sector */
      offset = buffer[0x02] * 256 + buffer[0x03];

      /* no more sectors to search*/
      if (offset == 0xFFFF)
        break;

      /* get next sector */
      offset *= block_size;
      sector = (block_location + offset) / 2048;
    } while (1);

    if (size == 0)
    {
      rc_cd_close_track(iterator, track_handle);
      return rc_hash_iterator_error(iterator, "Could not find LaunchMe");
    }

    sector = block_location / 2048;

    while (size > 2048)
    {
      rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer));
      md5_append(&md5, buffer, sizeof(buffer));

      ++sector;
      size -= 2048;
    }

    rc_cd_read_sector(iterator, track_handle, sector, buffer, size);
    md5_append(&md5, buffer, (int)size);
  }
  else
  {
    rc_cd_close_track(iterator, track_handle);
    return rc_hash_iterator_error(iterator, "Not a 3DO CD");
  }

  rc_cd_close_track(iterator, track_handle);

  return rc_hash_finalize(iterator, &md5, hash);
}

static int rc_hash_7800(char hash[33], const rc_hash_iterator_t* iterator)
{
  /* if the file contains a header, ignore it */
  if (memcmp(&iterator->buffer[1], "ATARI7800", 9) == 0)
  {
    rc_hash_iterator_verbose(iterator, "Ignoring 7800 header");
    return rc_hash_unheadered_iterator_buffer(hash, iterator, 128);
  }

  return rc_hash_iterator_buffer(hash, iterator);
}

static int rc_hash_arcade(char hash[33], const rc_hash_iterator_t* iterator)
{
  /* arcade hash is just the hash of the filename (no extension) - the cores are pretty stringent about having the right ROM data */
  const char* filename = rc_path_get_filename(iterator->path);
  const char* ext = rc_path_get_extension(filename);
  char buffer[128]; /* realistically, this should never need more than ~32 characters */
  size_t filename_length = ext - filename - 1;

  /* fbneo supports loading subsystems by using specific folder names.
   * if one is found, include it in the hash.
   * https://github.com/libretro/FBNeo/blob/master/src/burner/libretro/README.md#emulating-consoles-and-computers
   */
  if (filename > iterator->path + 1)
  {
    int include_folder = 0;
    const char* folder = filename - 1;
    size_t parent_folder_length = 0;

    do
    {
      if (folder[-1] == '/' || folder[-1] == '\\')
        break;

      --folder;
    } while (folder > iterator->path);

    parent_folder_length = filename - folder - 1;
    if (parent_folder_length < 16)
    {
      char* ptr = buffer;
      while (folder < filename - 1)
        *ptr++ = tolower(*folder++);
      *ptr = '\0';

      folder = buffer;
    }

    switch (parent_folder_length)
    {
      case 3:
        if (memcmp(folder, "nes", 3) == 0 || /* NES */
            memcmp(folder, "fds", 3) == 0 || /* FDS */
            memcmp(folder, "sms", 3) == 0 || /* Master System */
            memcmp(folder, "msx", 3) == 0 || /* MSX */
            memcmp(folder, "ngp", 3) == 0 || /* NeoGeo Pocket */
            memcmp(folder, "pce", 3) == 0 || /* PCEngine */
            memcmp(folder, "chf", 3) == 0 || /* ChannelF */
            memcmp(folder, "sgx", 3) == 0)   /* SuperGrafX */
          include_folder = 1;
        break;
      case 4:
        if (memcmp(folder, "tg16", 4) == 0 || /* TurboGrafx-16 */
            memcmp(folder, "msx1", 4) == 0)   /* MSX */
          include_folder = 1;
        break;
      case 5:
        if (memcmp(folder, "neocd", 5) == 0) /* NeoGeo CD */
          include_folder = 1;
        break;
      case 6:
        if (memcmp(folder, "coleco", 6) == 0 || /* Colecovision */
            memcmp(folder, "sg1000", 6) == 0)   /* SG-1000 */
          include_folder = 1;
        break;
      case 7:
        if (memcmp(folder, "genesis", 7) == 0) /* Megadrive (Genesis) */
          include_folder = 1;
        break;
      case 8:
        if (memcmp(folder, "gamegear", 8) == 0 || /* Game Gear */
            memcmp(folder, "megadriv", 8) == 0 || /* Megadrive */
            memcmp(folder, "pcengine", 8) == 0 || /* PCEngine */
            memcmp(folder, "channelf", 8) == 0 || /* ChannelF */
            memcmp(folder, "spectrum", 8) == 0)   /* ZX Spectrum */
          include_folder = 1;
        break;
      case 9:
        if (memcmp(folder, "megadrive", 9) == 0) /* Megadrive */
          include_folder = 1;
        break;
      case 10:
        if (memcmp(folder, "supergrafx", 10) == 0 || /* SuperGrafX */
            memcmp(folder, "zxspectrum", 10) == 0)   /* ZX Spectrum */
          include_folder = 1;
        break;
      case 12:
        if (memcmp(folder, "mastersystem", 12) == 0 || /* Master System */
            memcmp(folder, "colecovision", 12) == 0)   /* Colecovision */
          include_folder = 1;
        break;
      default:
        break;
    }

    if (include_folder)
    {
      if (parent_folder_length + filename_length + 1 < sizeof(buffer))
      {
        buffer[parent_folder_length] = '_';
        memcpy(&buffer[parent_folder_length + 1], filename, filename_length);
        return rc_hash_buffer(hash, (uint8_t*)&buffer[0], parent_folder_length + filename_length + 1, iterator);
      }
    }
  }

  return rc_hash_buffer(hash, (uint8_t*)filename, filename_length, iterator);
}

static int rc_hash_text(char hash[33], const rc_hash_iterator_t* iterator)
{
  md5_state_t md5;
  const uint8_t* scan = iterator->buffer;
  const uint8_t* stop = iterator->buffer + iterator->buffer_size;

  md5_init(&md5);

  do {
    const uint8_t* line = scan;

    /* find end of line */
    while (scan < stop && *scan != '\r' && *scan != '\n')
      ++scan;

    md5_append(&md5, line, (int)(scan - line));

    /* include a normalized line ending */
    /* NOTE: this causes a line ending to be hashed at the end of the file, even if one was not present */
    md5_append(&md5, (const uint8_t*)"\n", 1);

    /* skip newline */
    if (scan < stop && *scan == '\r')
      ++scan;
    if (scan < stop && *scan == '\n')
      ++scan;

  } while (scan < stop);

  return rc_hash_finalize(iterator, &md5, hash);
}

/* helper variable only used for testing */
const char* _rc_hash_jaguar_cd_homebrew_hash = NULL;

static int rc_hash_jaguar_cd(char hash[33], const rc_hash_iterator_t* iterator)
{
  uint8_t buffer[2352];
  void* track_handle;
  md5_state_t md5;
  int byteswapped = 0;
  uint32_t size = 0;
  uint32_t offset = 0;
  uint32_t sector = 0;
  uint32_t remaining;
  uint32_t i;

  /* Jaguar CD header is in the first sector of the first data track OF THE SECOND SESSION.
   * The first track must be an audio track, but may be a warning message or actual game audio */
  track_handle = rc_cd_open_track(iterator, RC_HASH_CDTRACK_FIRST_OF_SECOND_SESSION);
  if (!track_handle)
    return rc_hash_iterator_error(iterator, "Could not open track");

  /* The header is an unspecified distance into the first sector, but usually two bytes in.
   * It consists of 64 bytes of "TAIR" or "ATRI" repeating, depending on whether or not the data 
   * is byteswapped. Then another 32 byte that reads "ATARI APPROVED DATA HEADER ATRI "
   * (possibly byteswapped). Then a big-endian 32-bit value for the address where the boot code
   * should be loaded, and a second big-endian 32-bit value for the size of the boot code. */ 
  sector = rc_cd_first_track_sector(iterator, track_handle);
  rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer));

  for (i = 64; i < sizeof(buffer) - 32 - 4 * 3; i++)
  {
    if (memcmp(&buffer[i], "TARA IPARPVODED TA AEHDAREA RT I", 32) == 0)
    {
      byteswapped = 1;
      offset = i + 32 + 4;
      size = (buffer[offset] << 16) | (buffer[offset + 1] << 24) | (buffer[offset + 2]) | (buffer[offset + 3] << 8);
      break;
    }
    else if (memcmp(&buffer[i], "ATARI APPROVED DATA HEADER ATRI ", 32) == 0)
    {
      byteswapped = 0;
      offset = i + 32 + 4;
      size = (buffer[offset] << 24) | (buffer[offset + 1] << 16) | (buffer[offset + 2] << 8) | (buffer[offset + 3]);
      break;
    }
  }

  if (size == 0) /* did not see ATARI APPROVED DATA HEADER */
  {
    rc_cd_close_track(iterator, track_handle);
    return rc_hash_iterator_error(iterator, "Not a Jaguar CD");
  }

  i = 0; /* only loop once */
  do
  {
    md5_init(&md5);

    offset += 4;

    rc_hash_iterator_verbose_formatted(iterator, "Hashing boot executable (%u bytes starting at %u bytes into sector %u)", size, offset, sector);

    if (size > MAX_BUFFER_SIZE)
      size = MAX_BUFFER_SIZE;

    do
    {
      if (byteswapped)
        rc_hash_byteswap16(buffer, &buffer[sizeof(buffer)]);

      remaining = sizeof(buffer) - offset;
      if (remaining >= size)
      {
        md5_append(&md5, &buffer[offset], size);
        size = 0;
        break;
      }

      md5_append(&md5, &buffer[offset], remaining);
      size -= remaining;
      offset = 0;
    } while (rc_cd_read_sector(iterator, track_handle, ++sector, buffer, sizeof(buffer)) == sizeof(buffer));

    rc_cd_close_track(iterator, track_handle);

    if (size > 0)
      return rc_hash_iterator_error(iterator, "Not enough data");

    rc_hash_finalize(iterator, &md5, hash);

    /* homebrew games all seem to have the same boot executable and store the actual game code in track 2.
     * if we generated something other than the homebrew hash, return it. assume all homebrews are byteswapped. */
    if (strcmp(hash, "254487b59ab21bc005338e85cbf9fd2f") != 0 || !byteswapped) {
      if (_rc_hash_jaguar_cd_homebrew_hash == NULL || strcmp(hash, _rc_hash_jaguar_cd_homebrew_hash) != 0)
        return 1;
    }

    /* if we've already been through the loop a second time, just return the hash */
    if (i == 1)
      return 1;
    ++i;

    rc_hash_iterator_verbose_formatted(iterator, "Potential homebrew at sector %u, checking for KART data in track 2", sector);

    track_handle = rc_cd_open_track(iterator, 2);
    if (!track_handle)
      return rc_hash_iterator_error(iterator, "Could not open track");

    /* track 2 of the homebrew code has the 64 bytes or ATRI followed by 32 bytes of "ATARI APPROVED DATA HEADER ATRI!",
     * then 64 bytes of KART repeating. */
    sector = rc_cd_first_track_sector(iterator, track_handle);
    rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer));
    if (memcmp(&buffer[0x5E], "RT!IRTKA", 8) != 0)
      return rc_hash_iterator_error(iterator, "Homebrew executable not found in track 2");

    /* found KART data*/
    rc_hash_iterator_verbose(iterator, "Found KART data in track 2");

    offset = 0xA6;
    size = (buffer[offset] << 16) | (buffer[offset + 1] << 24) | (buffer[offset + 2]) | (buffer[offset + 3] << 8);
  } while (1);
}

static int rc_hash_lynx(char hash[33], const rc_hash_iterator_t* iterator)
{
  /* if the file contains a header, ignore it */
  if (memcmp(&iterator->buffer[0], "LYNX", 5) == 0)
  {
    rc_hash_iterator_verbose(iterator, "Ignoring LYNX header");
    return rc_hash_unheadered_iterator_buffer(hash, iterator, 64);
  }

  return rc_hash_iterator_buffer(hash, iterator);
}

static int rc_hash_neogeo_cd(char hash[33], const rc_hash_iterator_t* iterator)
{
  char buffer[1024], *ptr;
  void* track_handle;
  uint32_t sector;
  uint32_t size;
  md5_state_t md5;

  track_handle = rc_cd_open_track(iterator, 1);
  if (!track_handle)
    return rc_hash_iterator_error(iterator, "Could not open track");

  /* https://wiki.neogeodev.org/index.php?title=IPL_file, https://wiki.neogeodev.org/index.php?title=PRG_file
   * IPL file specifies data to be loaded before the game starts. PRG files are the executable code
   */
  sector = rc_cd_find_file_sector(iterator, track_handle, "IPL.TXT", &size);
  if (!sector)
  {
    rc_cd_close_track(iterator, track_handle);
    return rc_hash_iterator_error(iterator, "Not a NeoGeo CD game disc");
  }

  if (rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer)) == 0)
  {
    rc_cd_close_track(iterator, track_handle);
    return 0;
  }

  md5_init(&md5);

  buffer[sizeof(buffer) - 1] = '\0';
  ptr = &buffer[0];
  do
  {
    char* start = ptr;
    while (*ptr && *ptr != '.')
      ++ptr;

    if (strncasecmp(ptr, ".PRG", 4) == 0)
    {
      ptr += 4;
      *ptr++ = '\0';

      sector = rc_cd_find_file_sector(iterator, track_handle, start, &size);
      if (!sector || !rc_hash_cd_file(&md5, iterator, track_handle, sector, NULL, size, start))
      {
        rc_cd_close_track(iterator, track_handle);
        return rc_hash_iterator_error_formatted(iterator, "Could not read %.16s", start);
      }
    }

    while (*ptr && *ptr != '\n')
      ++ptr;
    if (*ptr != '\n')
      break;
    ++ptr;
  } while (*ptr != '\0' && *ptr != '\x1a');

  rc_cd_close_track(iterator, track_handle);
  return rc_hash_finalize(iterator, &md5, hash);
}

static int rc_hash_nes(char hash[33], const rc_hash_iterator_t* iterator)
{
  /* if the file contains a header, ignore it */
  if (memcmp(&iterator->buffer[0], "NES\x1a", 4) == 0)
  {
    rc_hash_iterator_verbose(iterator, "Ignoring NES header");
    return rc_hash_unheadered_iterator_buffer(hash, iterator, 16);
  }

  if (memcmp(&iterator->buffer[0], "FDS\x1a", 4) == 0)
  {
    rc_hash_iterator_verbose(iterator, "Ignoring FDS header");
    return rc_hash_unheadered_iterator_buffer(hash, iterator, 16);
  }

  return rc_hash_iterator_buffer(hash, iterator);
}

static int rc_hash_n64(char hash[33], const rc_hash_iterator_t* iterator)
{
  uint8_t* buffer;
  uint8_t* stop;
  const size_t buffer_size = 65536;
  md5_state_t md5;
  size_t remaining;
  void* file_handle;
  int is_v64 = 0;
  int is_n64 = 0;

  file_handle = rc_file_open(iterator, iterator->path);
  if (!file_handle)
    return rc_hash_iterator_error(iterator, "Could not open file");

  buffer = (uint8_t*)malloc(buffer_size);
  if (!buffer) {
    rc_file_close(iterator, file_handle);
    return rc_hash_iterator_error(iterator, "Could not allocate temporary buffer");
  }
  stop = buffer + buffer_size;

  /* read first byte so we can detect endianness */
  rc_file_seek(iterator, file_handle, 0, SEEK_SET);
  rc_file_read(iterator, file_handle, buffer, 1);

  if (buffer[0] == 0x80) { /* z64 format (big endian [native]) */
  }
  else if (buffer[0] == 0x37) { /* v64 format (byteswapped) */
    rc_hash_iterator_verbose(iterator, "converting v64 to z64");
    is_v64 = 1;
  }
  else if (buffer[0] == 0x40) { /* n64 format (little endian) */
    rc_hash_iterator_verbose(iterator, "converting n64 to z64");
    is_n64 = 1;
  }
  else if (buffer[0] == 0xE8 || buffer[0] == 0x22) { /* ndd format (don't byteswap) */
  }
  else {
    free(buffer);
    rc_file_close(iterator, file_handle);

    rc_hash_iterator_verbose(iterator, "Not a Nintendo 64 ROM");
    return 0;
  }

  /* calculate total file size */
  rc_file_seek(iterator, file_handle, 0, SEEK_END);
  remaining = (size_t)rc_file_tell(iterator, file_handle);
  if (remaining > MAX_BUFFER_SIZE)
    remaining = MAX_BUFFER_SIZE;

  rc_hash_iterator_verbose_formatted(iterator, "Hashing %u bytes", (unsigned)remaining);

  /* begin hashing */
  md5_init(&md5);

  rc_file_seek(iterator, file_handle, 0, SEEK_SET);
  while (remaining >= buffer_size) {
    rc_file_read(iterator, file_handle, buffer, (int)buffer_size);

    if (is_v64)
      rc_hash_byteswap16(buffer, stop);
    else if (is_n64)
      rc_hash_byteswap32(buffer, stop);

    md5_append(&md5, buffer, (int)buffer_size);
    remaining -= buffer_size;
  }

  if (remaining > 0) {
    rc_file_read(iterator, file_handle, buffer, (int)remaining);

    stop = buffer + remaining;
    if (is_v64)
      rc_hash_byteswap16(buffer, stop);
    else if (is_n64)
      rc_hash_byteswap32(buffer, stop);

    md5_append(&md5, buffer, (int)remaining);
  }

  /* cleanup */
  rc_file_close(iterator, file_handle);
  free(buffer);

  return rc_hash_finalize(iterator, &md5, hash);
}

static int rc_hash_nintendo_3ds_ncch(md5_state_t* md5, void* file_handle, uint8_t header[0x200],
                                     struct AES_ctx* cia_aes, const rc_hash_iterator_t* iterator)
{
  struct AES_ctx ncch_aes;
  uint8_t* hash_buffer;
  uint64_t exefs_offset, exefs_real_size;
  uint32_t exefs_buffer_size;
  uint8_t primary_key[AES_KEYLEN], secondary_key[AES_KEYLEN];
  uint8_t fixed_key_flag, no_crypto_flag, seed_crypto_flag;
  uint8_t crypto_method, secondary_key_x_slot;
  uint16_t ncch_version;
  uint32_t i;
  uint8_t primary_key_y[AES_KEYLEN], program_id[sizeof(uint64_t)];
  uint8_t iv[AES_BLOCKLEN], cia_iv[AES_BLOCKLEN];
  uint8_t exefs_section_name[8];
  uint64_t exefs_section_offset, exefs_section_size;

  exefs_offset = ((uint32_t)header[0x1A3] << 24) | (header[0x1A2] << 16) | (header[0x1A1] << 8) | header[0x1A0];
  exefs_real_size = ((uint32_t)header[0x1A7] << 24) | (header[0x1A6] << 16) | (header[0x1A5] << 8) | header[0x1A4];

  /* Offset and size are in "media units" (1 media unit = 0x200 bytes) */
  exefs_offset *= 0x200;
  exefs_real_size *= 0x200;

  if (exefs_real_size > MAX_BUFFER_SIZE)
    exefs_buffer_size = MAX_BUFFER_SIZE;
  else
    exefs_buffer_size = (uint32_t)exefs_real_size;

  /* This region is technically optional, but it should always be present for executable content (i.e. games) */
  if (exefs_offset == 0 || exefs_real_size == 0)
    return rc_hash_iterator_error(iterator, "ExeFS was not available");

  /* NCCH flag 7 is a bitfield of various crypto related flags */
  fixed_key_flag = header[0x188 + 7] & 0x01;
  no_crypto_flag = header[0x188 + 7] & 0x04;
  seed_crypto_flag = header[0x188 + 7] & 0x20;

  ncch_version = (header[0x113] << 8) | header[0x112];

  if (no_crypto_flag == 0)
  {
    rc_hash_iterator_verbose(iterator, "Encrypted NCCH detected");

    if (fixed_key_flag != 0)
    {
      /* Fixed crypto key means all 0s for both keys */
      memset(primary_key, 0, sizeof(primary_key));
      memset(secondary_key, 0, sizeof(secondary_key));
      rc_hash_iterator_verbose(iterator, "Using fixed key crypto");
    }
    else
    {
      if (_3ds_get_ncch_normal_keys_func == NULL)
        return rc_hash_iterator_error(iterator, "An encrypted NCCH was detected, but the NCCH normal keys callback was not set");

      /* Primary key y is just the first 16 bytes of the header */
      memcpy(primary_key_y, header, sizeof(primary_key_y));

      /* NCCH flag 3 indicates which secondary key x slot is used */
      crypto_method = header[0x188 + 3];

      switch (crypto_method)
      {
        case 0x00:
          rc_hash_iterator_verbose(iterator, "Using NCCH crypto method v1");
          secondary_key_x_slot = 0x2C;
          break;
        case 0x01:
          rc_hash_iterator_verbose(iterator, "Using NCCH crypto method v2");
          secondary_key_x_slot = 0x25;
          break;
        case 0x0A:
          rc_hash_iterator_verbose(iterator, "Using NCCH crypto method v3");
          secondary_key_x_slot = 0x18;
          break;
        case 0x0B:
          rc_hash_iterator_verbose(iterator, "Using NCCH crypto method v4");
          secondary_key_x_slot = 0x1B;
          break;
        default:
          return rc_hash_iterator_error_formatted(iterator, "Invalid crypto method %02X", (unsigned)crypto_method);
      }

      /* We only need the program id if we're doing seed crypto */
      if (seed_crypto_flag != 0)
      {
        rc_hash_iterator_verbose(iterator, "Using seed crypto");
        memcpy(program_id, &header[0x118], sizeof(program_id));
      }

      if (_3ds_get_ncch_normal_keys_func(primary_key_y, secondary_key_x_slot, seed_crypto_flag != 0 ? program_id : NULL, primary_key, secondary_key) == 0)
        return rc_hash_iterator_error(iterator, "Could not obtain NCCH normal keys");
    }

    switch (ncch_version)
    {
      case 0:
      case 2:
        rc_hash_iterator_verbose(iterator, "Detected NCCH version 0/2");
        for (i = 0; i < 8; i++)
        {
          /* First 8 bytes is the partition id in reverse byte order */
          iv[7 - i] = header[0x108 + i];
        }

        /* Magic number for ExeFS */
        iv[8] = 2;

        /* Rest of the bytes are 0 */
        memset(&iv[9], 0, sizeof(iv) - 9);
        break;
      case 1:
        rc_hash_iterator_verbose(iterator, "Detected NCCH version 1");
        for (i = 0; i < 8; i++)
        {
          /* First 8 bytes is the partition id in normal byte order */
          iv[i] = header[0x108 + i];
        }

        /* Next 4 bytes are 0 */
        memset(&iv[8], 0, 4);

        /* Last 4 bytes is the ExeFS byte offset in big endian */
        iv[12] = (exefs_offset >> 24) & 0xFF;
        iv[13] = (exefs_offset >> 16) & 0xFF;
        iv[14] = (exefs_offset >> 8) & 0xFF;
        iv[15] = exefs_offset & 0xFF;
        break;
      default:
        return rc_hash_iterator_error_formatted(iterator, "Invalid NCCH version %04X", (unsigned)ncch_version);
    }
  }

  /* ASSERT: file position must be +0x200 from start of NCCH (i.e. end of header) */
  exefs_offset -= 0x200;

  if (cia_aes)
  {
    /* CBC decryption works by setting the IV to the encrypted previous block.
     * Normally this means we would need to decrypt the data between the header and the ExeFS so the CIA AES state is correct.
     * However, we can abuse how CBC decryption works and just set the IV to last block we would otherwise decrypt.
     * We don't care about the data betweeen the header and ExeFS, so this works fine. */

    rc_file_seek(iterator, file_handle, (int64_t)exefs_offset - AES_BLOCKLEN, SEEK_CUR);
    if (rc_file_read(iterator, file_handle, cia_iv, AES_BLOCKLEN) != AES_BLOCKLEN)
      return rc_hash_iterator_error(iterator, "Could not read NCCH data");

    AES_ctx_set_iv(cia_aes, cia_iv);
  }
  else
  {
    /* No encryption present, just skip over the in-between data */
    rc_file_seek(iterator, file_handle, (int64_t)exefs_offset, SEEK_CUR);
  }

  hash_buffer = (uint8_t*)malloc(exefs_buffer_size);
  if (!hash_buffer)
    return rc_hash_iterator_error_formatted(iterator, "Failed to allocate %u bytes", (unsigned)exefs_buffer_size);

  /* Clear out crypto flags to ensure we get the same hash for decrypted and encrypted ROMs */
  memset(&header[0x114], 0, 4);
  header[0x188 + 3] = 0;
  header[0x188 + 7] &= ~(0x20 | 0x04 | 0x01);

  rc_hash_iterator_verbose(iterator, "Hashing 512 byte NCCH header");
  md5_append(md5, header, 0x200);

  rc_hash_iterator_verbose_formatted(iterator, "Hashing %u bytes for ExeFS (at NCCH offset %08X%08X)",
    (unsigned)exefs_buffer_size, (unsigned)(exefs_offset >> 32), (unsigned)exefs_offset);

  if (rc_file_read(iterator, file_handle, hash_buffer, exefs_buffer_size) != exefs_buffer_size)
  {
    free(hash_buffer);
    return rc_hash_iterator_error(iterator, "Could not read ExeFS data");
  }

  if (cia_aes)
  {
    rc_hash_iterator_verbose(iterator, "Performing CIA decryption for ExeFS");
    AES_CBC_decrypt_buffer(cia_aes, hash_buffer, exefs_buffer_size);
  }

  if (no_crypto_flag == 0)
  {
    rc_hash_iterator_verbose(iterator, "Performing NCCH decryption for ExeFS");

    AES_init_ctx_iv(&ncch_aes, primary_key, iv);
    AES_CTR_xcrypt_buffer(&ncch_aes, hash_buffer, 0x200);

    for (i = 0; i < 8; i++)
    {
      memcpy(exefs_section_name, &hash_buffer[i * 16], sizeof(exefs_section_name));
      exefs_section_offset = ((uint32_t)hash_buffer[i * 16 + 11] << 24) | (hash_buffer[i * 16 + 10] << 16) | (hash_buffer[i * 16 + 9] << 8) | hash_buffer[i * 16 + 8];
      exefs_section_size = ((uint32_t)hash_buffer[i * 16 + 15] << 24) | (hash_buffer[i * 16 + 14] << 16) | (hash_buffer[i * 16 + 13] << 8) | hash_buffer[i * 16 + 12];

      /* 0 size indicates an unused section */
      if (exefs_section_size == 0)
        continue;

      /* Offsets must be aligned by a media unit */
      if (exefs_section_offset & 0x1FF)
        return rc_hash_iterator_error(iterator, "ExeFS section offset is misaligned");

      /* Offset is relative to the end of the header */
      exefs_section_offset += 0x200;

      /* Check against malformed sections */
      if (exefs_section_offset + ((exefs_section_size + 0x1FF) & ~(uint64_t)0x1FF) > (uint64_t)exefs_real_size)
        return rc_hash_iterator_error(iterator, "ExeFS section would overflow");

      if (memcmp(exefs_section_name, "icon", 4) == 0 || memcmp(exefs_section_name, "banner", 6) == 0)
      {
        /* Align size up by a media unit */
        exefs_section_size = (exefs_section_size + 0x1FF) & ~(uint64_t)0x1FF;
        AES_init_ctx(&ncch_aes, primary_key);
      }
      else
      {
        /* We don't align size up here, as the padding bytes will use the primary key rather than the secondary key */
        AES_init_ctx(&ncch_aes, secondary_key);
      }

      /* In theory, the section offset + size could be greater than the buffer size */
      /* In practice, this likely never occurs, but just in case it does, ignore the section or constrict the size */
      if (exefs_section_offset + exefs_section_size > exefs_buffer_size)
      {
        if (exefs_section_offset >= exefs_buffer_size)
          continue;

        exefs_section_size = exefs_buffer_size - exefs_section_offset;
      }

      exefs_section_name[7] = '\0';
      rc_hash_iterator_verbose_formatted(iterator, "Decrypting ExeFS file %s at ExeFS offset %08X with size %08X",
        (const char*)exefs_section_name, (unsigned)exefs_section_offset, (unsigned)exefs_section_size);

      AES_CTR_xcrypt_buffer(&ncch_aes, &hash_buffer[exefs_section_offset], exefs_section_size & ~(uint64_t)0xF);

      if (exefs_section_size & 0x1FF)
      {
        /* Handle padding bytes, these always use the primary key */
        exefs_section_offset += exefs_section_size;
        exefs_section_size = 0x200 - (exefs_section_size & 0x1FF);

        rc_hash_iterator_verbose_formatted(iterator, "Decrypting ExeFS padding at ExeFS offset %08X with size %08X",
          (unsigned)exefs_section_offset, (unsigned)exefs_section_size);

        /* Align our decryption start to an AES block boundary */
        if (exefs_section_size & 0xF)
        {
          /* We're a little evil here re-using the IV like this, but this seems to be the best way to deal with this... */
          memcpy(iv, ncch_aes.Iv, sizeof(iv));
          exefs_section_offset &= ~(uint64_t)0xF;

          /* First decrypt these last bytes using the secondary key */
          AES_CTR_xcrypt_buffer(&ncch_aes, &hash_buffer[exefs_section_offset], 0x10 - (exefs_section_size & 0xF));

          /* Now re-encrypt these bytes using the primary key */
          AES_init_ctx_iv(&ncch_aes, primary_key, iv);
          AES_CTR_xcrypt_buffer(&ncch_aes, &hash_buffer[exefs_section_offset], 0x10 - (exefs_section_size & 0xF));

          /* All of the padding can now be decrypted using the primary key */
          AES_ctx_set_iv(&ncch_aes, iv);
          exefs_section_size += 0x10 - (exefs_section_size & 0xF);
        }

        AES_init_ctx(&ncch_aes, primary_key);
        AES_CTR_xcrypt_buffer(&ncch_aes, &hash_buffer[exefs_section_offset], (size_t)exefs_section_size);
      }
    }
  }

  md5_append(md5, hash_buffer, exefs_buffer_size);

  free(hash_buffer);
  return 1;
}

static uint32_t rc_hash_nintendo_3ds_cia_signature_size(uint8_t header[0x200], const rc_hash_iterator_t* iterator)
{
  uint32_t signature_type;

  signature_type = ((uint32_t)header[0] << 24) | (header[1] << 16) | (header[2] << 8) | header[3];
  switch (signature_type)
  {
    case 0x010000:
    case 0x010003:
      return 0x200 + 0x3C;
    case 0x010001:
    case 0x010004:
      return 0x100 + 0x3C;
    case 0x010002:
    case 0x010005:
      return 0x3C + 0x40;
    default:
      return rc_hash_iterator_error_formatted(iterator, "Invalid signature type %08X", (unsigned)signature_type);
  }
}

static int rc_hash_nintendo_3ds_cia(md5_state_t* md5, void* file_handle, uint8_t header[0x200],
                                    const rc_hash_iterator_t* iterator)
{
  const uint32_t CIA_HEADER_SIZE = 0x2020; /* Yes, this is larger than the header[0x200], but we only use the beginning of the header */
  const uint64_t CIA_ALIGNMENT_MASK = 64 - 1; /* sizes are aligned by 64 bytes */
  struct AES_ctx aes;
  uint8_t iv[AES_BLOCKLEN], normal_key[AES_KEYLEN], title_key[AES_KEYLEN], title_id[sizeof(uint64_t)];
  uint32_t cert_size, tik_size, tmd_size;
  int64_t cert_offset, tik_offset, tmd_offset, content_offset;
  uint32_t signature_size, i;
  uint16_t content_count;
  uint8_t common_key_index;

  cert_size = ((uint32_t)header[0x0B] << 24) | (header[0x0A] << 16) | (header[0x09] << 8) | header[0x08];
  tik_size = ((uint32_t)header[0x0F] << 24) | (header[0x0E] << 16) | (header[0x0D] << 8) | header[0x0C];
  tmd_size = ((uint32_t)header[0x13] << 24) | (header[0x12] << 16) | (header[0x11] << 8) | header[0x10];

  cert_offset = (CIA_HEADER_SIZE + CIA_ALIGNMENT_MASK) & ~CIA_ALIGNMENT_MASK;
  tik_offset = (cert_offset + cert_size + CIA_ALIGNMENT_MASK) & ~CIA_ALIGNMENT_MASK;
  tmd_offset = (tik_offset + tik_size + CIA_ALIGNMENT_MASK) & ~CIA_ALIGNMENT_MASK;
  content_offset = (tmd_offset + tmd_size + CIA_ALIGNMENT_MASK) & ~CIA_ALIGNMENT_MASK;

  /* Check if this CIA is encrypted, if it isn't, we can hash it right away */

  rc_file_seek(iterator, file_handle, tmd_offset, SEEK_SET);
  if (rc_file_read(iterator, file_handle, header, 4) != 4)
    return rc_hash_iterator_error(iterator, "Could not read TMD signature type");

  signature_size = rc_hash_nintendo_3ds_cia_signature_size(header, iterator);
  if (signature_size == 0)
    return 0; /* rc_hash_nintendo_3ds_cia_signature_size will call rc_hash_error, so we don't need to do so here */

  rc_file_seek(iterator, file_handle, signature_size + 0x9E, SEEK_CUR);
  if (rc_file_read(iterator, file_handle, header, 2) != 2)
    return rc_hash_iterator_error(iterator, "Could not read TMD content count");

  content_count = (header[0] << 8) | header[1];

  rc_file_seek(iterator, file_handle, 0x9C4 - 0x9E - 2, SEEK_CUR);
  for (i = 0; i < content_count; i++)
  {
    if (rc_file_read(iterator, file_handle, header, 0x30) != 0x30)
      return rc_hash_iterator_error(iterator, "Could not read TMD content chunk");

    /* Content index 0 is the main content (i.e. the 3DS executable)  */
    if (((header[4] << 8) | header[5]) == 0)
      break;

    content_offset += ((uint32_t)header[0xC] << 24) | (header[0xD] << 16) | (header[0xE] << 8) | header[0xF];
  }

  if (i == content_count)
    return rc_hash_iterator_error(iterator, "Could not find main content chunk in TMD");

  if ((header[7] & 1) == 0)
  {
    /* Not encrypted, we can hash the NCCH immediately */
    rc_file_seek(iterator, file_handle, content_offset, SEEK_SET);
    if (rc_file_read(iterator, file_handle, header, 0x200) != 0x200)
      return rc_hash_iterator_error(iterator, "Could not read NCCH header");

    if (memcmp(&header[0x100], "NCCH", 4) != 0)
      return rc_hash_iterator_error_formatted(iterator, "NCCH header was not at %08X%08X", (unsigned)(content_offset >> 32), (unsigned)content_offset);

    return rc_hash_nintendo_3ds_ncch(md5, file_handle, header, NULL, iterator);
  }

  if (_3ds_get_cia_normal_key_func == NULL)
    return rc_hash_iterator_error(iterator, "An encrypted CIA was detected, but the CIA normal key callback was not set");

  /* Acquire the encrypted title key, title id, and common key index from the ticket */
  /* These will be needed to decrypt the title key, and that will be needed to decrypt the CIA */

  rc_file_seek(iterator, file_handle, tik_offset, SEEK_SET);
  if (rc_file_read(iterator, file_handle, header, 4) != 4)
    return rc_hash_iterator_error(iterator, "Could not read ticket signature type");

  signature_size = rc_hash_nintendo_3ds_cia_signature_size(header, iterator);
  if (signature_size == 0)
    return 0;

  rc_file_seek(iterator, file_handle, signature_size, SEEK_CUR);
  if (rc_file_read(iterator, file_handle, header, 0xB2) != 0xB2)
    return rc_hash_iterator_error(iterator, "Could not read ticket data");

  memcpy(title_key, &header[0x7F], sizeof(title_key));
  memcpy(title_id, &header[0x9C], sizeof(title_id));
  common_key_index = header[0xB1];

  if (common_key_index > 5)
    return rc_hash_iterator_error_formatted(iterator, "Invalid common key index %02X", (unsigned)common_key_index);

  if (_3ds_get_cia_normal_key_func(common_key_index, normal_key) == 0)
    return rc_hash_iterator_error_formatted(iterator, "Could not obtain common key %02X", (unsigned)common_key_index);

  memset(iv, 0, sizeof(iv));
  memcpy(iv, title_id, sizeof(title_id));
  AES_init_ctx_iv(&aes, normal_key, iv);

  /* Finally, decrypt the title key */
  AES_CBC_decrypt_buffer(&aes, title_key, sizeof(title_key));

  /* Now we can hash the NCCH */

  rc_file_seek(iterator, file_handle, content_offset, SEEK_SET);
  if (rc_file_read(iterator, file_handle, header, 0x200) != 0x200)
    return rc_hash_iterator_error(iterator, "Could not read NCCH header");

  memset(iv, 0, sizeof(iv)); /* Content index is iv (which is always 0 for main content) */
  AES_init_ctx_iv(&aes, title_key, iv);
  AES_CBC_decrypt_buffer(&aes, header, 0x200);

  if (memcmp(&header[0x100], "NCCH", 4) != 0)
    return rc_hash_iterator_error_formatted(iterator, "NCCH header was not at %08X%08X", (unsigned)(content_offset >> 32), (unsigned)content_offset);

  return rc_hash_nintendo_3ds_ncch(md5, file_handle, header, &aes, iterator);
}

static int rc_hash_nintendo_3ds_3dsx(md5_state_t* md5, void* file_handle, uint8_t header[0x200], const rc_hash_iterator_t* iterator)
{
  uint8_t* hash_buffer;
  uint32_t header_size, reloc_header_size, code_size;
  int64_t code_offset;

  header_size = (header[5] << 8) | header[4];
  reloc_header_size = (header[7] << 8) | header[6];
  code_size = ((uint32_t)header[0x13] << 24) | (header[0x12] << 16) | (header[0x11] << 8) | header[0x10];

  /* 3 relocation headers are in-between the 3DSX header and code segment */
  code_offset = header_size + reloc_header_size * 3;

  if (code_size > MAX_BUFFER_SIZE)
    code_size = MAX_BUFFER_SIZE;

  hash_buffer = (uint8_t*)malloc(code_size);
  if (!hash_buffer)
    return rc_hash_iterator_error_formatted(iterator, "Failed to allocate %u bytes", (unsigned)code_size);

  rc_file_seek(iterator, file_handle, code_offset, SEEK_SET);

  rc_hash_iterator_verbose_formatted(iterator, "Hashing %u bytes for 3DSX (at %08X)", (unsigned)code_size, (unsigned)code_offset);

  if (rc_file_read(iterator, file_handle, hash_buffer, code_size) != code_size)
  {
    free(hash_buffer);
    return rc_hash_iterator_error(iterator, "Could not read 3DSX code segment");
  }

  md5_append(md5, hash_buffer, code_size);

  free(hash_buffer);
  return 1;
}

static int rc_hash_nintendo_3ds(char hash[33], const rc_hash_iterator_t* iterator)
{
  md5_state_t md5;
  void* file_handle;
  uint8_t header[0x200]; /* NCCH and NCSD headers are both 0x200 bytes */
  int64_t header_offset;

  file_handle = rc_file_open(iterator, iterator->path);
  if (!file_handle)
    return rc_hash_iterator_error(iterator, "Could not open file");

  rc_file_seek(iterator, file_handle, 0, SEEK_SET);

  /* If we don't have a full header, this is probably not a 3DS ROM */
  if (rc_file_read(iterator, file_handle, header, sizeof(header)) != sizeof(header))
  {
    rc_file_close(iterator, file_handle);
    return rc_hash_iterator_error(iterator, "Could not read 3DS ROM header");
  }

  md5_init(&md5);

  if (memcmp(&header[0x100], "NCSD", 4) == 0)
  {
    /* A NCSD container contains 1-8 NCCH partitions */
    /* The first partition (index 0) is reserved for executable content */
    header_offset = ((uint32_t)header[0x123] << 24) | (header[0x122] << 16) | (header[0x121] << 8) | header[0x120];
    /* Offset is in "media units" (1 media unit = 0x200 bytes) */
    header_offset *= 0x200;

    /* We include the NCSD header in the hash, as that will ensure different versions of a game result in a different hash
     * This is due to some revisions / languages only ever changing other NCCH paritions (e.g. the game manual)
     */
    rc_hash_iterator_verbose(iterator, "Hashing 512 byte NCSD header");
    md5_append(&md5, header, sizeof(header));

    rc_hash_iterator_verbose_formatted(iterator,
      "Detected NCSD header, seeking to NCCH partition at %08X%08X",
      (unsigned)(header_offset >> 32), (unsigned)header_offset);

    rc_file_seek(iterator, file_handle, header_offset, SEEK_SET);
    if (rc_file_read(iterator, file_handle, header, sizeof(header)) != sizeof(header))
    {
      rc_file_close(iterator, file_handle);
      return rc_hash_iterator_error(iterator, "Could not read 3DS NCCH header");
    }

    if (memcmp(&header[0x100], "NCCH", 4) != 0)
    {
      rc_file_close(iterator, file_handle);
      return rc_hash_iterator_error_formatted(iterator, "3DS NCCH header was not at %08X%08X", (unsigned)(header_offset >> 32), (unsigned)header_offset);
    }
  }

  if (memcmp(&header[0x100], "NCCH", 4) == 0)
  {
    if (rc_hash_nintendo_3ds_ncch(&md5, file_handle, header, NULL, iterator))
    {
      rc_file_close(iterator, file_handle);
      return rc_hash_finalize(iterator, &md5, hash);
    }

    rc_file_close(iterator, file_handle);
    return rc_hash_iterator_error(iterator, "Failed to hash 3DS NCCH container");
  }

  /* Couldn't identify either an NCSD or NCCH */

  /* Try to identify this as a CIA */
  if (header[0] == 0x20 && header[1] == 0x20 && header[2] == 0x00 && header[3] == 0x00)
  {
    rc_hash_iterator_verbose(iterator, "Detected CIA, attempting to find executable NCCH");

    if (rc_hash_nintendo_3ds_cia(&md5, file_handle, header, iterator))
    {
      rc_file_close(iterator, file_handle);
      return rc_hash_finalize(iterator, &md5, hash);
    }

    rc_file_close(iterator, file_handle);
    return rc_hash_iterator_error(iterator, "Failed to hash 3DS CIA container");
  }

  /* This might be a homebrew game, try to detect that */
  if (memcmp(&header[0], "3DSX", 4) == 0)
  {
    rc_hash_iterator_verbose(iterator, "Detected 3DSX");

    if (rc_hash_nintendo_3ds_3dsx(&md5, file_handle, header, iterator))
    {
      rc_file_close(iterator, file_handle);
      return rc_hash_finalize(iterator, &md5, hash);
    }

    rc_file_close(iterator, file_handle);
    return rc_hash_iterator_error(iterator, "Failed to hash 3DS 3DSX container");
  }

  /* Raw ELF marker (AXF/ELF files) */
  if (memcmp(&header[0], "\x7f\x45\x4c\x46", 4) == 0)
  {
    rc_hash_iterator_verbose(iterator, "Detected AXF/ELF file, hashing entire file");

    /* Don't bother doing anything fancy here, just hash entire file */
    rc_file_close(iterator, file_handle);
    return rc_hash_whole_file(hash, iterator);
  }

  rc_file_close(iterator, file_handle);
  return rc_hash_iterator_error(iterator, "Not a 3DS ROM");
}

static int rc_hash_nintendo_ds(char hash[33], const rc_hash_iterator_t* iterator)
{
  uint8_t header[512];
  uint8_t* hash_buffer;
  uint32_t hash_size, arm9_size, arm9_addr, arm7_size, arm7_addr, icon_addr;
  size_t num_read;
  int64_t offset = 0;
  md5_state_t md5;
  void* file_handle;

  file_handle = rc_file_open(iterator, iterator->path);
  if (!file_handle)
    return rc_hash_iterator_error(iterator, "Could not open file");

  rc_file_seek(iterator, file_handle, 0, SEEK_SET);
  if (rc_file_read(iterator, file_handle, header, sizeof(header)) != 512)
    return rc_hash_iterator_error(iterator, "Failed to read header");

  if (header[0] == 0x2E && header[1] == 0x00 && header[2] == 0x00 && header[3] == 0xEA &&
      header[0xB0] == 0x44 && header[0xB1] == 0x46 && header[0xB2] == 0x96 && header[0xB3] == 0)
  {
    /* SuperCard header detected, ignore it */
    rc_hash_iterator_verbose(iterator, "Ignoring SuperCard header");

    offset = 512;
    rc_file_seek(iterator, file_handle, offset, SEEK_SET);
    rc_file_read(iterator, file_handle, header, sizeof(header));
  }

  arm9_addr = header[0x20] | (header[0x21] << 8) | (header[0x22] << 16) | (header[0x23] << 24);
  arm9_size = header[0x2C] | (header[0x2D] << 8) | (header[0x2E] << 16) | (header[0x2F] << 24);
  arm7_addr = header[0x30] | (header[0x31] << 8) | (header[0x32] << 16) | (header[0x33] << 24);
  arm7_size = header[0x3C] | (header[0x3D] << 8) | (header[0x3E] << 16) | (header[0x3F] << 24);
  icon_addr = header[0x68] | (header[0x69] << 8) | (header[0x6A] << 16) | (header[0x6B] << 24);

  if (arm9_size + arm7_size > 16 * 1024 * 1024)
  {
    /* sanity check - code blocks are typically less than 1MB each - assume not a DS ROM */
    return rc_hash_iterator_error_formatted(iterator, "arm9 code size (%u) + arm7 code size (%u) exceeds 16MB", arm9_size, arm7_size);
  }

  hash_size = 0xA00;
  if (arm9_size > hash_size)
    hash_size = arm9_size;
  if (arm7_size > hash_size)
    hash_size = arm7_size;

  hash_buffer = (uint8_t*)malloc(hash_size);
  if (!hash_buffer)
  {
    rc_file_close(iterator, file_handle);
    return rc_hash_iterator_error_formatted(iterator, "Failed to allocate %u bytes", hash_size);
  }

  md5_init(&md5);

  rc_hash_iterator_verbose(iterator, "Hashing 352 byte header");
  md5_append(&md5, header, 0x160);

  rc_hash_iterator_verbose_formatted(iterator, "Hashing %u byte arm9 code (at %08X)", arm9_size, arm9_addr);

  rc_file_seek(iterator, file_handle, arm9_addr + offset, SEEK_SET);
  rc_file_read(iterator, file_handle, hash_buffer, arm9_size);
  md5_append(&md5, hash_buffer, arm9_size);

  rc_hash_iterator_verbose_formatted(iterator, "Hashing %u byte arm7 code (at %08X)", arm7_size, arm7_addr);

  rc_file_seek(iterator, file_handle, arm7_addr + offset, SEEK_SET);
  rc_file_read(iterator, file_handle, hash_buffer, arm7_size);
  md5_append(&md5, hash_buffer, arm7_size);

  rc_hash_iterator_verbose_formatted(iterator, "Hashing 2560 byte icon and labels data (at %08X)", icon_addr);

  rc_file_seek(iterator, file_handle, icon_addr + offset, SEEK_SET);
  num_read = rc_file_read(iterator, file_handle, hash_buffer, 0xA00);
  if (num_read < 0xA00)
  {
    /* some homebrew games don't provide a full icon block, and no data after the icon block.
     * if we didn't get a full icon block, fill the remaining portion with 0s
     */
    rc_hash_iterator_verbose_formatted(iterator,
      "Warning: only got %u bytes for icon and labels data, 0-padding to 2560 bytes", (unsigned)num_read);

    memset(&hash_buffer[num_read], 0, 0xA00 - num_read);
  }
  md5_append(&md5, hash_buffer, 0xA00);

  free(hash_buffer);
  rc_file_close(iterator, file_handle);

  return rc_hash_finalize(iterator, &md5, hash);
}

static int rc_hash_gamecube(char hash[33], const rc_hash_iterator_t* iterator)
{
  md5_state_t md5;
  void* file_handle;
  const uint32_t BASE_HEADER_SIZE = 0x2440;
  const uint32_t MAX_HEADER_SIZE = 1024 * 1024;
  uint32_t apploader_header_size, apploader_body_size, apploader_trailer_size, header_size;
  uint8_t quad_buffer[4];
  uint8_t addr_buffer[0xD8];
  uint8_t* buffer;
  uint32_t dol_offset;
  uint32_t dol_offsets[18];
  uint32_t dol_sizes[18];
  uint32_t dol_buf_size = 0;
  uint32_t ix;

  file_handle = rc_file_open(iterator, iterator->path);
  if (!file_handle)
    return rc_hash_iterator_error(iterator, "Could not open file");

  /* Verify Gamecube */
  rc_file_seek(iterator, file_handle, 0x1c, SEEK_SET);
  rc_file_read(iterator, file_handle, quad_buffer, 4);
  if (quad_buffer[0] != 0xC2|| quad_buffer[1] != 0x33 || quad_buffer[2] != 0x9F || quad_buffer[3] != 0x3D)
  {
    rc_file_close(iterator, file_handle);
    return rc_hash_iterator_error(iterator, "Not a Gamecube disc");
  }

  /* GetApploaderSize */
  rc_file_seek(iterator, file_handle, BASE_HEADER_SIZE + 0x14, SEEK_SET);
  apploader_header_size = 0x20;
  rc_file_read(iterator, file_handle, quad_buffer, 4);
  apploader_body_size =
    (quad_buffer[0] << 24) | (quad_buffer[1] << 16) | (quad_buffer[2] << 8) | quad_buffer[3];
  rc_file_read(iterator, file_handle, quad_buffer, 4);
  apploader_trailer_size =
    (quad_buffer[0] << 24) | (quad_buffer[1] << 16) | (quad_buffer[2] << 8) | quad_buffer[3];
  header_size = BASE_HEADER_SIZE + apploader_header_size + apploader_body_size + apploader_trailer_size;
  if (header_size > MAX_HEADER_SIZE)
    header_size = MAX_HEADER_SIZE;

  /* Hash headers */
  buffer = (uint8_t*)malloc(header_size);
  if (!buffer)
  {
    rc_file_close(iterator, file_handle);
    return rc_hash_iterator_error(iterator, "Could not allocate temporary buffer");
  }
  rc_file_seek(iterator, file_handle, 0, SEEK_SET);
  rc_file_read(iterator, file_handle, buffer, header_size);
  md5_init(&md5);

  rc_hash_iterator_verbose_formatted(iterator, "Hashing %u byte header", header_size);

  md5_append(&md5, buffer, header_size);

  /* GetBootDOLOffset
   * Base header size is guaranteed larger than 0x423 therefore buffer contains dol_offset right now
   */
  dol_offset = (buffer[0x420] << 24) | (buffer[0x421] << 16) | (buffer[0x422] << 8) | buffer[0x423];
  free(buffer);

  /* Find offsetsand sizes for the 7 main.dol code segments and 11 main.dol data segments */
  rc_file_seek(iterator, file_handle, dol_offset, SEEK_SET);
  rc_file_read(iterator, file_handle, addr_buffer, 0xD8);
  for (ix = 0; ix < 18; ix++)
  {
    dol_offsets[ix] =
      (addr_buffer[0x0 + ix * 4] << 24) |
      (addr_buffer[0x1 + ix * 4] << 16) |
      (addr_buffer[0x2 + ix * 4] << 8) |
      addr_buffer[0x3 + ix * 4];
    dol_sizes[ix] =
      (addr_buffer[0x90 + ix * 4] << 24) |
      (addr_buffer[0x91 + ix * 4] << 16) |
      (addr_buffer[0x92 + ix * 4] << 8) |
      addr_buffer[0x93 + ix * 4];
    dol_buf_size = (dol_sizes[ix] > dol_buf_size) ? dol_sizes[ix] : dol_buf_size;
  }

  /* Iterate through the 18 main.dol segments and hash each */
  buffer = (uint8_t*)malloc(dol_buf_size);
  if (!buffer)
  {
    rc_file_close(iterator, file_handle);
    return rc_hash_iterator_error(iterator, "Could not allocate temporary buffer");
  }
  for (ix = 0; ix < 18; ix++)
  {
    if (dol_sizes[ix] == 0)
      continue;

    if (ix < 7)
      rc_hash_iterator_verbose_formatted(iterator, "Hashing %u byte main.dol code segment %u", dol_sizes[ix], ix);
    else
      rc_hash_iterator_verbose_formatted(iterator, "Hashing %u byte main.dol data segment %u", dol_sizes[ix], ix - 7);

    rc_file_seek(iterator, file_handle, dol_offsets[ix], SEEK_SET);
    rc_file_read(iterator, file_handle, buffer, dol_sizes[ix]);

    md5_append(&md5, buffer, dol_sizes[ix]);
  }

  /* Finalize */
  rc_file_close(iterator, file_handle);
  free(buffer);

  return rc_hash_finalize(iterator, &md5, hash);
}

static int rc_hash_pce(char hash[33], const rc_hash_iterator_t* iterator)
{
  /* if the file contains a header, ignore it (expect ROM data to be multiple of 128KB) */
  uint32_t calc_size = ((uint32_t)iterator->buffer_size / 0x20000) * 0x20000;
  if (iterator->buffer_size - calc_size == 512)
  {
    rc_hash_iterator_verbose(iterator, "Ignoring PCE header");
    return rc_hash_unheadered_iterator_buffer(hash, iterator, 512);
  }

  return rc_hash_iterator_buffer(hash, iterator);
}

static int rc_hash_pce_track(char hash[33], void* track_handle, const rc_hash_iterator_t* iterator)
{
  uint8_t buffer[2048];
  md5_state_t md5;
  uint32_t sector, num_sectors;
  uint32_t size;

  /* the PC-Engine uses the second sector to specify boot information and program name.
   * the string "PC Engine CD-ROM SYSTEM" should exist at 32 bytes into the sector
   * http://shu.sheldows.com/shu/download/pcedocs/pce_cdrom.html
   */
  if (rc_cd_read_sector(iterator, track_handle, rc_cd_first_track_sector(iterator, track_handle) + 1, buffer, 128) < 128)
  {
    return rc_hash_iterator_error(iterator, "Not a PC Engine CD");
  }

  /* normal PC Engine CD will have a header block in sector 1 */
  if (memcmp("PC Engine CD-ROM SYSTEM", &buffer[32], 23) == 0)
  {
    /* the title of the disc is the last 22 bytes of the header */
    md5_init(&md5);
    md5_append(&md5, &buffer[106], 22);

    buffer[128] = '\0';
    rc_hash_iterator_verbose_formatted(iterator, "Found PC Engine CD, title=%.22s", &buffer[106]);

    /* the first three bytes specify the sector of the program data, and the fourth byte
     * is the number of sectors.
     */
    sector = (buffer[0] << 16) + (buffer[1] << 8) + buffer[2];
    num_sectors = buffer[3];

    rc_hash_iterator_verbose_formatted(iterator, "Hashing %d sectors starting at sector %d", num_sectors, sector);

    sector += rc_cd_first_track_sector(iterator, track_handle);
    while (num_sectors > 0)
    {
      rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer));
      md5_append(&md5, buffer, sizeof(buffer));

      ++sector;
      --num_sectors;
    }
  }
  /* GameExpress CDs use a standard Joliet filesystem - locate and hash the BOOT.BIN */
  else if ((sector = rc_cd_find_file_sector(iterator, track_handle, "BOOT.BIN", &size)) != 0 && size < MAX_BUFFER_SIZE)
  {
    md5_init(&md5);
    while (size > sizeof(buffer))
    {
      rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer));
      md5_append(&md5, buffer, sizeof(buffer));

      ++sector;
      size -= sizeof(buffer);
    }

    if (size > 0)
    {
      rc_cd_read_sector(iterator, track_handle, sector, buffer, size);
      md5_append(&md5, buffer, size);
    }
  }
  else
  {
    return rc_hash_iterator_error(iterator, "Not a PC Engine CD");
  }

  return rc_hash_finalize(iterator, &md5, hash);
}

static int rc_hash_pce_cd(char hash[33], const rc_hash_iterator_t* iterator)
{
  int result;
  void* track_handle = rc_cd_open_track(iterator, RC_HASH_CDTRACK_FIRST_DATA);
  if (!track_handle)
    return rc_hash_iterator_error(iterator, "Could not open track");

  result = rc_hash_pce_track(hash, track_handle, iterator);

  rc_cd_close_track(iterator, track_handle);

  return result;
}

static int rc_hash_pcfx_cd(char hash[33], const rc_hash_iterator_t* iterator)
{
  uint8_t buffer[2048];
  void* track_handle;
  md5_state_t md5;
  int sector, num_sectors;

  /* PC-FX executable can be in any track. Assume it's in the largest data track and check there first */
  track_handle = rc_cd_open_track(iterator, RC_HASH_CDTRACK_LARGEST);
  if (!track_handle)
    return rc_hash_iterator_error(iterator, "Could not open track");

  /* PC-FX CD will have a header marker in sector 0 */
  sector = rc_cd_first_track_sector(iterator, track_handle);
  rc_cd_read_sector(iterator, track_handle, sector, buffer, 32);
  if (memcmp("PC-FX:Hu_CD-ROM", &buffer[0], 15) != 0)
  {
    rc_cd_close_track(iterator, track_handle);

    /* not found in the largest data track, check track 2 */
    track_handle = rc_cd_open_track(iterator, 2);
    if (!track_handle)
      return rc_hash_iterator_error(iterator, "Could not open track");

    sector = rc_cd_first_track_sector(iterator, track_handle);
    rc_cd_read_sector(iterator, track_handle, sector, buffer, 32);
  }

  if (memcmp("PC-FX:Hu_CD-ROM", &buffer[0], 15) == 0)
  {
    /* PC-FX boot header fills the first two sectors of the disc
     * https://bitbucket.org/trap15/pcfxtools/src/master/pcfx-cdlink.c
     * the important stuff is the first 128 bytes of the second sector (title being the first 32) */
    rc_cd_read_sector(iterator, track_handle, sector + 1, buffer, 128);

    md5_init(&md5);
    md5_append(&md5, buffer, 128);

    rc_hash_iterator_verbose_formatted(iterator, "Found PC-FX CD, title=%.32s", &buffer[0]);

    /* the program sector is in bytes 33-36 (assume byte 36 is 0) */
    sector = (buffer[34] << 16) + (buffer[33] << 8) + buffer[32];

    /* the number of sectors the program occupies is in bytes 37-40 (assume byte 40 is 0) */
    num_sectors = (buffer[38] << 16) + (buffer[37] << 8) + buffer[36];

    rc_hash_iterator_verbose_formatted(iterator, "Hashing %d sectors starting at sector %d", num_sectors, sector);

    sector += rc_cd_first_track_sector(iterator, track_handle);
    while (num_sectors > 0)
    {
      rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer));
      md5_append(&md5, buffer, sizeof(buffer));

      ++sector;
      --num_sectors;
    }
  }
  else
  {
    int result = 0;
    rc_cd_read_sector(iterator, track_handle, sector + 1, buffer, 128);

    /* some PC-FX CDs still identify as PCE CDs */
    if (memcmp("PC Engine CD-ROM SYSTEM", &buffer[32], 23) == 0)
      result = rc_hash_pce_track(hash, track_handle, iterator);

    rc_cd_close_track(iterator, track_handle);
    if (result)
      return result;

    return rc_hash_iterator_error(iterator, "Not a PC-FX CD");
  }

  rc_cd_close_track(iterator, track_handle);

  return rc_hash_finalize(iterator, &md5, hash);
}

static int rc_hash_dreamcast(char hash[33], const rc_hash_iterator_t* iterator)
{
  uint8_t buffer[256] = "";
  void* track_handle;
  char exe_file[32] = "";
  uint32_t size;
  uint32_t sector;
  int result = 0;
  md5_state_t md5;
  int i = 0;

  /* track 03 is the data track that contains the TOC and IP.BIN */
  track_handle = rc_cd_open_track(iterator, 3);
  if (track_handle)
  {
    /* first 256 bytes from first sector should have IP.BIN structure that stores game meta information
     * https://mc.pp.se/dc/ip.bin.html */
    rc_cd_read_sector(iterator, track_handle, rc_cd_first_track_sector(iterator, track_handle), buffer, sizeof(buffer));
  }

  if (memcmp(&buffer[0], "SEGA SEGAKATANA ", 16) != 0)
  {
    if (track_handle)
      rc_cd_close_track(iterator, track_handle);

    /* not a gd-rom dreamcast file. check for mil-cd by looking for the marker in the first data track */
    track_handle = rc_cd_open_track(iterator, RC_HASH_CDTRACK_FIRST_DATA);
    if (!track_handle)
      return rc_hash_iterator_error(iterator, "Could not open track");

    rc_cd_read_sector(iterator, track_handle, rc_cd_first_track_sector(iterator, track_handle), buffer, sizeof(buffer));
    if (memcmp(&buffer[0], "SEGA SEGAKATANA ", 16) != 0)
    {
      /* did not find marker on track 3 or first data track */
      rc_cd_close_track(iterator, track_handle);
      return rc_hash_iterator_error(iterator, "Not a Dreamcast CD");
    }
  }

  /* start the hash with the game meta information */
  md5_init(&md5);
  md5_append(&md5, (md5_byte_t*)buffer, 256);

  if (iterator->callbacks.verbose_message)
  {
    char message[256];
    uint8_t* ptr = &buffer[0xFF];
    while (ptr > &buffer[0x80] && ptr[-1] == ' ')
      --ptr;
    *ptr = '\0';

    snprintf(message, sizeof(message), "Found Dreamcast CD: %.128s (%.16s)", (const char*)&buffer[0x80], (const char*)&buffer[0x40]);
    iterator->callbacks.verbose_message(message);
  }

  /* the boot filename is 96 bytes into the meta information (https://mc.pp.se/dc/ip0000.bin.html) */
  /* remove whitespace from bootfile */
  i = 0;
  while (!isspace((unsigned char)buffer[96 + i]) && i < 16)
    ++i;

  /* sometimes boot file isn't present on meta information.
   * nothing can be done, as even the core doesn't run the game in this case. */
  if (i == 0)
  {
    rc_cd_close_track(iterator, track_handle);
    return rc_hash_iterator_error(iterator, "Boot executable not specified on IP.BIN");
  }

  memcpy(exe_file, &buffer[96], i);
  exe_file[i] = '\0';

  sector = rc_cd_find_file_sector(iterator, track_handle, exe_file, &size);
  if (sector == 0)
  {
    rc_cd_close_track(iterator, track_handle);
    return rc_hash_iterator_error(iterator, "Could not locate boot executable");
  }

  if (rc_cd_read_sector(iterator, track_handle, sector, buffer, 1))
  {
    /* the boot executable is in the primary data track */
  }
  else
  {
    rc_cd_close_track(iterator, track_handle);

    /* the boot executable is normally in the last track */
    track_handle = rc_cd_open_track(iterator, RC_HASH_CDTRACK_LAST);
  }

  result = rc_hash_cd_file(&md5, iterator, track_handle, sector, NULL, size, "boot executable");
  rc_cd_close_track(iterator, track_handle);

  rc_hash_finalize(iterator, &md5, hash);
  return result;
}

static int rc_hash_find_playstation_executable(const rc_hash_iterator_t* iterator, void* track_handle,
                                               const char* boot_key, const char* cdrom_prefix,
                                               char exe_name[], uint32_t exe_name_size, uint32_t* exe_size)
{
  uint8_t buffer[2048];
  uint32_t size;
  char* ptr;
  char* start;
  const size_t boot_key_len = strlen(boot_key);
  const size_t cdrom_prefix_len = strlen(cdrom_prefix);
  int sector;

  sector = rc_cd_find_file_sector(iterator, track_handle, "SYSTEM.CNF", NULL);
  if (!sector)
    return 0;

  size = (uint32_t)rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer) - 1);
  buffer[size] = '\0';

  sector = 0;
  for (ptr = (char*)buffer; *ptr; ++ptr)
  {
    if (strncmp(ptr, boot_key, boot_key_len) == 0)
    {
      ptr += boot_key_len;
      while (isspace((unsigned char)*ptr))
        ++ptr;

      if (*ptr == '=')
      {
        ++ptr;
        while (isspace((unsigned char)*ptr))
          ++ptr;

        if (strncmp(ptr, cdrom_prefix, cdrom_prefix_len) == 0)
          ptr += cdrom_prefix_len;
        while (*ptr == '\\')
          ++ptr;

        start = ptr;
        while (!isspace((unsigned char)*ptr) && *ptr != ';')
          ++ptr;

        size = (uint32_t)(ptr - start);
        if (size >= exe_name_size)
          size = exe_name_size - 1;

        memcpy(exe_name, start, size);
        exe_name[size] = '\0';

        rc_hash_iterator_verbose_formatted(iterator, "Looking for boot executable: %s", exe_name);

        sector = rc_cd_find_file_sector(iterator, track_handle, exe_name, exe_size);
        break;
      }
    }

    /* advance to end of line */
    while (*ptr && *ptr != '\n')
      ++ptr;
  }

  return sector;
}

static int rc_hash_psx(char hash[33], const rc_hash_iterator_t* iterator)
{
  uint8_t buffer[32];
  char exe_name[64] = "";
  void* track_handle;
  uint32_t sector;
  uint32_t size;
  int result = 0;
  md5_state_t md5;

  track_handle = rc_cd_open_track(iterator, 1);
  if (!track_handle)
    return rc_hash_iterator_error(iterator, "Could not open track");

  sector = rc_hash_find_playstation_executable(iterator, track_handle, "BOOT", "cdrom:", exe_name, sizeof(exe_name), &size);
  if (!sector)
  {
    sector = rc_cd_find_file_sector(iterator, track_handle, "PSX.EXE", &size);
    if (sector)
      memcpy(exe_name, "PSX.EXE", 8);
  }

  if (!sector)
  {
    rc_hash_iterator_error(iterator, "Could not locate primary executable");
  }
  else if (rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer)) < sizeof(buffer))
  {
    rc_hash_iterator_error(iterator, "Could not read primary executable");
  }
  else
  {
    if (memcmp(buffer, "PS-X EXE", 7) != 0)
    {
      rc_hash_iterator_verbose_formatted(iterator, "%s did not contain PS-X EXE marker", exe_name);
    }
    else
    {
      /* the PS-X EXE header specifies the executable size as a 4-byte value 28 bytes into the header, which doesn't
       * include the header itself. We want to include the header in the hash, so append another 2048 to that value.
       */
      size = (((uint8_t)buffer[31] << 24) | ((uint8_t)buffer[30] << 16) | ((uint8_t)buffer[29] << 8) | (uint8_t)buffer[28]) + 2048;
    }

    /* there's a few games that use a singular engine and only differ via their data files. luckily, they have unique
     * serial numbers, and use the serial number as the boot file in the standard way. include the boot file name in the hash.
     */
    md5_init(&md5);
    md5_append(&md5, (md5_byte_t*)exe_name, (int)strlen(exe_name));

    result = rc_hash_cd_file(&md5, iterator, track_handle, sector, exe_name, size, "primary executable");
    rc_hash_finalize(iterator, &md5, hash);
  }

  rc_cd_close_track(iterator, track_handle);

  return result;
}

static int rc_hash_ps2(char hash[33], const rc_hash_iterator_t* iterator)
{
  uint8_t buffer[4];
  char exe_name[64] = "";
  void* track_handle;
  uint32_t sector;
  uint32_t size;
  int result = 0;
  md5_state_t md5;

  track_handle = rc_cd_open_track(iterator, 1);
  if (!track_handle)
    return rc_hash_iterator_error(iterator, "Could not open track");

  sector = rc_hash_find_playstation_executable(iterator, track_handle, "BOOT2", "cdrom0:", exe_name, sizeof(exe_name), &size);
  if (!sector)
  {
    rc_hash_iterator_error(iterator, "Could not locate primary executable");
  }
  else if (rc_cd_read_sector(iterator, track_handle, sector, buffer, sizeof(buffer)) < sizeof(buffer))
  {
    rc_hash_iterator_error(iterator, "Could not read primary executable");
  }
  else
  {
    if (memcmp(buffer, "\x7f\x45\x4c\x46", 4) != 0)
    {
      rc_hash_iterator_verbose_formatted(iterator, "%s did not contain ELF marker", exe_name);
    }

    /* there's a few games that use a singular engine and only differ via their data files. luckily, they have unique
     * serial numbers, and use the serial number as the boot file in the standard way. include the boot file name in the hash.
     */
    md5_init(&md5);
    md5_append(&md5, (md5_byte_t*)exe_name, (int)strlen(exe_name));

    result = rc_hash_cd_file(&md5, iterator, track_handle, sector, exe_name, size, "primary executable");
    rc_hash_finalize(iterator, &md5, hash);
  }

  rc_cd_close_track(iterator, track_handle);

  return result;
}

static int rc_hash_psp(char hash[33], const rc_hash_iterator_t* iterator)
{
  void* track_handle;
  uint32_t sector;
  uint32_t size;
  md5_state_t md5;

  /* https://www.psdevwiki.com/psp/PBP
   * A PBP file is an archive containing the PARAM.SFO, primary executable, and a bunch of metadata.
   * While we could extract the PARAM.SFO and primary executable to mimic the normal PSP hashing logic,
   * it's easier to just hash the entire file. This also helps alleviate issues where the primary
   * executable is just a game engine and the only differentiating data would be the metadata. */
  if (rc_path_compare_extension(iterator->path, "pbp"))
    return rc_hash_whole_file(hash, iterator);

  track_handle = rc_cd_open_track(iterator, 1);
  if (!track_handle)
    return rc_hash_iterator_error(iterator, "Could not open track");

  /* http://www.romhacking.net/forum/index.php?topic=30899.0
   * PSP_GAME/PARAM.SFO contains key/value pairs identifying the game for the system (i.e. serial number,
   * name, version). PSP_GAME/SYSDIR/EBOOT.BIN is the encrypted primary executable.
   */
  sector = rc_cd_find_file_sector(iterator, track_handle, "PSP_GAME\\PARAM.SFO", &size);
  if (!sector)
  {
    rc_cd_close_track(iterator, track_handle);
    return rc_hash_iterator_error(iterator, "Not a PSP game disc");
  }

  md5_init(&md5);
  if (!rc_hash_cd_file(&md5, iterator, track_handle, sector, NULL, size, "PSP_GAME\\PARAM.SFO"))
  {
    rc_cd_close_track(iterator, track_handle);
    return 0;
  }

  sector = rc_cd_find_file_sector(iterator, track_handle, "PSP_GAME\\SYSDIR\\EBOOT.BIN", &size);
  if (!sector)
  {
    rc_cd_close_track(iterator, track_handle);
    return rc_hash_iterator_error(iterator, "Could not find primary executable");
  }

  if (!rc_hash_cd_file(&md5, iterator, track_handle, sector, NULL, size, "PSP_GAME\\SYSDIR\\EBOOT.BIN"))
  {
    rc_cd_close_track(iterator, track_handle);
    return 0;
  }

  rc_cd_close_track(iterator, track_handle);
  return rc_hash_finalize(iterator, &md5, hash);
}

static int rc_hash_sega_cd(char hash[33], const rc_hash_iterator_t* iterator)
{
  uint8_t buffer[512];
  void* track_handle;

  track_handle = rc_cd_open_track(iterator, 1);
  if (!track_handle)
    return rc_hash_iterator_error(iterator, "Could not open track");

  /* the first 512 bytes of sector 0 are a volume header and ROM header that uniquely identify the game.
   * After that is an arbitrary amount of code that ensures the game is being run in the correct region.
   * Then more arbitrary code follows that actually starts the boot process. Somewhere in there, the
   * primary executable is loaded. In many cases, a single game will have multiple executables, so even
   * if we could determine the primary one, it's just the tip of the iceberg. As such, we've decided that
   * hashing the volume and ROM headers is sufficient for identifying the game, and we'll have to trust
   * that our players aren't modifying anything else on the disc.
   */
  rc_cd_read_sector(iterator, track_handle, 0, buffer, sizeof(buffer));
  rc_cd_close_track(iterator, track_handle);

  if (memcmp(buffer, "SEGADISCSYSTEM  ", 16) != 0 && /* Sega CD */
      memcmp(buffer, "SEGA SEGASATURN ", 16) != 0)   /* Sega Saturn */
  {
    return rc_hash_iterator_error(iterator, "Not a Sega CD");
  }

  return rc_hash_buffer(hash, buffer, sizeof(buffer), iterator);
}

static int rc_hash_scv(char hash[33], const rc_hash_iterator_t* iterator)
{
  /* if the file contains a header, ignore it */
  /* https://gitlab.com/MaaaX-EmuSCV/libretro-emuscv/-/blob/master/readme.txt#L211 */
  if (memcmp(iterator->buffer, "EmuSCV", 6) == 0)
  {
    rc_hash_iterator_verbose(iterator, "Ignoring SCV header");
    return rc_hash_unheadered_iterator_buffer(hash, iterator, 32);
  }

  return rc_hash_iterator_buffer(hash, iterator);
}

static int rc_hash_snes(char hash[33], const rc_hash_iterator_t* iterator)
{
  /* if the file contains a header, ignore it */
  uint32_t calc_size = ((uint32_t)iterator->buffer_size / 0x2000) * 0x2000;
  if (iterator->buffer_size - calc_size == 512)
  {
    rc_hash_iterator_verbose(iterator, "Ignoring SNES header");
    return rc_hash_unheadered_iterator_buffer(hash, iterator, 512);
  }

  return rc_hash_iterator_buffer(hash, iterator);
}

struct rc_buffered_file
{
  const uint8_t* read_ptr;
  const uint8_t* data;
  size_t data_size;
};

static struct rc_buffered_file rc_buffered_file;

static void* rc_file_open_buffered_file(const char* path)
{
  struct rc_buffered_file* handle = (struct rc_buffered_file*)malloc(sizeof(struct rc_buffered_file));
  (void)path;

  if (handle)
    memcpy(handle, &rc_buffered_file, sizeof(rc_buffered_file));

  return handle;
}

static void rc_file_seek_buffered_file(void* file_handle, int64_t offset, int origin)
{
  struct rc_buffered_file* buffered_file = (struct rc_buffered_file*)file_handle;
  switch (origin)
  {
    case SEEK_SET: buffered_file->read_ptr = buffered_file->data + offset; break;
    case SEEK_CUR: buffered_file->read_ptr += offset; break;
    case SEEK_END: buffered_file->read_ptr = buffered_file->data + buffered_file->data_size + offset; break;
  }

  if (buffered_file->read_ptr < buffered_file->data)
    buffered_file->read_ptr = buffered_file->data;
  else if (buffered_file->read_ptr > buffered_file->data + buffered_file->data_size)
    buffered_file->read_ptr = buffered_file->data + buffered_file->data_size;
}

static int64_t rc_file_tell_buffered_file(void* file_handle)
{
  struct rc_buffered_file* buffered_file = (struct rc_buffered_file*)file_handle;
  return (buffered_file->read_ptr - buffered_file->data);
}

static size_t rc_file_read_buffered_file(void* file_handle, void* buffer, size_t requested_bytes)
{
  struct rc_buffered_file* buffered_file = (struct rc_buffered_file*)file_handle;
  const int64_t remaining = buffered_file->data_size - (buffered_file->read_ptr - buffered_file->data);
  if ((int)requested_bytes > remaining)
     requested_bytes = (int)remaining;

  memcpy(buffer, buffered_file->read_ptr, requested_bytes);
  buffered_file->read_ptr += requested_bytes;
  return requested_bytes;
}

static void rc_file_close_buffered_file(void* file_handle)
{
  free(file_handle);
}

static int rc_hash_file_from_buffer(char hash[33], uint32_t console_id, const rc_hash_iterator_t* iterator)
{
  int result;

  rc_hash_iterator_t buffered_file_iterator;
  memset(&buffered_file_iterator, 0, sizeof(buffered_file_iterator));
  memcpy(&buffered_file_iterator.callbacks, &iterator->callbacks, sizeof(iterator->callbacks));

  buffered_file_iterator.callbacks.filereader.open = rc_file_open_buffered_file;
  buffered_file_iterator.callbacks.filereader.close = rc_file_close_buffered_file;
  buffered_file_iterator.callbacks.filereader.read = rc_file_read_buffered_file;
  buffered_file_iterator.callbacks.filereader.seek = rc_file_seek_buffered_file;
  buffered_file_iterator.callbacks.filereader.tell = rc_file_tell_buffered_file;
  buffered_file_iterator.path = "memory stream";

  rc_buffered_file.data = rc_buffered_file.read_ptr = iterator->buffer;
  rc_buffered_file.data_size = iterator->buffer_size;

  result = rc_hash_from_file(hash, console_id, &buffered_file_iterator);

  buffered_file_iterator.path = NULL;
  rc_hash_destroy_iterator(&buffered_file_iterator);
  return result;
}

static int rc_hash_from_buffer(char hash[33], uint32_t console_id, const rc_hash_iterator_t* iterator)
{
  switch (console_id)
  {
    default:
      return rc_hash_iterator_error_formatted(iterator, "Unsupported console for buffer hash: %d", console_id);

    case RC_CONSOLE_AMSTRAD_PC:
    case RC_CONSOLE_APPLE_II:
    case RC_CONSOLE_ARCADIA_2001:
    case RC_CONSOLE_ATARI_2600:
    case RC_CONSOLE_ATARI_JAGUAR:
    case RC_CONSOLE_COLECOVISION:
    case RC_CONSOLE_COMMODORE_64:
    case RC_CONSOLE_ELEKTOR_TV_GAMES_COMPUTER:
    case RC_CONSOLE_FAIRCHILD_CHANNEL_F:
    case RC_CONSOLE_GAMEBOY:
    case RC_CONSOLE_GAMEBOY_ADVANCE:
    case RC_CONSOLE_GAMEBOY_COLOR:
    case RC_CONSOLE_GAME_GEAR:
    case RC_CONSOLE_INTELLIVISION:
    case RC_CONSOLE_INTERTON_VC_4000:
    case RC_CONSOLE_MAGNAVOX_ODYSSEY2:
    case RC_CONSOLE_MASTER_SYSTEM:
    case RC_CONSOLE_MEGA_DRIVE:
    case RC_CONSOLE_MEGADUCK:
    case RC_CONSOLE_MSX:
    case RC_CONSOLE_NEOGEO_POCKET:
    case RC_CONSOLE_ORIC:
    case RC_CONSOLE_PC8800:
    case RC_CONSOLE_POKEMON_MINI:
    case RC_CONSOLE_SEGA_32X:
    case RC_CONSOLE_SG1000:
    case RC_CONSOLE_SUPERVISION:
    case RC_CONSOLE_TI83:
    case RC_CONSOLE_TIC80:
    case RC_CONSOLE_UZEBOX:
    case RC_CONSOLE_VECTREX:
    case RC_CONSOLE_VIRTUAL_BOY:
    case RC_CONSOLE_WASM4:
    case RC_CONSOLE_WONDERSWAN:
    case RC_CONSOLE_ZX_SPECTRUM:
      return rc_hash_iterator_buffer(hash, iterator);

    case RC_CONSOLE_ARDUBOY:
      /* https://en.wikipedia.org/wiki/Intel_HEX */
      return rc_hash_text(hash, iterator);

    case RC_CONSOLE_ATARI_7800:
      return rc_hash_7800(hash, iterator);

    case RC_CONSOLE_ATARI_LYNX:
      return rc_hash_lynx(hash, iterator);

    case RC_CONSOLE_FAMICOM_DISK_SYSTEM:
    case RC_CONSOLE_NINTENDO:
      return rc_hash_nes(hash, iterator);

    case RC_CONSOLE_PC_ENGINE: /* NOTE: does not support PCEngine CD */
      return rc_hash_pce(hash, iterator);

    case RC_CONSOLE_SUPER_CASSETTEVISION:
      return rc_hash_scv(hash, iterator);

    case RC_CONSOLE_SUPER_NINTENDO:
      return rc_hash_snes(hash, iterator);

    case RC_CONSOLE_NINTENDO_64:
    case RC_CONSOLE_NINTENDO_3DS:
    case RC_CONSOLE_NINTENDO_DS:
    case RC_CONSOLE_NINTENDO_DSI:
      return rc_hash_file_from_buffer(hash, console_id, iterator);
  }
}

static int rc_hash_whole_file(char hash[33], const rc_hash_iterator_t* iterator)
{
  md5_state_t md5;
  uint8_t* buffer;
  int64_t size;
  const size_t buffer_size = 65536;
  void* file_handle;
  size_t remaining;
  int result = 0;

  file_handle = rc_file_open(iterator, iterator->path);
  if (!file_handle)
    return rc_hash_iterator_error(iterator, "Could not open file");

  rc_file_seek(iterator, file_handle, 0, SEEK_END);
  size = rc_file_tell(iterator, file_handle);

  if (size > MAX_BUFFER_SIZE) {
    rc_hash_iterator_verbose_formatted(iterator, "Hashing first %u bytes (of %u bytes) of %s", MAX_BUFFER_SIZE, (unsigned)size, rc_path_get_filename(iterator->path));
    remaining = MAX_BUFFER_SIZE;
  }
  else {
    rc_hash_iterator_verbose_formatted(iterator, "Hashing %s (%u bytes)", rc_path_get_filename(iterator->path), (unsigned)size);
    remaining = (size_t)size;
  }

  md5_init(&md5);

  buffer = (uint8_t*)malloc(buffer_size);
  if (buffer)
  {
    rc_file_seek(iterator, file_handle, 0, SEEK_SET);
    while (remaining >= buffer_size)
    {
      rc_file_read(iterator, file_handle, buffer, (int)buffer_size);
      md5_append(&md5, buffer, (int)buffer_size);
      remaining -= buffer_size;
    }

    if (remaining > 0)
    {
      rc_file_read(iterator, file_handle, buffer, (int)remaining);
      md5_append(&md5, buffer, (int)remaining);
    }

    free(buffer);
    result = rc_hash_finalize(iterator, &md5, hash);
  }

  rc_file_close(iterator, file_handle);
  return result;
}

static int rc_hash_buffered_file(char hash[33], uint32_t console_id, const rc_hash_iterator_t* iterator)
{
  uint8_t* buffer;
  int64_t size;
  int result = 0;
  void* file_handle;

  file_handle = rc_file_open(iterator, iterator->path);
  if (!file_handle)
    return rc_hash_iterator_error(iterator, "Could not open file");

  rc_file_seek(iterator, file_handle, 0, SEEK_END);
  size = rc_file_tell(iterator, file_handle);

  if (size > MAX_BUFFER_SIZE) {
    rc_hash_iterator_verbose_formatted(iterator, "Buffering first %u bytes (of %d bytes) of %s", MAX_BUFFER_SIZE, (unsigned)size, rc_path_get_filename(iterator->path));
    size = MAX_BUFFER_SIZE;
  }
  else {
    rc_hash_iterator_verbose_formatted(iterator, "Buffering %s (%d bytes)", rc_path_get_filename(iterator->path), (unsigned)size);
  }

  buffer = (uint8_t*)malloc((size_t)size);
  if (buffer) {
    rc_hash_iterator_t buffer_iterator;
    memset(&buffer_iterator, 0, sizeof(buffer_iterator));
    memcpy(&buffer_iterator.callbacks, &iterator->callbacks, sizeof(iterator->callbacks));
    buffer_iterator.buffer = buffer;
    buffer_iterator.buffer_size = (size_t)size;

    rc_file_seek(iterator, file_handle, 0, SEEK_SET);
    rc_file_read(iterator, file_handle, buffer, (int)size);

    result = rc_hash_from_buffer(hash, console_id, &buffer_iterator);

    free(buffer);
  }

  rc_file_close(iterator, file_handle);
  return result;
}

static int rc_hash_path_is_absolute(const char* path)
{
  if (!path[0])
    return 0;

  /* "/path/to/file" or "\path\to\file" */
  if (path[0] == '/' || path[0] == '\\')
    return 1;

  /* "C:\path\to\file" */
  if (path[1] == ':' && path[2] == '\\')
    return 1;

  /* "scheme:/path/to/file" */
  while (*path)
  {
    if (path[0] == ':' && path[1] == '/')
      return 1;

    ++path;
  }

  return 0;
}

static const char* rc_hash_get_first_item_from_playlist(const rc_hash_iterator_t* iterator) {
  char buffer[1024];
  char* disc_path;
  char* ptr, *start, *next;
  size_t num_read, path_len, file_len;
  void* file_handle;

  file_handle = rc_file_open(iterator, iterator->path);
  if (!file_handle) {
    rc_hash_iterator_error(iterator, "Could not open playlist");
    return NULL;
  }

  num_read = rc_file_read(iterator, file_handle, buffer, sizeof(buffer) - 1);
  buffer[num_read] = '\0';

  rc_file_close(iterator, file_handle);

  ptr = start = buffer;
  do {
    /* ignore empty and commented lines */
    while (*ptr == '#' || *ptr == '\r' || *ptr == '\n') {
      while (*ptr && *ptr != '\n')
        ++ptr;
      if (*ptr)
        ++ptr;
    }

    /* find and extract the current line */
    start = ptr;
    while (*ptr && *ptr != '\n')
      ++ptr;
    next = ptr;

    /* remove trailing whitespace - especially '\r' */
    while (ptr > start && isspace((unsigned char)ptr[-1]))
      --ptr;

    /* if we found a non-empty line, break out of the loop to process it */
    file_len = ptr - start;
    if (file_len)
      break;

    /* did we reach the end of the file? */
    if (!*next)
      return NULL;

    /* if the line only contained whitespace, keep searching */
    ptr = next + 1;
  } while (1);

  rc_hash_iterator_verbose_formatted(iterator, "Extracted %.*s from playlist", (int)file_len, start);

  start[file_len++] = '\0';
  if (rc_hash_path_is_absolute(start))
    path_len = 0;
  else
    path_len = rc_path_get_filename(iterator->path) - iterator->path;

  disc_path = (char*)malloc(path_len + file_len + 1);
  if (!disc_path)
    return NULL;

  if (path_len)
    memcpy(disc_path, iterator->path, path_len);

  memcpy(&disc_path[path_len], start, file_len);
  return disc_path;
}

static int rc_hash_generate_from_playlist(char hash[33], uint32_t console_id, const rc_hash_iterator_t* iterator) {
  rc_hash_iterator_t first_file_iterator;
  const char* disc_path;
  int result;

  rc_hash_iterator_verbose_formatted(iterator, "Processing playlist: %s", rc_path_get_filename(iterator->path));

  disc_path = rc_hash_get_first_item_from_playlist(iterator);
  if (!disc_path)
    return rc_hash_iterator_error(iterator, "Failed to get first item from playlist");

  memset(&first_file_iterator, 0, sizeof(first_file_iterator));
  memcpy(&first_file_iterator.callbacks, &iterator->callbacks, sizeof(iterator->callbacks));
  first_file_iterator.path = disc_path; /* rc_hash_destory_iterator will free */

  result = rc_hash_from_file(hash, console_id, &first_file_iterator);

  rc_hash_destroy_iterator(&first_file_iterator);
  return result;
}

static int rc_hash_from_file(char hash[33], uint32_t console_id, const rc_hash_iterator_t* iterator)
{
  const char* path = iterator->path;

  switch (console_id)
  {
    default:
      if (iterator->callbacks.error_message)
      {
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "Unsupported console for file hash: %d", console_id);
        iterator->callbacks.error_message(buffer);
      }
      return 0;

    case RC_CONSOLE_ARCADIA_2001:
    case RC_CONSOLE_ATARI_2600:
    case RC_CONSOLE_ATARI_JAGUAR:
    case RC_CONSOLE_COLECOVISION:
    case RC_CONSOLE_ELEKTOR_TV_GAMES_COMPUTER:
    case RC_CONSOLE_FAIRCHILD_CHANNEL_F:
    case RC_CONSOLE_GAMEBOY:
    case RC_CONSOLE_GAMEBOY_ADVANCE:
    case RC_CONSOLE_GAMEBOY_COLOR:
    case RC_CONSOLE_GAME_GEAR:
    case RC_CONSOLE_INTELLIVISION:
    case RC_CONSOLE_INTERTON_VC_4000:
    case RC_CONSOLE_MAGNAVOX_ODYSSEY2:
    case RC_CONSOLE_MASTER_SYSTEM:
    case RC_CONSOLE_MEGADUCK:
    case RC_CONSOLE_NEOGEO_POCKET:
    case RC_CONSOLE_ORIC:
    case RC_CONSOLE_POKEMON_MINI:
    case RC_CONSOLE_SEGA_32X:
    case RC_CONSOLE_SG1000:
    case RC_CONSOLE_SUPERVISION:
    case RC_CONSOLE_TI83:
    case RC_CONSOLE_TIC80:
    case RC_CONSOLE_UZEBOX:
    case RC_CONSOLE_VECTREX:
    case RC_CONSOLE_VIRTUAL_BOY:
    case RC_CONSOLE_WASM4:
    case RC_CONSOLE_WONDERSWAN:
    case RC_CONSOLE_ZX_SPECTRUM:
      /* generic whole-file hash - don't buffer */
      return rc_hash_whole_file(hash, iterator);

    case RC_CONSOLE_AMSTRAD_PC:
    case RC_CONSOLE_APPLE_II:
    case RC_CONSOLE_COMMODORE_64:
    case RC_CONSOLE_MEGA_DRIVE:
    case RC_CONSOLE_MSX:
    case RC_CONSOLE_PC8800:
      /* generic whole-file hash with m3u support - don't buffer */
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, iterator);

      return rc_hash_whole_file(hash, iterator);

    case RC_CONSOLE_ARDUBOY:
    case RC_CONSOLE_ATARI_7800:
    case RC_CONSOLE_ATARI_LYNX:
    case RC_CONSOLE_FAMICOM_DISK_SYSTEM:
    case RC_CONSOLE_NINTENDO:
    case RC_CONSOLE_PC_ENGINE:
    case RC_CONSOLE_SUPER_CASSETTEVISION:
    case RC_CONSOLE_SUPER_NINTENDO:
      /* additional logic whole-file hash - buffer then call rc_hash_generate_from_buffer */
      return rc_hash_buffered_file(hash, console_id, iterator);

    case RC_CONSOLE_3DO:
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, iterator);

      return rc_hash_3do(hash, iterator);

    case RC_CONSOLE_ARCADE:
      return rc_hash_arcade(hash, iterator);

    case RC_CONSOLE_ATARI_JAGUAR_CD:
      return rc_hash_jaguar_cd(hash, iterator);

    case RC_CONSOLE_DREAMCAST:
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, iterator);

      return rc_hash_dreamcast(hash, iterator);

    case RC_CONSOLE_GAMECUBE:
      return rc_hash_gamecube(hash, iterator);

    case RC_CONSOLE_MS_DOS:
      return rc_hash_ms_dos(hash, iterator);

    case RC_CONSOLE_NEO_GEO_CD:
      return rc_hash_neogeo_cd(hash, iterator);

    case RC_CONSOLE_NINTENDO_64:
      return rc_hash_n64(hash, iterator);

    case RC_CONSOLE_NINTENDO_3DS:
      return rc_hash_nintendo_3ds(hash, iterator);

    case RC_CONSOLE_NINTENDO_DS:
    case RC_CONSOLE_NINTENDO_DSI:
      return rc_hash_nintendo_ds(hash, iterator);

    case RC_CONSOLE_PC_ENGINE_CD:
      if (rc_path_compare_extension(path, "cue") || rc_path_compare_extension(path, "chd"))
        return rc_hash_pce_cd(hash, iterator);

      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, iterator);

      return rc_hash_buffered_file(hash, console_id, iterator);

    case RC_CONSOLE_PCFX:
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, iterator);

      return rc_hash_pcfx_cd(hash, iterator);

    case RC_CONSOLE_PLAYSTATION:
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, iterator);

      return rc_hash_psx(hash, iterator);

    case RC_CONSOLE_PLAYSTATION_2:
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, iterator);

      return rc_hash_ps2(hash, iterator);

    case RC_CONSOLE_PSP:
      return rc_hash_psp(hash, iterator);

    case RC_CONSOLE_SEGA_CD:
    case RC_CONSOLE_SATURN:
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, iterator);

      return rc_hash_sega_cd(hash, iterator);
  }
}

static void rc_hash_initialize_iterator_from_path(rc_hash_iterator_t* iterator, const char* path);

static void rc_hash_iterator_append_console(struct rc_hash_iterator* iterator, uint8_t console_id) {
  int i = 0;
  while (iterator->consoles[i] != 0) {
    if (iterator->consoles[i] == console_id)
      return;

    ++i;
  }

  iterator->consoles[i] = console_id;
}

static void rc_hash_reset_iterator(rc_hash_iterator_t* iterator) {
  memset(iterator, 0, sizeof(*iterator));

  iterator->callbacks.verbose_message = g_verbose_message_callback;
  iterator->callbacks.error_message = g_error_message_callback;

  if (g_filereader) {
    memcpy(&iterator->callbacks.filereader, g_filereader, sizeof(*g_filereader));
  } else if (!iterator->callbacks.filereader.open) {
    iterator->callbacks.filereader.open = filereader_open;
    iterator->callbacks.filereader.close = filereader_close;
    iterator->callbacks.filereader.seek = filereader_seek;
    iterator->callbacks.filereader.tell = filereader_tell;
    iterator->callbacks.filereader.read = filereader_read;
  }

  if (g_cdreader)
    memcpy(&iterator->callbacks.cdreader, g_cdreader, sizeof(*g_cdreader));
  else
    rc_hash_get_default_cdreader(&iterator->callbacks.cdreader);
}

static void rc_hash_initialize_iterator_single(rc_hash_iterator_t* iterator, const char* path, int data) {
  (void)path;
  iterator->consoles[0] = (uint8_t)data;
}

static void rc_hash_initialize_iterator_single_with_path(rc_hash_iterator_t* iterator, const char* path, int data) {
  iterator->consoles[0] = (uint8_t)data;

  if (!iterator->path)
    iterator->path = strdup(path);
}

static void rc_hash_initialize_iterator_bin(rc_hash_iterator_t* iterator, const char* path, int data) {
  (void)data;

  if (iterator->buffer_size == 0) {
    /* raw bin file may be a CD track. if it's more than 32MB, try a CD hash. */
    const int64_t size = rc_file_size(iterator, path);
    if (size > 32 * 1024 * 1024) {
      iterator->consoles[0] = RC_CONSOLE_3DO; /* 4DO supports directly opening the bin file */
      iterator->consoles[1] = RC_CONSOLE_PLAYSTATION; /* PCSX ReARMed supports directly opening the bin file*/
      iterator->consoles[2] = RC_CONSOLE_PLAYSTATION_2; /* PCSX2 supports directly opening the bin file*/
      iterator->consoles[3] = RC_CONSOLE_SEGA_CD; /* Genesis Plus GX supports directly opening the bin file*/

      /* fallback to megadrive which just does a full hash. */
      iterator->consoles[4] = RC_CONSOLE_MEGA_DRIVE;
      return;
    }
  }

  /* bin is associated with MegaDrive, Sega32X, Atari 2600, Watara Supervision, MegaDuck,
   * Fairchild Channel F, Arcadia 2001, Interton VC 4000, and Super Cassette Vision.
   * Since they all use the same hashing algorithm, only specify one of them */
  iterator->consoles[0] = RC_CONSOLE_MEGA_DRIVE;
}

static void rc_hash_initialize_iterator_chd(rc_hash_iterator_t* iterator, const char* path, int data) {
  (void)data;

  iterator->consoles[0] = RC_CONSOLE_PLAYSTATION;
  iterator->consoles[1] = RC_CONSOLE_PLAYSTATION_2;
  iterator->consoles[2] = RC_CONSOLE_DREAMCAST;
  iterator->consoles[3] = RC_CONSOLE_SEGA_CD; /* ASSERT: handles both Sega CD and Saturn */
  iterator->consoles[4] = RC_CONSOLE_PSP;
  iterator->consoles[5] = RC_CONSOLE_PC_ENGINE_CD;
  iterator->consoles[6] = RC_CONSOLE_3DO;
  iterator->consoles[7] = RC_CONSOLE_NEO_GEO_CD;
  iterator->consoles[8] = RC_CONSOLE_PCFX;

  if (!iterator->path)
    iterator->path = strdup(path);
}

static void rc_hash_initialize_iterator_cue(rc_hash_iterator_t* iterator, const char* path, int data) {
  (void)data;

  iterator->consoles[0] = RC_CONSOLE_PLAYSTATION;
  iterator->consoles[1] = RC_CONSOLE_PLAYSTATION_2;
  iterator->consoles[2] = RC_CONSOLE_DREAMCAST;
  iterator->consoles[3] = RC_CONSOLE_SEGA_CD; /* ASSERT: handles both Sega CD and Saturn */
  iterator->consoles[4] = RC_CONSOLE_PC_ENGINE_CD;
  iterator->consoles[5] = RC_CONSOLE_3DO;
  iterator->consoles[6] = RC_CONSOLE_PCFX;
  iterator->consoles[7] = RC_CONSOLE_NEO_GEO_CD;
  iterator->consoles[8] = RC_CONSOLE_ATARI_JAGUAR_CD;

  if (!iterator->path)
    iterator->path = strdup(path);
}

static void rc_hash_initialize_iterator_d88(rc_hash_iterator_t* iterator, const char* path, int data) {
  (void)path;
  (void)data;

  iterator->consoles[0] = RC_CONSOLE_PC8800;
  iterator->consoles[1] = RC_CONSOLE_SHARPX1;
}

static void rc_hash_initialize_iterator_dsk(rc_hash_iterator_t* iterator, const char* path, int data) {
  size_t size = iterator->buffer_size;
  if (size == 0)
    size = (size_t)rc_file_size(iterator, path);

  (void)data;

  if (size == 512 * 9 * 80) { /* 360KB */
    /* FAT-12 3.5" DD (512 byte sectors, 9 sectors per track, 80 tracks per side */
    /* FAT-12 5.25" DD double-sided (512 byte sectors, 9 sectors per track, 80 tracks per side */
    iterator->consoles[0] = RC_CONSOLE_MSX;
  }
  else if (size == 512 * 9 * 80 * 2) { /* 720KB */
    /* FAT-12 3.5" DD double-sided (512 byte sectors, 9 sectors per track, 80 tracks per side */
    iterator->consoles[0] = RC_CONSOLE_MSX;
  }
  else if (size == 512 * 9 * 40) { /* 180KB */
    /* FAT-12 5.25" DD (512 byte sectors, 9 sectors per track, 40 tracks per side */
    iterator->consoles[0] = RC_CONSOLE_MSX;

    /* AMSDOS 3" - 40 tracks */
    iterator->consoles[1] = RC_CONSOLE_AMSTRAD_PC;
  }
  else if (size == 256 * 16 * 35) { /* 140KB */
    /* Apple II new format - 256 byte sectors, 16 sectors per track, 35 tracks per side */
    iterator->consoles[0] = RC_CONSOLE_APPLE_II;
  }
  else if (size == 256 * 13 * 35) { /* 113.75KB */
    /* Apple II old format - 256 byte sectors, 13 sectors per track, 35 tracks per side */
    iterator->consoles[0] = RC_CONSOLE_APPLE_II;
  }

  /* once a best guess has been identified, make sure the others are added as fallbacks */

  /* check MSX first, as Apple II isn't supported by RetroArch, and RAppleWin won't use the iterator */
  rc_hash_iterator_append_console(iterator, RC_CONSOLE_MSX);
  rc_hash_iterator_append_console(iterator, RC_CONSOLE_AMSTRAD_PC);
  rc_hash_iterator_append_console(iterator, RC_CONSOLE_ZX_SPECTRUM);
  rc_hash_iterator_append_console(iterator, RC_CONSOLE_APPLE_II);
}

static void rc_hash_initialize_iterator_iso(rc_hash_iterator_t* iterator, const char* path, int data) {
  (void)data;

  iterator->consoles[0] = RC_CONSOLE_PLAYSTATION_2;
  iterator->consoles[1] = RC_CONSOLE_PSP;
  iterator->consoles[2] = RC_CONSOLE_3DO;
  iterator->consoles[3] = RC_CONSOLE_SEGA_CD; /* ASSERT: handles both Sega CD and Saturn */
  iterator->consoles[4] = RC_CONSOLE_GAMECUBE;

  if (!iterator->path)
    iterator->path = strdup(path);
}

static void rc_hash_initialize_iterator_m3u(rc_hash_iterator_t* iterator, const char* path, int data) {
  (void)data;

  /* temporarily set the iterator path to the m3u file so we can extract the
   * path of the first disc. rc_hash_get_first_item_from_playlist will return
   * an allocated string or NULL, so rc_hash_destroy_iterator won't get tripped
   * up by the non-allocted value we're about to assign.
   */
  iterator->path = path;
  iterator->path = rc_hash_get_first_item_from_playlist(iterator);
  if (!iterator->path) /* did not find a disc */
    return;

  iterator->buffer = NULL; /* ignore buffer; assume it's the m3u contents */

  rc_hash_initialize_iterator_from_path(iterator, iterator->path);
}

static void rc_hash_initialize_iterator_nib(rc_hash_iterator_t* iterator, const char* path, int data) {
  (void)path;
  (void)data;

  iterator->consoles[0] = RC_CONSOLE_APPLE_II;
  iterator->consoles[1] = RC_CONSOLE_COMMODORE_64;
}

static void rc_hash_initialize_iterator_rom(rc_hash_iterator_t* iterator, const char* path, int data) {
  (void)path;
  (void)data;

  /* rom is associated with MSX, Thomson TO-8, and Fairchild Channel F.
   * Since they all use the same hashing algorithm, only specify one of them */
  iterator->consoles[0] = RC_CONSOLE_MSX;
}

static void rc_hash_initialize_iterator_tap(rc_hash_iterator_t* iterator, const char* path, int data) {
  (void)path;
  (void)data;

  /* also Oric and ZX Spectrum, but all are full file hashes */
  iterator->consoles[0] = RC_CONSOLE_COMMODORE_64;
}

static const rc_hash_iterator_ext_handler_entry_t rc_hash_iterator_ext_handlers[] = {
  { "2d", rc_hash_initialize_iterator_single, RC_CONSOLE_SHARPX1 },
  { "3ds", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_3DS },
  { "3dsx", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_3DS },
  { "7z", rc_hash_initialize_iterator_single_with_path, RC_CONSOLE_ARCADE },
  { "83g", rc_hash_initialize_iterator_single, RC_CONSOLE_TI83 }, /* http://tibasicdev.wikidot.com/file-extensions */
  { "83p", rc_hash_initialize_iterator_single, RC_CONSOLE_TI83 },
  { "a26", rc_hash_initialize_iterator_single, RC_CONSOLE_ATARI_2600 },
  { "a78", rc_hash_initialize_iterator_single, RC_CONSOLE_ATARI_7800 },
  { "app", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_3DS },
  { "axf", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_3DS },
  { "bin", rc_hash_initialize_iterator_bin, 0 },
  { "bs", rc_hash_initialize_iterator_single, RC_CONSOLE_SUPER_NINTENDO },
  { "cart", rc_hash_initialize_iterator_single, RC_CONSOLE_SUPER_CASSETTEVISION },
  { "cas", rc_hash_initialize_iterator_single, RC_CONSOLE_MSX },
  { "cci", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_3DS },
  { "chd", rc_hash_initialize_iterator_chd, 0 },
  { "chf", rc_hash_initialize_iterator_single, RC_CONSOLE_FAIRCHILD_CHANNEL_F },
  { "cia", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_3DS },
  { "col", rc_hash_initialize_iterator_single, RC_CONSOLE_COLECOVISION },
  { "csw", rc_hash_initialize_iterator_single, RC_CONSOLE_ZX_SPECTRUM },
  { "cue", rc_hash_initialize_iterator_cue, 0 },
  { "cxi", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_3DS },
  { "d64", rc_hash_initialize_iterator_single, RC_CONSOLE_COMMODORE_64 },
  { "d88", rc_hash_initialize_iterator_d88, 0 },
  { "dosz", rc_hash_initialize_iterator_single, RC_CONSOLE_MS_DOS },
  { "dsk", rc_hash_initialize_iterator_dsk, 0 },
  { "elf", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_3DS },
  { "fd", rc_hash_initialize_iterator_single, RC_CONSOLE_THOMSONTO8 },
  { "fds", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO },
  { "fig", rc_hash_initialize_iterator_single, RC_CONSOLE_SUPER_NINTENDO },
  { "gb", rc_hash_initialize_iterator_single, RC_CONSOLE_GAMEBOY },
  { "gba", rc_hash_initialize_iterator_single, RC_CONSOLE_GAMEBOY_ADVANCE },
  { "gbc", rc_hash_initialize_iterator_single, RC_CONSOLE_GAMEBOY_COLOR },
  { "gdi", rc_hash_initialize_iterator_single, RC_CONSOLE_DREAMCAST },
  { "gg", rc_hash_initialize_iterator_single, RC_CONSOLE_GAME_GEAR },
  { "hex", rc_hash_initialize_iterator_single, RC_CONSOLE_ARDUBOY },
  { "iso", rc_hash_initialize_iterator_iso, 0 },
  { "jag", rc_hash_initialize_iterator_single, RC_CONSOLE_ATARI_JAGUAR },
  { "k7", rc_hash_initialize_iterator_single, RC_CONSOLE_THOMSONTO8 }, /* tape */
  { "lnx", rc_hash_initialize_iterator_single, RC_CONSOLE_ATARI_LYNX },
  { "m3u", rc_hash_initialize_iterator_m3u, 0 },
  { "m5", rc_hash_initialize_iterator_single, RC_CONSOLE_THOMSONTO8 }, /* cartridge */
  { "m7", rc_hash_initialize_iterator_single, RC_CONSOLE_THOMSONTO8 }, /* cartridge */
  { "md", rc_hash_initialize_iterator_single, RC_CONSOLE_MEGA_DRIVE },
  { "min", rc_hash_initialize_iterator_single, RC_CONSOLE_POKEMON_MINI },
  { "mx1", rc_hash_initialize_iterator_single, RC_CONSOLE_MSX },
  { "mx2", rc_hash_initialize_iterator_single, RC_CONSOLE_MSX },
  { "n64", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_64 },
  { "ndd", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_64 },
  { "nds", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_DS }, /* handles both DS and DSi */
  { "nes", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO },
  { "ngc", rc_hash_initialize_iterator_single, RC_CONSOLE_NEOGEO_POCKET },
  { "nib", rc_hash_initialize_iterator_nib, 0 },
  { "pbp", rc_hash_initialize_iterator_single, RC_CONSOLE_PSP },
  { "pce", rc_hash_initialize_iterator_single, RC_CONSOLE_PC_ENGINE },
  { "pgm", rc_hash_initialize_iterator_single, RC_CONSOLE_ELEKTOR_TV_GAMES_COMPUTER },
  { "pzx", rc_hash_initialize_iterator_single, RC_CONSOLE_ZX_SPECTRUM },
  { "ri", rc_hash_initialize_iterator_single, RC_CONSOLE_MSX },
  { "rom", rc_hash_initialize_iterator_rom, 0 },
  { "sap", rc_hash_initialize_iterator_single, RC_CONSOLE_THOMSONTO8 }, /* disk */
  { "scl", rc_hash_initialize_iterator_single, RC_CONSOLE_ZX_SPECTRUM },
  { "sfc", rc_hash_initialize_iterator_single, RC_CONSOLE_SUPER_NINTENDO },
  { "sg", rc_hash_initialize_iterator_single, RC_CONSOLE_SG1000 },
  { "sgx", rc_hash_initialize_iterator_single, RC_CONSOLE_PC_ENGINE },
  { "smc", rc_hash_initialize_iterator_single, RC_CONSOLE_SUPER_NINTENDO },
  { "sv", rc_hash_initialize_iterator_single, RC_CONSOLE_SUPERVISION },
  { "swc", rc_hash_initialize_iterator_single, RC_CONSOLE_SUPER_NINTENDO },
  { "tap", rc_hash_initialize_iterator_tap, 0 },
  { "tic", rc_hash_initialize_iterator_single, RC_CONSOLE_TIC80 },
  { "trd", rc_hash_initialize_iterator_single, RC_CONSOLE_ZX_SPECTRUM },
  { "tvc", rc_hash_initialize_iterator_single, RC_CONSOLE_ELEKTOR_TV_GAMES_COMPUTER },
  { "tzx", rc_hash_initialize_iterator_single, RC_CONSOLE_ZX_SPECTRUM },
  { "uze", rc_hash_initialize_iterator_single, RC_CONSOLE_UZEBOX },
  { "v64", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_64 },
  { "vb", rc_hash_initialize_iterator_single, RC_CONSOLE_VIRTUAL_BOY },
  { "wasm", rc_hash_initialize_iterator_single, RC_CONSOLE_WASM4 },
  { "woz", rc_hash_initialize_iterator_single, RC_CONSOLE_APPLE_II },
  { "wsc", rc_hash_initialize_iterator_single, RC_CONSOLE_WONDERSWAN },
  { "z64", rc_hash_initialize_iterator_single, RC_CONSOLE_NINTENDO_64 },
  { "zip", rc_hash_initialize_iterator_single_with_path, RC_CONSOLE_ARCADE }
};

const rc_hash_iterator_ext_handler_entry_t* rc_hash_get_iterator_ext_handlers(size_t* num_handlers) {
  *num_handlers = sizeof(rc_hash_iterator_ext_handlers) / sizeof(rc_hash_iterator_ext_handlers[0]);
  return rc_hash_iterator_ext_handlers;
}

static int rc_hash_iterator_find_handler(const void* left, const void* right) {
  const rc_hash_iterator_ext_handler_entry_t* left_handler =
    (const rc_hash_iterator_ext_handler_entry_t*)left;
  const rc_hash_iterator_ext_handler_entry_t* right_handler =
    (const rc_hash_iterator_ext_handler_entry_t*)right;

  return strcmp(left_handler->ext, right_handler->ext);
}

static void rc_hash_initialize_iterator_from_path(rc_hash_iterator_t* iterator, const char* path) {
  size_t num_handlers;
  const rc_hash_iterator_ext_handler_entry_t* handlers = rc_hash_get_iterator_ext_handlers(&num_handlers);
  const rc_hash_iterator_ext_handler_entry_t* handler;
  rc_hash_iterator_ext_handler_entry_t search;
  const char* ext = rc_path_get_extension(path);
  size_t index;

  /* lowercase the extension as we copy it into the search object */
  memset(&search, 0, sizeof(search));
  for (index = 0; index < sizeof(search.ext) - 1; ++index) {
    const int c = (int)ext[index];
    if (!c)
      break;

    search.ext[index] = tolower(c);
  }

  /* find the handler for the extension */
  handler = bsearch(&search, handlers, num_handlers, sizeof(*handler), rc_hash_iterator_find_handler);
  if (handler) {
    handler->handler(iterator, path, handler->data);
  } else {
    /* if we didn't match the extension, default to something that does a whole file hash */
    if (!iterator->consoles[0])
      iterator->consoles[0] = RC_CONSOLE_GAMEBOY;
  }
}

void rc_hash_initialize_iterator(rc_hash_iterator_t* iterator, const char* path, const uint8_t* buffer, size_t buffer_size)
{
  rc_hash_reset_iterator(iterator);
  iterator->buffer = buffer;
  iterator->buffer_size = buffer_size;

  rc_hash_initialize_iterator_from_path(iterator, path);

  if (iterator->callbacks.verbose_message) {
    char message[256];
    int count = 0;
    while (iterator->consoles[count])
      ++count;

    snprintf(message, sizeof(message), "Found %d potential consoles for %s file extension", count, rc_path_get_extension(path));
    iterator->callbacks.verbose_message(message);
  }

  if (!iterator->buffer && !iterator->path)
    iterator->path = strdup(path);
}

void rc_hash_destroy_iterator(rc_hash_iterator_t* iterator) {
  if (iterator->path) {
    free((void*)iterator->path);
    iterator->path = NULL;
  }
}

int rc_hash_iterate(char hash[33], rc_hash_iterator_t* iterator) {
  int next_console;
  int result = 0;

  do {
    next_console = iterator->consoles[iterator->index];
    if (next_console == 0) {
      hash[0] = '\0';
      break;
    }

    ++iterator->index;

    rc_hash_iterator_verbose_formatted(iterator, "Trying console %d", next_console);

    result = rc_hash_generate(hash, next_console, iterator);
  } while (!result);

  return result;
}

int rc_hash_generate(char hash[33], uint32_t console_id, const rc_hash_iterator_t* iterator) {
  if (iterator->buffer)
    return rc_hash_from_buffer(hash, console_id, iterator);

  return rc_hash_generate_from_file(hash, console_id, iterator->path);
}

int rc_hash_generate_from_buffer(char hash[33], uint32_t console_id, const uint8_t* buffer, size_t buffer_size) {
  rc_hash_iterator_t iterator;
  int result;

  rc_hash_reset_iterator(&iterator);
  iterator.buffer = buffer;
  iterator.buffer_size = buffer_size;

  result = rc_hash_from_buffer(hash, console_id, &iterator);

  rc_hash_destroy_iterator(&iterator);

  return result;
}

int rc_hash_generate_from_file(char hash[33], uint32_t console_id, const char* path){
  rc_hash_iterator_t iterator;
  int result;

  rc_hash_reset_iterator(&iterator);
  iterator.path = path;

  result = rc_hash_from_file(hash, console_id, &iterator);

  iterator.path = NULL; /* prevent free. we didn't strdup */

  rc_hash_destroy_iterator(&iterator);

  return result;
}
