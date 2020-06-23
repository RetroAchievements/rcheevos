#include "data.h"

#include <stdlib.h>
#include <string.h>

static void fill_image(uint8_t* image, size_t size)
{
  int seed = (int)(size ^ (size >> 8) ^ ((size - 1) * 25387));
  int count;
  uint8_t value;

  while (size > 0)
  {
    switch (seed & 0xFF)
    {
      case 0:
        count = (((seed >> 8) & 0x3F) & ~(size & 0x0F));
        if (count == 0)
          count = 1;
        value = 0;
        break;

      case 1:
        count = ((seed >> 8) & 0x07) + 1;
        value = ((seed >> 16) & 0xFF);
        break;

      case 2:
        count = ((seed >> 8) & 0x03) + 1;
        value = ((seed >> 16) & 0xFF) ^ 0xFF;
        break;

      case 3:
        count = ((seed >> 8) & 0x03) + 1;
        value = ((seed >> 16) & 0xFF) ^ 0xA5;
        break;

      case 4:
        count = ((seed >> 8) & 0x03) + 1;
        value = ((seed >> 16) & 0xFF) ^ 0xC3;
        break;

      case 5:
        count = ((seed >> 8) & 0x03) + 1;
        value = ((seed >> 16) & 0xFF) ^ 0x96;
        break;

      case 6:
        count = ((seed >> 8) & 0x03) + 1;
        value = ((seed >> 16) & 0xFF) ^ 0x78;
        break;

      case 7:
        count = ((seed >> 8) & 0x03) + 1;
        value = ((seed >> 16) & 0xFF) ^ 0x78;
        break;

      default:
        count = 1;
        value = (seed >> 8) ^ (seed >> 16);
        break;
    }

    do
    {
      *image++ = value;
      --size;
    } while (size && --count);

    /* state mutation from psuedo-random number generator */
    seed = (seed * 0x41C64E6D + 12345) & 0x7FFFFFFF;
  }
}

uint8_t* generate_nes_file(size_t kb, int with_header, size_t* image_size)
{
  uint8_t* image;
  size_t size_needed = kb * 1024;
  if (with_header)
    size_needed += 16;

  image = (uint8_t*)calloc(size_needed, 1);
  if (image != NULL)
  {
    if (with_header)
    {
      image[0] = 'N';
      image[1] = 'E';
      image[2] = 'S';
      image[3] = '\x1A';
      image[4] = (uint8_t)(kb / 16);

      fill_image(image + 16, size_needed - 16);
    }
    else
    {
      fill_image(image, size_needed);
    }
  }

  if (image_size)
    *image_size = size_needed;
  return image;
}

uint8_t* generate_fds_file(size_t sides, int with_header, size_t* image_size)
{
  uint8_t* image;
  size_t size_needed = sides * 65500;
  if (with_header)
    size_needed += 16;

  image = (uint8_t*)calloc(size_needed, 1);
  if (image != NULL)
  {
    if (with_header)
    {
      image[0] = 'F';
      image[1] = 'D';
      image[2] = 'S';
      image[3] = '\x1A';
      image[4] = (uint8_t)sides;

      fill_image(image + 16, size_needed - 16);
    }
    else
    {
      fill_image(image, size_needed);
    }
  }

  if (image_size)
    *image_size = size_needed;
  return image;
}

