
#include <stdlib.h>
#include <stdbool.h>
#include <string.h> // strpbrk

#include "lex.h"



// char helpers

static bool Is_horizontal_white_space(char ch)
{
	return ch == ' ' || ch == '\t' || ch == '\f' || ch == '\v';
}

static bool Is_digit(char ch)
{
	unsigned char uch = (unsigned char)ch;
	uch -= '0';
	return uch < 10;
}

static bool Is_line_break(char ch)
{
	return ch == '\n' || ch == '\r'; 
}

static bool Is_lowercase(char ch)
{
	unsigned char uch = (unsigned char)ch;
	uch -= 'a';
	return uch < 26;
}

static bool Is_uppercase(char ch)
{
	unsigned char uch = (unsigned char)ch;
	uch -= 'A';
	return uch < 26;
}

static bool Is_letter(char ch)
{
	return Is_lowercase(ch | 0x20);
}



// str helpers

static int Len_line_break(const char* str)
{
	if (str[0] == '\n')
		return 1;

	if (str[0] == '\r')
	{
		if (str[1] == '\n')
			return 2;

		return 1;
	}

	return 0;
}

static int Len_line_continue(const char * str)
{
	const char * str_peek = str;

	if (str_peek[0] != '\\')
		return 0;

	++str_peek;

	// Skip whitespace after the backslash as an extension

	while (Is_horizontal_white_space(str_peek[0]))
	{
		++str_peek;
	}

	int len_line_break = Len_line_break(str_peek);
	if (len_line_break == 0)
		return 0;

	return (int)(str_peek - str) + len_line_break;
}

static int Len_line_continues(const char * str, int * out_num_lines)
{
	const char * str_peek = str;
	int num_lines = 0;

	while (true)
	{
		int len_line_continue = Len_line_continue(str_peek);
		if (!len_line_continue)
			break;

		str_peek += len_line_continue;
		++num_lines;
	}

	if (out_num_lines)
	{
		*out_num_lines = num_lines;
	}

	return (int)(str_peek - str);
}



// input_t

void Init_input(input_t * input, const char * str)
{
	input->str = str;
	input->line_start = str;
	input->line = 1;
}

__declspec(noinline) static char Peek_input_slow(input_t * input)
{
	int len_line_continue = Len_line_continues(input->str, NULL);
	if (!len_line_continue)
		return input->str[0];

	return input->str[len_line_continue];
}

static char Peek_input(input_t * input)
{
	char ch = input->str[0];
	if (ch != '\\')
		return ch;

	return Peek_input_slow(input);
}

__declspec(noinline) static void Advance_input_slow(input_t * input)
{
	int num_lines;
	int len_line_continue = Len_line_continues(input->str, &num_lines);
	if (!len_line_continue)
	{
		++input->str;
		return;
	}

	input->str += len_line_continue;
	input->line_start = input->str;
	input->line += num_lines;

	if (input->str[0] != '\0')
	{
		++input->str;
	}
}

// NOTE you should not call Advance_input
//  if Peek_input(input) might be a line break...

static void Advance_input(input_t * input)
{
	if (input->str[0] != '\\')
	{
		++input->str;
		return;
	}
	
	Advance_input_slow(input);
}



// Main lex function (and helpers)

static bool Skip_horizontal_white_space(const char ** p_str);

static bool Lex_after_carriage_return(input_t * input);
static bool Lex_after_line_break(input_t * input);

static bool Lex_rest_of_ppnum(input_t * input);
static bool Lex_after_dot(input_t * input);

static bool Lex_after_fslash(input_t * input);
static bool Lex_rest_of_block_comment(input_t * input);
static bool Lex_rest_of_line_comment(input_t * input);

static bool Lex_after_u(input_t * input);
static bool Lex_after_U(input_t * input);
static bool Lex_after_L(input_t * input);
static bool Lex_rest_of_str_lit(char ch_sential, input_t * input);

static bool Lex_rest_of_id(input_t * input);

static bool Lex_after_percent(input_t * input);
static bool Lex_after_percent_colon(input_t * input);
static bool Lex_after_lt(input_t * input);
static bool Lex_after_gt(input_t * input);
static bool Lex_after_bang(input_t * input);
static bool Lex_after_htag(input_t * input);
static bool Lex_after_amp(input_t * input);
static bool Lex_after_star(input_t * input);
static bool Lex_after_plus(input_t * input);
static bool Lex_after_minus(input_t * input);
static bool Lex_after_colon(input_t * input);
static bool Lex_after_eq(input_t * input);
static bool Lex_after_caret(input_t * input);
static bool Lex_after_vbar(input_t * input);

