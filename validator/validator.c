#include "rc_internal.h"
#include "rc_url.h"
#include "rc_api_runtime.h"

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

  printf("%d OK", ret);
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

  printf("%d OK", ret);
  free(buffer);
}

static void validate_richpresence(const char* script)
{
  char* buffer;
  rc_richpresence_t* compiled;
  int lines;

  int ret = rc_richpresence_size_lines(script, &lines);
  if (ret < 0) {
    printf("Line %d: %s", lines, rc_error_str(ret));
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

  printf("%d OK", ret);
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

static void validate_patchdata_file(const char* patchdata_file) {
  char* file_contents;
  size_t file_size;
  FILE* file;
  rc_api_fetch_game_data_response_t fetch_game_data_response;
  int result;
  size_t i;

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

  result = rc_api_process_fetch_game_data_response(&fetch_game_data_response, file_contents);
  if (result != RC_OK) {
    printf("%s", rc_error_str(result));
    return;
  }

  free(file_contents);

  printf("%s\n", fetch_game_data_response.title);

  if (fetch_game_data_response.rich_presence_script && *fetch_game_data_response.rich_presence_script) {
    printf(" rich presence %d: ", fetch_game_data_response.id);
    validate_richpresence(fetch_game_data_response.rich_presence_script);
    printf("\n");
  }

  for (i = 0; i < fetch_game_data_response.num_achievements; ++i) {
    printf(" achievement %d: ", fetch_game_data_response.achievements[i].id);
    validate_trigger(fetch_game_data_response.achievements[i].definition);
    printf("\n");
  }

  for (i = 0; i < fetch_game_data_response.num_leaderboards; ++i) {
    printf(" leaderboard %d: ", fetch_game_data_response.leaderboards[i].id);
    validate_leaderboard(fetch_game_data_response.leaderboards[i].definition);
    printf("\n");
  }

  rc_api_destroy_fetch_game_data_response(&fetch_game_data_response);
}

#ifdef _CRT_SECURE_NO_WARNINGS
static void validate_patchdata_directory(const char* patchdata_directory) {
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
static void validate_patchdata_directory(const char* patchdata_directory) {
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
      printf("Directory: %s:\n", argv[2]);
      validate_patchdata_directory(argv[2]);
      break;

    default:
      return usage();
  }

  printf("\n");
  return 0;
}
