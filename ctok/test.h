#pragma once

#include <stdint.h>

typedef struct
{
	uint64_t * token_positions;
	const char * inputs;
} tests_t;

void Try_load_tests_from_file(tests_t ** ptests, const char * fpath);

void Try_dump_loose_cases_to_file(
	uint64_t num_cases,
	uint64_t ** token_positions,
	uint64_t * token_position_lengths,
	const char ** inputs,
	uint64_t * input_lengths, 
	const char * fpath);