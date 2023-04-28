
#include <stdio.h>
#include <stdlib.h>

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

static void Print_token(
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

static void Print_toks_in_str(const char * str_)
{
	input_t input;
	input.str = str_;
	input.line_start = str_;
	input.line = 1;

	while (input.str[0])
	{
		const char * token_start = input.str;
		int line_prev = input.line;
		int loc_in_line = (int)(input.str - input.line_start + 1);

		Skip_leading_token(&input);
		if (input.str == token_start)
		{
			printf("Lex error\n");
			return;
		}

		Print_token(
			token_start, 
			(int)(input.str - token_start), 
			line_prev, 
			loc_in_line);
	}
}

void Print_tokens_in_file(const char * fpath)
{
	char * file_buf = Try_read_file_at_path_to_buffer(fpath);
	if (!file_buf)
	{
		printf("Failed to read file '%s'.\n", fpath);
		exit(1);
	}

	Print_toks_in_str(file_buf);
}