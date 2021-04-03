#include "rc_libretro.h"

#include "rc_consoles.h"

#include "../test_framework.h"

static void* retro_memory_data[4] = { NULL, NULL, NULL, NULL };
static size_t retro_memory_size[4] = { 0, 0, 0, 0 };

static void libretro_get_core_memory_info(unsigned id, rc_libretro_core_memory_info_t* info)
{
  info->data = retro_memory_data[id];
  info->size = retro_memory_size[id];
}

static void test_allowed_setting(const char* library_name, const char* setting, const char* value) {
  const rc_disallowed_setting_t* settings = rc_libretro_get_disallowed_settings(library_name);
  if (!settings)
	return;

  ASSERT_TRUE(rc_libretro_is_setting_allowed(settings, setting, value));
}

static void test_disallowed_setting(const char* library_name, const char* setting, const char* value) {
  const rc_disallowed_setting_t* settings = rc_libretro_get_disallowed_settings(library_name);
  ASSERT_PTR_NOT_NULL(settings);
  ASSERT_FALSE(rc_libretro_is_setting_allowed(settings, setting, value));
}

static void test_memory_init_without_regions() {
  rc_libretro_memory_regions_t regions;
  unsigned char buffer1[16], buffer2[8];
  retro_memory_data[RETRO_MEMORY_SYSTEM_RAM] = buffer1;
  retro_memory_size[RETRO_MEMORY_SYSTEM_RAM] = sizeof(buffer1);
  retro_memory_data[RETRO_MEMORY_SAVE_RAM] = buffer2;
  retro_memory_size[RETRO_MEMORY_SAVE_RAM] = sizeof(buffer2);

  ASSERT_TRUE(rc_libretro_memory_init(&regions, NULL, libretro_get_core_memory_info, RC_CONSOLE_HUBS));

  ASSERT_NUM_EQUALS(regions.count, 2);
  ASSERT_NUM_EQUALS(regions.total_size, sizeof(buffer1) + sizeof(buffer2));
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 2), &buffer1[2]);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, sizeof(buffer1) + 2), &buffer2[2]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, sizeof(buffer1) + sizeof(buffer2) + 2));
}

static void test_memory_init_without_regions_system_ram_only() {
  rc_libretro_memory_regions_t regions;
  unsigned char buffer1[16];
  retro_memory_data[RETRO_MEMORY_SYSTEM_RAM] = buffer1;
  retro_memory_size[RETRO_MEMORY_SYSTEM_RAM] = sizeof(buffer1);
  retro_memory_data[RETRO_MEMORY_SAVE_RAM] = NULL;
  retro_memory_size[RETRO_MEMORY_SAVE_RAM] = 0;

  ASSERT_TRUE(rc_libretro_memory_init(&regions, NULL, libretro_get_core_memory_info, RC_CONSOLE_HUBS));

  ASSERT_NUM_EQUALS(regions.count, 1);
  ASSERT_NUM_EQUALS(regions.total_size, sizeof(buffer1));
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 2), &buffer1[2]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, sizeof(buffer1) + 2));
}

static void test_memory_init_without_regions_save_ram_only() {
  rc_libretro_memory_regions_t regions;
  unsigned char buffer2[8];
  retro_memory_data[RETRO_MEMORY_SYSTEM_RAM] = NULL;
  retro_memory_size[RETRO_MEMORY_SYSTEM_RAM] = 0;
  retro_memory_data[RETRO_MEMORY_SAVE_RAM] = buffer2;
  retro_memory_size[RETRO_MEMORY_SAVE_RAM] = sizeof(buffer2);

  ASSERT_TRUE(rc_libretro_memory_init(&regions, NULL, libretro_get_core_memory_info, RC_CONSOLE_HUBS));

  ASSERT_NUM_EQUALS(regions.count, 1);
  ASSERT_NUM_EQUALS(regions.total_size, sizeof(buffer2));
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 2), &buffer2[2]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, sizeof(buffer2) + 2));
}

