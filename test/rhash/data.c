#include "data.h"

#include <stdlib.h>

static void fill_image(uint8_t* image, size_t size)
{
  int seed = size ^ (size >> 8) ^ ((size - 1) * 25387);
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

uint8_t* generate_generic_file(size_t size)
{
  uint8_t* image;
  image = (uint8_t*)calloc(size, 1);
  if (image != NULL)
    fill_image(image, size);

  return image;
}
