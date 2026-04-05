#include "rc_hash_internal.h"

#include "../rc_compat.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

static int cdreader_get_sector(uint8_t header[16])
{
  int minutes = (header[12] >> 4) * 10 + (header[12] & 0x0F);
  int seconds = (header[13] >> 4) * 10 + (header[13] & 0x0F);
  int frames = (header[14] >> 4) * 10 + (header[14] & 0x0F);

  /* convert the MSF value to a sector index, and subtract 150 (2 seconds) per:
   *   For data and mixed mode media (those conforming to ISO/IEC 10149), logical block address
   *   zero shall be assigned to the block at MSF address 00/02/00 */
  return ((minutes * 60) + seconds) * 75 + frames - 150;
}

static void cdreader_determine_sector_size(rc_hash_cdrom_track_t* cdrom)
{
  /* Attempt to determine the sector and header sizes. The CUE file may be lying.
   * Look for the sync pattern using each of the supported sector sizes.
   * Then check for the presence of "CD001", which is gauranteed to be in either the
   * boot record or primary volume descriptor, one of which is always at sector 16.
   */
  const uint8_t sync_pattern[] = {
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00
  };

  uint8_t header[32];
  const int64_t toc_sector = 16 + cdrom->track_pregap_sectors;

  cdrom->sector_size = 0;
  cdrom->sector_header_size = 0;
  cdrom->raw_data_size = 2048;

  cdrom->file_reader->seek(cdrom->file_handle, toc_sector * 2352 + cdrom->file_track_offset, SEEK_SET);
  if (cdrom->file_reader->read(cdrom->file_handle, header, sizeof(header)) < sizeof(header))
    return;

  if (memcmp(header, sync_pattern, 12) == 0)
  {
    cdrom->sector_size = 2352;

    if (memcmp(&header[25], "CD001", 5) == 0)
      cdrom->sector_header_size = 24;
    else
      cdrom->sector_header_size = 16;

    cdrom->track_first_sector = cdreader_get_sector(header) - (int)toc_sector;
  }
  else
  {
    cdrom->file_reader->seek(cdrom->file_handle, toc_sector * 2336 + cdrom->file_track_offset, SEEK_SET);
    cdrom->file_reader->read(cdrom->file_handle, header, sizeof(header));

    if (memcmp(header, sync_pattern, 12) == 0)
    {
      cdrom->sector_size = 2336;

      if (memcmp(&header[25], "CD001", 5) == 0)
        cdrom->sector_header_size = 24;
      else
        cdrom->sector_header_size = 16;

      cdrom->track_first_sector = cdreader_get_sector(header) - (int)toc_sector;
    }
    else
    {
      cdrom->file_reader->seek(cdrom->file_handle, toc_sector * 2048 + cdrom->file_track_offset, SEEK_SET);
      cdrom->file_reader->read(cdrom->file_handle, header, sizeof(header));

      if (memcmp(&header[1], "CD001", 5) == 0)
      {
        cdrom->sector_size = 2048;
        cdrom->sector_header_size = 0;
      }
    }
  }
}

static void* cdreader_open_bin_track(const char* path, uint32_t track, const rc_hash_iterator_t* iterator)
{
  void* file_handle;
  rc_hash_cdrom_track_t* cdrom;

  if (track > 1) {
    rc_hash_iterator_verbose(iterator, "Cannot locate secondary tracks without a cue sheet");
    return NULL;
  }

  file_handle = iterator->callbacks.filereader.open(path);
  if (!file_handle)
    return NULL;

  cdrom = (rc_hash_cdrom_track_t*)calloc(1, sizeof(*cdrom));
  if (!cdrom)
    return NULL;
  cdrom->file_reader = &iterator->callbacks.filereader;
  cdrom->file_handle = file_handle;
#ifndef NDEBUG
  cdrom->track_id = track;
#endif

  cdreader_determine_sector_size(cdrom);

  if (cdrom->sector_size == 0)
  {
    int64_t size;

    iterator->callbacks.filereader.seek(file_handle, 0, SEEK_END);
    size = iterator->callbacks.filereader.tell(file_handle);

    if ((size % 2352) == 0)
    {
      /* raw tracks use all 2352 bytes and have a 24 byte header */
      cdrom->sector_size = 2352;
      cdrom->sector_header_size = 24;
    }
    else if ((size % 2048) == 0)
    {
      /* cooked tracks eliminate all header/footer data */
      cdrom->sector_size = 2048;
      cdrom->sector_header_size = 0;
    }
    else if ((size % 2336) == 0)
    {
      /* MODE 2 format without 16-byte sync data */
      cdrom->sector_size = 2336;
      cdrom->sector_header_size = 8;
    }
    else
    {
      if (iterator->callbacks.filereader.close)
        iterator->callbacks.filereader.close(file_handle);
      free(cdrom);

      rc_hash_iterator_verbose(iterator, "Could not determine sector size");

      return NULL;
    }
  }

  return cdrom;
}