static void test_memory_init_without_regions_no_ram() {
  rc_libretro_memory_regions_t regions;
  retro_memory_data[RETRO_MEMORY_SYSTEM_RAM] = NULL;
  retro_memory_size[RETRO_MEMORY_SYSTEM_RAM] = 0;
  retro_memory_data[RETRO_MEMORY_SAVE_RAM] = NULL;
  retro_memory_size[RETRO_MEMORY_SAVE_RAM] = 0;

  ASSERT_FALSE(rc_libretro_memory_init(&regions, NULL, libretro_get_core_memory_info, RC_CONSOLE_HUBS));

  ASSERT_NUM_EQUALS(regions.count, 0);
  ASSERT_NUM_EQUALS(regions.total_size, 0);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 2));
}

static void test_memory_init_from_unmapped_memory() {
  rc_libretro_memory_regions_t regions;
  unsigned char buffer1[8], buffer2[8];
  retro_memory_data[RETRO_MEMORY_SYSTEM_RAM] = buffer1;
  retro_memory_size[RETRO_MEMORY_SYSTEM_RAM] = 0x10000;
  retro_memory_data[RETRO_MEMORY_SAVE_RAM] = buffer2;
  retro_memory_size[RETRO_MEMORY_SAVE_RAM] = 0x10000;

  ASSERT_TRUE(rc_libretro_memory_init(&regions, NULL, libretro_get_core_memory_info, RC_CONSOLE_MEGA_DRIVE));

  ASSERT_NUM_EQUALS(regions.count, 2);
  ASSERT_NUM_EQUALS(regions.total_size, 0x20000);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x00002), &buffer1[2]);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x10002), &buffer2[2]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x20002));
}

static void test_memory_init_from_unmapped_memory_null_filler() {
  rc_libretro_memory_regions_t regions;
  unsigned char buffer1[16], buffer2[8];
  retro_memory_data[RETRO_MEMORY_SYSTEM_RAM] = buffer1;
  retro_memory_size[RETRO_MEMORY_SYSTEM_RAM] = sizeof(buffer1);
  retro_memory_data[RETRO_MEMORY_SAVE_RAM] = buffer2;
  retro_memory_size[RETRO_MEMORY_SAVE_RAM] = sizeof(buffer2);

  ASSERT_TRUE(rc_libretro_memory_init(&regions, NULL, libretro_get_core_memory_info, RC_CONSOLE_MEGA_DRIVE));

  ASSERT_NUM_EQUALS(regions.count, 4); /* two valid regions and two null fillers */
  ASSERT_NUM_EQUALS(regions.total_size, 0x20000);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x00002), &buffer1[2]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x00012));
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x10002), &buffer2[2]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x1000A));
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x20002));
}

static void test_memory_init_from_unmapped_memory_no_save_ram() {
  rc_libretro_memory_regions_t regions;
  unsigned char buffer1[16];
  retro_memory_data[RETRO_MEMORY_SYSTEM_RAM] = buffer1;
  retro_memory_size[RETRO_MEMORY_SYSTEM_RAM] = 0x10000;
  retro_memory_data[RETRO_MEMORY_SAVE_RAM] = NULL;
  retro_memory_size[RETRO_MEMORY_SAVE_RAM] = 0;

  ASSERT_TRUE(rc_libretro_memory_init(&regions, NULL, libretro_get_core_memory_info, RC_CONSOLE_MEGA_DRIVE));

  ASSERT_NUM_EQUALS(regions.count, 2);
  ASSERT_NUM_EQUALS(regions.total_size, 0x20000);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x00002), &buffer1[2]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x10002));
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x20002));
}

static void test_memory_init_from_unmapped_memory_merge_neighbors() {
  rc_libretro_memory_regions_t regions;
  unsigned char* buffer1 = malloc(0x10000); /* have to malloc to prevent array-bounds compiler warnings */
  retro_memory_data[RETRO_MEMORY_SYSTEM_RAM] = buffer1;
  retro_memory_size[RETRO_MEMORY_SYSTEM_RAM] = 0x10000;
  retro_memory_data[RETRO_MEMORY_SAVE_RAM] = NULL;
  retro_memory_size[RETRO_MEMORY_SAVE_RAM] = 0;

  ASSERT_TRUE(rc_libretro_memory_init(&regions, NULL, libretro_get_core_memory_info, RC_CONSOLE_ATARI_LYNX));

  ASSERT_NUM_EQUALS(regions.count, 1); /* all regions are adjacent, so should be merged */
  ASSERT_NUM_EQUALS(regions.total_size, 0x10000);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x0002), &buffer1[0x0002]);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x0102), &buffer1[0x0102]);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0xFFFF), &buffer1[0xFFFF]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x10000));

  free(buffer1);
}