bool Lex(input_t * input)
{
	// This is a very perf sensitive function.
	//	roughly half of the time (that we are not in some other function)
	//  is spent on the 'switch (ch)' line, and roughly the other half 
	//  is spent on function proloug + epilogue

	// To avoid the prologue + epilogue perf hit, we probably could/should
	//  make this function lex a few tokens at once. We probably do not want to go all the way
	//  and lex the whole file at once (that would be a lot of memory to store the lex-ed tokens),
	//  but if me maintained a small token buffer, we could read several tokens in batches, to 
	//  minimize the cost of this functions prologue + epilogue.
	//  ... we could also try to force this funnction be inlined, but that sounds like a bad idea,
	//  and reading tokens in batches sounds like it would just be better generally

	// As for the 'switch (ch)' ... that is harder
	//  A fair bit of the time spent there is to deal with the fact that we need to
	//  patch the jump offsets against the current RIP. If we had "computed goto's"
	//  like in gcc, or if we did some dark linker magic, we could maybe get the
	//  OS loader to patch these on load.
	//
	//  A larger idea, would be to pivot to a lexer style like in 
	//   https://nothings.org/computer/lexing.html
	//   I tried this a while ago, and the perf was not impressive
	//   (at least in my first version), but I wonder if it might be worth
	//   revisting at some point... it would be a massive re-write, though...

	// An even larger idea, would be to try and SIMD-ify things, that is,
	//  to classify blocks of characters at once, and then be able to
	//  skip over more than one char at a time, instead of spending so much time
	//  doing "++input->str;". But, that would be a lot of work, I am not
	//  familar with writing simd code, and I wonder if the gains would even really be that big,
	//  since is real code you tend to not have super long tokens...

	// NOTE (matthewd) you may be tempted to try and pull up all the 
	//  "++input->str;"s below, and just do "--input->str;" in 
	//  the '\\' and '\0' cases, but some initial profiling seemed 
	//  to suggest that was actually SLOWER than the way it is now
	//  ... which I do not fully understand ...

switch_on_str_0:
	char ch = input->str[0];
	switch (ch)
	{
	case '\\':
		{
			// Doing a bespoke version of Peek_input + Advance_input here.
			//  This lets us handle '\\' in this top level switch,
			//  AND means we only have to do "++input->str;" in
			//  all the other cases.
			//
			// This is more opaque than I would like, but
			//  the perf gain is worth it

			int num_lines;
			int len_line_continue = Len_line_continues(input->str, &num_lines);
			if (len_line_continue)
			{
				input->str += len_line_continue;
				input->line_start = input->str;
				input->line += num_lines;

				goto switch_on_str_0;
			}
			else
			{
				++input->str; // wrong, should handle UCNs
				return true;
			}
		}

	case '\0':
		return false;

	case ' ':
	case '\t':
	case '\f':
	case '\v':
		++input->str;
		return Skip_horizontal_white_space(&input->str);
		
	case '\r':
		++input->str;
		return Lex_after_carriage_return(input);

	case '\n':
		++input->str;
		return Lex_after_line_break(input);

	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		++input->str;
		return Lex_rest_of_ppnum(input);

	case '.':
		++input->str;
		return Lex_after_dot(input);

	case '/':
		++input->str;
		return Lex_after_fslash(input);

	case 'u':
		++input->str;
		return Lex_after_u(input);

	case 'U':
		++input->str;
		return Lex_after_U(input);

	case 'L':
		++input->str;
		return Lex_after_L(input);

	case '"':
		++input->str;
		return Lex_rest_of_str_lit('"', input);

	case '\'':
		++input->str;
		return Lex_rest_of_str_lit('\'', input);

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
		++input->str;
		return Lex_rest_of_id(input);

	case '%':
		++input->str;
		return Lex_after_percent(input);

	case '<':
		++input->str;
		return Lex_after_lt(input);

	case '>':
		++input->str;
		return Lex_after_gt(input);

	case '!':
		++input->str;
		return Lex_after_bang(input);

	case '#':
		++input->str;
		return Lex_after_htag(input);

	case '&':
		++input->str;
		return Lex_after_amp(input);

	case '*':
		++input->str;
		return Lex_after_star(input);

	case '+':
		++input->str;
		return Lex_after_plus(input);

	case '-':
		++input->str;
		return Lex_after_minus(input);
		
	case ':':
		++input->str;
		return Lex_after_colon(input);

	case '=':
		++input->str;
		return Lex_after_eq(input);

	case '^':
		++input->str;
		return Lex_after_caret(input);

	case '|':
		++input->str;
		return Lex_after_vbar(input);

	case '(':
		++input->str;
		return true;
	case ')':
		++input->str;
		return true;
	case ',':
		++input->str;
		return true;
	case ';':
		++input->str;
		return true;
	case '?':
		++input->str;
		return true;
	case '[':
		++input->str;
		return true;
	case ']':
		++input->str;
		return true;
	case '{':
		++input->str;
		return true;
	case '}':
		++input->str;
		return true;
	case '~':
		++input->str;
		return true;

	case '`':
	case '@':
		++input->str;
		return true;

	case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
	case 0x08: /* '\t' */ /* '\n' */ /* '\v' */ /* '\f' */ /* '\r' */ case 0x0E:
	case 0x0F: case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15:
	case 0x16: case 0x17: case 0x18: case 0x19: case 0x1A: case 0x1B: case 0x1C:
	case 0x1D: case 0x1E: case 0x1F:
	case 0x7f:
		++input->str;
		return true;

	default: // wrong, should handle utf8
		++input->str;
		return true;
	}
}

