#pragma once

#include <stdbool.h>

typedef struct
{
	const char * cursor;
	const char * terminator;

	const char * line_start;
	int line;

	int _padding;
} input_t;

void Lex(input_t * input);
void Init_input(input_t * input, const char * cursor, const char * terminator);
bool Is_input_exhausted(input_t * input);