static int cdreader_open_bin(rc_hash_cdrom_track_t* cdrom, const char* path, const char* mode)
{
  cdrom->file_handle = cdrom->file_reader->open(path);
  if (!cdrom->file_handle)
    return 0;

  /* determine sector size */
  cdreader_determine_sector_size(cdrom);

  /* could not determine, which means we'll probably have more issues later
   * but use the CUE provided information anyway
   */
  if (cdrom->sector_size == 0)
  {
    /* All of these modes have 2048 byte payloads. In MODE1/2352 and MODE2/2352
     * modes, the mode can actually be specified per sector to change the payload
     * size, but that reduces the ability to recover from errors when the disc
     * is damaged, so it's seldomly used, and when it is, it's mostly for audio
     * or video data where a blip or two probably won't be noticed by the user.
     * So, while we techincally support all of the following modes, we only do
     * so with 2048 byte payloads.
     * http://totalsonicmastering.com/cuesheetsyntax.htm
     * MODE1/2048 ? CDROM Mode1 Data (cooked) [no header, no footer]
     * MODE1/2352 ? CDROM Mode1 Data (raw)    [16 byte header, 288 byte footer]
     * MODE2/2336 ? CDROM-XA Mode2 Data       [8 byte header, 280 byte footer]
     * MODE2/2352 ? CDROM-XA Mode2 Data       [24 byte header, 280 byte footer]
     */
    if (memcmp(mode, "MODE2/2352", 10) == 0)
    {
      cdrom->sector_size = 2352;
      cdrom->sector_header_size = 24;
    }
    else if (memcmp(mode, "MODE1/2048", 10) == 0)
    {
      cdrom->sector_size = 2048;
      cdrom->sector_header_size = 0;
    }
    else if (memcmp(mode, "MODE2/2336", 10) == 0)
    {
      cdrom->sector_size = 2336;
      cdrom->sector_header_size = 8;
    }
    else if (memcmp(mode, "MODE1/2352", 10) == 0)
    {
      cdrom->sector_size = 2352;
      cdrom->sector_header_size = 16;
    }
    else if (memcmp(mode, "AUDIO", 5) == 0)
    {
      cdrom->sector_size = 2352;
      cdrom->sector_header_size = 0;
      cdrom->raw_data_size = 2352; /* no header or footer data on audio tracks */
    }
  }

  return (cdrom->sector_size != 0);
}

static char* cdreader_get_bin_path(const char* cue_path, const char* bin_name, const rc_hash_iterator_t* iterator)
{
  const char* filename = rc_path_get_filename(cue_path);
  const size_t bin_name_len = strlen(bin_name);
  const size_t cue_path_len = filename - cue_path;
  const size_t needed = cue_path_len + bin_name_len + 1;

  char* bin_filename = (char*)malloc(needed);
  if (!bin_filename)
  {
    rc_hash_iterator_error_formatted(iterator, "Failed to allocate %u bytes", (unsigned)needed);
  }
  else
  {
    memcpy(bin_filename, cue_path, cue_path_len);
    memcpy(bin_filename + cue_path_len, bin_name, bin_name_len + 1);
  }

  return bin_filename;
}

static int64_t cdreader_get_bin_size(const char* cue_path, const char* bin_name, const rc_hash_iterator_t* iterator)
{
  int64_t size = 0;
  char* bin_filename = cdreader_get_bin_path(cue_path, bin_name, iterator);
  if (bin_filename)
  {
    void* handle = iterator->callbacks.filereader.open(bin_filename);
    if (handle)
    {
      iterator->callbacks.filereader.seek(handle, 0, SEEK_END);
      size = iterator->callbacks.filereader.tell(handle);

      if (iterator->callbacks.filereader.close)
        iterator->callbacks.filereader.close(handle);
    }

    free(bin_filename);
  }

  return size;
}

