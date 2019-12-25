#include "rhash.h"

#ifdef RARCH_INTERNAL
 #include <libretro-common/include/rhash.h>
 #define md5_state_t MD5_CTX
 #define md5_byte_t unsigned char
 #define md5_init(state) MD5_Init(state)
 #define md5_append(state, buffer, size) MD5_Update(state, buffer, size)
 #define md5_finish(state, hash) MD5_Final(hash, state)
#else
 #include "md5.h"
#endif

#include <ctype.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
 #ifndef strcasecmp
  #define strcasecmp _stricmp
 #endif
 #ifndef strncasecmp
  #define strncasecmp _strnicmp
 #endif
#endif

/* arbitrary limit to prevent allocating and hashing large files */
#define MAX_BUFFER_SIZE 64 * 1024 * 1024

const char* rc_path_get_filename(const char* path);

/* ===================================================== */

static rc_hash_message_callback error_message_callback = NULL;
rc_hash_message_callback verbose_message_callback = NULL;

void rc_hash_init_error_message_callback(rc_hash_message_callback callback)
{
  error_message_callback = callback;
}

int rc_hash_error(const char* message)
{
  if (error_message_callback)
    error_message_callback(message);

  return 0;
}

void rc_hash_init_verbose_message_callback(rc_hash_message_callback callback)
{
  verbose_message_callback = callback;
}

static void rc_hash_verbose(const char* message)
{
  if (verbose_message_callback)
    verbose_message_callback(message);
}

/* ===================================================== */

static struct rc_hash_filereader filereader_funcs;
static struct rc_hash_filereader* filereader = NULL;

void rc_hash_init_custom_filereader(struct rc_hash_filereader* reader)
{
  memcpy(&filereader_funcs, reader, sizeof(filereader_funcs));
  filereader = &filereader_funcs;
}

static void* filereader_open(const char* path)
{
  return fopen(path, "rb");
}

static void filereader_seek(void* file_handle, size_t offset, int origin)
{
  fseek((FILE*)file_handle, offset, origin);
}

static size_t filereader_tell(void* file_handle)
{
  return ftell((FILE*)file_handle);
}

static size_t filereader_read(void* file_handle, void* buffer, size_t requested_bytes)
{
  return fread(buffer, 1, requested_bytes, (FILE*)file_handle);
}

static void filereader_close(void* file_handle)
{
  fclose((FILE*)file_handle);
}

void* rc_file_open(const char* path)
{
  void* handle;

  if (!filereader)
  {
    filereader_funcs.open = filereader_open;
    filereader_funcs.seek = filereader_seek;
    filereader_funcs.tell = filereader_tell;
    filereader_funcs.read = filereader_read;
    filereader_funcs.close = filereader_close;

    filereader = &filereader_funcs;
  }

  handle = filereader->open(path);
  if (handle && verbose_message_callback)
  {
    char buffer[1024];
    snprintf(buffer, sizeof(buffer), "Opened %s", rc_path_get_filename(path));
    verbose_message_callback(buffer);
  }

  return handle;
}

void rc_file_seek(void* file_handle, size_t offset, int origin)
{
  if (filereader)
    filereader->seek(file_handle, offset, origin);
}

size_t rc_file_tell(void* file_handle)
{
  return (filereader) ? filereader->tell(file_handle) : 0;
}

size_t rc_file_read(void* file_handle, void* buffer, int requested_bytes)
{
  return (filereader) ? filereader->read(file_handle, buffer, requested_bytes) : 0;
}

void rc_file_close(void* file_handle)
{
  if (filereader)
    filereader->close(file_handle);
}

/* ===================================================== */

static struct rc_hash_cdreader cdreader_funcs;
static struct rc_hash_cdreader* cdreader = NULL;

void rc_hash_init_custom_cdreader(struct rc_hash_cdreader* reader)
{
  memcpy(&cdreader_funcs, reader, sizeof(cdreader_funcs));
  cdreader = &cdreader_funcs;
}

static void* rc_cd_open_track(const char* path, uint32_t track)
{
  if (cdreader && cdreader->open_track)
    return cdreader->open_track(path, track);

  rc_hash_error("no hook registered for cdreader_open_track");
  return NULL;
}

static size_t rc_cd_read_sector(void* track_handle, uint32_t sector, void* buffer, size_t requested_bytes)
{
  if (cdreader && cdreader->read_sector)
    return cdreader->read_sector(track_handle, sector, buffer, requested_bytes);

  rc_hash_error("no hook registered for cdreader_read_sector");
  return 0;
}

static void rc_cd_close_track(void* track_handle)
{
  if (cdreader && cdreader->close_track)
  {
    cdreader->close_track(track_handle);
    return;
  }

  rc_hash_error("no hook registered for cdreader_close_track");
}

