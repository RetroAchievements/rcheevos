#include "rconsoles.h"

#include "../test_framework.h"

static void test_name(int console_id, const char* expected_name)
{
  ASSERT_STR_EQUALS(rc_console_name(console_id), expected_name);
}

void test_consoleinfo(void) {
  TEST_SUITE_BEGIN();

  /* use raw numbers instead of constants to ensure constants don't change */
  TEST_PARAMS2(test_name,  0, "Unknown");
  TEST_PARAMS2(test_name,  1, "Sega Genesis");
  TEST_PARAMS2(test_name,  2, "Nintendo 64");
  TEST_PARAMS2(test_name,  3, "Super Nintendo Entertainment System");
  TEST_PARAMS2(test_name,  4, "GameBoy");
  TEST_PARAMS2(test_name,  5, "GameBoy Advance");
  TEST_PARAMS2(test_name,  6, "GameBoy Color");
  TEST_PARAMS2(test_name,  7, "Nintendo Entertainment System");
  TEST_PARAMS2(test_name,  8, "PCEngine");
  TEST_PARAMS2(test_name,  9, "Sega CD");
  TEST_PARAMS2(test_name, 10, "Sega 32X");
  TEST_PARAMS2(test_name, 11, "Master System");
  TEST_PARAMS2(test_name, 12, "PlayStation");
  TEST_PARAMS2(test_name, 13, "Atari Lynx");
  TEST_PARAMS2(test_name, 14, "Neo Geo Pocket");
  TEST_PARAMS2(test_name, 15, "Game Gear");
  TEST_PARAMS2(test_name, 16, "GameCube");
  TEST_PARAMS2(test_name, 17, "Atari Jaguar");
  TEST_PARAMS2(test_name, 18, "Nintendo DS");
  TEST_PARAMS2(test_name, 19, "Wii");
  TEST_PARAMS2(test_name, 20, "Wii-U");
  TEST_PARAMS2(test_name, 21, "PlayStation 2");
  TEST_PARAMS2(test_name, 22, "XBOX");
  TEST_PARAMS2(test_name, 23, "Events");
  TEST_PARAMS2(test_name, 24, "Pokemon Mini");
  TEST_PARAMS2(test_name, 25, "Atari 2600");
  TEST_PARAMS2(test_name, 26, "MS-DOS");
  TEST_PARAMS2(test_name, 27, "Arcade");
  TEST_PARAMS2(test_name, 28, "Virtual Boy");
  TEST_PARAMS2(test_name, 29, "MSX");
  TEST_PARAMS2(test_name, 30, "Commodore 64");
  TEST_PARAMS2(test_name, 31, "ZX-81");
  TEST_PARAMS2(test_name, 32, "Oric");
  TEST_PARAMS2(test_name, 33, "SG-1000");
  TEST_PARAMS2(test_name, 34, "VIC-20");
  TEST_PARAMS2(test_name, 35, "Amiga");
  TEST_PARAMS2(test_name, 36, "Amiga ST");
  TEST_PARAMS2(test_name, 37, "Amstrad CPC");
  TEST_PARAMS2(test_name, 38, "Apple II");
  TEST_PARAMS2(test_name, 39, "Sega Saturn");
  TEST_PARAMS2(test_name, 40, "Dreamcast");
  TEST_PARAMS2(test_name, 41, "PlayStation Portable");
  TEST_PARAMS2(test_name, 42, "CD-I");
  TEST_PARAMS2(test_name, 43, "3DO");
  TEST_PARAMS2(test_name, 44, "ColecoVision");
  TEST_PARAMS2(test_name, 45, "Intellivision");
  TEST_PARAMS2(test_name, 46, "Vectrex");
  TEST_PARAMS2(test_name, 47, "PC-8000/8800");
  TEST_PARAMS2(test_name, 48, "PC-9800");
  TEST_PARAMS2(test_name, 49, "PCFX");
  TEST_PARAMS2(test_name, 50, "Atari 5200");
  TEST_PARAMS2(test_name, 51, "Atari 7800");
  TEST_PARAMS2(test_name, 52, "X68K");
  TEST_PARAMS2(test_name, 53, "WonderSwan");
  TEST_PARAMS2(test_name, 54, "CassetteVision");
  TEST_PARAMS2(test_name, 55, "Super CassetteVision");
  TEST_PARAMS2(test_name, 56, "Unknown");

  TEST_SUITE_END();
}