static void* cdreader_open_cue_track(const char* path, uint32_t track, const rc_hash_iterator_t* iterator)
{
  void* cue_handle;
  int64_t cue_offset = 0;
  char buffer[1024];
  char* bin_filename = NULL;
  char *ptr, *ptr2, *end;
  int done = 0;
  int session = 1;
  size_t num_read = 0;
  rc_hash_cdrom_track_t* cdrom = NULL;

  struct track_t
  {
    uint32_t id;
    int sector_size;
    int sector_count;
    int first_sector;
    int pregap_sectors;
    int is_data;
    int file_track_offset;
    int file_first_sector;
    char mode[16];
    char filename[256];
  } current_track, previous_track, largest_track;

  cue_handle = iterator->callbacks.filereader.open(path);
  if (!cue_handle)
    return NULL;

  memset(&current_track, 0, sizeof(current_track));
  memset(&previous_track, 0, sizeof(previous_track));
  memset(&largest_track, 0, sizeof(largest_track));

  do
  {
    num_read = iterator->callbacks.filereader.read(cue_handle, buffer, sizeof(buffer) - 1);
    if (num_read == 0)
      break;

    buffer[num_read] = 0;
    if (num_read == sizeof(buffer) - 1)
      end = buffer + sizeof(buffer) * 3 / 4;
    else
      end = buffer + num_read;

    for (ptr = buffer; ptr < end; ++ptr)
    {
      while (*ptr == ' ')
        ++ptr;

      if (strncasecmp(ptr, "INDEX ", 6) == 0)
      {
        int m = 0, s = 0, f = 0;
        int index;
        int sector_offset;

        ptr += 6;
        index = atoi(ptr);

        while (*ptr != ' ' && *ptr != '\n')
          ++ptr;
        while (*ptr == ' ')
          ++ptr;

        /* convert mm:ss:ff to sector count */
        sscanf_s(ptr, "%d:%d:%d", &m, &s, &f);
        sector_offset = ((m * 60) + s) * 75 + f;

        if (current_track.first_sector == -1)
        {
          current_track.first_sector = sector_offset;
          if (strcmp(current_track.filename, previous_track.filename) == 0)
          {
            previous_track.sector_count = current_track.first_sector - previous_track.first_sector;
            current_track.file_track_offset += previous_track.sector_count * previous_track.sector_size;
          }

          /* if looking for the largest data track, determine previous track size */
          if (track == RC_HASH_CDTRACK_LARGEST && previous_track.sector_count > largest_track.sector_count &&
              previous_track.is_data)
          {
            memcpy(&largest_track, &previous_track, sizeof(largest_track));
          }
        }

        if (index == 1)
        {
          current_track.pregap_sectors = (sector_offset - current_track.first_sector);

          {
            char* scan = current_track.mode;
            while (*scan && !isspace((unsigned char)*scan))
              ++scan;
            *scan = '\0';

            /* it's undesirable to truncate offset to 32-bits, but %lld isn't defined in c89. */
            rc_hash_iterator_verbose_formatted(iterator, "Found %s track %d (first sector %d, sector size %d, %d pregap sectors)",
                     current_track.mode, current_track.id, current_track.first_sector, current_track.sector_size, current_track.pregap_sectors);
          }

          if (current_track.id == track)
          {
            done = 1;
            break;
          }

          if (track == RC_HASH_CDTRACK_FIRST_DATA && current_track.is_data)
          {
            track = current_track.id;
            done = 1;
            break;
          }

          if (track == RC_HASH_CDTRACK_FIRST_OF_SECOND_SESSION && session == 2)
          {
            track = current_track.id;
            done = 1;
            break;
          }
        }
      }
      else if (strncasecmp(ptr, "TRACK ", 6) == 0)
      {
        if (current_track.sector_size)
          memcpy(&previous_track, &current_track, sizeof(current_track));

        ptr += 6;
        current_track.id = atoi(ptr);

        current_track.pregap_sectors = -1;
        current_track.first_sector = -1;

        while (*ptr != ' ')
          ++ptr;
        while (*ptr == ' ')
          ++ptr;
        memcpy(current_track.mode, ptr, sizeof(current_track.mode));
        current_track.is_data = (memcmp(current_track.mode, "MODE", 4) == 0);

        if (current_track.is_data)
        {
          current_track.sector_size = atoi(ptr + 6);
        }
        else
        {
          /* assume AUDIO */
          current_track.sector_size = 2352;
        }
      }
      else if (strncasecmp(ptr, "FILE ", 5) == 0)
      {
        if (current_track.sector_size)
        {
          memcpy(&previous_track, &current_track, sizeof(previous_track));

          if (previous_track.sector_count == 0)
          {
            const int64_t bin_size = cdreader_get_bin_size(path, previous_track.filename, iterator);
            const uint32_t file_sector_count = (uint32_t)bin_size / previous_track.sector_size;
            previous_track.sector_count = file_sector_count - previous_track.first_sector;
          }

          /* if looking for the largest data track, check to see if this one is larger */
          if (track == RC_HASH_CDTRACK_LARGEST && previous_track.is_data &&
              previous_track.sector_count > largest_track.sector_count)
          {
            memcpy(&largest_track, &previous_track, sizeof(largest_track));
          }
        }

        memset(&current_track, 0, sizeof(current_track));

        current_track.file_first_sector = previous_track.file_first_sector + 
            previous_track.first_sector + previous_track.sector_count;

        ptr += 5;
        ptr2 = ptr;
        if (*ptr == '"')
        {
          ++ptr;
          do
          {
            ++ptr2;
          } while (*ptr2 && *ptr2 != '\n' && *ptr2 != '"');
        }
        else
        {
          do
          {
            ++ptr2;
          } while (*ptr2 && *ptr2 != '\n' && *ptr2 != ' ');
        }

        if (ptr2 - ptr < (int)sizeof(current_track.filename))
          memcpy(current_track.filename, ptr, ptr2 - ptr);
      }
      else if (strncasecmp(ptr, "REM ", 4) == 0)
      {
        ptr += 4;
        while (*ptr == ' ')
          ++ptr;

        if (strncasecmp(ptr, "SESSION ", 8) == 0)
        {
          ptr += 8;
          while (*ptr == ' ')
            ++ptr;
          session = atoi(ptr);
        }
      }

      while (*ptr && *ptr != '\n')
        ++ptr;
    }

    if (done)
      break;

    cue_offset += (ptr - buffer);
    iterator->callbacks.filereader.seek(cue_handle, cue_offset, SEEK_SET);

  } while (1);

  if (iterator->callbacks.filereader.close)
    iterator->callbacks.filereader.close(cue_handle);

  if (track == RC_HASH_CDTRACK_LARGEST)
  {
    if (current_track.sector_size && current_track.is_data)
    {
      const int64_t bin_size = cdreader_get_bin_size(path, current_track.filename, iterator);
      const uint32_t file_sector_count = (uint32_t)bin_size / current_track.sector_size;
      current_track.sector_count = file_sector_count - current_track.first_sector;

      if (largest_track.sector_count > current_track.sector_count)
        memcpy(&current_track, &largest_track, sizeof(current_track));
    }
    else
    {
      memcpy(&current_track, &largest_track, sizeof(current_track));
    }

    track = current_track.id;
  }
  else if (track == RC_HASH_CDTRACK_LAST && !done)
  {
    track = current_track.id;
  }

  if (current_track.id == track)
  {
    cdrom = (rc_hash_cdrom_track_t*)calloc(1, sizeof(*cdrom));
    if (!cdrom)
    {
      rc_hash_iterator_error_formatted(iterator, "Failed to allocate %u bytes", (unsigned)sizeof(*cdrom));
      return NULL;
    }

    cdrom->file_reader = &iterator->callbacks.filereader;
    cdrom->file_track_offset = current_track.file_track_offset;
    cdrom->track_pregap_sectors = current_track.pregap_sectors;
    cdrom->track_first_sector = current_track.file_first_sector + current_track.first_sector;
#ifndef NDEBUG
    cdrom->track_id = current_track.id;
#endif

    /* verify existance of bin file */
    bin_filename = cdreader_get_bin_path(path, current_track.filename, iterator);
    if (bin_filename)
    {
      if (cdreader_open_bin(cdrom, bin_filename, current_track.mode))
      {
        if (cdrom->track_pregap_sectors)
          rc_hash_iterator_verbose_formatted(iterator, "Opened track %d (sector size %d, %d pregap sectors)",
                    track, cdrom->sector_size, cdrom->track_pregap_sectors);
        else
          rc_hash_iterator_verbose_formatted(iterator, "Opened track %d (sector size %d)", track, cdrom->sector_size);
      }
      else
      {
        if (cdrom->file_handle)
        {
          cdrom->file_reader->close(cdrom->file_handle);
          rc_hash_iterator_error_formatted(iterator, "Could not determine sector size for %s track", current_track.mode);
        }
        else
        {
          rc_hash_iterator_error_formatted(iterator, "Could not open %s", bin_filename);
        }

        free(cdrom);
        cdrom = NULL;
      }

      free(bin_filename);
    }
  }

  return cdrom;
}