static uint32_t rc_cd_find_file_sector(void* track_handle, const char* path, unsigned* size)
{
  uint8_t buffer[2048], *tmp;
  int sector;
  int filename_length;
  const char* slash;

  if (!track_handle)
    return 0;

  filename_length = strlen(path);
  slash = strrchr(path, '\\');
  if (slash)
  {
    /* find the directory record for the first part of the path */
    memcpy(buffer, path, slash - path);
    buffer[slash - path] = '\0';

    sector = rc_cd_find_file_sector(track_handle, (const char *)buffer, NULL);
    if (!sector)
      return 0;

    ++slash;
    filename_length -= (slash - path);
    path = slash;
  }
  else
  {
    /* find the cd information (always 16 frames in) */
    if (!rc_cd_read_sector(track_handle, 16, buffer, 256))
      return 0;

    /* the directory_record starts at 156, the sector containing the table of contents is 2 bytes into that.
     * https://www.cdroller.com/htm/readdata.html
     */
    sector = buffer[156 + 2] | (buffer[156 + 3] << 8) | (buffer[156 + 4] << 16);
  }

  /* fetch and process the directory record */
  if (!rc_cd_read_sector(track_handle, sector, buffer, sizeof(buffer)))
    return 0;

  tmp = buffer;
  while (tmp < buffer + sizeof(buffer))
  {
    if (!*tmp)
      return 0;

    /* filename is 33 bytes into the record and the format is "FILENAME;version" or "DIRECTORY" */
    if ((tmp[33 + filename_length] == ';' || tmp[33 + filename_length] == '\0') && 
        strncasecmp((const char*)(tmp + 33), path, filename_length) == 0)
    {
      sector = tmp[2] | (tmp[3] << 8) | (tmp[4] << 16);

      if (verbose_message_callback)
      {
        snprintf((char*)buffer, sizeof(buffer), "Found %s at sector %d", path, sector);
        verbose_message_callback((const char*)buffer);
      }

      if (size)
        *size = tmp[10] | (tmp[11] << 8) | (tmp[12] << 16) | (tmp[13] << 24);

      return sector;
    }

    /* the first byte of the record is the length of the record */
    tmp += *tmp;
  }

  return 0;
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
      break;

    --ptr;
  } while (ptr > path);

  return ptr;
}

int rc_path_compare_extension(const char* path, const char* ext)
{
  int path_len = strlen(path);
  int ext_len = strlen(ext);
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

static int rc_hash_finalize(md5_state_t* md5, char hash[33])
{
  md5_byte_t digest[16];

  md5_finish(md5, digest);

  /* NOTE: sizeof(hash) is 4 because it's still treated like a pointer, despite specifying a size */
  snprintf(hash, 33, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
    digest[0], digest[1], digest[2], digest[3], digest[4], digest[5], digest[6], digest[7],
    digest[8], digest[9], digest[10], digest[11], digest[12], digest[13], digest[14], digest[15]
  );

  if (verbose_message_callback)
  {
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "Generated hash %s", hash);
    verbose_message_callback(buffer);
  }

  return 1;
}

static int rc_hash_buffer(char hash[33], uint8_t* buffer, size_t buffer_size)
{
  md5_state_t md5;
  md5_init(&md5);

  if (buffer_size > MAX_BUFFER_SIZE)
    buffer_size = MAX_BUFFER_SIZE;

  md5_append(&md5, buffer, buffer_size);

  if (verbose_message_callback)
  {
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "Hashing %zu byte buffer", buffer_size);
    verbose_message_callback(buffer);
  }

  return rc_hash_finalize(&md5, hash);
}

static int rc_hash_arcade(char hash[33], const char* path)
{
  /* arcade hash is just the hash of the filename (no extension) - the cores are pretty stringent about having the right ROM data */
  const char* ptr = rc_path_get_filename(path);
  const char* ext = rc_path_get_extension(ptr);
  return rc_hash_buffer(hash, (uint8_t*)ptr, ext - ptr - 1);
}

static int rc_hash_lynx(char hash[33], uint8_t* buffer, size_t buffer_size)
{
  /* if the file contains a header, ignore it */
  if (buffer[0] == 'L' && buffer[1] == 'Y' && buffer[2] == 'N' && buffer[3] == 'X' && buffer[4] == 0)
  {
    rc_hash_verbose("Ignoring LYNX header");

    buffer += 64;
    buffer_size -= 64;
  }

  return rc_hash_buffer(hash, buffer, buffer_size);
}

static int rc_hash_nes(char hash[33], uint8_t* buffer, size_t buffer_size)
{
  /* if the file contains a header, ignore it */
  if (buffer[0] == 'N' && buffer[1] == 'E' && buffer[2] == 'S' && buffer[3] == 0x1A)
  {
    rc_hash_verbose("Ignoring NES header");

    buffer += 16;
    buffer_size -= 16;
  }

  return rc_hash_buffer(hash, buffer, buffer_size);
}