static void test_memory_init_from_unmapped_memory_no_ram() {
  rc_libretro_memory_regions_t regions;
  retro_memory_data[RETRO_MEMORY_SYSTEM_RAM] = NULL;
  retro_memory_size[RETRO_MEMORY_SYSTEM_RAM] = 0;
  retro_memory_data[RETRO_MEMORY_SAVE_RAM] = NULL;
  retro_memory_size[RETRO_MEMORY_SAVE_RAM] = 0;

  /* init returns false */
  ASSERT_FALSE(rc_libretro_memory_init(&regions, NULL, libretro_get_core_memory_info, RC_CONSOLE_MEGA_DRIVE));

  /* but one null-filled region is still generated */
  ASSERT_NUM_EQUALS(regions.count, 1);
  ASSERT_NUM_EQUALS(regions.total_size, 0x20000);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x00002));
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x10002));
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x20002));
}

static void test_memory_init_from_unmapped_memory_save_ram_first() {
  rc_libretro_memory_regions_t regions;
  unsigned char buffer1[8], buffer2[8];
  retro_memory_data[RETRO_MEMORY_SYSTEM_RAM] = buffer1;
  retro_memory_size[RETRO_MEMORY_SYSTEM_RAM] = 0x40000;
  retro_memory_data[RETRO_MEMORY_SAVE_RAM] = buffer2;
  retro_memory_size[RETRO_MEMORY_SAVE_RAM] = 0x8000;

  ASSERT_TRUE(rc_libretro_memory_init(&regions, NULL, libretro_get_core_memory_info, RC_CONSOLE_GAMEBOY_ADVANCE));

  ASSERT_NUM_EQUALS(regions.count, 2);
  ASSERT_NUM_EQUALS(regions.total_size, 0x48000);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x00002), &buffer2[2]);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x08002), &buffer1[2]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x48002));
}

static void test_memory_init_from_memory_map() {
  rc_libretro_memory_regions_t regions;
  unsigned char buffer1[8], buffer2[8];
  const struct retro_memory_descriptor mmap_desc[] = {
	{ RETRO_MEMDESC_SYSTEM_RAM, &buffer1[0], 0, 0xFF0000U, 0, 0, 0x10000, "RAM" },
	{ RETRO_MEMDESC_SAVE_RAM,   &buffer2[0], 0, 0x000000U, 0, 0, 0x10000, "SRAM" }
  };
  const struct retro_memory_map mmap = { mmap_desc, sizeof(mmap_desc) / sizeof(mmap_desc[0]) };

  ASSERT_TRUE(rc_libretro_memory_init(&regions, &mmap, libretro_get_core_memory_info, RC_CONSOLE_MEGA_DRIVE));

  ASSERT_NUM_EQUALS(regions.count, 2);
  ASSERT_NUM_EQUALS(regions.total_size, 0x20000);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x00002), &buffer1[2]);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x10002), &buffer2[2]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x20002));
}

static void test_memory_init_from_memory_map_null_filler() {
  rc_libretro_memory_regions_t regions;
  unsigned char buffer1[8], buffer2[8];
  const struct retro_memory_descriptor mmap_desc[] = {
	{ RETRO_MEMDESC_SYSTEM_RAM, &buffer1[0], 0, 0xFF0000U, 0, 0, 0x10000, "RAM" },
	{ RETRO_MEMDESC_SAVE_RAM,   &buffer2[0], 0, 0x000000U, 0, 0, 0x10000, "SRAM" }
  };
  const struct retro_memory_map mmap = { mmap_desc, sizeof(mmap_desc) / sizeof(mmap_desc[0]) };

  ASSERT_TRUE(rc_libretro_memory_init(&regions, &mmap, libretro_get_core_memory_info, RC_CONSOLE_MEGA_DRIVE));

  ASSERT_NUM_EQUALS(regions.count, 2);
  ASSERT_NUM_EQUALS(regions.total_size, 0x20000);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x00002), &buffer1[2]);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x10002), &buffer2[2]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x20002));
}

