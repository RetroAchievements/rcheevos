#ifndef RHASH_TEST_DATA_H
#define RHASH_TEST_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

uint8_t* generate_generic_file(size_t size);

uint8_t* generate_3do_bin(unsigned root_directory_sectors, unsigned binary_size, size_t* image_size);
uint8_t* generate_pce_cd_bin(unsigned binary_sectors, size_t* image_size);
uint8_t* generate_pcfx_bin(unsigned binary_sectors, size_t* image_size);

uint8_t* generate_nes_file(size_t kb, int with_header, size_t* image_size);
uint8_t* generate_fds_file(size_t sides, int with_header, size_t* image_size);

#ifdef __cplusplus
}
#endif

#endif /* RHASH_TEST_DATA_H */