static bool Skip_horizontal_white_space(const char ** p_str)
{
	const char * str = *p_str;

	while (true)
	{
		if (!Is_horizontal_white_space(*str))
			break;

		++str;
	}

	*p_str = str;

	return true;
}

static bool Lex_after_carriage_return(input_t * input)
{
	if (input->str[0] == '\n')
	{
		++input->str;
	}

	return Lex_after_line_break(input);
}

static bool Lex_after_line_break(input_t * input)
{
	const char * str = input->str;
	const char * line_start = str;

	int line = input->line + 1;

	while (true)
	{
		Skip_horizontal_white_space(&str);

		int len_line_break = Len_line_break(str);
		if (!len_line_break)
			break;

		// Assume that where there is one line break,
		//  there may be another ...
		// BUG double check this is actually better perf
		//  than just going back up to Skip_white_space ...

		do
		{
			str += len_line_break;
			++line;

			len_line_break = Len_line_break(str);
		} 
		while (len_line_break);

		line_start = str;
	}

	input->str = str;
	input->line_start = line_start;
	input->line = line;

	return true;
}

static bool Lex_after_dot(input_t * input)
{
	char ch = Peek_input(input);
	
	if (Is_digit(ch))
	{
		Advance_input(input);
		return Lex_rest_of_ppnum(input);
	}

	if (ch == '.')
	{
		input_t input_peek = *input;
		Advance_input(&input_peek);

		if (Peek_input(&input_peek) == '.')
		{
			Advance_input(&input_peek);
			*input = input_peek;
		}
	}

	return true;
}

static bool Lex_after_fslash(input_t * input)
{
	switch (Peek_input(input))
	{
	case '*':
		Advance_input(input);
		return Lex_rest_of_block_comment(input);
	case '/':
		Advance_input(input);
		return Lex_rest_of_line_comment(input);
	case '=':
		Advance_input(input);
		return true;
	default:
		return true;
	}
}

static bool Lex_rest_of_block_comment(input_t * input)
{
	while (true)
	{
		// char *strpbrk( const char *dest, const char *breakset );
		//  Scans the null-terminated byte string pointed to by dest 
		//  for any character from the null-terminated byte string 
		//  pointed to by breakset, and returns a pointer to that character.
		//  returns null pointer if no such character exists

		const char * pbrk = strpbrk(input->str, "*\n\r");

		// Deal with unterminated block comment

		if (!pbrk)
			break;

		// Deal with line breaks

		// NOTE not using Len_line_break here, 
		//  since we are checking for '\r'/'\n' ourselves

		char ch = pbrk[0];
		if (ch == '\r')
		{
			if (pbrk[1] == '\n')
			{
				++pbrk;
			}
			goto after_line_break;
		}

		if (ch == '\n')
		{
		after_line_break:
			++pbrk;
			input->str = pbrk;
			input->line_start = pbrk;
			input->line += 1;
			continue;
		}
		
		// ch == '*', so check for closing "*/"

		if (pbrk[1] == '/')
		{
			input->str = pbrk + 2;
			break;
		}

		// stray '*', just move on

		input->str = pbrk + 1;
	}

	return true;
}

static bool Lex_rest_of_line_comment(input_t * input)
{
	while (true)
	{
		char ch = input->str[0];

		if (ch == '\r' || ch == '\n' || ch == '\0')
			break;

		if (ch == '\\')
		{
			// BUG this is duplicated from the '\\' case in Lex...
			//  should commonize?

			int num_lines;
			int len_line_continue = Len_line_continues(input->str, &num_lines);
			if (len_line_continue)
			{
				input->str += len_line_continue;
				input->line_start = input->str;
				input->line += num_lines;

				continue;
			}
		}

		++input->str;
	}

	return true;
}

