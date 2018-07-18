#include "internal.h"

#include <stdio.h>

void rc_format_value(char* buffer, int size, unsigned value, int format) {
  unsigned a, b, c;

  switch (format) {
    case RC_FORMAT_FRAMES:
      a = value * 10 / 6; /* centisecs */
      b = a / 100; /* seconds */
      a -= b * 100;
      c = b / 60; /* minutes */
      b -= c * 60;
      snprintf(buffer, size, "%02u:%02u.%02u", c, b, a);
      break;

    case RC_FORMAT_SECONDS:
      a = value / 60; /* minutes */
      value -= a * 60;
      snprintf(buffer, size, "%02u:%02u", a, value);
      break;

    case RC_FORMAT_CENTISECS:
      a = value / 100; /* seconds */
      value -= a * 100;
      b = a / 60; /* minutes */
      a -= b * 60;
      snprintf(buffer, size, "%02u:%02u.%02u", b, a, value);
      break;

    case RC_FORMAT_SCORE:
      snprintf(buffer, size, "%06u Points", value);
      break;

    case RC_FORMAT_VALUE:
      snprintf(buffer, size, "%01u", value);
      break;

    case RC_FORMAT_OTHER:
    default:
      snprintf(buffer, size, "%06u", value);
  }
}
