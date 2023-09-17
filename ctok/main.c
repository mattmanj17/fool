

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "lex.h"
#include "file.h"



void Print_tokens_in_file(const wchar_t * fpath);



int wmain(int argc, wchar_t *argv[])
{
	if (argc != 2)
	{
		printf("wrong number of arguments, only expected a file path\n");
		return 1;
	}
	
	Print_tokens_in_file(argv[1]);
	return 0;
}



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

bool Starts_with_invalid_BOM(const bounded_c_str_t * bstr)
{
	typedef struct
	{
		const char * name;
		const char * bytes;
		int len;

		int _padding;
	} bom_t;

	static bom_t s_boms[10] =
	{
		{"UTF-32 (BE)", "\x00\x00\xFE\xFF", 4},
		{"UTF-32 (LE)", "\xFF\xFE\x00\x00", 4},
		{"UTF-16 (BE)", "\xFE\xFF", 2},
		{"UTF-16 (LE)", "\xFF\xFE", 2},
		{"UTF-7", "\x2B\x2F\x76", 3},
		{"UTF-1", "\xF7\x64\x4C", 3},
		{"UTF-EBCDIC", "\xDD\x73\x66\x73", 4},
		{"SCSU", "\x0E\xFE\xFF", 3},
		{"BOCU-1", "\xFB\xEE\x28", 3},
		{"GB-18030", "\x84\x31\x95\x33", 4},
	};

	int cCh = (int)(bstr->terminator - bstr->cursor);

	for (int iBom = 0; iBom < 10; ++iBom)
	{
		bom_t * bom = &s_boms[iBom];
		if (bom->len > cCh)
			continue;

		bool match = true;
		for (int iCh = 0; iCh < bom->len; ++iCh)
		{
			if (bstr->cursor[iCh] != bom->bytes[iCh])
			{
				match = false;
				break;
			}
		}

		if (match)
		{
			printf(
				"File started with invalid byte order mark. "
				"This file seems to be encoded in %s, which is not supported.",
				bom->name);

			return true;
		}
	}

	return false;
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

	// Check for invalid BOM

	if (Starts_with_invalid_BOM(&bstr))
	{
		// Err msg printed in Starts_with_invalid_BOM...

		exit(1);
	}

	// Cache allocated buffer

	char * buf = bstr.cursor;

	// Print tokens

	Print_toks_in_ch_range(&bstr);

	// BUG this is scuffed...

	free(buf);
}