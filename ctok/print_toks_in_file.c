
#include <stdio.h>
#include <stdlib.h>

#include "print_toks_in_file.h"
#include "lex.h"
#include "file.h"



static void Print_toks_in_ch_range(const bounded_c_str_t * bstr)
{
	input_t input;
	Init_input(&input, bstr->cursor, bstr->terminator);

	while (!Is_input_exhausted(&input))
	{
		token_t tok;
		Lex(&input, &tok);
		Print_token(&tok);
	}
}

void Print_tokens_in_file(const wchar_t * fpath)
{
	bounded_c_str_t bstr;
	bool success = Try_read_file_at_path_to_buffer(fpath, &bstr);
	if (!success)
	{
		printf("Failed to read file '%ls'.\n", fpath);
		exit(1);
	}

	// Cache allocated buffer

	char * buf = bstr.cursor;

	// Print tokens

	Print_toks_in_ch_range(&bstr);

	// BUG this is scuffed...

	free(buf);
}