static bool Lex_after_u(input_t * input)
{
	if (Peek_input(input) == '8')
	{
		Advance_input(input);

		// Reusing Lex_after_U because it does what we want

		return Lex_after_U(input);
	}

	return Lex_rest_of_id(input);
}

static bool Lex_after_U(input_t * input)
{
	if (Peek_input(input) == '"')
	{
		Advance_input(input);
		return Lex_rest_of_str_lit('"', input);
	}

	return Lex_rest_of_id(input);
}

static bool Lex_after_L(input_t * input)
{
	char ch = Peek_input(input);
	if (ch == '"' || ch == '\'')
	{
		Advance_input(input);
		return Lex_rest_of_str_lit(ch, input);
	}

	return Lex_rest_of_id(input);
}

static bool Lex_rest_of_str_lit(char ch_sential, input_t * input)
{
	//??? TODO support utf8 chars? or do we get that for free?
	//  should probably at least check for mal-formed utf8, instead
	//  of just accepting all the bytes between the quotes

	while (true)
	{
		char ch0 = Peek_input(input);

		// In raw lexing mode, we accept string/char literals without the closing quote

		if (ch0 == '\0')
			return true;

		if (Is_line_break(ch0))
			return true;

		// Whatever ch is at this point, we know
		//  we are going to include it in the str lit

		Advance_input(input);

		// Check for closing quote

		if (ch0 == ch_sential)
			return true;

		// Deal with back slash

		if (ch0 == '\\')
		{
			char ch1 = Peek_input(input);

			if (ch1 == '\0')
				return true;

			if (Is_line_break(ch1))
				return true;

			Advance_input(input);
		}
	}

	return true;
}

static bool Lex_rest_of_id(input_t * input)
{
	//??? todo add support for universal-character-names
	//??? todo support utf8 in ids

	while (true)
	{
		char ch = input->str[0];

		// We assume lower case letters are by far
		//  the most common thing in id's, so we
		//  check for multiple once we see one
		//  as an optimization

		if (Is_lowercase(ch))
		{
			while (true)
			{
				++input->str;
				ch = input->str[0];
				if (!Is_lowercase(ch))
					break;
			}
		}

		// For all other valid chars, we expect to only
		//  see one (in the most common case), and then
		//  start seeing lower case letters again

		if (Is_uppercase(ch))
		{
			++input->str;
			continue;
		}

		if (ch == '_')
		{
			++input->str;
			continue;
		}

		if (Is_digit(ch))
		{
			++input->str;
			continue;
		}

		if (ch == '$') // '$' allowed as an extension :/
		{
			++input->str;
			continue;
		}

		if (ch == '\\')
		{
			// Deal with line continues...

			ch = Peek_input_slow(input);

			if (Is_letter(ch) || 
				Is_digit(ch) || 
				ch == '_' ||
				ch == '$')
			{
				Advance_input_slow(input);
				continue;
			}
		}

		// Found a non-id char. return now that we have found the
		//  end of the current id.

		// BUG returning true here is confusing, but eventually
		//  we will be returning some TOKEN kind enum, and this will make
		//  more sense. We return a value from this function,
		//  (and the other lex_ helper functions) so that they can
		//  be tail calls in the main lex function

		return true;
	}
}

static bool Lex_after_percent(input_t * input)
{
	switch (Peek_input(input))
	{
	case ':':
		Advance_input(input);
		return Lex_after_percent_colon(input);

	case '=':
		Advance_input(input);
		return true;
	case '>':
		Advance_input(input);
		return true;
	default:
		return true;
	}
}

static bool Lex_after_percent_colon(input_t * input)
{
	if (Peek_input(input) == '%')
	{
		input_t input_peek = *input;
		Advance_input(&input_peek);

		if (Peek_input(&input_peek) == ':')
		{
			Advance_input(&input_peek);
			*input = input_peek;
		}
	}

	return true;
}

static bool Lex_after_lt(input_t * input)
{
	switch (Peek_input(input))
	{
	case '<':
		Advance_input(input);
		if (Peek_input(input) == '=')
		{
			Advance_input(input);
			return true;
		}
		return true;

	case '%':
		Advance_input(input);
		return true;

	case ':':
		Advance_input(input);
		return true;

	case '=':
		Advance_input(input);
		return true;

	default:
		return true;
	}
}

