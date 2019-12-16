#include "rhash.h"

#include <ctype.h>
#include <memory.h>
#include <stdlib.h>

/* internal helper functions in hash.c */
extern void* rc_file_open(const char* path);
extern void rc_file_seek(void* file_handle, size_t offset, int origin);
extern size_t rc_file_tell(void* file_handle);
extern size_t rc_file_read(void* file_handle, void* buffer, int requested_bytes);
extern void rc_file_close(void* file_handle);
extern int rc_hash_error(const char* message);
extern const char* rc_path_get_filename(const char* path);
extern rc_hash_message_callback verbose_message_callback;

struct cdrom_t
{
  void* file_handle;
  int sector_size;
  int sector_header_size;
  int first_sector_offset;
};

static void cdreader_determine_sector_size(struct cdrom_t* cdrom)
{
  /* Attempt to determine the sector and header sizes. The CUE file may be lying.
   * Look for the sync pattern using each of the supported sector sizes.
   * Then check for the presence of "CD001", which is gauranteed to be in either the
   * boot record or primary volume descriptor, one of which is always at sector 16.
   */
  const unsigned char sync_pattern[] = {
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00
  };

  unsigned char header[32];
  const int toc_sector = 16;

  cdrom->sector_size = 0;

  rc_file_seek(cdrom->file_handle, toc_sector * 2352 + cdrom->first_sector_offset, SEEK_SET);
  rc_file_read(cdrom->file_handle, header, sizeof(header));

  if (memcmp(header, sync_pattern, 12) == 0)
  {
    cdrom->sector_size = 2352;

    if (memcmp(&header[25], "CD001", 5) == 0)
      cdrom->sector_header_size = 24;
    else
      cdrom->sector_header_size = 16;
  }
  else
  {
    rc_file_seek(cdrom->file_handle, toc_sector * 2336 + cdrom->first_sector_offset, SEEK_SET);
    rc_file_read(cdrom->file_handle, header, sizeof(header));

    if (memcmp(header, sync_pattern, 12) == 0)
    {
      cdrom->sector_size = 2336;

      if (memcmp(&header[25], "CD001", 5) == 0)
        cdrom->sector_header_size = 24;
      else
        cdrom->sector_header_size = 16;
    }
    else
    {
      rc_file_seek(cdrom->file_handle, toc_sector * 2048 + cdrom->first_sector_offset, SEEK_SET);
      rc_file_read(cdrom->file_handle, header, sizeof(header));

      if (memcmp(&header[1], "CD001", 5) == 0)
      {
        cdrom->sector_size = 2048;
        cdrom->sector_header_size = 0;
      }
    }
  }

  /* no recognizable header - attempt to determine sector size from stream size */
  if (cdrom->sector_size == 0)
  {
    long size;

    rc_file_seek(cdrom->file_handle, 0, SEEK_END);
    size = ftell(cdrom->file_handle) - cdrom->first_sector_offset;

    if ((size % 2352) == 0)
    {
      /* audio tracks use all 2352 bytes without a header */
      cdrom->sector_size = 2352;
      cdrom->sector_header_size = 0;
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
  }
}

static void* cdreader_open_track(const char* path, uint32_t track)
{
  void* file_handle;
  char buffer[1024], *file = buffer, *ptr, *ptr2, *mode = buffer, *bin_filename;
  int current_track = 0;
  int sector_size = 0;
  int previous_sector_size = 0;
  int previous_index_sector_offset = 0;
  int offset = 0;
  size_t num_read = 0;
  struct cdrom_t* cdrom = NULL;

  file_handle = rc_file_open(path);
  if (!file_handle)
    return NULL;

  num_read = rc_file_read(file_handle, buffer, sizeof(buffer) - 1);
  buffer[num_read] = 0;
  rc_file_close(file_handle);

  for (ptr = buffer; *ptr; ++ptr)
  {
    while (*ptr == ' ')
      ++ptr;

    if (memcmp(ptr, "FILE ", 5) == 0)
    {
      file = ptr + 5;
      current_track = 0;
      previous_sector_size = 0;
      offset = 0;
    }
    else if (memcmp(ptr, "TRACK ", 6) == 0)
    {
      ptr += 6;
      current_track = atoi(ptr);

      while (*ptr != ' ')
        ++ptr;
      while (*ptr == ' ')
        ++ptr;
      mode = ptr;

      previous_sector_size = sector_size;
      previous_index_sector_offset = 0;

      if (memcmp(mode, "MODE", 4) == 0)
      {
        if (track == 0)
          track = current_track;

        sector_size = atoi(ptr + 6);
      }
      else
      {
        /* assume AUDIO */
        sector_size = 2352;
      }
    }
    else if (memcmp(ptr, "INDEX ", 6) == 0)
    {
      int m = 0, s = 0, f = 0;
      int index, sector_offset;

      ptr += 6;
      index = atoi(ptr);

      while (*ptr != ' ' && *ptr != '\n')
        ++ptr;
      while (*ptr == ' ')
        ++ptr;

      sscanf_s(ptr, "%d:%d:%d", &m, &s, &f);
      sector_offset = ((m * 60) + s) * 75 + f;
      sector_offset -= previous_index_sector_offset;
      offset += sector_offset * previous_sector_size;
      previous_sector_size = sector_size;
      previous_index_sector_offset += sector_offset;

      if (index == 1 && current_track == track)
      {
        cdrom = (struct cdrom_t*)malloc(sizeof(*cdrom));
        cdrom->first_sector_offset = offset;
          
        /* open bin file */
        ptr2 = file;
        if (*ptr2 == '"')
        {
          ++file;
          do
          {
            ++ptr2;
          } while (*ptr2 != '\n' && *ptr2 != '"');
        }
        else
        {
          do
          {
            ++ptr2;
          } while (*ptr2 != '\n' && *ptr2 != ' ');
        }
        *ptr2 = '\0';

        ptr = (char*)rc_path_get_filename(path);
        num_read = (ptr - path) + (ptr2 - file) + 1;

        bin_filename = (char*)malloc(num_read);
        if (bin_filename)
        {
          memcpy(bin_filename, path, ptr - path);
          memcpy(bin_filename + (ptr - path), file, ptr2 - file + 1);

          cdrom->file_handle = rc_file_open(bin_filename);
          if (cdrom->file_handle)
          {
            free(bin_filename);
              
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
              cdrom->sector_size = sector_size;
              cdrom->sector_header_size = 16;

              if (memcmp(mode, "MODE2/2352", 10) == 0)
              {
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
            }

            if (verbose_message_callback)
            {
              if (cdrom->first_sector_offset)
                snprintf((char*)buffer, sizeof(buffer), "Opened track %d (sector size %d, track starts at %d)", track, cdrom->sector_size, cdrom->first_sector_offset);
              else
                snprintf((char*)buffer, sizeof(buffer), "Opened track %d (sector size %d)", track, cdrom->sector_size);
              verbose_message_callback((const char*)buffer);
            }

            return cdrom;
          }

          snprintf((char*)buffer, sizeof(buffer), "Could not open %s", bin_filename);
          rc_hash_error((const char*)buffer);

          free(bin_filename);
        }

        free(cdrom);
      }
    }

    while (*ptr && *ptr != '\n')
      ++ptr;
  }

  return NULL;
}

static size_t cdreader_read_sector(void* track_handle, uint32_t sector, void* buffer, size_t requested_bytes)
{
  size_t sector_start;
  size_t num_read, total_read = 0;

  struct cdrom_t* cdrom = (struct cdrom_t*)track_handle;
  if (!cdrom)
    return 0;

  sector_start = sector * cdrom->sector_size + cdrom->sector_header_size + cdrom->first_sector_offset;

  while (requested_bytes > 2048)
  {
    rc_file_seek(cdrom->file_handle, sector_start, SEEK_SET);
    num_read = rc_file_read(cdrom->file_handle, buffer, requested_bytes);
    total_read += num_read;

    if (num_read < 2048)
      return total_read;

    sector_start += cdrom->sector_size;
    requested_bytes -= 2048;
  }

  rc_file_seek(cdrom->file_handle, sector_start, SEEK_SET);
  num_read = rc_file_read(cdrom->file_handle, buffer, requested_bytes);
  total_read += num_read;

  return total_read;
}

static void cdreader_close_track(void* track_handle)
{
  struct cdrom_t* cdrom = (struct cdrom_t*)track_handle;
  if (cdrom)
  {
    if (cdrom->file_handle)
      rc_file_close(cdrom->file_handle);

    free(track_handle);
  }
}

void rc_hash_init_default_cdreader()
{
  struct rc_hash_cdreader cdreader;

  cdreader.open_track = cdreader_open_track;
  cdreader.read_sector = cdreader_read_sector;
  cdreader.close_track = cdreader_close_track;

  rc_hash_init_custom_cdreader(&cdreader);
}
