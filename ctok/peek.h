#pragma once

#include <stdint.h>



typedef struct
{
	const char * after_last_eol;
	uint32_t cp;
	int len;
	int num_eol;

	int _padding;
} peek_cp_t;

peek_cp_t Peek_cp(const char * mic, const char * mac);
void Peek_escaped_end_of_lines(const char * mic, const char * mac, peek_cp_t * peek_cp_out);