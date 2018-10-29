#include "internal.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

enum {
  RC_FORMAT_STRING = RC_FORMAT_OTHER + 1,
  RC_FORMAT_LOOKUP = RC_FORMAT_OTHER + 2
};

const char* rc_parse_line(const char* line, const char** end) {
  const char* nextline;
  const char* endline;

  /* get a single line */
  nextline = line;
  while (*nextline && *nextline != '\n')
    ++nextline;

  /* find a trailing comment marker (//) */
  endline = line;
  while (endline < nextline && (endline[0] != '/' || endline[1] != '/' || (endline > line && endline[-1] == '\\')))
    ++endline;

  /* remove trailing whitespace */
  if (endline == nextline) {
    if (endline > line && endline[-1] == '\r')
      --endline;
  } else {
    while (endline > line && isspace(endline[-1]))
      --endline;
  }

  /* end is pointing at the first character to ignore - makes subtraction for length easier */
  *end = endline;

  if (*nextline == '\n')
    ++nextline;
  return nextline;
}

rc_richpresence_display_t* rc_parse_richpresence_display_internal(void* buffer, int* ret, rc_scratch_t* scratch, const char* line, const char* endline, lua_State* L, int funcs_ndx, rc_richpresence_t* richpresence) {
  rc_richpresence_display_t* self;
  rc_richpresence_display_part_t* part;
  rc_richpresence_display_part_t** next;
  rc_richpresence_lookup_t* lookup;
  const char* ptr;
  const char* in;
  char* out;

  if (endline - line < 1)
    return 0;

  {
    self = RC_ALLOC(rc_richpresence_display_t, buffer, ret, scratch);
    memset(self, 0, sizeof(rc_richpresence_display_t));
    next = &self->display;
  }

  /* break the string up on macros: text @macro() moretext */
  do {
    ptr = line;
    while (ptr < endline) {
      if (*ptr == '@' && (ptr == line || ptr[-1] != '\\')) /* ignore escaped @s */
        break;

      ++ptr;
    }

    if (ptr > line) {
      part = RC_ALLOC(rc_richpresence_display_part_t, buffer, ret, scratch);
      memset(part, 0, sizeof(rc_richpresence_display_part_t));
      *next = part;
      next = &part->next;

      /* handle string part */
      part->display_type = RC_FORMAT_STRING;
      part->text = rc_alloc_str(buffer, ret, line, ptr - line);
      if (part->text) {
        /* remove backslashes used for escaping */
        in = part->text;
        while (*in && *in != '\\')
          ++in;

        if (*in == '\\') {
          out = (char*)in++;
          while (*in) {
            *out++ = *in++;
            if (*in == '\\')
              ++in;
          }
          *out = '\0';
        }
      }
    }

    if (*ptr == '@') {
      /* handle macro part */
      line = ++ptr;
      while (ptr < endline && *ptr != '(')
        ++ptr;

      if (ptr > line) {
        if (!buffer) {
          /* just calculating size, can't confirm lookup exists */
          part = RC_ALLOC(rc_richpresence_display_part_t, buffer, ret, scratch);

          line = ++ptr;
          while (ptr < endline && *ptr != ')')
            ++ptr;
          if (*ptr == ')') {
            rc_parse_value_internal(&part->value, ret, buffer, scratch, &line, L, funcs_ndx);
            if (ret < 0)
              return 0;
            ++ptr;
          }

        } else {
          /* find the lookup and hook it up */
          lookup = richpresence->first_lookup;
          while (lookup) {
            if (strncmp(lookup->name, line, ptr - line) == 0 && lookup->name[ptr - line] == '\0') {
              part = RC_ALLOC(rc_richpresence_display_part_t, buffer, ret, scratch);
              memset(part, 0, sizeof(rc_richpresence_display_part_t));
              *next = part;
              next = &part->next;

              part->text = lookup->name;
              part->display_type = lookup->format;

              line = ++ptr;
              while (ptr < endline && *ptr != ')')
                ++ptr;
              if (*ptr == ')') {
                rc_parse_value_internal(&part->value, ret, buffer, scratch, &line, L, funcs_ndx);
                if (ret < 0)
                  return 0;
                ++ptr;
              }

              break;
            }

            lookup = lookup->next;
          }

          if (!lookup) {
            part = RC_ALLOC(rc_richpresence_display_part_t, buffer, ret, scratch);
            memset(part, 0, sizeof(rc_richpresence_display_part_t));
            *next = part;
            next = &part->next;

            ptr = line;

            part->display_type = RC_FORMAT_STRING;
            part->text = rc_alloc_str(buffer, ret, "[Unknown macro]", 15);
          }
        }
      }
    }

    line = ptr;
  } while (line < endline);

  return self;
}