uint8_t* generate_3do_bin(unsigned root_directory_sectors, unsigned binary_size, size_t* image_size)
{
  const uint8_t volume_header[] = {
    0x01, 0x5A, 0x5A, 0x5A, 0x5A, 0x5A, 0x01, 0x00, /* header */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* comment */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    'C', 'D', '-', 'R', 'O', 'M', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* label */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0x2D, 0x79, 0x6E, 0x96, /* identifier */
    0x00, 0x00, 0x08, 0x00, /* block size */
    0x00, 0x00, 0x05, 0x00, /* block count */
    0x31, 0x5a, 0xf2, 0xe6, /* root directory identifier */
    0x00, 0x00, 0x00, 0x01, /* root directory size in blocks */
    0x00, 0x00, 0x08, 0x00, /* block size in root directory */
    0x00, 0x00, 0x00, 0x06, /* number of copies of root directory */
    0x00, 0x00, 0x00, 0x01, /* block location of root directory */
    0x00, 0x00, 0x00, 0x01, /* block location of first copy of root directory */
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01,
    0x00, 0x00, 0x00, 0x01, /* block location of last copy of root directory */
  };

  const uint8_t directory_data[] = {
    0xFF, 0xFF, 0xFF, 0xFF, /* next block */
    0xFF, 0xFF, 0xFF, 0xFF, /* previous block */
    0x00, 0x00, 0x00, 0x00, /* flags */
    0x00, 0x00, 0x00, 0xA4, /* end of block */
    0x00, 0x00, 0x00, 0x14, /* start of block */

    0x00, 0x00, 0x00, 0x07, /* flags - directory */
    0x00, 0x00, 0x00, 0x00, /* identifier */
    0x00, 0x00, 0x00, 0x00, /* type */
    0x00, 0x00, 0x08, 0x00, /* block size */
    0x00, 0x00, 0x00, 0x00, /* length in bytes */
    0x00, 0x00, 0x00, 0x00, /* length in blocks */
    0x00, 0x00, 0x00, 0x00, /* burst */
    0x00, 0x00, 0x00, 0x00, /* gap */
    'f', 'o', 'l', 'd', 'e', 'r', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* filename */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0x00, 0x00, 0x00, 0x00, /* extra copies */
    0x00, 0x00, 0x00, 0x00, /* directory block address */

    0x00, 0x00, 0x00, 0x02, /* flags - file */
    0x00, 0x00, 0x00, 0x00, /* identifier */
    0x00, 0x00, 0x00, 0x00, /* type */
    0x00, 0x00, 0x08, 0x00, /* block size */
    0x00, 0x00, 0x00, 0x00, /* length in bytes */
    0x00, 0x00, 0x00, 0x00, /* length in blocks */
    0x00, 0x00, 0x00, 0x00, /* burst */
    0x00, 0x00, 0x00, 0x00, /* gap */
    'L', 'a', 'u', 'n', 'c', 'h', 'M', 'e', 0, 0, 0, 0, 0, 0, 0, 0, /* filename */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0x00, 0x00, 0x00, 0x00, /* extra copies */
    0x00, 0x00, 0x00, 0x02, /* directory block address */
  };

  size_t size_needed = (root_directory_sectors + 1 + ((binary_size + 2047) / 2048)) * 2048;
  uint8_t* image = (uint8_t*)calloc(size_needed, 1);
  size_t offset = 2048;
  unsigned i;

  /* first sector - volume header */
  memcpy(image, volume_header, sizeof(volume_header));
  image[0x5B] = (uint8_t)root_directory_sectors;

  /* root directory sectors */
  for (i = 0; i < root_directory_sectors; ++i)
  {
    memcpy(&image[offset], directory_data, sizeof(directory_data));
    if (i < root_directory_sectors - 1)
    {
      image[offset + 0] = 0;
      image[offset + 1] = 0;
      image[offset + 2] = 0;
      image[offset + 3] = (uint8_t)(i + 1);

      memcpy(&image[offset + 0x14 + 0x48 + 0x20], "filename", 8);
    }
    else
    {
      image[offset + 0x14 + 0x48 + 0x11] = (binary_size >> 16);
      image[offset + 0x14 + 0x48 + 0x12] = (binary_size >> 8) & 0xFF;
      image[offset + 0x14 + 0x48 + 0x13] = (binary_size & 0xFF);

      image[offset + 0x14 + 0x48 + 0x16] = ((binary_size + 2047) / 2048) >> 8;
      image[offset + 0x14 + 0x48 + 0x17] = ((binary_size + 2047) / 2048) & 0xFF;

      image[offset + 0x14 + 0x48 + 0x47] = (uint8_t)(i + 2);
    }

    if (i > 0)
    {
      image[offset + 4] = 0;
      image[offset + 5] = 0;
      image[offset + 6] = 0;
      image[offset + 7] = (uint8_t)(i - 1);
    }

    offset += 2048;
  }

  /* binary data */
  fill_image(&image[offset], binary_size);

  *image_size = size_needed;
  return image;
}

