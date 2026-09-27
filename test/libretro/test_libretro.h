#ifndef TEST_LIBRETRO_H
#define TEST_LIBRETRO_H

#ifdef RC_CLIENT_SUPPORTS_HASH
/* no direct compile option for hash support, so leverage RC_CLIENT_SUPPORTS_HASH */
void test_rc_libretro(); /* libretro extensions require hash support */
#endif

#endif
