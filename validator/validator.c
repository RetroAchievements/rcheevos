#include "internal.h"
#include "rurl.h"

#include <stdio.h>
#include <stddef.h>
#include <malloc.h>
#include <string.h> /* memset */

#ifdef _CRT_SECURE_NO_WARNINGS /* windows build*/
 #define WIN32_LEAN_AND_MEAN
 #include <windows.h>
#else
 #include <dirent.h>
 #include <strings.h>
 #define stricmp strcasecmp
#endif

/* usage exmaple:
 *
 * ./validator.exe d "E:\RetroAchievements\dump" | sort > results.txt
 * grep -v ": OK" results.txt  | grep -v "File: " | grep .
 */

static void validate_trigger(const char* trigger) {
  char* buffer;
  rc_trigger_t* compiled;

  int ret = rc_trigger_size(trigger);
  if (ret < 0) {
    printf("%s", rc_error_str(ret));
    return;
  }

  buffer = (char*)malloc(ret + 4);
  memset(buffer + ret, 0xCD, 4);
  compiled = rc_parse_trigger(buffer, trigger, NULL, 0);
  if (compiled == NULL) {
    printf("parse failed");
    free(buffer);
    return;
  }

  if (*(unsigned*)&buffer[ret] != 0xCDCDCDCD) {
    printf("write past end of buffer");
    free(buffer);
    return;
  }

  printf("OK");
  free(buffer);
}

static void validate_leaderboard(const char* leaderboard)
{
  char* buffer;
  rc_lboard_t* compiled;

  int ret = rc_lboard_size(leaderboard);
  if (ret < 0) {
    printf("%s", rc_error_str(ret));
    return;
  }

  buffer = (char*)malloc(ret + 4);
  memset(buffer + ret, 0xCD, 4);
  compiled = rc_parse_lboard(buffer, leaderboard, NULL, 0);
  if (compiled == NULL) {
    printf("parse failed");
    free(buffer);
    return;
  }

  if (*(unsigned*)&buffer[ret] != 0xCDCDCDCD) {
    printf("write past end of buffer");
    free(buffer);
    return;
  }

  printf("OK");
  free(buffer);
}

static void validate_richpresence(const char* script)
{
  char* buffer;
  rc_richpresence_t* compiled;

  int ret = rc_richpresence_size(script);
  if (ret < 0) {
    printf("%s", rc_error_str(ret));
    return;
  }

  buffer = (char*)malloc(ret + 4);
  memset(buffer + ret, 0xCD, 4);
  compiled = rc_parse_richpresence(buffer, script, NULL, 0);
  if (compiled == NULL) {
    printf("parse failed");
    free(buffer);
    return;
  }

  if (*(unsigned*)&buffer[ret] != 0xCDCDCDCD) {
    printf("write past end of buffer");
    free(buffer);
    return;
  }

  printf("OK");
  free(buffer);
}

static void validate_richpresence_file(const char* richpresence_file)
{
  char* file_contents;
  size_t file_size;
  FILE* file;

  file = fopen(richpresence_file, "rb");
  if (!file) {
    printf("could not open file");
    return;
  }

  fseek(file, 0, SEEK_END);
  file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  file_contents = (char*)malloc(file_size + 1);
  fread(file_contents, 1, file_size, file);
  file_contents[file_size] = '\0';
  fclose(file);

  validate_richpresence(file_contents);

  free(file_contents);
}

static char* get_json_value(char* start, const char* id, char** next)
{
  char* ptr = start;
  char* value;
  size_t id_len = strlen(id);

  while (*ptr) {
    if (*ptr == '"' && ptr[id_len + 1] == '"' && memcmp(ptr + 1, id, id_len) == 0) {
      ptr += id_len + 3; /* skip over id and colon */
      if (*ptr == '"') {
        char* copy = ++ptr;
        value = copy;
        while (*ptr && *ptr != '"') {
          if (*ptr != '\\') {
            *copy++ = *ptr++;
          } else {
            ++ptr;
            switch (*ptr) {
              case 'r': *copy++ = '\r'; break;
              case 'n': *copy++ = '\n'; break;
              case 't': *copy++ = '\t'; break;
              default: *copy++ = *ptr; break;
            }
            ++ptr;
          }
        }
        if (*ptr)
          *ptr++ = '\0';
        *copy = '\0';
      } else if (*ptr == '[') {
        if (next)
          *next = ptr;
        return ptr;
      } else {
        value = ptr;
        while (*ptr && *ptr != ',' && *ptr != '}')
          ++ptr;
        if (*ptr)
          *ptr++ = '\0';
      }

      if (next)
        *next = ptr;

      return value;
    }

    ++ptr;
  }

  return NULL;
}