static int rc_hash_nintendo_ds(char hash[33], const char* path)
{
  uint8_t header[512];
  uint8_t* hash_buffer;
  unsigned int hash_size, arm9_size, arm9_addr, arm7_size, arm7_addr, icon_addr;
  size_t num_read;
  int offset = 0;
  md5_state_t md5;
  void* file_handle;

  file_handle = rc_file_open(path);
  if (!file_handle)
    return rc_hash_error("Could not open file");

  rc_file_seek(file_handle, 0, SEEK_SET);
  if (rc_file_read(file_handle, header, sizeof(header)) != 512)
    return rc_hash_error("Failed to read header");

  if (header[0] == 0x2E && header[1] == 0x00 && header[2] == 0x00 && header[3] == 0xEA &&
    header[0xB0] == 0x44 && header[0xB1] == 0x46 && header[0xB2] == 0x96 && header[0xB3] == 0)
  {
    /* SuperCard header detected, ignore it */
    rc_hash_verbose("Ignoring SuperCard header");

    offset = 512;
    rc_file_seek(file_handle, offset, SEEK_SET);
    rc_file_read(file_handle, header, sizeof(header));
  }

  arm9_addr = header[0x20] | (header[0x21] << 8) | (header[0x22] << 16) | (header[0x23] << 24);
  arm9_size = header[0x2C] | (header[0x2D] << 8) | (header[0x2E] << 16) | (header[0x2F] << 24);
  arm7_addr = header[0x30] | (header[0x31] << 8) | (header[0x32] << 16) | (header[0x33] << 24);
  arm7_size = header[0x3C] | (header[0x3D] << 8) | (header[0x3E] << 16) | (header[0x3F] << 24);
  icon_addr = header[0x68] | (header[0x69] << 8) | (header[0x6A] << 16) | (header[0x6B] << 24);

  if (arm9_size + arm7_size > 16 * 1024 * 1024)
  {
    /* sanity check - code blocks are typically less than 1MB each - assume not a DS ROM */
    snprintf((char*)header, sizeof(header), "arm9 code size (%u) + arm7 code size (%u) exceeds 16MB", arm9_size, arm7_size);
    return rc_hash_error((const char*)header);
  }

  hash_size = 0xA00;
  if (arm9_size > hash_size)
    hash_size = arm9_size;
  if (arm7_size > hash_size)
    hash_size = arm7_size;

  hash_buffer = (uint8_t*)malloc(hash_size);
  if (!hash_buffer)
  {
    rc_file_close(file_handle);

    snprintf((char*)header, sizeof(header), "Failed to allocate %u bytes", hash_size);
    return rc_hash_error((const char*)header);
  }

  md5_init(&md5);

  rc_hash_verbose("Hashing 352 byte header");
  md5_append(&md5, header, 0x160);

  if (verbose_message_callback)
  {
    snprintf((char*)header, sizeof(header), "Hashing %u byte arm9 code (at %08X)", arm9_size, arm9_addr);
    verbose_message_callback((const char*)header);
  }

  rc_file_seek(file_handle, arm9_addr + offset, SEEK_SET);
  rc_file_read(file_handle, hash_buffer, arm9_size);
  md5_append(&md5, hash_buffer, arm9_size);

  if (verbose_message_callback)
  {
    snprintf((char*)header, sizeof(header), "Hashing %u byte arm7 code (at %08X)", arm7_size, arm7_addr);
    verbose_message_callback((const char*)header);
  }

  rc_file_seek(file_handle, arm7_addr + offset, SEEK_SET);
  rc_file_read(file_handle, hash_buffer, arm7_size);
  md5_append(&md5, hash_buffer, arm7_size);

  if (verbose_message_callback)
  {
    snprintf((char*)header, sizeof(header), "Hashing 2560 byte icon and labels data (at %08X)", icon_addr);
    verbose_message_callback((const char*)header);
  }

  rc_file_seek(file_handle, icon_addr + offset, SEEK_SET);
  num_read = rc_file_read(file_handle, hash_buffer, 0xA00);
  if (num_read < 0xA00)
  {
    /* some homebrew games don't provide a full icon block, and no data after the icon block.
     * if we didn't get a full icon block, fill the remaining portion with 0s
     */
    if (verbose_message_callback)
    {
      snprintf((char*)header, sizeof(header), "Warning: only got %zu bytes for icon and labels data, 0-padding to 2560 bytes", num_read);
      verbose_message_callback((const char*)header);
    }

    memset(&hash_buffer[num_read], 0, 0xA00 - num_read);
  }
  md5_append(&md5, hash_buffer, 0xA00);

  free(hash_buffer);
  rc_file_close(file_handle);

  return rc_hash_finalize(&md5, hash);
}

