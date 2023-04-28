
#include "lex.h"



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



static void Skip_leading_horizontal_whitespace(input_t * input)
{
	while (true)
	{
		if (!Is_horizontal_whitespace(input->str[0]))
			return;

		++input->str;
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

static void Skip_leading_line_break_or_space(input_t * input)
{
	if (Is_horizontal_whitespace(input->str[0]))
	{
		Skip_leading_horizontal_whitespace(input);
		return;
	}

	int len_break = Len_line_break(input->str);
	if (len_break)
	{
		input->line += 1;
		input->str += len_break;
		input->line_start = input->str;
	}
}

static void Skip_leading_line_breaks(input_t * input)
{
	if (!Is_line_break(input->str[0]))
		return;

	input->str += Len_line_break(input->str);
	input->line += 1;
	input->line_start = input->str;

	while (Is_line_break(input->str[0]) || Is_horizontal_whitespace(input->str[0]))
	{
		Skip_leading_line_break_or_space(input);
	}
}



static void Skip_rest_of_block_comment(input_t * input)
{
	while (true)
	{
		if (input->str[0] == '*' && input->str[1] == '/')
		{
			input->str += 2;
			break;
		}

		int len_line_break = Len_line_break(input->str);
		if (len_line_break)
		{
			input->str += len_line_break;
			input->line += 1;
			input->line_start = input->str;
			continue;
		}

		++input->str;
	}
}



static void Skip_rest_of_line_comment(input_t * input)
{
	while (true)
	{
		char ch = input->str[0];
		if (!ch)
			break;

		if (Is_line_break(ch))
			break;

		++input->str;
	}
}



static void Skip_rest_of_str_lit(char ch_sential, input_t * input)
{
	//??? TODO support utf8 chars? or do we get that for free?
	//  should probably at least check for mal-formed utf8, instead
	//  of just accepting all the bytes between the quotes

	while (true)
	{
		char ch0 = input->str[0];

		// In raw lexing mode, we accept string/char literals without the closing quote

		if (ch0 == '\0')
			break;

		if (Is_line_break(ch0))
			break;

		++input->str;

		if (ch0 == ch_sential)
			break;

		if (ch0 == '\\')
		{
			// In raw lexing mode, the only escapes
			//  that matters are '\"', '\'', and '\\'. Otherwise, we just
			//  'allow '\\' in strings like a normal char,
			//  and wait till parsing/etc to validate escapes

			if (input->str[0] == ch_sential || input->str[0] == '\\')
			{
				++input->str;
			}
		}
	}
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

// Len_rest_of_pp_num is called after we see ( '.'? [0-9] ), that is, pp_num_start
// 'rest_of_pp_num' is equivalent to pp_num_continue*

static void Skip_rest_of_pp_num(input_t * input)
{
	while (true)
	{
		char ch0 = input->str[0];

		if (ch0 == '.')
		{
			++input->str;
		}
		else if (ch0 == 'e' || ch0 == 'E' || ch0 == 'p' || ch0 == 'P')
		{
			char ch1 = input->str[1];
			if (ch1 == '-' || ch1 == '+')
			{
				input->str += 2;
			}
			else
			{
				++input->str;
			}
		}
		else if (Extends_id(ch0))
		{
			++input->str;
		}
		else
		{
			break;
		}
	}
}



static void Skip_rest_of_id(input_t * input)
{
	//??? todo add support for universal-character-names
	//??? todo support utf8 in ids

	while (input->str[0])
	{
		if (!Extends_id(input->str[0]))
			break;

		++input->str;
	}
}



void Skip_leading_token(input_t * input)
{
	char ch0 = input->str[0];
	char ch1 = (ch0) ? input->str[1] : '\0'; 

	switch (ch0)
	{
	case '\0':
		return;

	case ' ':
	case '\t':
	case '\f':
	case '\v':
		Skip_leading_horizontal_whitespace(input);
		return;

	case '\r':
	case '\n':
		Skip_leading_line_breaks(input);
		return;

	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		++input->str;
		Skip_rest_of_pp_num(input);
		return;

	case '.':
		if (Is_digit(ch1))
		{
			input->str += 2;
			Skip_rest_of_pp_num(input);
			return;
		}
		else if (ch1 == '.' && input->str[2] == '.')
		{
			input->str += 3;
			return;
		}
		else
		{
			++input->str;
			return;
		}

	case '/':
		switch (ch1)
		{
		case '*':
			input->str += 2;
			Skip_rest_of_block_comment(input);
			return;
		case '/':
			input->str += 2;
			Skip_rest_of_line_comment(input);
			return;
		case '=':
			input->str += 2;
			return;
		default:
			++input->str;
			return;
		}

	case 'u':
		if (ch1 == '8' && input->str[2] == '"')
		{
			input->str += 3;
			Skip_rest_of_str_lit('"', input);
			return;
		}
		else
		{
			input->str += 1;
			Skip_rest_of_id(input);
			return;
		}

	case 'U':
		if (ch1 == '"')
		{
			input->str += 2;
			Skip_rest_of_str_lit('"', input);
			return;
		}
		else
		{
			input->str += 1;
			Skip_rest_of_id(input);
			return;
		}

	case 'L':
		switch (ch1)
		{
		case '"':
			{
				input->str += 2;
				Skip_rest_of_str_lit('"', input);
				return;
			}
		case '\'':
			{
				input->str += 2;
				Skip_rest_of_str_lit('\'', input);
				return;
			}
		default:
			{
				input->str += 1;
				Skip_rest_of_id(input);
				return;
			}
		}

	case '"':
		input->str += 1;
		Skip_rest_of_str_lit('"', input);
		return;

	case '\'':
		input->str += 1;
		Skip_rest_of_str_lit('\'', input);
		return;

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
		input->str += 1;
		Skip_rest_of_id(input);
		return;

	case '%':
		switch (ch1)
		{
		case ':':
			if (input->str[2] == '%' && input->str[3] == ':')
			{
				input->str += 4;
				return;
			}
			else
			{
				input->str += 2;
				return;
			}
		case '=':
			input->str += 2;
			return;
		case '>':
			input->str += 2;
			return;
		default:
			input->str += 1;
			return;
		}

	case '<':
		switch (ch1)
		{
		case '<':
			if (input->str[2] == '=')
			{
				input->str += 3;
				return;
			}
			else
			{
				input->str += 2;
				return;
			}
		case '%':
			input->str += 2;
			return;
		case ':':
			input->str += 2;
			return;
		case '=':
			input->str += 2;
			return;
		default:
			input->str += 1;
			return;
		}
	case '>':
		switch (ch1)
		{
		case '>':
			if (input->str[2] == '=')
			{
				input->str += 3;
				return;
			}
			else
			{
				input->str += 2;
				return;
			}
		case '=':
			input->str += 2;
			return;
		default:
			input->str += 1;
			return;
		}
		return;
	case '!':
		if (ch1 == '=')
		{
			input->str += 2;
			return;
		}
		else
		{
			input->str += 1;
			return;
		}
	case '#':
		if (ch1 == '#')
		{
			input->str += 2;
			return;
		}
		else
		{
			input->str += 1;
			return;
		}
	case '&':
		switch (ch1)
		{
		case '&':
			input->str += 2;
			return;
		case '=':
			input->str += 2;
			return;
		default:
			input->str += 1;
			return;
		}
	case '*':
		if (ch1 == '=')
		{
			input->str += 2;
			return;
		}
		else
		{
			input->str += 1;
			return;
		}
	case '+':
		switch (ch1)
		{
		case '+':
			input->str += 2;
			return;
		case '=':
			input->str += 2;
			return;
		default:
			input->str += 1;
			return;
		}
	case '-':
		switch (ch1)
		{
		case '>':
			input->str += 2;
			return;
		case '-':
			input->str += 2;
			return;
		case '=':
			input->str += 2;
			return;
		default:
			input->str += 1;
			return;
		}
	case ':':
		if (ch1 == '>')
		{
			input->str += 2;
			return;
		}
		else
		{
			input->str += 1;
			return;
		}
	case '=':
		if (ch1 == '=')
		{
			input->str += 2;
			return;
		}
		else
		{
			input->str += 1;
			return;
		}
	case '^':
		if (ch1 == '=')
		{
			input->str += 2;
			return;
		}
		else
		{
			input->str += 1;
			return;
		}
	case '|':
		switch (ch1)
		{
		case '|':
			input->str += 2;
			return;
		case '=':
			input->str += 2;
			return;
		default:
			input->str += 1;
			return;
		}
	case '(':
		input->str += 1;
		return;
	case ')':
		input->str += 1;
		return;
	case ',':
		input->str += 1;
		return;
	case ';':
		input->str += 1;
		return;
	case '?':
		input->str += 1;
		return;
	case '[':
		input->str += 1;
		return;
	case ']':
		input->str += 1;
		return;
	case '{':
		input->str += 1;
		return;
	case '}':
		input->str += 1;
		return;
	case '~':
		input->str += 1;
		return;

	case '`':
	case '@':
		input->str += 1; // error, bad punct
		return;

	case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
	case 0x08: /* '\t' */ /* '\n' */ /* '\v' */ /* '\f' */ /* '\r' */ case 0x0E:
	case 0x0F: case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15:
	case 0x16: case 0x17: case 0x18: case 0x19: case 0x1A: case 0x1B: case 0x1C:
	case 0x1D: case 0x1E: case 0x1F:
	case 0x7f:
		input->str += 1; // error, ctrl char
		return;
	}

	// (ch0 > 0x7f) || (ch0 == '\\')
}