static void* cdreader_open_gdi_track(const char* path, uint32_t track, const rc_hash_iterator_t* iterator)
{
  void* file_handle;
  char buffer[1024];
  char mode[16] = "MODE1/";
  char sector_size[16];
  char file[256];
  int64_t track_size;
  int track_type;
  char* bin_path = NULL;
  uint32_t current_track = 0;
  char* ptr, *ptr2, *end;
  int lba = 0;

  uint32_t largest_track = 0;
  int64_t largest_track_size = 0;
  char largest_track_file[256];
  char largest_track_sector_size[16];
  int largest_track_lba = 0;

  int found = 0;
  size_t num_read = 0;
  int64_t file_offset = 0;
  rc_hash_cdrom_track_t* cdrom = NULL;

  file_handle = iterator->callbacks.filereader.open(path);
  if (!file_handle)
    return NULL;

  file[0] = '\0';
  do
  {
    num_read = iterator->callbacks.filereader.read(file_handle, buffer, sizeof(buffer) - 1);
    if (num_read == 0)
      break;

    buffer[num_read] = 0;
    if (num_read == sizeof(buffer) - 1)
      end = buffer + sizeof(buffer) * 3 / 4;
    else
      end = buffer + num_read;

    ptr = buffer;

    /* the first line contains the number of tracks, so we can get the last track index from it */
    if (track == RC_HASH_CDTRACK_LAST)
      track = atoi(ptr);

    /* first line contains the number of tracks and will be skipped */
    while (ptr < end)
    {
      /* skip until next newline */
      while (*ptr != '\n' && ptr < end)
        ++ptr;

      /* skip newlines */
      while ((*ptr == '\n' || *ptr == '\r') && ptr < end)
        ++ptr;

      /* line format: [trackid] [lba] [type] [sectorsize] [file] [?] */
      while (isspace((unsigned char)*ptr))
        ++ptr;

      current_track = (uint32_t)atoi(ptr);
      if (track && current_track != track && track != RC_HASH_CDTRACK_FIRST_DATA)
        continue;

      while (isdigit((unsigned char)*ptr))
        ++ptr;
      ++ptr;

      while (isspace((unsigned char)*ptr))
        ++ptr;

      lba = atoi(ptr);
      while (isdigit((unsigned char)*ptr))
        ++ptr;
      ++ptr;

      while (isspace((unsigned char)*ptr))
        ++ptr;

      track_type = atoi(ptr);
      while (isdigit((unsigned char)*ptr))
        ++ptr;
      ++ptr;

      while (isspace((unsigned char)*ptr))
        ++ptr;

      ptr2 = sector_size;
      while (isdigit((unsigned char)*ptr))
        *ptr2++ = *ptr++;
      *ptr2 = '\0';
      ++ptr;

      while (isspace((unsigned char)*ptr))
        ++ptr;

      ptr2 = file;
      if (*ptr == '\"')
      {
        ++ptr;
        while (*ptr != '\"')
          *ptr2++ = *ptr++;
        ++ptr;
      }
      else
      {
        while (*ptr != ' ')
          *ptr2++ = *ptr++;
      }
      *ptr2 = '\0';

      if (track == current_track)
      {
        found = 1;
        break;
      }
      else if (track == RC_HASH_CDTRACK_FIRST_DATA && track_type == 4)
      {
        track = current_track;
        found = 1;
        break;
      }
      else if (track == RC_HASH_CDTRACK_LARGEST && track_type == 4)
      {
        track_size = cdreader_get_bin_size(path, file, iterator);
        if (track_size > largest_track_size)
        {
          largest_track_size = track_size;
          largest_track = current_track;
          largest_track_lba = lba;
          strcpy_s(largest_track_file, sizeof(largest_track_file), file);
          strcpy_s(largest_track_sector_size, sizeof(largest_track_sector_size), sector_size);
        }
      }
    }

    if (found)
      break;

    file_offset += (ptr - buffer);
    iterator->callbacks.filereader.seek(file_handle, file_offset, SEEK_SET);

  } while (1);

  if (iterator->callbacks.filereader.close)
    iterator->callbacks.filereader.close(file_handle);

  cdrom = (rc_hash_cdrom_track_t*)calloc(1, sizeof(*cdrom));
  if (!cdrom)
  {
    rc_hash_iterator_error_formatted(iterator, "Failed to allocate %u bytes", (unsigned)sizeof(*cdrom));
    return NULL;
  }

  cdrom->file_reader = &iterator->callbacks.filereader;

  /* if we were tracking the largest track, make it the current track.
   * otherwise, current_track will be the requested track, or last track. */
  if (largest_track != 0 && largest_track != current_track)
  {
    current_track = largest_track;
    strcpy_s(file, sizeof(file), largest_track_file);
    strcpy_s(sector_size, sizeof(sector_size), largest_track_sector_size);
    lba = largest_track_lba;
  }

  /* open the bin file for the track - construct mode parameter from sector_size */
  ptr = &mode[6];
  ptr2 = sector_size;
  while (*ptr2 && *ptr2 != '\"')
    *ptr++ = *ptr2++;
  *ptr = '\0';

  bin_path = cdreader_get_bin_path(path, file, iterator);
  if (cdreader_open_bin(cdrom, bin_path, mode))
  {
    cdrom->track_pregap_sectors = 0;
    cdrom->track_first_sector = lba;
#ifndef NDEBUG
    cdrom->track_id = current_track;
#endif

    rc_hash_iterator_verbose_formatted(iterator, "Opened track %d (sector size %d)", current_track, cdrom->sector_size);
  }
  else
  {
    rc_hash_iterator_error_formatted(iterator, "Could not open %s", bin_path);

    free(cdrom);
    cdrom = NULL;
  }

  free(bin_path);

  return cdrom;
}