static int rc_hash_pce_cd(char hash[33], const char* path)
{
  uint8_t buffer[2048];
  void* track_handle;
  md5_state_t md5;
  int sector, num_sectors;
  unsigned size;

  track_handle = rc_cd_open_track(path, 0);
  if (!track_handle)
    return rc_hash_error("Could not open track");

  /* the PC-Engine uses the second sector to specify boot information and program name.
   * the string "PC Engine CD-ROM SYSTEM" should exist at 32 bytes into the sector
   * http://shu.sheldows.com/shu/download/pcedocs/pce_cdrom.html
   */
  rc_cd_read_sector(track_handle, 1, buffer, 128);

  /* normal PC Engine CD will have a header block in sector 1 */
  if (strncmp("PC Engine CD-ROM SYSTEM", (const char*)&buffer[32], 23) == 0)
  {
    /* the title of the disc is the last 22 bytes of the header */
    md5_init(&md5);
    md5_append(&md5, &buffer[106], 22);

    if (verbose_message_callback)
    {
      char message[128];
      buffer[128] = '\0';
      snprintf(message, sizeof(message), "Found PC Engine CD, title=%s", &buffer[106]);
      verbose_message_callback(message);
    }

    /* the first three bytes specify the sector of the program data, and the fourth byte
     * is the number of sectors.
     */
    sector = buffer[0] * 65536 + buffer[1] * 256 + buffer[2];
    num_sectors = buffer[3];

    if (verbose_message_callback)
    {
      char message[128];
      snprintf(message, sizeof(message), "Hashing %d sectors starting at sector %d", num_sectors, sector);
      verbose_message_callback(message);
    }

    while (num_sectors > 0)
    {
      rc_cd_read_sector(track_handle, sector, buffer, sizeof(buffer));
      md5_append(&md5, buffer, sizeof(buffer));

      ++sector;
      --num_sectors;
    }
  }
  /* GameExpress CDs use a standard Joliet filesystem - locate and hash the BOOT.BIN */
  else if ((sector = rc_cd_find_file_sector(track_handle, "BOOT.BIN", &size)) != 0 && size < MAX_BUFFER_SIZE)
  {
    md5_init(&md5);
    while (size > sizeof(buffer))
    {
      rc_cd_read_sector(track_handle, sector, buffer, sizeof(buffer));
      md5_append(&md5, buffer, sizeof(buffer));

      ++sector;
      size -= sizeof(buffer);
    }

    if (size > 0)
    {
      rc_cd_read_sector(track_handle, sector, buffer, size);
      md5_append(&md5, buffer, size);
    }
  }
  else
  {
    rc_cd_close_track(track_handle);
    return rc_hash_error("Not a PC Engine CD");
  }

  rc_cd_close_track(track_handle);

  return rc_hash_finalize(&md5, hash);
}

static int rc_hash_psx(char hash[33], const char* path)
{
  uint8_t buffer[2048];
  char exe_name[64] = "";
  char* ptr;
  char* start;
  void* track_handle;
  uint32_t sector;
  unsigned size;
  size_t num_read;
  int result = 0;
  md5_state_t md5;

  track_handle = rc_cd_open_track(path, 1);
  if (!track_handle)
    return rc_hash_error("Could not open track");

  sector = rc_cd_find_file_sector(track_handle, "SYSTEM.CNF", NULL);
  if (!sector)
  {
    sector = rc_cd_find_file_sector(track_handle, "PSX.EXE", &size);
    if (sector)
      strcpy(exe_name, "PSX.EXE");
  }
  else
  {
    size = rc_cd_read_sector(track_handle, sector, buffer, sizeof(buffer) - 1);
    buffer[size] = '\0';

    for (ptr = (char*)buffer; *ptr; ++ptr)
    {
      if (strncmp(ptr, "BOOT", 4) == 0)
      {
        ptr += 4;
        while (isspace(*ptr))
          ++ptr;

        if (*ptr == '=')
        {
          ++ptr;
          while (isspace(*ptr))
            ++ptr;

          if (strncmp(ptr, "cdrom:", 6) == 0)
            ptr += 6;
          if (*ptr == '\\')
            ++ptr;

          start = ptr;
          while (!isspace(*ptr) && *ptr != ';')
            ++ptr;

          size = ptr - start;
          if (size >= sizeof(exe_name))
            size = sizeof(exe_name) - 1;

          memcpy(exe_name, start, size);
          exe_name[size] = '\0';

          if (verbose_message_callback)
          {
            snprintf((char*)buffer, sizeof(buffer), "Looking for boot executable: %s", exe_name);
            verbose_message_callback((const char*)buffer);
          }

          sector = rc_cd_find_file_sector(track_handle, exe_name, &size);
          break;
        }
      }

      /* advance to end of line */
      while (*ptr && *ptr != '\n')
        ++ptr;
    }
  }

  if (!sector)
  {
    rc_hash_error("Could not locate primary executable");
  }
  else if ((num_read = rc_cd_read_sector(track_handle, sector, buffer, sizeof(buffer))) < sizeof(buffer))
  {
    rc_hash_error("Could not read primary executable");
  }
  else
  {
    if (memcmp(buffer, "PS-X EXE", 7) != 0)
    {
      if (verbose_message_callback)
      {
        char message[128];
        snprintf(message, sizeof(message), "%s did not contain PS-X EXE marker", exe_name);
        verbose_message_callback(message);
      }
    }
    else
    {
      /* the PS-X EXE header specifies the executable size as a 4-byte value 28 bytes into the header, which doesn't
       * include the header itself. We want to include the header in the hash, so append another 2048 to that value.
       */
      size = (((uint8_t)buffer[31] << 24) | ((uint8_t)buffer[30] << 16) | ((uint8_t)buffer[29] << 8) | (uint8_t)buffer[28]) + 2048;
    }

    if (size > MAX_BUFFER_SIZE)
      size = MAX_BUFFER_SIZE;

    if (verbose_message_callback)
    {
      char message[128];
      snprintf(message, sizeof(message), "Hashing %s title (%zu bytes) and contents (%zu bytes) ", exe_name, strlen(exe_name), size);
      verbose_message_callback(message);
    }

    /* there's also a few games that are use a singular engine and only differ via their data files. luckily, they have
     * unique serial numbers, and use the serial number as the boot file in the standard way. include the boot file in the hash
     */
    md5_init(&md5);
    md5_append(&md5, exe_name, strlen(exe_name));

    do
    {
      md5_append(&md5, buffer, num_read);

      size -= num_read;
      if (size == 0)
        break;

      ++sector;
      if (size >= sizeof(buffer))
        num_read = rc_cd_read_sector(track_handle, sector, buffer, sizeof(buffer));
      else
        num_read = rc_cd_read_sector(track_handle, sector, buffer, size);
    } while (num_read > 0);

    result = rc_hash_finalize(&md5, hash);
  }

  rc_cd_close_track(track_handle);

  return result;
}

