#pragma once

#include <stdint.h>



typedef struct
{
	const char * new_line_start;
	uint32_t cp;
	int length;
	int num_lines;

	int _padding;
} peek_t;

peek_t Peek(const char * mic, const char * mac);