static bool Lex_after_gt(input_t * input)
{
	switch (Peek_input(input))
	{
	case '>':
		Advance_input(input);
		if (Peek_input(input) == '=')
		{
			Advance_input(input);
			return true;
		}
		return true;

	case '=':
		Advance_input(input);
		return true;

	default:
		return true;
	}
}

static bool Lex_after_bang(input_t * input)
{
	if (Peek_input(input) == '=')
	{
		Advance_input(input);
	}

	return true;
}

static bool Lex_after_htag(input_t * input)
{
	if (Peek_input(input) == '#')
	{
		Advance_input(input);
	}

	return true;
}

static bool Lex_after_amp(input_t * input)
{
	switch (Peek_input(input))
	{
	case '&':
		Advance_input(input);
		return true;
	case '=':
		Advance_input(input);
		return true;
	default:
		return true;
	}
}

static bool Lex_after_star(input_t * input)
{
	if (Peek_input(input) == '=')
	{
		Advance_input(input);
	}

	return true;
}

static bool Lex_after_plus(input_t * input)
{
	switch (Peek_input(input))
	{
	case '+':
		Advance_input(input);
		return true;
	case '=':
		Advance_input(input);
		return true;
	default:
		return true;
	}
}

static bool Lex_after_minus(input_t * input)
{
	switch (Peek_input(input))
	{
	case '>':
		Advance_input(input);
		return true;
	case '-':
		Advance_input(input);
		return true;
	case '=':
		Advance_input(input);
		return true;
	default:
		return true;
	}
}

static bool Lex_after_colon(input_t * input)
{
	if (Peek_input(input) == '>')
	{
		Advance_input(input);
	}

	return true;
}

static bool Lex_after_eq(input_t * input)
{
	if (Peek_input(input) == '=')
	{
		Advance_input(input);
	}

	return true;
}

static bool Lex_after_caret(input_t * input)
{
	if (Peek_input(input) == '=')
	{
		Advance_input(input);
	}

	return true;
}

static bool Lex_after_vbar(input_t * input)
{
	switch (Peek_input(input))
	{
	case '|':
		Advance_input(input);
		return true;
	case '=':
		Advance_input(input);
		return true;
	default:
		return true;
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
	[a-zA-Z_] OR a "universal character name"

	oh, and '$' is included in identifier_nondigit as well,
	since we support that as an 'implentation defined identifier character'
*/

// Len_rest_of_pp_num is called after we see ( '.'? [0-9] ), that is, pp_num_start
// 'rest_of_pp_num' is equivalent to pp_num_continue*

static bool Lex_rest_of_ppnum(input_t * input)
{
	while (true)
	{
		char ch = input->str[0]; 

	after_read_ch:

		if (Is_digit(ch))
		{
			// Look for digits in a loop, sicne they
			//  are the most common thing in ppnums

			while (true)
			{
				++input->str;
				ch = input->str[0];
				if (!Is_digit(ch))
					break;
			}
		}

		// BUG assuming normal float literals are more common than
		//  hex/etc literals. In some code, letters may actually be more common
		//  than '.', but we have to pick one of them to optimize for.
		//  In either case, Is_letter is more expensive than comparing to '.'
		//  so it seems 'fine' to check '.' first

		if (ch == '.')
		{
			++input->str;
			continue;
		}

		if (Is_letter(ch))
		{
			++input->str;

			// deal with float exponents

			// NOTE We peek at the next char, 
			//  since '+'/'-' in ppnums is less common than 'e'/'E' 
			//  (since 'e'/'E' are valid hex digits, and I think
			//  it is safe to say that hex literals are more common that
			//  float literals with exponents)
			//  So, this way, we leave the slow 'E'/'P' check for the case
			//  where we actually see a '+'/'-'

			char ch0 = ch;
			ch = input->str[0];

			if (ch == '-' || ch == '+')
			{
				if (ch0 == 'e' || ch0 == 'E' || ch0 == 'p' || ch0 == 'P')
				{
					++input->str;
					continue;
				}
				
				// '+'/'-' not after 'E'/'P'? then this is the end
				//  of this ppnum.

				break;
			}

			goto after_read_ch;
		}

		// The lang spec says these can show up in ppnums, but
		//  highly doubt there is any real code where they do...

		if (ch == '_' || ch == '$')
		{
			++input->str;
			continue;
		}

		// deal with line continues... (should be very uncommon)

		if (ch == '\\')
		{
			ch = Peek_input_slow(input);

			if (Is_digit(ch) ||
				ch == '.' ||
				Is_letter(ch) ||
				ch == '_' || ch == '$')
			{
				Advance_input_slow(input);
				--input->str;
				goto after_read_ch;
			}
		}

		break;
	}

	return true;
}