static int rc_hash_sega_cd(char hash[33], const char* path)
{
  uint8_t buffer[512];
  void* track_handle;

  track_handle = rc_cd_open_track(path, 1);
  if (!track_handle)
    return rc_hash_error("Could not open track");

  /* the first 512 bytes of sector 0 are a volume header and ROM header that uniquely identify the game.
   * After that is an arbitrary amount of code that ensures the game is being run in the correct region.
   * Then more arbitrary code follows that actually starts the boot process. Somewhere in there, the
   * primary executable is loaded. In many cases, a single game will have multiple executables, so even
   * if we could determine the primary one, it's just the tip of the iceberg. As such, we've decided that
   * hashing the volume and ROM headers is sufficient for identifying the game, and we'll have to trust
   * that our players aren't modifying anything else on the disc.
   */
  rc_cd_read_sector(track_handle, 0, buffer, sizeof(buffer));

  return rc_hash_buffer(hash, buffer, sizeof(buffer));
}

static int rc_hash_snes(char hash[33], uint8_t* buffer, size_t buffer_size)
{
  /* if the file contains a header, ignore it */
  uint32_t calc_size = (buffer_size / 0x2000) * 0x2000;
  if (buffer_size - calc_size == 512)
  {
    rc_hash_verbose("Ignoring SNES header");

    buffer += 512;
    buffer_size -= 512;
  }

  return rc_hash_buffer(hash, buffer, buffer_size);
}

int rc_hash_generate_from_buffer(char hash[33], int console_id, uint8_t* buffer, size_t buffer_size)
{
  switch (console_id)
  {
    default:
    {
      char buffer[128];
      snprintf(buffer, sizeof(buffer), "Unsupported console for buffer hash: %d", console_id);
      return rc_hash_error(buffer);
    }

    case RC_CONSOLE_APPLE_II:
    case RC_CONSOLE_ATARI_2600:
    case RC_CONSOLE_ATARI_7800:
    case RC_CONSOLE_ATARI_JAGUAR:
    case RC_CONSOLE_COLECOVISION:
    case RC_CONSOLE_GAMEBOY:
    case RC_CONSOLE_GAMEBOY_ADVANCE:
    case RC_CONSOLE_GAMEBOY_COLOR:
    case RC_CONSOLE_GAME_GEAR:
    case RC_CONSOLE_MASTER_SYSTEM:
    case RC_CONSOLE_MEGA_DRIVE:
    case RC_CONSOLE_NEOGEO_POCKET:
    case RC_CONSOLE_NINTENDO_64:
    case RC_CONSOLE_ORIC:
    case RC_CONSOLE_PC_ENGINE: /* NOTE: does not support PCEngine CD */
    case RC_CONSOLE_PC8800:
    case RC_CONSOLE_POKEMON_MINI:
    case RC_CONSOLE_SEGA_32X:
    case RC_CONSOLE_SG1000:
    case RC_CONSOLE_VIRTUAL_BOY:
    case RC_CONSOLE_WONDERSWAN:
      return rc_hash_buffer(hash, buffer, buffer_size);

    case RC_CONSOLE_ATARI_LYNX:
      return rc_hash_lynx(hash, buffer, buffer_size);
      
    case RC_CONSOLE_NINTENDO:
      return rc_hash_nes(hash, buffer, buffer_size);

    case RC_CONSOLE_SUPER_NINTENDO:
      return rc_hash_snes(hash, buffer, buffer_size);
  }
}