static void validate_patchdata_file(const char* patchdata_file) {
  char* file_contents;
  size_t file_size;
  FILE* file;
  char* ptr, *value, *lboard_start, *ach_start, *game_id;

  file = fopen(patchdata_file, "rb");
  if (!file) {
    printf("could not open file");
    return;
  }

  fseek(file, 0, SEEK_END);
  file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  file_contents = (char*)malloc(file_size + 1);
  fread(file_contents, 1, file_size, file);
  file_contents[file_size] = '\0';
  fclose(file);

  /* this makes assumptions about the layout of the JSON */
  game_id = get_json_value(file_contents, "ID", &ptr);
  if (game_id == NULL) {
    printf("not a patchdata file");
    return;
  }

  value = get_json_value(ptr, "Title", &ptr);
  printf("%s\n", value);

  value = get_json_value(ptr, "RichPresencePatch", &ptr);
  if (value && *value && strcmp(value, "null") != 0) {
    printf(" rich presence %s: ", game_id);
    validate_richpresence(value);
    printf("\n");
  }

  lboard_start = get_json_value(ptr, "Leaderboards", NULL);
  if (lboard_start)
    lboard_start[-1] = '\0';

  ach_start = get_json_value(ptr, "Achievements", NULL);
  if (ach_start) {
    do {
      value = get_json_value(ptr, "ID", &ptr);
      if (!value)
        break;

      printf(" achievement %s: ", value);
      value = get_json_value(ptr, "MemAddr", &ptr);
      validate_trigger(value);
      printf("\n");
    } while (1);
  }

  if (lboard_start) {
    ptr = lboard_start;
    do {
      value = get_json_value(ptr, "ID", &ptr);
      if (!value)
        break;

      printf(" leaderboard %s: ", value);
      value = get_json_value(ptr, "Mem", &ptr);
      validate_leaderboard(value);
      printf("\n");
    } while (1);
  }

  free(file_contents);
}

#ifdef _CRT_SECURE_NO_WARNINGS
static void validate_pathdata_directory(const char* patchdata_directory) {
  WIN32_FIND_DATA fdFile;
  HANDLE hFind = NULL;

  char filename[MAX_PATH];
  sprintf(filename, "%s\\*.json", patchdata_directory);

  if ((hFind = FindFirstFile(filename, &fdFile)) == INVALID_HANDLE_VALUE) {
    printf("failed to open directory");
    return;
  }

  do
  {
    sprintf(filename, "%s\\%s", patchdata_directory, fdFile.cFileName);

    if (!(fdFile.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
      printf("File: %s: ", fdFile.cFileName);
      validate_patchdata_file(filename);
      printf("\n");
    }
  } while(FindNextFile(hFind, &fdFile));

  FindClose(hFind);
}
#else
static void validate_pathdata_directory(const char* patchdata_directory) {
  struct dirent* entry;
  char* filename;
  size_t filename_len;
  char path[2048];

  DIR* dir = opendir(patchdata_directory);
  if (!dir) {
    printf("failed to open directory");
    return;
  }

  while ((entry = readdir(dir)) != NULL) {
    filename = entry->d_name;
    filename_len = strlen(filename);
    if (filename_len > 5 && stricmp(&filename[filename_len - 5], ".json") == 0) {
      sprintf(path, "%s/%s", patchdata_directory, filename);
      printf("File: %s: ", filename);
      validate_patchdata_file(path);
      printf("\n");
    }
  }

  closedir(dir);
}
#endif

static int usage() {
  printf("validator [type] [data]\n"
         "\n"
         "where [type] is one of the following:\n"
         "  a   achievement, [data] = trigger definition\n"
         "  l   leaderboard, [data] = leaderboard definition\n"
         "  r   rich presence, [data] = path to rich presence script\n"
         "  f   patchdata file, [data] = path to patchdata json file\n"
         "  d   patchdata directory, [data] = path to directory containing one or more patchdata json files\n"
  );

  return 0;
}

int main(int argc, char* argv[]) {

  if (argc < 3)
    return usage();

  switch (argv[1][0])
  {
    case 'a':
      printf("Achievement: ");
      validate_trigger(argv[2]);
      break;

    case 'l':
      printf("Leaderboard: ");
      validate_leaderboard(argv[2]);
      break;

    case 'r':
      printf("Rich Presence: ");
      validate_richpresence_file(argv[2]);
      break;

    case 'f':
      printf("File: %s: ", argv[2]);
      validate_patchdata_file(argv[2]);
      break;

    case 'd':
      printf("Directory: %s: ", argv[2]);
      validate_pathdata_directory(argv[2]);
      break;

    default:
      return usage();
  }

  printf("\n");
  return 0;
}
