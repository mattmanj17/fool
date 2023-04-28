#pragma once

#include <stdint.h>

char * Try_read_file_at_path_to_buffer(const char * fpath);
void Try_write_buffer_to_file_at_path(void * buffer, uint64_t size, const char * fpath);