static int rc_hash_whole_file(char hash[33], int console_id, const char* path)
{
  md5_state_t md5;
  uint8_t* buffer;
  int size;
  const int buffer_size = 65536;
  void* file_handle;
  int result = 0;

  file_handle = rc_file_open(path);
  if (!file_handle)
    return rc_hash_error("Could not open file");

  rc_file_seek(file_handle, 0, SEEK_END);
  size = rc_file_tell(file_handle);

  if (verbose_message_callback)
  {
    char message[1024];
    if (size > MAX_BUFFER_SIZE)
      snprintf(message, sizeof(message), "Hashing first %zu bytes (of %zu bytes) of %s", MAX_BUFFER_SIZE, size, rc_path_get_filename(path));
    else
      snprintf(message, sizeof(message), "Hashing %s (%zu bytes)", rc_path_get_filename(path), size);
    verbose_message_callback(message);
  }

  if (size > MAX_BUFFER_SIZE)
    size = MAX_BUFFER_SIZE;

  md5_init(&md5);

  buffer = (uint8_t*)malloc(buffer_size);
  if (buffer)
  {
    rc_file_seek(file_handle, 0, SEEK_SET);
    while (size >= buffer_size)
    {
      rc_file_read(file_handle, buffer, buffer_size);
      md5_append(&md5, buffer, buffer_size);
      size -= buffer_size;
    }

    if (size > 0)
    {
      rc_file_read(file_handle, buffer, size);
      md5_append(&md5, buffer, size);
    }

    free(buffer);
    result = rc_hash_finalize(&md5, hash);
  }

  rc_file_close(file_handle);
  return result;
}

static int rc_hash_buffered_file(char hash[33], int console_id, const char* path)
{
  uint8_t* buffer;
  int size;
  int result = 0;
  void* file_handle;

  file_handle = rc_file_open(path);
  if (!file_handle)
    return rc_hash_error("Could not open file");

  rc_file_seek(file_handle, 0, SEEK_END);
  size = rc_file_tell(file_handle);

  if (verbose_message_callback)
  {
    char message[1024];
    if (size > MAX_BUFFER_SIZE)
      snprintf(message, sizeof(message), "Buffering first %zu bytes (of %zu bytes) of %s", MAX_BUFFER_SIZE, size, rc_path_get_filename(path));
    else
      snprintf(message, sizeof(message), "Buffering %s (%zu bytes)", rc_path_get_filename(path), size);
    verbose_message_callback(message);
  }

  if (size > MAX_BUFFER_SIZE)
    size = MAX_BUFFER_SIZE;

  buffer = (uint8_t*)malloc(size);
  if (buffer)
  {
    rc_file_seek(file_handle, 0, SEEK_SET);
    rc_file_read(file_handle, buffer, size);

    result = rc_hash_generate_from_buffer(hash, console_id, buffer, size);

    free(buffer);
  }

  rc_file_close(file_handle);
  return result;
}

static const char* rc_hash_get_first_item_from_playlist(const char* path)
{
  char buffer[1024];
  char* disc_path;
  char* ptr;
  size_t num_read;
  void* file_handle;

  file_handle = rc_file_open(path);
  if (!file_handle)
  {
    rc_hash_error("Could not open playlist");
    return NULL;
  }

  num_read = rc_file_read(file_handle, buffer, sizeof(buffer) - 1);
  buffer[num_read] = '\0';

  rc_file_close(file_handle);

  ptr = buffer;
  while (*ptr && *ptr != '\n')
    ++ptr;
  if (ptr > buffer && ptr[-1] == '\r')
    --ptr;
  *ptr = '\0';

  if (verbose_message_callback)
  {
    char message[1024];
    snprintf(message, sizeof(message), "Extracted %s from playlist", buffer);
    verbose_message_callback(message);
  }

  ptr = (char*)rc_path_get_filename(path);
  num_read = (ptr - path) + strlen(buffer) + 1;

  disc_path = (char*)malloc(num_read);
  if (!disc_path)
    return NULL;

  memcpy(disc_path, path, ptr - path);
  strcpy(disc_path + (ptr - path), buffer);
  return disc_path;
}

static int rc_hash_generate_from_playlist(char hash[33], int console_id, const char* path)
{
  int result;
  const char* disc_path;

  if (verbose_message_callback)
  {
    char message[1024];
    snprintf(message, sizeof(message), "Processing playlist: %s", rc_path_get_filename(path));
    verbose_message_callback(message);
  }
  
  disc_path = rc_hash_get_first_item_from_playlist(path);
  if (!disc_path)
    return rc_hash_error("Failed to get first item from playlist");

  result = rc_hash_generate_from_file(hash, console_id, disc_path);

  free((void*)disc_path);
  return result;
}