static void test_memory_init_from_memory_map_no_save_ram() {
  rc_libretro_memory_regions_t regions;
  unsigned char buffer1[8];
  const struct retro_memory_descriptor mmap_desc[] = {
	{ RETRO_MEMDESC_SYSTEM_RAM, &buffer1[0], 0, 0xFF0000U, 0, 0, 0x10000, "RAM" }
  };
  const struct retro_memory_map mmap = { mmap_desc, sizeof(mmap_desc) / sizeof(mmap_desc[0]) };

  ASSERT_TRUE(rc_libretro_memory_init(&regions, &mmap, libretro_get_core_memory_info, RC_CONSOLE_MEGA_DRIVE));

  ASSERT_NUM_EQUALS(regions.count, 2);
  ASSERT_NUM_EQUALS(regions.total_size, 0x20000);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x00002), &buffer1[2]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x10002));
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x20002));
}

static void test_memory_init_from_memory_map_merge_neighbors() {
  rc_libretro_memory_regions_t regions;
  unsigned char* buffer1 = malloc(0x10000); /* have to malloc to prevent array-bounds compiler warnings */
  const struct retro_memory_descriptor mmap_desc[] = {
	{ RETRO_MEMDESC_SYSTEM_RAM, &buffer1[0x0000], 0, 0x0000U, 0, 0, 0xFC00, "RAM" },
	{ RETRO_MEMDESC_SYSTEM_RAM, &buffer1[0xFC00], 0, 0xFC00U, 0, 0, 0x0400, "Hardware controllers" }
  };
  const struct retro_memory_map mmap = { mmap_desc, sizeof(mmap_desc) / sizeof(mmap_desc[0]) };

  ASSERT_TRUE(rc_libretro_memory_init(&regions, &mmap, libretro_get_core_memory_info, RC_CONSOLE_ATARI_LYNX));

  ASSERT_NUM_EQUALS(regions.count, 1); /* all regions are adjacent, so should be merged */
  ASSERT_NUM_EQUALS(regions.total_size, 0x10000);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x0002), &buffer1[0x0002]);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x0102), &buffer1[0x0102]);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0xFFFF), &buffer1[0xFFFF]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x10000));

  free(buffer1);
}

static void test_memory_init_from_memory_map_no_ram() {
  rc_libretro_memory_regions_t regions;
  const struct retro_memory_descriptor mmap_desc[] = {
	{ RETRO_MEMDESC_SYSTEM_RAM, NULL, 0, 0xFF0000U, 0, 0, 0x10000, "RAM" },
	{ RETRO_MEMDESC_SAVE_RAM,   NULL, 0, 0x000000U, 0, 0, 0x10000, "SRAM" }
  };
  const struct retro_memory_map mmap = { mmap_desc, sizeof(mmap_desc) / sizeof(mmap_desc[0]) };

  /* init returns false */
  ASSERT_FALSE(rc_libretro_memory_init(&regions, &mmap, libretro_get_core_memory_info, RC_CONSOLE_MEGA_DRIVE));

  /* but one null-filled region is still generated */
  ASSERT_NUM_EQUALS(regions.count, 1);
  ASSERT_NUM_EQUALS(regions.total_size, 0x20000);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x00002));
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x10002));
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x20002));
}