#ifdef HAVE_CHD
#include "libchdr/chd.h"
#include "libchdr/cdrom.h"
typedef struct rc_chd_reader_t {
  chd_file* chd;
  uint32_t hunkbytes;
  uint8_t* hunk_buffer;
  uint32_t buffered_hunk;
  int64_t current_pos;
  int64_t total_bytes;
} rc_chd_reader_t;
#endif /* HAVE_CHD */

#ifdef HAVE_CHD
static void chd_filereader_seek(void* h, int64_t o, int origin) {
  rc_chd_reader_t* r = (rc_chd_reader_t*)h;
  if (origin == SEEK_SET) r->current_pos = o;
  else if (origin == SEEK_CUR) r->current_pos += o;
  else r->current_pos = r->total_bytes + o;
  if (r->current_pos < 0) r->current_pos = 0;
  if (r->current_pos > r->total_bytes) r->current_pos = r->total_bytes;
}
static int64_t chd_filereader_tell(void* h) { return ((rc_chd_reader_t*)h)->current_pos; }
static size_t chd_filereader_read(void* h, void* buf, size_t n) {
  rc_chd_reader_t* r = (rc_chd_reader_t*)h;
  uint8_t* out = (uint8_t*)buf;
  size_t total = 0;
  while (n > 0 && r->current_pos < r->total_bytes) {
    uint32_t hi = (uint32_t)(r->current_pos / r->hunkbytes);
    uint32_t off = (uint32_t)(r->current_pos % r->hunkbytes);
    size_t avail = (size_t)(r->hunkbytes - off);
    size_t copy = avail < n ? avail : n;
    if (r->current_pos + (int64_t)copy > r->total_bytes)
      copy = (size_t)(r->total_bytes - r->current_pos);
    if (!copy) break;
    if (hi != r->buffered_hunk) {
      if (chd_read(r->chd, hi, r->hunk_buffer) != CHDERR_NONE) break;
      r->buffered_hunk = hi;
    }
    memcpy(out, r->hunk_buffer + off, copy);
    out += copy; r->current_pos += copy; total += copy; n -= copy;
  }
  return total;
}
static void chd_filereader_close(void* h) {
  rc_chd_reader_t* r = (rc_chd_reader_t*)h;
  if (r) { if (r->hunk_buffer) free(r->hunk_buffer); if (r->chd) chd_close(r->chd); free(r); }
}
static const rc_hash_filereader_t s_chd_filereader = {
  NULL, chd_filereader_seek, chd_filereader_tell, chd_filereader_read, chd_filereader_close
};
#endif /* HAVE_CHD */