int rc_hash_generate_from_file(char hash[33], int console_id, const char* path)
{
  switch (console_id)
  {
    default:
    {
      char buffer[128];
      snprintf(buffer, sizeof(buffer), "Unsupported console for file hash: %d", console_id);
      return rc_hash_error(buffer);
    }

    case RC_CONSOLE_APPLE_II:
    case RC_CONSOLE_ATARI_2600:
    case RC_CONSOLE_ATARI_7800:
    case RC_CONSOLE_ATARI_JAGUAR:
    case RC_CONSOLE_COLECOVISION:
    case RC_CONSOLE_GAMEBOY:
    case RC_CONSOLE_GAMEBOY_ADVANCE:
    case RC_CONSOLE_GAMEBOY_COLOR:
    case RC_CONSOLE_GAME_GEAR:
    case RC_CONSOLE_MASTER_SYSTEM:
    case RC_CONSOLE_MEGA_DRIVE:
    case RC_CONSOLE_NEOGEO_POCKET:
    case RC_CONSOLE_NINTENDO_64:
    case RC_CONSOLE_ORIC:
    case RC_CONSOLE_PC8800:
    case RC_CONSOLE_POKEMON_MINI:
    case RC_CONSOLE_SEGA_32X:
    case RC_CONSOLE_SG1000:
    case RC_CONSOLE_VIRTUAL_BOY:
    case RC_CONSOLE_WONDERSWAN:
      /* generic whole-file hash - don't buffer */
      return rc_hash_whole_file(hash, console_id, path);

    case RC_CONSOLE_ATARI_LYNX:
    case RC_CONSOLE_NINTENDO:
    case RC_CONSOLE_SUPER_NINTENDO:
      /* additional logic whole-file hash - buffer then call rc_hash_generate_from_buffer */
      return rc_hash_buffered_file(hash, console_id, path);

    case RC_CONSOLE_ARCADE:
      return rc_hash_arcade(hash, path);

    case RC_CONSOLE_NINTENDO_DS:
      return rc_hash_nintendo_ds(hash, path);

    case RC_CONSOLE_PC_ENGINE:
      if (rc_path_compare_extension(path, "cue"))
        return rc_hash_pce_cd(hash, path);

      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, path);

      return rc_hash_whole_file(hash, console_id, path);

    case RC_CONSOLE_PLAYSTATION:
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, path);

      return rc_hash_psx(hash, path);

    case RC_CONSOLE_SEGA_CD:
    case RC_CONSOLE_SATURN:
      if (rc_path_compare_extension(path, "m3u"))
        return rc_hash_generate_from_playlist(hash, console_id, path);

      return rc_hash_sega_cd(hash, path);
  }
}