void rc_parse_richpresence_internal(rc_richpresence_t* self, int* ret, void* buffer, void* scratch, const char* script, lua_State* L, int funcs_ndx) {
  rc_richpresence_display_t** nextdisplay;
  rc_richpresence_lookup_t** nextlookup;
  rc_richpresence_lookup_t* lookup;
  char format[64];
  const char* display = 0;
  const char* line;
  const char* nextline;
  const char* endline;
  const char* ptr;
  int hasdisplay = 0;
  int chars;

  nextdisplay = &self->first_display;
  nextlookup = &self->first_lookup;
  *nextlookup = 0;

  /* first pass: process macro initializers */
  line = script;
  while (*line)
  {
    nextline = rc_parse_line(line, &endline);
    if (strncmp(line, "Lookup:", 7) == 0) {

    } else if (strncmp(line, "Format:", 7) == 0) {
      line += 7;

      lookup = RC_ALLOC(rc_richpresence_lookup_t, buffer, ret, scratch);
      lookup->name = rc_alloc_str(buffer, ret, line, endline - line);
      lookup->first_item = 0;
      lookup->next = 0;
      *nextlookup = lookup;
      nextlookup = &lookup->next;

      line = nextline;
      nextline = rc_parse_line(line, &endline);
      if (buffer && strncmp(line, "FormatType=", 11) == 0) {
        line += 11;

        chars = endline - line;
        if (chars > 63)
          chars = 63;
        memcpy(format, line, chars);
        format[chars] = '\0';

        lookup->format = rc_parse_format(format);
      } else {
        lookup->format = RC_FORMAT_VALUE;
      }
    } else if (strncmp(line, "Display:", 8) == 0) {
      display = nextline;

      do {
        line = nextline;
        nextline = rc_parse_line(line, &endline);
      } while (*line == '?');
    }

    line = nextline;
  }

  /* second pass, process display string*/
  if (display) {
    line = display;
    nextline = rc_parse_line(line, &endline);

    while (*line == '?') {
      /* conditional display: ?trigger?string */
      ptr = ++line;
      while (ptr < endline && *ptr != '?')
        ++ptr;

      if (ptr < endline) {
        *nextdisplay = rc_parse_richpresence_display_internal(buffer, ret, scratch, ptr + 1, endline, L, funcs_ndx, self);
        rc_parse_trigger_internal(&((*nextdisplay)->trigger), ret, buffer, scratch, &line, L, funcs_ndx);
        if (*ret < 0)
          return;
        if (buffer)
          nextdisplay = &((*nextdisplay)->next);
      }

      line = nextline;
      nextline = rc_parse_line(line, &endline);
    }

    /* non-conditional display: string */
    *nextdisplay = rc_parse_richpresence_display_internal(buffer, ret, scratch, line, endline, L, funcs_ndx, self);
    hasdisplay = (*nextdisplay != NULL);
    if (buffer)
      nextdisplay = &((*nextdisplay)->next);
  }

  /* finalize */
  *nextdisplay = 0;

  if (!hasdisplay && ret > 0)
    *ret = RC_MISSING_DISPLAY_STRING;
}

int rc_richpresence_size(const char* script) {
  int ret;
  rc_richpresence_t* self;
  rc_scratch_t scratch;

  ret = 0;
  self = RC_ALLOC(rc_richpresence_t, 0, &ret, &scratch);
  rc_parse_richpresence_internal(self, &ret, 0, &scratch, script, 0, 0);
  return ret;
}

rc_richpresence_t* rc_parse_richpresence(void* buffer, const char* script, lua_State* L, int funcs_ndx) {
  int ret;
  rc_richpresence_t* self;
  rc_scratch_t scratch;

  ret = 0;
  self = RC_ALLOC(rc_richpresence_t, buffer, &ret, &scratch);
  rc_parse_richpresence_internal(self, &ret, buffer, 0, script, L, funcs_ndx);
  return ret >= 0 ? self : 0;
}

int rc_evaluate_richpresence(rc_richpresence_t* richpresence, char* buffer, unsigned buffersize, rc_peek_t peek, void* peek_ud, lua_State* L) {
  rc_richpresence_display_t* display;
  rc_richpresence_display_part_t* part;
  char* ptr;
  int chars;
  unsigned value;

  ptr = buffer;
  display = richpresence->first_display;
  while (display) {
    if (!display->next || rc_test_trigger(&display->trigger, peek, peek_ud, L)) {
      part = display->display;
      while (part) {
        switch (part->display_type) {
          case RC_FORMAT_STRING:
            chars = snprintf(ptr, buffersize, "%s", part->text);
            break;

          default:
            value = rc_evaluate_value(&part->value, peek, peek_ud, L);
            chars = rc_format_value(ptr, buffersize, value, part->display_type);
            break;
        }

        if (chars > 0) {
          ptr += chars;
          buffersize -= chars;
        }

        part = part->next;
      }

      return (ptr - buffer);
    }

    display = display->next;
  }

  buffer[0] = '\0';
  return 0;
}
