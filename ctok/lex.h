#pragma once

typedef struct
{
	const char * str;
	const char * line_start;
	int line;
	int _padding;
} input_t;

void Skip_leading_token(input_t * input);