uint8_t* generate_pce_cd_bin(unsigned binary_sectors, size_t* image_size)
{
  const uint8_t volume_header[] = {
    0x00, 0x00, 0x02,       /* first sector of boot code */
    0x14,                   /* number of sectors for boot code */
    0x00, 0x40,             /* program load address */
    0x00, 0x40,             /* program execute address  */
    0, 1, 2, 3, 4,          /* IPLMPR */
    0,                      /* open mode */
    0, 0, 0, 0, 0, 0,       /* GRPBLK and addr */
    0, 0, 0, 0, 0,          /* ADPBLK and rate */
    0, 0, 0, 0, 0, 0, 0,    /* reserved */
    'P', 'C', ' ', 'E', 'n', 'g', 'i', 'n', 'e', ' ', 'C', 'D', '-', 'R', 'O', 'M',
    ' ', 'S', 'Y', 'S', 'T', 'E', 'M', '\0', 'C', 'o', 'p', 'y', 'r', 'i', 'g', 'h',
    't', ' ', 'H', 'U', 'D', 'S', 'O', 'N', ' ', 'S', 'O', 'F', 'T', ' ', '/', ' ',
    'N', 'E', 'C', ' ', 'H', 'o', 'm', 'e', ' ', 'E', 'l', 'e', 'c', 't', 'r', 'o',
    'n', 'i', 'c', 's', ',', 'L', 't', 'd', '.', '\0', 'G', 'A', 'M', 'E', 'N', 'A',
    'M', 'E', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '
  };

  size_t size_needed = (binary_sectors + 2) * 2048;
  uint8_t* image = (uint8_t*)calloc(size_needed, 1);

  /* volume header goes in second sector */
  memcpy(&image[2048], volume_header, sizeof(volume_header));
  image[2048 + 0x03] = (uint8_t)binary_sectors;

  /* binary data */
  fill_image(&image[4096], binary_sectors * 2048);

  *image_size = size_needed;
  return image;
}

uint8_t* generate_pcfx_bin(unsigned binary_sectors, size_t* image_size)
{
  const uint8_t volume_header[] = {
    'G', 'A', 'M', 'E', 'N', 'A', 'M', 'E', 0, 0, 0, 0, 0, 0, 0, 0, /* title (32-bytes) */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0x02, 0x00, 0x00, 0x00, /* first sector of boot code */
    0x14, 0x00, 0x00, 0x00, /* number of sectors for boot code */
    0x00, 0x80, 0x00, 0x00, /* program load address */
    0x00, 0x80, 0x00, 0x00, /* program execute address  */
    'N', '/', 'A', '\0',    /* maker id */
    'r', 'c', 'h', 'e', 'e', 'v', 'o', 's', 't', 'e', 's', 't', 0, 0, 0, 0, /* maker name (60-bytes) */
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0x00, 0x00, 0x00, 0x00, /* volume number */
    0x00, 0x01,             /* version */
    0x01, 0x00,             /* country */
    '2', '0', '2', '0', 'X', 'X', 'X', 'X', /* date */
  };

  size_t size_needed = (binary_sectors + 2) * 2048;
  uint8_t* image = (uint8_t*)calloc(size_needed, 1);

  /* volume header goes in second sector */
  strcpy((char*)&image[0], "PC-FX:Hu_CD-ROM");
  memcpy(&image[2048], volume_header, sizeof(volume_header));
  image[2048 + 0x24] = (uint8_t)binary_sectors;

  /* binary data */
  fill_image(&image[4096], binary_sectors * 2048);

  *image_size = size_needed;
  return image;
}

uint8_t* generate_generic_file(size_t size)
{
  uint8_t* image;
  image = (uint8_t*)calloc(size, 1);
  if (image != NULL)
    fill_image(image, size);

  return image;
}