void rc_hash_initialize_iterator(struct rc_hash_iterator* iterator, const char* path, uint8_t* buffer, size_t buffer_size)
{
  int need_path = !buffer;

  memset(iterator, 0, sizeof(*iterator));
  iterator->buffer = buffer;
  iterator->buffer_size = buffer_size;

  iterator->consoles[0] = 0;

  do
  {
    const char* ext = rc_path_get_extension(path);
    switch (tolower(*ext--))
    {
      case 'a':
        if (rc_path_compare_extension(ext, "a78"))
        {
          iterator->consoles[0] = RC_CONSOLE_ATARI_7800;
        }
        break;

      case 'b':
        if (rc_path_compare_extension(ext, "bin"))
        {
          /* bin is associated with MegaDrive, Sega32X and Atari 2600. Since they all use the same
           * hashing algorithm, only specify one of them */
          iterator->consoles[0] = RC_CONSOLE_MEGA_DRIVE;
        }
        break;

      case 'c':
        if (rc_path_compare_extension(ext, "cue"))
        {
          iterator->consoles[0] = RC_CONSOLE_PLAYSTATION;
          iterator->consoles[1] = RC_CONSOLE_PC_ENGINE;
          /* SEGA CD hash doesn't have any logic to ensure it's being used against a SEGA CD, so it should always be last */
          iterator->consoles[2] = RC_CONSOLE_SEGA_CD; 
          need_path = 1;
        }
        else if (rc_path_compare_extension(ext, "col"))
        {
          iterator->consoles[0] = RC_CONSOLE_COLECOVISION;
        }
        break;

      case 'd':
        if (rc_path_compare_extension(ext, "dsk"))
        {
          iterator->consoles[0] = RC_CONSOLE_APPLE_II;
        }
        else if (rc_path_compare_extension(ext, "d88"))
        {
          iterator->consoles[0] = RC_CONSOLE_PC8800;
        }
        break;

      case 'f':
        if (rc_path_compare_extension(ext, "fig"))
        {
          iterator->consoles[0] = RC_CONSOLE_SUPER_NINTENDO;
        }
        else if (rc_path_compare_extension(ext, "fds"))
        {
          iterator->consoles[0] = RC_CONSOLE_NINTENDO;
        }
        break;

      case 'g':
        if (rc_path_compare_extension(ext, "gba"))
        {
          iterator->consoles[0] = RC_CONSOLE_GAMEBOY_ADVANCE;
        }
        else if (rc_path_compare_extension(ext, "gbc"))
        {
          iterator->consoles[0] = RC_CONSOLE_GAMEBOY_COLOR;
        }
        else if (rc_path_compare_extension(ext, "gb"))
        {
          iterator->consoles[0] = RC_CONSOLE_GAMEBOY;
        }
        else if (rc_path_compare_extension(ext, "gg"))
        {
          iterator->consoles[0] = RC_CONSOLE_GAME_GEAR;
        }
        break;

      case 'i':
        if (rc_path_compare_extension(ext, "iso"))
        {
          iterator->consoles[0] = RC_CONSOLE_SEGA_CD;
          need_path = 1;
        }
        break;

      case 'j':
        if (rc_path_compare_extension(ext, "jag"))
        {
          iterator->consoles[0] = RC_CONSOLE_ATARI_JAGUAR;
        }
        break;

      case 'l':
        if (rc_path_compare_extension(ext, "lnx"))
        {
          iterator->consoles[0] = RC_CONSOLE_ATARI_LYNX;
        }
        break;

      case 'm':
        if (rc_path_compare_extension(ext, "m3u"))
        {
          const char* disc_path = rc_hash_get_first_item_from_playlist(path);
          if (disc_path)
          {
            path = iterator->path = disc_path;
            continue; /* retry with disc_path */
          }
        }
        else if (rc_path_compare_extension(ext, "md"))
        {
          iterator->consoles[0] = RC_CONSOLE_MEGA_DRIVE;
        }
        else if (rc_path_compare_extension(ext, "min"))
        {
          iterator->consoles[0] = RC_CONSOLE_POKEMON_MINI;
        }
        break;

      case 'n':
        if (rc_path_compare_extension(ext, "nes"))
        {
          iterator->consoles[0] = RC_CONSOLE_NINTENDO;
        }
        else if (rc_path_compare_extension(ext, "nds"))
        {
          iterator->consoles[0] = RC_CONSOLE_NINTENDO_DS;
        }
        else if (rc_path_compare_extension(ext, "n64"))
        {
          iterator->consoles[0] = RC_CONSOLE_NINTENDO_64;
        }
        else if (rc_path_compare_extension(ext, "ngc"))
        {
          iterator->consoles[0] = RC_CONSOLE_NEOGEO_POCKET;
        }
        break;

      case 'p':
        if (rc_path_compare_extension(ext, "pce"))
        {
          iterator->consoles[0] = RC_CONSOLE_PC_ENGINE;
        }
        break;

      case 's':
        if (rc_path_compare_extension(ext, "smc") || rc_path_compare_extension(ext, "sfc"))
        {
          iterator->consoles[0] = RC_CONSOLE_SUPER_NINTENDO;
        }
        else if (rc_path_compare_extension(ext, "sg"))
        {
          iterator->consoles[0] = RC_CONSOLE_SG1000;
        }
        else if (rc_path_compare_extension(ext, "sgx"))
        {
          iterator->consoles[0] = RC_CONSOLE_PC_ENGINE;
        }
        break;

      case 't':
        if (rc_path_compare_extension(ext, "tap"))
        {
          iterator->consoles[0] = RC_CONSOLE_ORIC;
        }
        break;

      case 'v':
        if (rc_path_compare_extension(ext, "vb"))
        {
          iterator->consoles[0] = RC_CONSOLE_VIRTUAL_BOY;
        }
        break;

      case 'w':
        if (rc_path_compare_extension(ext, "wsc"))
        {
          iterator->consoles[0] = RC_CONSOLE_WONDERSWAN;
        }
        break;

      case 'z':
        if (rc_path_compare_extension(ext, "zip"))
        {
          /* decompressing zip file not supported */
          iterator->consoles[0] = RC_CONSOLE_ARCADE;
          need_path = 1;
        }
        break;
    }

    if (verbose_message_callback)
    {
      char message[256];
      int count = 0;
      while (iterator->consoles[count])
        ++count;

      snprintf(message, sizeof(message), "Found %d potential consoles for %s file extension", count, ext);
      verbose_message_callback(message);
    }

    /* loop is only for specific cases that redirect to another file - like m3u */
    break;
  } while (1);

  if (need_path && !iterator->path)
    iterator->path = strdup(path);

  /* if we didn't match the extension, default to something that does a whole file hash */
  if (!iterator->consoles[0])
    iterator->consoles[0] = RC_CONSOLE_GAMEBOY;
}

void rc_hash_destroy_iterator(struct rc_hash_iterator* iterator)
{
  if (iterator->path)
  {
    free((void*)iterator->path);
    iterator->path = NULL;
  }
}

int rc_hash_iterate(char hash[33], struct rc_hash_iterator* iterator)
{
  int next_console;
  int result = 0;

  do
  {
    next_console = iterator->consoles[iterator->index];
    if (next_console == 0)
      break;

    ++iterator->index;

    if (verbose_message_callback)
    {
      char message[128];
      snprintf(message, sizeof(message), "Trying console %d", next_console);
      verbose_message_callback(message);
    }

    if (iterator->buffer)
      result = rc_hash_generate_from_buffer(hash, next_console, iterator->buffer, iterator->buffer_size);
    else
      result = rc_hash_generate_from_file(hash, next_console, iterator->path);

  } while (!result);

  return result;
}