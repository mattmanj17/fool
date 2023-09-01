
#include <stdio.h>
#include <stdlib.h>

#include "print_toks_in_file.h"
#include "lex.h"
#include "file.h"



static void Clean_and_print_ch(char ch)
{
	// NOTE we print ' ' as \x20 so that we can split output on spaces

	switch (ch)
	{
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
	case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
	case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u':
	case 'v': case 'w': case 'x': case 'y': case 'z':
	case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
	case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
	case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
	case 'V': case 'W': case 'X': case 'Y': case 'Z':
	case '_': case '$':
	case '!': case '#': case '%': case '&': case '\'': case '(': case ')': 
	case '*': case '+': case ',': case '-': case '.': case '/': case ':': 
	case ';': case '<': case '=': case '>': case '?': case '@': case '[': 
	case ']': case '^': case '`': case '{': case '|': case '}': case '~':
		putchar(ch);
		break;
	case '\a':
		printf("\\a");
		break;
	case '\b':
		printf("\\b");
		break;
	case '\f':
		printf("\\f");
		break;
	case '\n':
		printf("\\n");
		break;
	case '\r':
		printf("\\r");
		break;
	case '\t':
		printf("\\t");
		break;
	case '\v':
		printf("\\v");
		break;
	case '"':
		printf("\\\"");
		break;
	case '\\':
		printf("\\\\");
		break;
	default:
		printf("\\x%02hhx", ch);
		break;
	}
}

void Print_token(
	const char * tok_start, 
	int tok_len,
	int line_number,
	int loc_in_line)
{
	printf("\"");
	for (int i = 0; i < tok_len; ++i)
	{
		Clean_and_print_ch(tok_start[i]);
	}
	printf("\"");

	printf(
		" %d:%d\n",
		line_number,
		loc_in_line);
}

int count_raw_lines(const char * min, const char * max)
{
	int count = 0;

	while (min < max)
	{
		char ch = min[0];
		if (ch == '\n')
		{
			++count;
		}
		else if (ch == '\r')
		{
			if (min[1] == '\n')
			{
				++min;
			}

			++count;
		}

		++min;
	}

	return count;
}

static void Print_toks_in_ch_range(const bounded_c_str_t * bstr)
{
	input_t input;
	Init_input(&input, bstr->cursor, bstr->terminator);

	while (true)
	{
		// BUG refactor this so it is not reaching into input

		const char * token_start = input.cursor;
		int line_prev = input.line;
		int loc_in_line = (int)(input.cursor - input.line_start + 1);

		if (!Lex(&input))
			break;

		Print_token(
			token_start,
			(int)(input.cursor - token_start), 
			line_prev, 
			loc_in_line);
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

	char * buf = bstr.cursor;

	Print_toks_in_ch_range(&bstr);

	// BUG this is scuffed...

	free(buf);
}