#ifndef RHASH_MOCK_FILEREADER_H
#define RHASH_MOCK_FILEREADER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

void init_mock_filereader();
void init_mock_cdreader();

void mock_file(int index, const char* filename, const uint8_t* buffer, size_t buffer_size);
void mock_empty_file(int index, const char* filename, size_t mock_size);
void mock_file_size(int index, size_t mock_size);

const char* get_mock_filename(void* file_handle);

#ifdef __cplusplus
}
#endif

#endif /* RHASH_MOCK_FILEREADER_H */
