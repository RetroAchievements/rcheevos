#include "rconsoles.h"
#include "rcheevos.h"

#include "../test_framework.h"

static void test_name(int console_id, const char* expected_name)
{
  ASSERT_STR_EQUALS(rc_console_name(console_id), expected_name);
}

static void test_memory(int console_id, unsigned expected_total_memory)
{
  const rc_memory_regions_t* regions = rc_console_memory_regions(console_id);
  unsigned total_memory = 0;
  unsigned max_address = 0;
  unsigned i;
  ASSERT_PTR_NOT_NULL(regions);

  if (expected_total_memory == 0)
  {
    ASSERT_NUM_EQUALS(regions->num_regions, 0);
    return;
  }

  ASSERT_NUM_GREATER(regions->num_regions, 0);
  for (i = 0; i < regions->num_regions; ++i) {
    total_memory += (regions->region[i].end_address - regions->region[i].start_address + 1);
    if (regions->region[i].end_address > max_address)
      max_address = regions->region[i].end_address;

    ASSERT_PTR_NOT_NULL(regions->region[i].description);
  }

  ASSERT_NUM_EQUALS(total_memory, expected_total_memory);
  ASSERT_NUM_EQUALS(max_address, expected_total_memory - 1);
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
  TEST_PARAMS2(test_name, 23, "Unknown");
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

  TEST_PARAMS2(test_name, 100, "Hubs");
  TEST_PARAMS2(test_name, 101, "Events");

  /* memory maps */
  TEST_PARAMS2(test_memory, RC_CONSOLE_3DO, 0x200000);
  TEST_PARAMS2(test_memory, RC_CONSOLE_APPLE_II, 0x020000);
  TEST_PARAMS2(test_memory, RC_CONSOLE_ARCADE, 0x000000);
  TEST_PARAMS2(test_memory, RC_CONSOLE_ATARI_2600, 0x000080);
  TEST_PARAMS2(test_memory, RC_CONSOLE_ATARI_7800, 0x010000);
  TEST_PARAMS2(test_memory, RC_CONSOLE_ATARI_JAGUAR, 0x200000);
  TEST_PARAMS2(test_memory, RC_CONSOLE_ATARI_LYNX, 0x010000);
  TEST_PARAMS2(test_memory, RC_CONSOLE_COLECOVISION, 0x000400);
  TEST_PARAMS2(test_memory, RC_CONSOLE_GAMEBOY, 0x010000);
  TEST_PARAMS2(test_memory, RC_CONSOLE_GAMEBOY_COLOR, 0x016000);
  TEST_PARAMS2(test_memory, RC_CONSOLE_GAMEBOY_ADVANCE, 0x048000);
  TEST_PARAMS2(test_memory, RC_CONSOLE_GAME_GEAR, 0x002000);
  TEST_PARAMS2(test_memory, RC_CONSOLE_INTELLIVISION, 0x010000);
  TEST_PARAMS2(test_memory, RC_CONSOLE_MASTER_SYSTEM, 0x002000);
  TEST_PARAMS2(test_memory, RC_CONSOLE_MEGA_DRIVE, 0x020000);
  TEST_PARAMS2(test_memory, RC_CONSOLE_MSX, 0x080000);
  TEST_PARAMS2(test_memory, RC_CONSOLE_NEOGEO_POCKET, 0x004000);
  TEST_PARAMS2(test_memory, RC_CONSOLE_NINTENDO, 0x010000);
  TEST_PARAMS2(test_memory, RC_CONSOLE_NINTENDO_64, 0x800000);
  TEST_PARAMS2(test_memory, RC_CONSOLE_NINTENDO_DS, 0x400000);
  TEST_PARAMS2(test_memory, RC_CONSOLE_ORIC, 0x010000);
  TEST_PARAMS2(test_memory, RC_CONSOLE_PC8800, 0x011000);
  TEST_PARAMS2(test_memory, RC_CONSOLE_PC_ENGINE, 0x42800);
  TEST_PARAMS2(test_memory, RC_CONSOLE_PLAYSTATION, 0x200000);
  TEST_PARAMS2(test_memory, RC_CONSOLE_POKEMON_MINI, 0x002000);
  TEST_PARAMS2(test_memory, RC_CONSOLE_SATURN, 0x200000);
  TEST_PARAMS2(test_memory, RC_CONSOLE_SEGA_32X, 0x020000);
  TEST_PARAMS2(test_memory, RC_CONSOLE_SEGA_CD, 0x090000);
  TEST_PARAMS2(test_memory, RC_CONSOLE_SG1000, 0x000400);
  TEST_PARAMS2(test_memory, RC_CONSOLE_SUPER_CASSETTEVISION, 0x010000);
  TEST_PARAMS2(test_memory, RC_CONSOLE_SUPER_NINTENDO, 0x040000);
  TEST_PARAMS2(test_memory, RC_CONSOLE_WONDERSWAN, 0x020000);
  TEST_PARAMS2(test_memory, RC_CONSOLE_VECTREX, 0x000400);
  TEST_PARAMS2(test_memory, RC_CONSOLE_VIRTUAL_BOY, 0x020000);

  TEST_SUITE_END();
}