static void test_memory_init_from_memory_map_splice() {
  rc_libretro_memory_regions_t regions;
  unsigned char buffer1[8], buffer2[8], buffer3[8];
  const struct retro_memory_descriptor mmap_desc[] = {
	{ RETRO_MEMDESC_SYSTEM_RAM, &buffer1[0], 0, 0xFF0000U, 0, 0, 0x08000, "RAM1" },
	{ RETRO_MEMDESC_SYSTEM_RAM, &buffer2[0], 0, 0xFF8000U, 0, 0, 0x08000, "RAM2" },
	{ RETRO_MEMDESC_SAVE_RAM,   &buffer3[0], 0, 0x000000U, 0, 0, 0x10000, "SRAM" }
  };
  const struct retro_memory_map mmap = { mmap_desc, sizeof(mmap_desc) / sizeof(mmap_desc[0]) };

  ASSERT_TRUE(rc_libretro_memory_init(&regions, &mmap, libretro_get_core_memory_info, RC_CONSOLE_MEGA_DRIVE));

  ASSERT_NUM_EQUALS(regions.count, 3);
  ASSERT_NUM_EQUALS(regions.total_size, 0x20000);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x00002), &buffer1[2]);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x08002), &buffer2[2]);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x10002), &buffer3[2]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x20002));
}

static void test_memory_init_from_memory_map_mirrored() {
  rc_libretro_memory_regions_t regions;
  unsigned char buffer1[8], buffer2[8];
  const struct retro_memory_descriptor mmap_desc[] = {
	{ RETRO_MEMDESC_SYSTEM_RAM, &buffer1[0], 0, 0xFF0000U, 0xFF0000U, 0x00C000U, 0x04000, "RAM" },
	{ RETRO_MEMDESC_SAVE_RAM,   &buffer2[0], 0, 0x000000U, 0x000000U, 0x000000U, 0x10000, "SRAM" }
  };
  const struct retro_memory_map mmap = { mmap_desc, sizeof(mmap_desc) / sizeof(mmap_desc[0]) };

  ASSERT_TRUE(rc_libretro_memory_init(&regions, &mmap, libretro_get_core_memory_info, RC_CONSOLE_MEGA_DRIVE));

  /* select of 0xFF0000 should mirror the 0x4000 bytes at 0xFF0000 into 0xFF4000, 0xFF8000, and 0xFFC000 */
  ASSERT_NUM_EQUALS(regions.count, 5);
  ASSERT_NUM_EQUALS(regions.total_size, 0x20000);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x00002), &buffer1[2]);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x04002), &buffer1[2]);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x08002), &buffer1[2]);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x0C002), &buffer1[2]);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x10002), &buffer2[2]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x20002));
}

static void test_memory_init_from_memory_map_out_of_order() {
  rc_libretro_memory_regions_t regions;
  unsigned char buffer1[8], buffer2[8];
  const struct retro_memory_descriptor mmap_desc[] = {
	{ RETRO_MEMDESC_SAVE_RAM,   &buffer2[0], 0, 0x000000U, 0, 0, 0x10000, "SRAM" },
	{ RETRO_MEMDESC_SYSTEM_RAM, &buffer1[0], 0, 0xFF0000U, 0, 0, 0x10000, "RAM" }
  };
  const struct retro_memory_map mmap = { mmap_desc, sizeof(mmap_desc) / sizeof(mmap_desc[0]) };

  ASSERT_TRUE(rc_libretro_memory_init(&regions, &mmap, libretro_get_core_memory_info, RC_CONSOLE_MEGA_DRIVE));

  ASSERT_NUM_EQUALS(regions.count, 2);
  ASSERT_NUM_EQUALS(regions.total_size, 0x20000);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x00002), &buffer1[2]);
  ASSERT_PTR_EQUALS(rc_libretro_memory_find(&regions, 0x10002), &buffer2[2]);
  ASSERT_PTR_NULL(rc_libretro_memory_find(&regions, 0x20002));
}


