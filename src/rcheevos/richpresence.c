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

rc_richpresence_display_t* rc_parse_richpresence_display_internal(void* buffer, int* ret, rc_scratch_t* scratch, const char* line, const char* endline) {
  rc_richpresence_display_t* self;
  rc_richpresence_display_part_t* part;
  char* in;
  char* out;

  {
    self = RC_ALLOC(rc_richpresence_display_t, buffer, ret, scratch);
    memset(self, 0, sizeof(rc_richpresence_display_t));
  }

  {
    self->display = part = RC_ALLOC(rc_richpresence_display_part_t, buffer, ret, scratch);
    memset(part, 0, sizeof(rc_richpresence_display_part_t));
  }

  part->display_type = RC_FORMAT_STRING;
  part->text = rc_alloc_str(buffer, ret, line, endline - line);
  if (part->text) {
    /* remove backslashes used for escaping */
    in = part->text;
    while (*in && *in != '\\')
      ++in;

    if (*in == '\\') {
      out = in++;
      while (*in) {
        *out++ = *in++;
        if (*in == '\\')
          ++in;
      }
      *out = '\0';
    }
  }

  return self;
}

void rc_parse_richpresence_internal(rc_richpresence_t* self, int* ret, void* buffer, void* scratch, const char* script, lua_State* L, int funcs_ndx) {
  rc_richpresence_display_t** nextdisplay;
  rc_richpresence_lookup_t** nextlookup;
  const char* nextline;
  const char* endline;
  const char* ptr;

  nextdisplay = &self->first_display;
  *nextdisplay = 0;
  nextlookup = &self->first_lookup;
  *nextlookup = 0;

  const char* line = script;
  while (*line)
  {
    nextline = rc_parse_line(line, &endline);
    if (strncmp(line, "Lookup:", 7) == 0) {

    } else if (strncmp(line, "Format:", 7) == 0) {

    } else if (strncmp(line, "Display:", 8) == 0) {
      line = nextline;
      nextline = rc_parse_line(line, &endline);

      while (*line == '?') {
        /* conditional display: ?trigger?string */
        ptr = ++line;
        while (ptr < endline && *ptr != '?')
          ++ptr;

        if (ptr < endline) {
          *nextdisplay = rc_parse_richpresence_display_internal(buffer, ret, scratch, ptr + 1, endline);
          rc_parse_trigger_internal(&((*nextdisplay)->trigger), ret, buffer, scratch, &line, L, funcs_ndx);
          if (buffer)
            nextdisplay = &((*nextdisplay)->next);
        }

        line = nextline;
        nextline = rc_parse_line(line, &endline);
      }

      /* non-conditional display: string */
      *nextdisplay = rc_parse_richpresence_display_internal(buffer, ret, scratch, line, endline);
      if (buffer)
        nextdisplay = &((*nextdisplay)->next);
    }

    line = nextline;
  }
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