#ifdef HAVE_CHD
static void cdreader_get_chd_sector_info(const char* t, int* ss, int* hs, int* rds) {
  if (strncmp(t,"AUDIO",5)==0) { *ss=CD_FRAME_SIZE; *hs=0; *rds=CD_MAX_SECTOR_DATA; }
  else if (strncmp(t,"MODE2",5)==0) { *ss=CD_FRAME_SIZE; *hs=24; *rds=2048; }
  else { *ss=CD_FRAME_SIZE; *hs=16; *rds=2048; }
}
static rc_chd_reader_t* cdreader_alloc_chd_reader(chd_file* chd) {
  const chd_header* h = chd_get_header(chd);
  rc_chd_reader_t* r = (rc_chd_reader_t*)calloc(1, sizeof(*r));
  if (!r) { chd_close(chd); return NULL; }
  r->chd = chd; r->hunkbytes = h->hunkbytes;
  r->total_bytes = (int64_t)h->logicalbytes;
  r->hunk_buffer = (uint8_t*)malloc(h->hunkbytes);
  r->buffered_hunk = (uint32_t)-1;
  if (!r->hunk_buffer) { chd_filereader_close(r); return NULL; }
  return r;
}
static void* cdreader_open_chd_track(const char* path, uint32_t track, const rc_hash_iterator_t* iterator) {
  chd_file* chd = NULL;
  const chd_header* header;
  rc_chd_reader_t* rdr;
  rc_hash_cdrom_track_t* cdrom;
  chd_error err;
  char meta[256]; uint32_t mlen;
  char ttype[16], tsub[16], pgtype[16], pgsub[16];
  uint32_t tnum, frames, pregap, postgap;
  uint32_t cum, idx, rb, rp, lf; int rs, rh, rrd, found;
  int ss, hs, rds;
  err = chd_open(path, CHD_OPEN_READ, NULL, &chd);
  if (err != CHDERR_NONE) { rc_hash_iterator_error_formatted(iterator,"Could not open CHD: %s",chd_error_string(err)); return NULL; }
  header = chd_get_header(chd);
  rdr = cdreader_alloc_chd_reader(chd);
  if (!rdr) return NULL;
  cdrom = (rc_hash_cdrom_track_t*)calloc(1, sizeof(*cdrom));
  if (!cdrom) { chd_filereader_close(rdr); return NULL; }
  cdrom->file_reader = &s_chd_filereader;
  cdrom->file_handle = rdr;
  cdrom->raw_data_size = 2048;
  if (track == 0) track = RC_HASH_CDTRACK_LARGEST;
  found=0; cum=0; rb=0; rp=0; rs=0; rh=0; rrd=2048; lf=0;
  for (idx=0; ; ++idx) {
    err = chd_get_metadata(chd,CDROM_TRACK_METADATA2_TAG,idx,meta,sizeof(meta),&mlen,NULL,NULL);
    if (err==CHDERR_NONE) sscanf(meta,CDROM_TRACK_METADATA2_FORMAT,&tnum,ttype,tsub,&frames,&pregap,pgtype,pgsub,&postgap);
    else { err=chd_get_metadata(chd,CDROM_TRACK_METADATA_TAG,idx,meta,sizeof(meta),&mlen,NULL,NULL);
      if (err==CHDERR_NONE) { sscanf(meta,CDROM_TRACK_METADATA_FORMAT,&tnum,ttype,tsub,&frames); pregap=0; postgap=0; }
      else break; }
    cdreader_get_chd_sector_info(ttype,&ss,&hs,&rds);
    if (track==tnum) { rb=cum; rp=pregap; rs=ss; rh=hs; rrd=rds; found=1; break; }
    if (track==RC_HASH_CDTRACK_FIRST_DATA && strncmp(ttype,"MODE",4)==0) { rb=cum; rp=pregap; rs=ss; rh=hs; rrd=rds; found=1; break; }
    if (track==RC_HASH_CDTRACK_LARGEST && strncmp(ttype,"MODE",4)==0) { uint32_t df=(frames>pregap)?(frames-pregap):0; if(df>lf){lf=df;rb=cum;rp=pregap;rs=ss;rh=hs;rrd=rds;} }
    if (track==RC_HASH_CDTRACK_LAST) { rb=cum; rp=pregap; rs=ss; rh=hs; rrd=rds; }
    cum+=frames;
  }
  if (!found && (track==RC_HASH_CDTRACK_LARGEST||track==RC_HASH_CDTRACK_LAST) && rs>0) found=1;
  if (found) {
    cdrom->track_first_sector=rb; cdrom->track_pregap_sectors=rp;
    cdrom->sector_size=rs; cdrom->sector_header_size=rh; cdrom->raw_data_size=rrd;
    cdrom->file_track_offset=(int64_t)rb*rs;
    rc_hash_iterator_verbose_formatted(iterator,"Opened CHD CD track (sector size %d, first sector %u, pregap %u)",rs,rb,rp);
    return cdrom;
  }
  if (header->unitbytes==2048 && (track==1||track==RC_HASH_CDTRACK_FIRST_DATA||track==RC_HASH_CDTRACK_LARGEST)) {
    cdrom->track_first_sector=0; cdrom->track_pregap_sectors=0;
    cdrom->sector_size=2048; cdrom->sector_header_size=0; cdrom->raw_data_size=2048;
    cdrom->file_track_offset=0;
    rc_hash_iterator_verbose_formatted(iterator,"Opened CHD as DVD track (2048-byte sectors, %u total sectors)",(uint32_t)(header->logicalbytes/2048));
    return cdrom;
  }
  rc_hash_iterator_error(iterator,"Could not open track");
  free(cdrom); chd_filereader_close(rdr); return NULL;
}
#endif /* HAVE_CHD */

