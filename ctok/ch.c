
#include "ch.h"



bool Is_ch_horizontal_white_space(char ch)
{
	return ch == ' ' || ch == '\t' || ch == '\f' || ch == '\v';
}

bool Is_ch_white_space(char ch)
{
	return ch == ' ' || ch == '\t' || ch == '\f' || ch == '\v' || ch == '\n' || ch == '\r';
}

bool Is_ascii_digit(uint32_t cp)
{
	return cp >= '0' && cp <= '9';
}

bool Is_ascii_lowercase(uint32_t cp)
{
	return cp >= 'a' && cp <= 'z';
}

bool Is_ascii_uppercase(uint32_t cp)
{
	return cp >= 'A' && cp <= 'Z';
}



static int Len_line_continue(const char * str, int * out_num_lines)
{
	const char * str_peek = str;
	*out_num_lines = 0;

	if (str_peek[0] != '\\')
		return 0;

	++str_peek;

	// Skip whitespace after the backslash as an extension

	while (Is_ch_horizontal_white_space(str_peek[0]))
	{
		++str_peek;
	}

#if 0
	int len_line_break = Len_line_break(str_peek);
#else
	// BUG clang does a horrifying thing where it slurps
	//  up a \n\r as a single line break when measuring
	//  a line continue, EVEN THOUGH it only defines
	//  "physical line breaks" as \n, \r, and \r\n.
	//  It has been like that since the very first
	//  version of the tokenizer, go figure.

	int len_line_break = 0;
	if (str_peek[0] == '\n')
	{
		if (str_peek[1] == '\r')
		{
			// :(

			*out_num_lines = 2;
			len_line_break = 2;
		}
		else
		{
			*out_num_lines = 1;
			len_line_break = 1;
		}
	}
	else if (str_peek[0] == '\r')
	{
		if (str_peek[1] == '\n')
		{
			*out_num_lines = 1;
			len_line_break = 2;
		}
		else
		{
			*out_num_lines = 1;
			len_line_break = 1;
		}
	}
#endif
	if (len_line_break == 0)
		return 0;

	return (int)(str_peek - str) + len_line_break;
}

int Len_line_continues(const char * str, int * out_num_lines)
{
	const char * str_peek = str;
	int num_lines = 0;

	while (true)
	{
		int num_lines_single;
		int len_line_continue = Len_line_continue(str_peek, &num_lines_single);
		if (!len_line_continue)
			break;

		str_peek += len_line_continue;
		num_lines += num_lines_single;
	}

	if (out_num_lines)
	{
		*out_num_lines = num_lines;
	}

	return (int)(str_peek - str);
}