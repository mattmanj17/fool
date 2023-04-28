#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#include "test.h"
#include "file.h"

static void Patch_ptr_to_base(void ** pp, void * base)
{
	*pp = (void *)((uintptr_t)(*pp) + (uintptr_t)base);
}

static void Unpatch_ptr_from_base(void ** pp, void * base)
{
	*pp = (void *)((uintptr_t)(*pp) - (uintptr_t)base);
}

static void Patch_tests(tests_t * tests)
{
	Patch_ptr_to_base((void **)&tests->token_positions, tests);
	Patch_ptr_to_base((void **)&tests->inputs, tests);
}

void Try_load_tests_from_file(tests_t ** ptests, const char * fpath)
{
	*ptests = (tests_t *)Try_read_file_at_path_to_buffer(fpath);

	if (!*ptests)
		return;

	//??? should mabey have some leading signature bits that we can check to see
	// if the file we loaded is a legit set of test cases

	Patch_tests(*ptests);
}



static void Try_dump_loose_cases(
	uint64_t num_cases,
	uint64_t ** token_positions,
	uint64_t * token_position_lengths,
	const char ** inputs,
	uint64_t * input_lengths,
	uint64_t * out_size, 
	void ** out_buffer)
{
	// Clear out_ ptrs

	*out_size = 0;
	*out_buffer = NULL;

	// size_of_positions

	uint64_t size_of_positions;

	{
		uint64_t num_positions = 0;

		for (uint64_t i = 0; i < num_cases; ++i)
		{
			// +1 for the trailing -1 that indicates end of case

			num_positions += token_position_lengths[i] + 1;
		}

		// +1 for the trailing -2 that indicates end of cases

		++num_positions;

		size_of_positions = num_positions * sizeof(uint64_t);
	}

	// size_of_inputs

	uint64_t size_of_inputs = 0;
	for (uint64_t i = 0; i < num_cases; ++i)
	{
		// +1 for trailing '\0'

		size_of_inputs += input_lengths[i] + 1;
	}

	// Allocate buffer

	uint64_t size = sizeof(tests_t) + size_of_positions + size_of_inputs;

	void * buffer = calloc(size, 1);

	if (!buffer)
		return;

	// Set out_ ptrs

	*out_size = size;
	*out_buffer = buffer;

	// Dump tests_t

	uint64_t * out_positions;
	char * out_inputs;

	{
		uintptr_t i_byte_in_buf = (uintptr_t)buffer;

		tests_t * tests_out = (tests_t *)i_byte_in_buf;
		i_byte_in_buf += sizeof(tests_t);
	
		out_positions = (uint64_t *)i_byte_in_buf;
		i_byte_in_buf += size_of_positions;
		tests_out->token_positions = out_positions;
		Unpatch_ptr_from_base((void**)&tests_out->token_positions, buffer);

		out_inputs = (char *)i_byte_in_buf;
		tests_out->inputs = out_inputs;
		Unpatch_ptr_from_base((void**)&tests_out->inputs, buffer);
	}

	// dump token_positions and inputs

	for (uint64_t i = 0; i < num_cases; ++i)
	{
		// out_positions

		uint64_t len_pos = token_position_lengths[i];
		memcpy(
			out_positions, 
			token_positions[i], 
			len_pos * sizeof(uint64_t));
		out_positions += len_pos;
		
		// trailing -1 that indicates end of case

		*out_positions = (uint64_t)-1;
		++out_positions;

		// out_inputs

		uint64_t len_input = input_lengths[i];
		memcpy(
			out_inputs,
			inputs[i],
			len_input);
		out_inputs += len_input;

		// trailing '\0' (already 0 because of calloc)

		++out_inputs;
	}

	// trailing -2 that indicates end of cases

	*out_positions = (uint64_t)-2;
}

void Try_dump_loose_cases_to_file(
	uint64_t num_cases,
	uint64_t ** token_positions,
	uint64_t * token_position_lengths,
	const char ** inputs,
	uint64_t * input_lengths, 
	const char * fpath)
{
	uint64_t size;
	void * buffer;
	Try_dump_loose_cases(
		num_cases, 
		token_positions, 
		token_position_lengths, 
		inputs, 
		input_lengths, 
		&size, 
		&buffer);

	if (!buffer)
		return;

	Try_write_buffer_to_file_at_path(buffer, size, fpath);

	free(buffer);
}