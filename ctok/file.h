#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct
{
	char * cursor;
	const char * terminator;
} bounded_c_str_t;

bool Try_read_file_at_path_to_buffer(const char * fpath, bounded_c_str_t * bstr);
void Try_write_buffer_to_file_at_path(void * buffer, uint64_t size, const char * fpath);
