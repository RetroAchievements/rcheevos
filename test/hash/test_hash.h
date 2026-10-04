#ifndef TEST_RAPI_H
#define TEST_RAPI_H

void test_hash();

#ifndef RC_HASH_NO_ROM
void test_hash_rom();
#endif

#ifndef RC_HASH_NO_DISC
void test_cdreader();
void test_hash_disc();
#endif

#ifndef RC_HASH_NO_ZIP
void test_hash_zip();
#endif

#endif
