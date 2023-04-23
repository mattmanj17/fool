
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h> // for calloc



static bool Is_horizontal_whitespace(char ch)
{
	return ch == ' ' || ch == '\t' || ch == '\f' || ch == '\v';
}

static bool Is_line_break(char ch)
{
	return ch == '\r' || ch == '\n';
}

static bool Is_digit(char ch)
{
	ch -= '0';
	return ch >= 0 && ch < 10;
}

static bool Can_start_id(char ch)
{
	if (ch == '$') // allowed in ids as an extention :/
		return true;

	if (ch == '_')
		return true;

	// make lower case

	ch |= 0x20;

	// Check if letter

	ch -= 'a';
	return ch >= 0 && ch < 26;
}

static bool Extends_id(char ch)
{
	return Is_digit(ch) || Can_start_id(ch);
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

// BUG clang has weird handling of line breaks + white space in raw mode...
//  it returns whitespace before line breaks as their own token,
//  but merges all adjacent lines breaks/white space after a line break into
//  one token... this goo is to match that

static int Len_leading_line_break_or_space(
	const char * str, 
	int * line_breaks,
	const char ** new_start_of_line)
{
	int len_ws = Len_leading_horizontal_whitespace(str);
	if (len_ws)
		return len_ws;

	int len_break = Len_line_break(str);
	if (len_break)
	{
		*line_breaks += 1;
		*new_start_of_line = str + len_break;
		return len_break;
	}

	return 0;
}

static int Len_leading_line_breaks(
	const char * str, 
	int * line_breaks,
	const char ** new_start_of_line)
{
	const char * begin = str;
	str += Len_line_break(str);

	if (str == begin)
		return 0;
	
	*line_breaks = 1;
	*new_start_of_line = str;

	while (true)
	{
		int len = Len_leading_line_break_or_space(str, line_breaks, new_start_of_line);
		if (!len)
			break;

		str += len;
	}
		
	return (int)(str - begin);
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

static int Len_leading_str_lit(const char * str)
{
	//??? TODO support utf8 chars? or do we get that for free?
	//  should probably at least check for mal-formed utf8, instead
	//  of just accepting all the bytes between the quotes

	const char * begin = str;

	str += Length_of_str_encoding_prefix(str);

	if (str[0] != '"')
		return 0; // we only call this if we see a '"' ...

	++str;

	while (true)
	{
		char ch0 = str[0];

		if (ch0 == '\0')
			break;

		if (Is_line_break(ch0))
			break;

		++str;

		if (ch0 == '"')
			break;

		if (ch0 == '\\')
		{
			// In raw lexing mode, the only escapes
			//  that matters are '\"' and '\\'. Otherwise, we just
			//  'allow '\\' in strings like a normal char,
			//  and wait till parsing/etc to validate escapes

			if (str[0] == '"' || str[0] == '\\')
			{
				++str;
			}
		}
	}

	return (int)(str - begin);
}



// BUG almost a duplicate of Len_leading_str_lit...

static int Len_leading_char_lit(const char * str)
{
	const char * begin = str;

	if (str[0] == 'L')
		++str;

	if (str[0] != '\'')
		return 0; // we only ever call this if we saw a '\''

	++str;

	while (true)
	{
		char ch0 = str[0];

		if (ch0 == '\0')
			break;

		if (Is_line_break(ch0))
			break;

		++str;

		if (ch0 == '\'')
			break;

		if (ch0 == '\\')
		{
			if (str[0] == '\'' || str[0] == '\\')
			{
				++str;
			}
		}
	}

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
	//??? todo support utf8 in ids

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



static int Len_leading_token(
	const char * str, 
	int * line_breaks,
	const char ** new_start_of_line)
{
	*line_breaks = 0;

	char ch0 = str[0];
	char ch1 = (ch0) ? str[1] : '\0'; 

	switch (ch0)
	{
	case '\0':
		return 0; // EOF

	case ' ':
	case '\t':
	case '\f':
	case '\v':
		return Len_leading_horizontal_whitespace(str); // whitespace

	case '\r':
	case '\n':
		return Len_leading_line_breaks(str, line_breaks, new_start_of_line); // line breaks

	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		return Len_leading_pp_num(str); // pp_num

	case '.':
		if (Is_digit(ch1))
			return Len_leading_pp_num(str); // pp_num
		if (ch1 == '.' && str[2] == '.')
			return 3; // ...
		return 1; // .

	case '/':
		switch (ch1)
		{
		case '*':
			return Len_leading_block_comment(str, line_breaks, new_start_of_line); // /*
		case '/':
			return Len_leading_line_comment(str); // //
		case '=':
			return 2; // /=
		default:
			return 1; // /
		}

	case 'u':
		if (ch1 == '8' && str[2] == '"')
			return Len_leading_str_lit(str); // u8""
		return Len_leading_id(str); // identifier

	case 'U':
		if (ch1 == '"')
			return Len_leading_str_lit(str); // U""
		return Len_leading_id(str); // identifier

	case 'L':
		switch (ch1)
		{
		case '"':
			return Len_leading_str_lit(str); // L""
		case '\'':
			return Len_leading_char_lit(str); // L''
		default:
			return Len_leading_id(str); // identifier
		}

	case '"':
		return Len_leading_str_lit(str); // ""

	case '\'':
		return Len_leading_char_lit(str); // ''

	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
	case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
	case 'o': case 'p': case 'q': case 'r': case 's': case 't': /* 'u' */
	case 'v': case 'w': case 'x': case 'y': case 'z':
	case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
	case 'H': case 'I': case 'J': case 'K': /* 'L' */ case 'M': case 'N':
	case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': /* 'U' */
	case 'V': case 'W': case 'X': case 'Y': case 'Z':
	case '_':
	case '$': // $ allowed in ids as an extension :/
		return Len_leading_id(str); // identifier

	case '%':
		switch (ch1)
		{
		case ':':
			if (str[2] == '%' && str[3] == ':')
				return 4; // %:%:
			else
				return 2; // %:
		case '=':
			return 2; // %=
		case '>':
			return 2; // %>
		default:
			return 1; // %
		}

	case '<':
		switch (ch1)
		{
		case '<':
			if (str[2] == '=')
				return 3; // <<=
			else
				return 2; // <<
		case '%':
			return 2; // <%
		case ':':
			return 2; // <:
		case '=':
			return 2; // <=
		default:
			return 1; // <
		}
	case '>':
		switch (ch1)
		{
		case '>':
			if (str[2] == '=')
				return 3; // >>=
			else
				return 2; // >>
		case '=':
			return 2; // >=
		default:
			return 1; // >
		}
	case '!':
		if (ch1 == '=')
			return 2; // !=
		return 1; // !
	case '#':
		if (ch1 == '#')
			return 2; // ##
		return 1; // #
	case '&':
		switch (ch1)
		{
		case '&':
			return 2; // &&
		case '=':
			return 2; // &=
		default:
			return 1; // &
		}
	case '*':
		if (ch1 == '=')
			return 2; // *=
		return 1; // *
	case '+':
		switch (ch1)
		{
		case '+':
			return 2; // ++
		case '=':
			return 2; // +=
		default:
			return 1; // +
		}
	case '-':
		switch (ch1)
		{
		case '>':
			return 2; // ->
		case '-':
			return 2; // --
		case '=':
			return 2; // -=
		default:
			return 1; // -
		}
	case ':':
		if (ch1 == '>')
			return 2; // :>
		return 1; // :
	case '=':
		if (ch1 == '=')
			return 2; // ==
		return 1; // =
	case '^':
		if (ch1 == '=')
			return 2; // ^=
		return 1; // ^
	case '|':
		switch (ch1)
		{
		case '|':
			return 2; // ||
		case '=':
			return 2; // |=
		default:
			return 1; // |
		}
	case '(':
		return 1; // (
	case ')':
		return 1; // )
	case ',':
		return 1; // ,
	case ';':
		return 1; // ;
	case '?':
		return 1; // ?
	case '[':
		return 1; // [
	case ']':
		return 1; // ]
	case '{':
		return 1; // {
	case '}':
		return 1; // }
	case '~':
		return 1; // ~

	case '`':
	case '@':
		return 1; // error, bad punct

	case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
	case 0x08: /* '\t' */ /* '\n' */ /* '\v' */ /* '\f' */ /* '\r' */ case 0x0E:
	case 0x0F: case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15:
	case 0x16: case 0x17: case 0x18: case 0x19: case 0x1A: case 0x1B: case 0x1C:
	case 0x1D: case 0x1E: case 0x1F:
	case 0x7f:
		return 1; // error, ctrl char
	}

	// (ch0 > 0x7f) || (ch0 == '\\')

	return 0;
}


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



static char * Try_read_file_to_buffer(FILE * file)
{
	int err = fseek(file, 0, SEEK_END);
	if (err)
		return NULL;

	long len_file = ftell(file);
	if (len_file < 0)
		return NULL;

	err = fseek(file, 0, SEEK_SET);
	if (err)
		return NULL;

	char * buf = (char *)calloc((size_t)(len_file + 1), 1);
	if (!buf)
		return NULL;

	size_t bytes_read = fread(buf, 1, (size_t)len_file, file);
	if (bytes_read != (size_t)len_file)
		return NULL;

	return buf;
}

static char * Try_read_file_at_path_to_buffer(const char * fpath)
{
	FILE * file = fopen(fpath, "rb");
	if (!file)
		return NULL;

	char * buf = Try_read_file_to_buffer(file);

	fclose(file); // BUG (matthewd) ignoring return value?

	return buf;
}



int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		printf("wrong number of arguments, expect a single file name\n");
		return 1;
	}

	char * file_buf = Try_read_file_at_path_to_buffer(argv[1]);
	if (!file_buf)
	{
		printf("Failed to read file '%s'.\n", argv[1]);
		return 1;
	}

	Print_toks_in_str(file_buf);

	return 0;
}