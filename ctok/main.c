
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h> // for calloc



static const char * Find_in_str(
	char ch, 
	const char * str)
{
	while (true)
	{
		if (!str[0])
			break;

		if (str[0] == ch)
			return str;

		++str;
	}

	return NULL;
}

static bool Is_horizontal_whitespace(char ch)
{
	return ch == ' ' || ch == '\t';
}

static bool Is_line_break(char ch)
{
	return ch == '\r' || ch == '\n';
}

static bool Is_digit(char ch)
{
	return Find_in_str(ch, "0123456789") != NULL;
}

static bool Is_hex(char ch)
{
	return Find_in_str(
			ch, 
			"0123456789"
			"abcdef"
			"ABCDEF") != NULL;
}

static bool Is_octal(char ch)
{
	return Find_in_str(ch, "01234567") != NULL;
}

static bool Can_start_id(char ch)
{
	return Find_in_str(
			ch, 
			"_"
			"abcdefghijklmnopqrstuvwxyz"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ") != NULL;
}

static bool Extends_id(char ch)
{
	return Is_digit(ch) || Can_start_id(ch);
}

static int Count_hex(const char * str)
{
	int len = 0;

	while (str[0])
	{
		if (!Is_hex(str[0]))
			break;

		++str;
		++len;
	}

	return len;
}

static int Count_octal(const char * str)
{
	int len = 0;

	while (str[0])
	{
		if (!Is_octal(str[0]))
			break;

		++str;
		++len;
	}

	return len;
}



static int Len_leading_horizontal_whitespace(
	const char * str)
{
	const char * begin = str;

	while (true)
	{
		if (!Is_horizontal_whitespace(str[0]))
			return (int)(str - begin);

		++str;
	}
}



static int Len_line_break(const char * str)
{
	char ch = str[0];

	if (ch == '\n')
		return 1;

	if (ch == '\r')
	{
		if (str[1] == '\n')
			return 2;

		return 1;
	}

	return 0;
}

static int Len_leading_line_breaks(
	const char * str, 
	int * line_breaks,
	const char ** new_start_of_line)
{
	int len = Len_line_break(str);
	if (!len)
		return 0;
	
	// BUG clang has weird handling of line breaks + white space in raw mode...
	//  it returns whitespace before line breaks as their own token,
	//  but merges all adjacent lines breaks/white space after a line break into
	//  one token... this goo is to match that

	str += len;
	*line_breaks = 1;
	*new_start_of_line = str;

	while (true)
	{
		int len_ws = Len_leading_horizontal_whitespace(str);
		if (len_ws)
		{
			str += len_ws;
			len += len_ws;
			continue;
		}

		int len_break = Len_line_break(str);
		if (len_break)
		{
			str += len_break;
			len += len_break;
			*line_breaks += 1;
			*new_start_of_line = str;
			continue;
		}

		break;
	}
		
	return len;
}



static int Len_leading_block_comment(
	const char * str, 
	int * line_breaks,
	const char ** new_start_of_line)
{
	if (str[0] != '/' || str[1] != '*')
		return 0;

	const char * begin = str;
	str += 2;

	*line_breaks = 0;

	while (true)
	{
		if (str[0] == '*' && str[1] == '/')
		{
			str += 2;
			break;
		}

		int len_line_break = Len_line_break(str);
		if (len_line_break)
		{
			str += len_line_break;
			*line_breaks += 1;
			*new_start_of_line = str;
			continue;
		}

		++str;
	}

	return (int)(str - begin);
}



static int Len_leading_line_comment(const char * str)
{
	if (str[0] != '/' || str[1] != '/')
		return 0;

	const char * begin = str;
	str += 2;

	while (true)
	{
		char ch = str[0];
		if (!ch)
			break;

		if (Is_line_break(ch))
			break;

		++str;
	}

	return (int)(str - begin);
}



static int Length_of_escape(const char * str) //??? bug, this starts AFTER the leading \, which could be confusing... also do not love the name
{
	const char * simple_escapes = "abfnrtv'\"\\?";

	if (Find_in_str(str[0], simple_escapes))
		return 1;

	if (Is_octal(str[0]))
	{
		int len_octal = Count_octal(str);
		if (len_octal > 3)
			return 0;

		return len_octal;
	}

	if (str[0] == 'x' || str[0] == 'X')
	{
		++str;
		return Count_hex(str) + 1;
	}

	if (str[0] == 'u' || str[0] == 'U')
	{
		int len_expected = (str[0] == 'u') ? 4 : 8;

		++str;
		int len_hex = Count_hex(str);
		if (len_hex != len_expected)
			return 0;

		return len_expected + 1;
	}

	return 0;
}

static int Length_of_str_encoding_prefix(const char * str)
{
	if (str[0] == 'u')
	{
		if (str[1] == '8')
			return 2;

		return 1;
	}

	if (str[0] == 'U')
		return 1;

	if (str[0] == 'L')
		return 1;

	return 0;
}

static int Len_leading_str_lit(const char * str) //??? this function is a little long
{
	const char * begin = str;

	str += Length_of_str_encoding_prefix(str);

	if (str[0] != '"')
		return 0;

	++str;

	while (true)
	{
		char ch0 = str[0];

		if (str[0] == '\0')
			return 0;

		++str;

		if (ch0 == '"')
			break;

		if (ch0 == '\\')
		{
			int len_esc = Length_of_escape(str);
			if (len_esc == 0)
				return 0;

			str += len_esc;
		}

		if (Find_in_str(ch0, "\r\n"))
			return 0;
	}

	return (int)(str - begin);
}



static int Len_leading_char_lit(const char * str) //??? this fucntion is a little long
{
	const char * begin = str;

	if (str[0] == 'L')
		++str;

	if (str[0] != '\'')
		return 0;

	++str;

	// NOTE (matthewd) deciding NOT to handle multi char literals...

	char ch_body_0 = str[0];
	if (ch_body_0 == '\0')
		return 0;

	if (Find_in_str(ch_body_0, "\'\r\n"))
		return 0;

	++str;

	if (ch_body_0 == '\\')
	{
		int len_esc = Length_of_escape(str);
		if (len_esc == 0)
			return 0;

		str += len_esc;
	}

	if (str[0] != '\'')
		return 0;

	++str;

	return (int)(str - begin);
}



/* NOTE (matthewd)
	preprocesor numbers are a bit unintuative,
	since they can match things that are not valid tokens
	in later translation phases.

	so, here is a quick antlr style definition of pp-num
	
	pp_num
		: pp_num_start pp_num_continue*
		;

	pp_num_start
		: [0-9]
		| '.' [0-9]
		;

	pp_num_continue
		: '.'
		| [eEpP][+-]
		| [0-9a-zA-Z_]
		;

	note that, pp_num_continue is technically also supposed
	to include "universal character names" (\uxxxx or \Uxxxxxxxx),
	but I have not implemented that, for now.

	also note that, this is not how this is defined in the standard.
	That standard defines it with left recursion, so I factored out
	the left recursion so it would be more obvious what the code was doing

	the original definition is

	pp_num
		: [0-9]
		| '.' [0-9]
		| pp_num [0-9]
		| pp_num identifier_nondigit
		| pp_num 'e' [+-]
		| pp_num 'E' [+-]
		| pp_num 'p' [+-]
		| pp_num 'P' [+-]
		| pp_num '.'
		;

	where identifier_nondigit is 
	[0-9a-zA-Z_] OR a "universal character name"
*/

static int Len_pp_num_start(const char * str)
{
	if (Is_digit(str[0]))
		return 1;

	if (str[0] == '.' && Is_digit(str[1]))
		return 2;
	
	return 0;
}

static bool Starts_pp_num_sign(char ch)
{
	return ch == 'e' || ch == 'E' || ch == 'p' || ch == 'P';
}

static bool Is_sign(char ch)
{
	return ch == '-' || ch == '+';
}

static int Len_pp_num_continue(const char * str)
{
	if (str[0] == '.')
		return 1;

	if (Starts_pp_num_sign(str[0]) && Is_sign(str[1]))
		return 2;

	if (Extends_id(str[0]))
		return 1;
	
	return 0;
}

static int Len_leading_pp_num(const char * str)
{
	int len_start = Len_pp_num_start(str);
	if (!len_start)
		return 0;

	const char * begin = str;
	str += len_start;

	while (true)
	{
		int len_continue = Len_pp_num_continue(str);
		if (!len_continue)
			break;

		str += len_continue;
	}

	return (int)(str - begin);
}



static int Len_leading_id(const char * str)
{
	//??? todo add support for universal-character-names

	if (!Can_start_id(str[0]))
		return 0;

	int len = 0;

	while (str[0])
	{
		if (!Extends_id(str[0]))
			break;

		++str;
		++len;
	}

	return len;
}



static const char * punct_3[]
{
	"...", "<<=", ">>=",

	"\0",
};

static const char * punct_2[]
{
	"do", "if", "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=", 
	"&&", "||", "*=", "/=", "%=", "+=", "-=", "&=", "^=", "|=",

	"\0",
};

static const char * punct_1[]
{
	"[", "]", "(", ")", "{", "}", ".", "&", "*", "+", "-", "~", "!", 
	"/", "%", "<", ">", "^", "|", "?", ":", ";", "=", ",",

	"\0",
};

static const char ** punctuation[]
{
	punct_1,
	punct_2,
	punct_3,
};

static bool First_n_ch_equal(const char * str0, const char * str1, int len)
{
	for (int i = 0; i < len; ++i)
	{
		if (!str0[i])
			return false;

		if (!str1[i])
			return false;

		if (str0[i] != str1[i])
			return false;
	}

	return true;
}

static int Len_leading_punct(const char * str)
{
	for (int len = 3; len > 0; --len)
	{
		int i = len - 1;
		const char ** punct_of_len = punctuation[i];

		while (true)
		{
			const char * punct = *punct_of_len;
			if (!punct[0])
				break;

			if (First_n_ch_equal(str, punct, len))
			{
				return (int)len;
			}

			++punct_of_len;
		}
	}

	return 0;
}


// BUG it would be better if we did not do any backtracking...

static int Len_leading_token(
	const char * str, 
	int * line_breaks,
	const char ** new_start_of_line)
{
	*line_breaks = 0;

	int len = Len_leading_horizontal_whitespace(str);
	if (len) return len;

	len = Len_leading_line_breaks(str, line_breaks, new_start_of_line);
	if (len) return len;

	len = Len_leading_block_comment(str, line_breaks, new_start_of_line);
	if (len) return len;

	len = Len_leading_line_comment(str);
	if (len) return len;

	len = Len_leading_str_lit(str);
	if (len) return len;

	len = Len_leading_char_lit(str);
	if (len) return len;

	len = Len_leading_pp_num(str);
	if (len) return len;

	len = Len_leading_id(str);
	if (len) return len;

	len = Len_leading_punct(str);
	if (len) return len;

	return 0;
}


static void Clean_and_print_ch(char ch)
{
	const char * good_ch = 
		"0123456789"
		"abcdefghijklmnopqrstuvwxyz"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"!#$%&'()*+,-./:;<=>?@[]^_`{|}~";

	if (Find_in_str(ch, good_ch))
		putchar(ch);
	else if (ch == '\a') 
        printf("\\a");
    else if (ch == '\b') 
        printf("\\b");
    else if (ch == '\f') 
        printf("\\f");
    else if (ch == '\n') 
        printf("\\n");
    else if (ch == '\r') 
        printf("\\r");
    else if (ch == '\t') 
        printf("\\t");
    else if (ch == '\v') 
        printf("\\v");
    else if (ch == '"') 
        printf("\\\"");
    else if (ch == '\\') 
        printf("\\\\");
	else
		printf("\\x%02hhx", ch);
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

static void Print_toks_in_str(const char * str)
{
	int line_number = 1;
	const char * start_of_line = str;

	while (str[0])
	{
		int line_breaks;
		const char * new_start_of_line;
		int len = Len_leading_token(str, &line_breaks, &new_start_of_line);
		if (!len)
		{
			printf("Lex error\n");
			return;
		}

		int loc_in_line = (int)(str - start_of_line + 1);

		Print_token(str, len, line_number, loc_in_line);

		str += len;

		if (line_breaks)
		{
			line_number += line_breaks;
			start_of_line = new_start_of_line;
		}
	}
}



static char * Try_read_file_to_buffer(FILE * file, char * buf, int len_buf)
{
	int err = fseek(file, 0, SEEK_END);
	if (err)
		return NULL;

	long len_file = ftell(file);
	if (len_file < 0 || len_file >= len_buf)
		return NULL;

	err = fseek(file, 0, SEEK_SET);
	if (err)
		return NULL;

	size_t bytes_read = fread(buf, 1, (size_t)len_file, file);
	if (bytes_read != (size_t)len_file)
		return NULL;

	return buf + len_file;
}

static char * Try_read_file_at_path_to_buffer(const char * fpath, char * buf, int len_buf)
{
	FILE * file = fopen(fpath, "rb");
	if (!file)
		return NULL;

	char * end = Try_read_file_to_buffer(file, buf, len_buf);

	fclose(file); // BUG (matthewd) ignoring return value?

	return end;
}



int main(int argc, char *argv[])
{
	const int len_file_buf = 0x100000;

	if (argc != 2)
	{
		printf("wrong number of arguments, expect a single file name\n");
		return 1;
	}

	char * file_buf = (char *)calloc((size_t)len_file_buf, 1);
	if (!file_buf)
	{
		printf("Failed to allocate memory \n");
		return 1;
	}

	char * end = Try_read_file_at_path_to_buffer(argv[1], file_buf, len_file_buf);
	if (!end)
	{
		printf("Failed to read file '%s'.\n", argv[1]);
		return 1;
	}

	Print_toks_in_str(file_buf);

	return 0;
}