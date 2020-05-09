#ifndef RCONSOLES_H
#define RCONSOLES_H

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************\
| Console identifiers                                                         |
\*****************************************************************************/

enum {
  RC_CONSOLE_MEGA_DRIVE = 1,
  RC_CONSOLE_NINTENDO_64 = 2,
  RC_CONSOLE_SUPER_NINTENDO = 3,
  RC_CONSOLE_GAMEBOY = 4,
  RC_CONSOLE_GAMEBOY_ADVANCE = 5,
  RC_CONSOLE_GAMEBOY_COLOR = 6,
  RC_CONSOLE_NINTENDO = 7,
  RC_CONSOLE_PC_ENGINE = 8,
  RC_CONSOLE_SEGA_CD = 9,
  RC_CONSOLE_SEGA_32X = 10,
  RC_CONSOLE_MASTER_SYSTEM = 11,
  RC_CONSOLE_PLAYSTATION = 12,
  RC_CONSOLE_ATARI_LYNX = 13,
  RC_CONSOLE_NEOGEO_POCKET = 14,
  RC_CONSOLE_GAME_GEAR = 15,
  RC_CONSOLE_GAMECUBE = 16,
  RC_CONSOLE_ATARI_JAGUAR = 17,
  RC_CONSOLE_NINTENDO_DS = 18,
  RC_CONSOLE_WII = 19,
  RC_CONSOLE_WII_U = 20,
  RC_CONSOLE_PLAYSTATION_2 = 21,
  RC_CONSOLE_XBOX = 22,
  /* 23 used to be EVENTS */
  RC_CONSOLE_POKEMON_MINI = 24,
  RC_CONSOLE_ATARI_2600 = 25,
  RC_CONSOLE_MS_DOS = 26,
  RC_CONSOLE_ARCADE = 27,
  RC_CONSOLE_VIRTUAL_BOY = 28,
  RC_CONSOLE_MSX = 29,
  RC_CONSOLE_COMMODORE_64 = 30,
  RC_CONSOLE_ZX81 = 31,
  RC_CONSOLE_ORIC = 32,
  RC_CONSOLE_SG1000 = 33,
  RC_CONSOLE_VIC20 = 34,
  RC_CONSOLE_AMIGA = 35,
  RC_CONSOLE_AMIGA_ST = 36,
  RC_CONSOLE_AMSTRAD_PC = 37,
  RC_CONSOLE_APPLE_II = 38,
  RC_CONSOLE_SATURN = 39,
  RC_CONSOLE_DREAMCAST = 40,
  RC_CONSOLE_PSP = 41,
  RC_CONSOLE_CDI = 42,
  RC_CONSOLE_3DO = 43,
  RC_CONSOLE_COLECOVISION = 44,
  RC_CONSOLE_INTELLIVISION = 45,
  RC_CONSOLE_VECTREX = 46,
  RC_CONSOLE_PC8800 = 47,
  RC_CONSOLE_PC9800 = 48,
  RC_CONSOLE_PCFX = 49,
  RC_CONSOLE_ATARI_5200 = 50,
  RC_CONSOLE_ATARI_7800 = 51,
  RC_CONSOLE_X68K = 52,
  RC_CONSOLE_WONDERSWAN = 53,
  RC_CONSOLE_CASSETTEVISION = 54,
  RC_CONSOLE_SUPER_CASSETTEVISION = 55,

  RC_CONSOLE_HUBS = 100,
  RC_CONSOLE_EVENTS = 101
};

const char* rc_console_name(int console_id);

#ifdef __cplusplus
}
#endif

#endif /* RCONSOLES_H */
