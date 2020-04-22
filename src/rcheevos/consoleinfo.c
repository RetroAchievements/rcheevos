#include "rconsoles.h"

#include <ctype.h>

const char* rc_console_name(int console_id)
{
  switch (console_id)
  {
    case RC_CONSOLE_3DO:
      return "3DO";

    case RC_CONSOLE_AMIGA:
      return "Amiga";

    case RC_CONSOLE_AMIGA_ST:
      return "Amiga ST";

    case RC_CONSOLE_AMSTRAD_PC:
      return "Amstrad CPC";

    case RC_CONSOLE_APPLE_II:
      return "Apple II";

    case RC_CONSOLE_ARCADE:
      return "Arcade";

    case RC_CONSOLE_ATARI_2600:
      return "Atari 2600";

    case RC_CONSOLE_ATARI_5200:
      return "Atari 5200";

    case RC_CONSOLE_ATARI_7800:
      return "Atari 7800";

    case RC_CONSOLE_ATARI_JAGUAR:
      return "Atari Jaguar";

    case RC_CONSOLE_ATARI_LYNX:
      return "Atari Lynx";

    case RC_CONSOLE_CASSETTEVISION:
      return "CassetteVision";

    case RC_CONSOLE_CDI:
      return "CD-I";

    case RC_CONSOLE_COLECOVISION:
      return "ColecoVision";

    case RC_CONSOLE_COMMODORE_64:
      return "Commodore 64";

    case RC_CONSOLE_DREAMCAST:
      return "Dreamcast";

    case RC_CONSOLE_EVENTS:
      return "Events";

    case RC_CONSOLE_GAMEBOY:
      return "GameBoy";

    case RC_CONSOLE_GAMEBOY_ADVANCE:
      return "GameBoy Advance";

    case RC_CONSOLE_GAMEBOY_COLOR:
      return "GameBoy Color";

    case RC_CONSOLE_GAMECUBE:
      return "GameCube";

    case RC_CONSOLE_GAME_GEAR:
      return "Game Gear";

    case RC_CONSOLE_INTELLIVISION:
      return "Intellivision";

    case RC_CONSOLE_MASTER_SYSTEM:
      return "Master System";

    case RC_CONSOLE_MEGA_DRIVE:
      return "Sega Genesis";

    case RC_CONSOLE_MS_DOS:
      return "MS-DOS";

    case RC_CONSOLE_MSX:
      return "MSX";

    case RC_CONSOLE_NINTENDO:
      return "Nintendo Entertainment System";

    case RC_CONSOLE_NINTENDO_64:
      return "Nintendo 64";

    case RC_CONSOLE_NINTENDO_DS:
      return "Nintendo DS";

    case RC_CONSOLE_NEOGEO_POCKET:
      return "Neo Geo Pocket";

    case RC_CONSOLE_ORIC:
      return "Oric";

    case RC_CONSOLE_PC8800:
      return "PC-8000/8800";

    case RC_CONSOLE_PC9800:
      return "PC-9800";

    case RC_CONSOLE_PCFX:
      return "PCFX";

    case RC_CONSOLE_PC_ENGINE:
      return "PCEngine";

    case RC_CONSOLE_PLAYSTATION:
      return "PlayStation";

    case RC_CONSOLE_PLAYSTATION_2:
      return "PlayStation 2";

    case RC_CONSOLE_PSP:
      return "PlayStation Portable";

    case RC_CONSOLE_POKEMON_MINI:
      return "Pokemon Mini";

    case RC_CONSOLE_SEGA_32X:
      return "Sega 32X";

    case RC_CONSOLE_SEGA_CD:
      return "Sega CD";

    case RC_CONSOLE_SATURN:
      return "Sega Saturn";

    case RC_CONSOLE_SG1000:
      return "SG-1000";

    case RC_CONSOLE_SUPER_NINTENDO:
      return "Super Nintendo Entertainment System";

    case RC_CONSOLE_SUPER_CASSETTEVISION:
      return "Super CassetteVision";

    case RC_CONSOLE_WONDERSWAN:
      return "WonderSwan";

    case RC_CONSOLE_VECTREX:
      return "Vectrex";

    case RC_CONSOLE_VIC20:
      return "VIC-20";

    case RC_CONSOLE_VIRTUAL_BOY:
      return "Virtual Boy";

    case RC_CONSOLE_WII:
      return "Wii";

    case RC_CONSOLE_WII_U:
      return "Wii-U";

    case RC_CONSOLE_X68K:
      return "X68K";

    case RC_CONSOLE_XBOX:
      return "XBOX";

    case RC_CONSOLE_ZX81:
      return "ZX-81";

    default:
      return "Unknown";
  }
}
