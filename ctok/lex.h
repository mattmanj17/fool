#pragma once

typedef struct
{
	const char * str;
	const char * line_start;
	int line;

	int _padding;
} input_t;

bool Lex(input_t * input);
void Init_input(input_t * input, const char * str);
