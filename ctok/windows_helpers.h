#pragma once

#include <stdint.h>
#include <stdbool.h>

void Display_error_box_and_exit(const char * function, uint32_t exit_code);

typedef bool (*walk_dir_callback)(
	const char * root_dir, 
	const char * this_dir, 
	const char * full_path);

typedef void (*walk_file_callback)(
	const char * root_dir, 
	const char * this_file, 
	const char * full_path);

void Walk_dir(
	const char * dir, 
	walk_dir_callback dir_callback,
	walk_file_callback file_callback);