void test_rc_libretro(void) {
  TEST_SUITE_BEGIN();

  /* rc_libretro_disallowed_settings */
  TEST_PARAMS3(test_allowed_setting,    "bsnes-mercury", "bsnes_region", "Auto");
  TEST_PARAMS3(test_allowed_setting,    "bsnes-mercury", "bsnes_region", "NTSC");
  TEST_PARAMS3(test_disallowed_setting, "bsnes-mercury", "bsnes_region", "PAL");

  TEST_PARAMS3(test_allowed_setting,    "dolphin-emu", "dolphin_cheats_enabled", "disabled");
  TEST_PARAMS3(test_disallowed_setting, "dolphin-emu", "dolphin_cheats_enabled", "enabled");

  TEST_PARAMS3(test_allowed_setting,    "ecwolf", "ecwolf-invulnerability", "disabled");
  TEST_PARAMS3(test_disallowed_setting, "ecwolf", "ecwolf-invulnerability", "enabled");

  TEST_PARAMS3(test_allowed_setting,    "FCEUmm", "fceumm_region", "Auto");
  TEST_PARAMS3(test_allowed_setting,    "FCEUmm", "fceumm_region", "NTSC");
  TEST_PARAMS3(test_disallowed_setting, "FCEUmm", "fceumm_region", "PAL");
  TEST_PARAMS3(test_disallowed_setting, "FCEUmm", "fceumm_region", "pal"); /* case insensitive */
  TEST_PARAMS3(test_disallowed_setting, "FCEUmm", "fceumm_region", "Dendy");
  TEST_PARAMS3(test_allowed_setting,    "FCEUmm", "fceumm_palette", "default"); /* setting we don't care about */

  TEST_PARAMS3(test_allowed_setting,    "FinalBurn Neo", "fbneo-allow-patched-romsets", "disabled");
  TEST_PARAMS3(test_disallowed_setting, "FinalBurn Neo", "fbneo-allow-patched-romsets", "enabled");
  TEST_PARAMS3(test_allowed_setting,    "FinalBurn Neo", "fbneo-cheat-mvsc-P1_Char_1_Easy_Hyper_Combo", "disabled"); /* wildcard key match */
  TEST_PARAMS3(test_disallowed_setting, "FinalBurn Neo", "fbneo-cheat-mvsc-P1_Char_1_Easy_Hyper_Combo", "enabled");
  TEST_PARAMS3(test_allowed_setting,    "FinalBurn Neo", "fbneo-cheat-mvsc-P1_Char_1_Easy_Hyper_Combo", "0 - Disabled"); /* multi-not value match */
  TEST_PARAMS3(test_disallowed_setting, "FinalBurn Neo", "fbneo-cheat-mvsc-P1_Char_1_Easy_Hyper_Combo", "1 - Enabled");

  TEST_PARAMS3(test_allowed_setting,    "Genesis Plus GX", "genesis_plus_gx_lock_on", "disabled");
  TEST_PARAMS3(test_disallowed_setting, "Genesis Plus GX", "genesis_plus_gx_lock_on", "action replay (pro)");
  TEST_PARAMS3(test_disallowed_setting, "Genesis Plus GX", "genesis_plus_gx_lock_on", "game genie");
  TEST_PARAMS3(test_allowed_setting,    "Genesis Plus GX", "genesis_plus_gx_lock_on", "sonic & knuckles");
  TEST_PARAMS3(test_allowed_setting,    "Genesis Plus GX", "genesis_plus_gx_region_detect", "Auto");
  TEST_PARAMS3(test_allowed_setting,    "Genesis Plus GX", "genesis_plus_gx_region_detect", "NTSC-U");
  TEST_PARAMS3(test_disallowed_setting, "Genesis Plus GX", "genesis_plus_gx_region_detect", "PAL");
  TEST_PARAMS3(test_allowed_setting,    "Genesis Plus GX", "genesis_plus_gx_region_detect", "NTSC-J");

  TEST_PARAMS3(test_allowed_setting,    "Genesis Plus GX Wide", "genesis_plus_gx_wide_lock_on", "disabled");
  TEST_PARAMS3(test_disallowed_setting, "Genesis Plus GX Wide", "genesis_plus_gx_wide_lock_on", "action replay (pro)");
  TEST_PARAMS3(test_disallowed_setting, "Genesis Plus GX Wide", "genesis_plus_gx_wide_lock_on", "game genie");
  TEST_PARAMS3(test_allowed_setting,    "Genesis Plus GX Wide", "genesis_plus_gx_wide_lock_on", "sonic & knuckles");
  TEST_PARAMS3(test_allowed_setting,    "Genesis Plus GX Wide", "genesis_plus_gx_wide_region_detect", "Auto");
  TEST_PARAMS3(test_allowed_setting,    "Genesis Plus GX Wide", "genesis_plus_gx_wide_region_detect", "NTSC-U");
  TEST_PARAMS3(test_disallowed_setting, "Genesis Plus GX Wide", "genesis_plus_gx_wide_region_detect", "PAL");
  TEST_PARAMS3(test_allowed_setting,    "Genesis Plus GX Wide", "genesis_plus_gx_wide_region_detect", "NTSC-J");

  TEST_PARAMS3(test_allowed_setting,    "Mesen", "mesen_region", "Auto");
  TEST_PARAMS3(test_allowed_setting,    "Mesen", "mesen_region", "NTSC");
  TEST_PARAMS3(test_disallowed_setting, "Mesen", "mesen_region", "PAL");
  TEST_PARAMS3(test_disallowed_setting, "Mesen", "mesen_region", "Dendy");

  TEST_PARAMS3(test_allowed_setting,    "Mesen-S", "mesen-s_region", "Auto");
  TEST_PARAMS3(test_allowed_setting,    "Mesen-S", "mesen-s_region", "NTSC");
  TEST_PARAMS3(test_disallowed_setting, "Mesen-S", "mesen-s_region", "PAL");

  TEST_PARAMS3(test_allowed_setting,    "PPSSPP", "ppsspp_cheats", "disabled");
  TEST_PARAMS3(test_disallowed_setting, "PPSSPP", "ppsspp_cheats", "enabled");

  TEST_PARAMS3(test_allowed_setting,    "PCSX-ReARMed", "pcsx_rearmed_region", "Auto");
  TEST_PARAMS3(test_allowed_setting,    "PCSX-ReARMed", "pcsx_rearmed_region", "NTSC");
  TEST_PARAMS3(test_disallowed_setting, "PCSX-ReARMed", "pcsx_rearmed_region", "PAL");
  
  TEST_PARAMS3(test_allowed_setting,    "PicoDrive", "picodrive_region", "Auto");
  TEST_PARAMS3(test_allowed_setting,    "PicoDrive", "picodrive_region", "US");
  TEST_PARAMS3(test_allowed_setting,    "PicoDrive", "picodrive_region", "Japan NTSC");
  TEST_PARAMS3(test_disallowed_setting, "PicoDrive", "picodrive_region", "Europe");
  TEST_PARAMS3(test_disallowed_setting, "PicoDrive", "picodrive_region", "Japan PAL");
  
  TEST_PARAMS3(test_allowed_setting,    "Snes9x", "snes9x_region", "Auto");
  TEST_PARAMS3(test_allowed_setting,    "Snes9x", "snes9x_region", "NTSC");
  TEST_PARAMS3(test_disallowed_setting, "Snes9x", "snes9x_region", "PAL");
  
  TEST_PARAMS3(test_allowed_setting,    "Virtual Jaguar", "virtualjaguar_pal", "disabled");
  TEST_PARAMS3(test_disallowed_setting, "Virtual Jaguar", "virtualjaguar_pal", "enabled");

  /* rc_libretro_memory_init */
  TEST(test_memory_init_without_regions);
  TEST(test_memory_init_without_regions_system_ram_only);
  TEST(test_memory_init_without_regions_save_ram_only);
  TEST(test_memory_init_without_regions_no_ram);

  TEST(test_memory_init_from_unmapped_memory);
  TEST(test_memory_init_from_unmapped_memory_null_filler);
  TEST(test_memory_init_from_unmapped_memory_no_save_ram);
  TEST(test_memory_init_from_unmapped_memory_merge_neighbors);
  TEST(test_memory_init_from_unmapped_memory_no_ram);
  TEST(test_memory_init_from_unmapped_memory_save_ram_first);

  TEST(test_memory_init_from_memory_map);
  TEST(test_memory_init_from_memory_map_null_filler);
  TEST(test_memory_init_from_memory_map_no_save_ram);
  TEST(test_memory_init_from_memory_map_merge_neighbors);
  TEST(test_memory_init_from_memory_map_no_ram);
  TEST(test_memory_init_from_memory_map_splice);
  TEST(test_memory_init_from_memory_map_mirrored);
  TEST(test_memory_init_from_memory_map_out_of_order);

  TEST_SUITE_END();
}