static void* cdreader_open_track_iterator(const char* path, uint32_t track, const rc_hash_iterator_t* iterator)
{
  /* backwards compatibility - 0 used to mean largest */
  if (track == 0)
    track = RC_HASH_CDTRACK_LARGEST;

#ifdef HAVE_CHD
  if (rc_path_compare_extension(path, "chd"))
    return cdreader_open_chd_track(path, track, iterator);
#endif

  if (rc_path_compare_extension(path, "cue"))
    return cdreader_open_cue_track(path, track, iterator);
  if (rc_path_compare_extension(path, "gdi"))
    return cdreader_open_gdi_track(path, track, iterator);

  return cdreader_open_bin_track(path, track, iterator);
}

static size_t cdreader_read_sector(void* track_handle, uint32_t sector, void* buffer, size_t requested_bytes)
{
  int64_t sector_start;
  size_t num_read, total_read = 0;
  uint8_t* buffer_ptr = (uint8_t*)buffer;

  rc_hash_cdrom_track_t* cdrom = (rc_hash_cdrom_track_t*)track_handle;
  if (!cdrom)
    return 0;

  if (sector < (uint32_t)cdrom->track_first_sector)
    return 0;

  sector_start = (int64_t)(sector - cdrom->track_first_sector) * cdrom->sector_size + 
      cdrom->sector_header_size + cdrom->file_track_offset;

  while (requested_bytes > (size_t)cdrom->raw_data_size)
  {
    cdrom->file_reader->seek(cdrom->file_handle, sector_start, SEEK_SET);
    num_read = cdrom->file_reader->read(cdrom->file_handle, buffer_ptr, cdrom->raw_data_size);
    total_read += num_read;

    if (num_read < (size_t)cdrom->raw_data_size)
      return total_read;

    buffer_ptr += cdrom->raw_data_size;
    sector_start += cdrom->sector_size;
    requested_bytes -= cdrom->raw_data_size;
  }

  cdrom->file_reader->seek(cdrom->file_handle, sector_start, SEEK_SET);
  num_read = cdrom->file_reader->read(cdrom->file_handle, buffer_ptr, (int)requested_bytes);
  total_read += num_read;

  return total_read;
}

static void cdreader_close_track(void* track_handle)
{
  rc_hash_cdrom_track_t* cdrom = (rc_hash_cdrom_track_t*)track_handle;
  if (cdrom)
  {
    if (cdrom->file_handle && cdrom->file_reader->close)
      cdrom->file_reader->close(cdrom->file_handle);

    free(track_handle);
  }
}

static uint32_t cdreader_first_track_sector(void* track_handle)
{
  rc_hash_cdrom_track_t* cdrom = (rc_hash_cdrom_track_t*)track_handle;
  if (cdrom)
    return cdrom->track_first_sector + cdrom->track_pregap_sectors;

  return 0;
}

void rc_hash_get_default_cdreader(struct rc_hash_cdreader* cdreader)
{
  cdreader->open_track = NULL;
  cdreader->read_sector = cdreader_read_sector;
  cdreader->close_track = cdreader_close_track;
  cdreader->first_track_sector = cdreader_first_track_sector;
  cdreader->open_track_iterator = cdreader_open_track_iterator;
}

void rc_hash_init_default_cdreader(void)
{
  struct rc_hash_cdreader cdreader;
  rc_hash_get_default_cdreader(&cdreader);
  rc_hash_init_custom_cdreader(&cdreader);
}
