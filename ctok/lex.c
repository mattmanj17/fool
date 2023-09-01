
#include <stdlib.h>
#include <stdbool.h>

#include "lex.h"

#include "utf8.h"
#include "unicode_ids.h"


// codepoint helpers

static bool Is_ch_horizontal_white_space(char ch)
{
	return ch == ' ' || ch == '\t' || ch == '\f' || ch == '\v';
}

static bool Is_ch_white_space(char ch)
{
	return ch == ' ' || ch == '\t' || ch == '\f' || ch == '\v' || ch == '\n' || ch == '\r';
}

static bool Is_ascii_digit(uint32_t cp)
{
	return cp >= '0' && cp <= '9';
}

static bool Is_ascii_lowercase(uint32_t cp)
{
	return cp >= 'a' && cp <= 'z';
}

static bool Is_ascii_uppercase(uint32_t cp)
{
	return cp >= 'A' && cp <= 'Z';
}



// Line continues

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

static int Len_line_continues(const char * str, int * out_num_lines)
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



// input_t

void Init_input(input_t * input, const char * cursor, const char * terminator)
{
	input->cursor = cursor;
	input->terminator = terminator;
	input->line_start = cursor;
	input->line = 1;
}

bool Does_input_physically_start_with_horizontal_whitespace(input_t * input)
{
	return Is_ch_horizontal_white_space(input->cursor[0]);
}

void Skip_horizontal_whitespace(input_t * input)
{
	while (Is_ch_horizontal_white_space(input->cursor[0]))
	{
		++input->cursor;
	}
}

bool Is_input_exhausted(input_t * input)
{
	return input->cursor >= input->terminator;
}

typedef struct
{
	const char * new_line_start;
	uint32_t cp;
	int length;
	int num_lines;

	int _padding;
} peek_res_t;

peek_res_t Peek_input_internal(input_t * input, bool discard_unicode_after_line_escape)
{
	peek_res_t res;
	res.length = 0;
	res.num_lines = 0;
	res.new_line_start = input->line_start;

	const char * str = input->cursor;
	if (str[0] == '\\')
	{
		int num_lines;
		int len_line_continue = Len_line_continues(str, &num_lines);
		if (len_line_continue)
		{
			str += len_line_continue;

			res.length = len_line_continue;
			res.num_lines = num_lines;
			res.new_line_start = str;
		}

		// BUG should handle universal character names
	}

	if (str == input->terminator)
	{
		res.cp = '\0';
		return res;
	}

	unsigned char ch = (unsigned char)str[0];
	if (ch == '\n' || ch == '\r')
	{
		res.cp = '\n';
		++res.length;

		res.new_line_start = str + 1;
		++res.num_lines;

		if (ch == '\r' && str[1] == '\n')
		{
			++res.new_line_start;
			++res.length;
		}
	}
	else if (ch > 0x7f)
	{
		input_byte_span_t span;
		span.cursor = (const uint8_t *)str;
		span.max = (const uint8_t *)input->terminator;

		uint32_t cp;
		utf8_decode_error_t err = Try_decode_utf8(&span, &cp);

		if (err == utf8_decode_ok)
		{
			// Turbo hack to replicate a bug in clang
			// https://github.com/llvm/llvm-project/issues/65156

			if (res.length && discard_unicode_after_line_escape)
			{
				res.cp = UINT32_MAX;
			}
			else
			{
				res.cp = cp;
			}

			res.length += (int)((const char *)span.cursor - str);
		}
		else
		{
			res.cp = UINT32_MAX;
			res.length += 1;
		}
	}
	else
	{
		res.cp = ch;
		++res.length;
	}

	return res;
}

uint32_t Peek_input(input_t * input)
{
	return Peek_input_internal(input, false).cp;
}

uint32_t Peek_input_after_id_start_hack(input_t * input)
{
	return Peek_input_internal(input, true).cp;
}

void Advance_input(input_t * input)
{
	peek_res_t peek = Peek_input_internal(input, false);

	input->cursor += peek.length;
	input->line_start = peek.new_line_start;
	input->line += peek.num_lines;
}

void Advance_past_line_continue(input_t * input)
{
	// BUG the fact that this function is needed is awful

	if (input->cursor[0] == '\\')
	{
		int num_lines;
		int len_line_continue = Len_line_continues(input->cursor, &num_lines);
		if (len_line_continue)
		{
			input->cursor += len_line_continue;
			input->line += num_lines;
			input->line_start = input->cursor;
		}
	}
}

void Skip_whitespace(input_t * input)
{
	while (Is_ch_white_space(input->cursor[0]))
	{
		Advance_input(input);
	}
}



// Main lex function (and helpers)

static bool Lex_rest_of_ppnum(input_t * input);
static bool Lex_after_dot(input_t * input);

static bool Lex_after_fslash(input_t * input);
static bool Lex_rest_of_block_comment(input_t * input);
static bool Lex_rest_of_line_comment(input_t * input);

static bool Lex_after_u(input_t * input);
static bool Lex_after_L_or_U(input_t * input);
static bool Lex_rest_of_str_lit(uint32_t cp_sential, input_t * input);

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
	// Check for EOF

	if (Is_input_exhausted(input))
		return false;

	// Special handling of horizontal WS to match clang ... :(

	if (Does_input_physically_start_with_horizontal_whitespace(input))
	{
		Skip_horizontal_whitespace(input);
		return true;
	}

	// Advance input and decide what to do

	uint32_t cp = Peek_input(input);
	Advance_input(input);

	switch (cp)
	{
	case '\0': // Clang skips ws after a bogus \0
	case ' ': // Will likely only see horz ws in this switch if it is after an escaped newline
	case '\t':
	case '\f':
	case '\v':
	case '\n':
		Skip_whitespace(input);
		return  true;

	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		return Lex_rest_of_ppnum(input);

	case '.':
		return Lex_after_dot(input);

	case '/':
		return Lex_after_fslash(input);

	case 'u':
		return Lex_after_u(input);

	case 'U':
		return Lex_after_L_or_U(input);

	case 'L':
		return Lex_after_L_or_U(input);

	case '"':
		return Lex_rest_of_str_lit('"', input);

	case '\'':
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
		return Lex_rest_of_id(input);

	case '%':
		return Lex_after_percent(input);

	case '<':
		return Lex_after_lt(input);

	case '>':
		return Lex_after_gt(input);

	case '!':
		return Lex_after_bang(input);

	case '#':
		return Lex_after_htag(input);

	case '&':
		return Lex_after_amp(input);

	case '*':
		return Lex_after_star(input);

	case '+':
		return Lex_after_plus(input);

	case '-':
		return Lex_after_minus(input);
		
	case ':':
		return Lex_after_colon(input);

	case '=':
		return Lex_after_eq(input);

	case '^':
		return Lex_after_caret(input);

	case '|':
		return Lex_after_vbar(input);

	case '(':
		return true;
	case ')':
		return true;
	case ',':
		return true;
	case ';':
		return true;
	case '?':
		return true;
	case '[':
		return true;
	case ']':
		return true;
	case '{':
		return true;
	case '}':
		return true;
	case '~':
		return true;

	case '`':
	case '@':
	case '\\':
	case '\r': // Peek_input should never return '\r', but here for completeness 
		return true;

	case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
	case 0x08: /* '\t' */ /* '\n' */ /* '\v' */ /* '\f' */ /* '\r' */ case 0x0E:
	case 0x0F: case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15:
	case 0x16: case 0x17: case 0x18: case 0x19: case 0x1A: case 0x1B: case 0x1C:
	case 0x1D: case 0x1E: case 0x1F:
	case 0x7f:
		return true;

	default:
		{
			if (May_non_ascii_codepoint_start_id(lang_ver_c11, cp))
			{
				return Lex_rest_of_id(input);
			}
			else
			{
				return true;
			}
		}
	}
}

static bool Lex_after_dot(input_t * input)
{
	uint32_t cp = Peek_input(input);
	
	if (Is_ascii_digit(cp))
	{
		Advance_input(input);
		return Lex_rest_of_ppnum(input);
	}

	if (cp == '.')
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
	while (!Is_input_exhausted(input))
	{
		uint32_t cp = Peek_input(input);
		Advance_input(input);

		if (cp == '*' && Peek_input(input) == '/')
		{
			Advance_input(input);
			return true;
		}
	}

	return true;
}

static bool Lex_rest_of_line_comment(input_t * input)
{
	while (!Is_input_exhausted(input))
	{
		uint32_t cp = Peek_input(input);

		if (cp == '\n')
			break;

		Advance_input(input);
	}

	return true;
}

static bool Lex_after_u(input_t * input)
{
	if (Peek_input(input) == '8')
	{
		Advance_input(input);
	}

	return Lex_after_L_or_U(input);
}

static bool Lex_after_L_or_U(input_t * input)
{
	uint32_t cp = Peek_input(input);
	if (cp == '"' || cp == '\'')
	{
		Advance_input(input);
		return Lex_rest_of_str_lit(cp, input);
	}

	return Lex_rest_of_id(input);
}

static bool Lex_rest_of_str_lit(uint32_t cp_sential, input_t * input)
{
	//??? TODO support utf8 chars? or do we get that for free?
	//  should probably at least check for mal-formed utf8, instead
	//  of just accepting all the bytes between the quotes

	while (!Is_input_exhausted(input))
	{
		uint32_t cp = Peek_input(input);

		// String without closing quote (which we support in raw lexing mode..)

		if (cp == '\n')
		{
			// BUG hack to match clang

			Advance_past_line_continue(input);
			break;
		}

		// Anything else will be part of the str lit

		Advance_input(input);

		// Closing quote

		if (cp == cp_sential)
			break;

		// Deal with back slash

		if (cp == '\\')
		{
			// Check if escaped char is '\"', '\'', or '\\',
			//  the only escapes we need to handle in raw mode

			cp = Peek_input(input);
			if (cp == cp_sential || cp == '\\')
			{
				Advance_input(input);
			}
		}
	}

	return true;
}

static bool Does_cp_extend_id(uint32_t cp)
{
	if (Is_ascii_lowercase(cp))
		return true;

	if (Is_ascii_uppercase(cp))
		return true;

	if (cp == '_')
		return true;

	if (Is_ascii_digit(cp))
		return true;

	if (cp == '$') // '$' allowed as an extension :/
		return true;

	if (Is_cp_ascii(cp)) // All other ascii is invalid
		return false;

	if (cp == UINT32_MAX) // Bogus utf8 does not extend ids
		return false;

	// We are lexing in 'raw mode', and to match clang, 
	//  once we are parsing an id, we just slurp up all
	//  valid non-ascii-non-whitespace utf8...

	// I do not like this. Clang seems to do the wrong thing here,
	//  and produce an invalid pp token. I suspect no one
	//  actually cares, since dump_raw_tokens is only for debugging...

	if (!Is_unicode_whitespace(cp))
		return true;

	return false;
}

static bool Lex_rest_of_id(input_t * input)
{
	while (true)
	{
		uint32_t cp = Peek_input_after_id_start_hack(input);
		if (Does_cp_extend_id(cp))
		{
			Advance_input(input);
			continue;
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
		uint32_t cp = Peek_input(input);

		if (cp == '.')
		{
			Advance_input(input);
			continue;
		}
		else if (cp == 'e' || cp == 'E' || cp == 'p' || cp == 'P')
		{
			Advance_input(input);
		
			cp = Peek_input(input);
			if (cp == '+' || cp == '-')
			{
				Advance_input(input);
			}

			continue;
		}
		else if (cp == '$')
		{
			// Clang does not allow '$' in ppnums, 
			//  even though the spec would seem to suggest that 
			//  implimentation defined id chars should be included in PP nums...

			break;
		}
		else if (Does_cp_extend_id(cp))
		{
			// Everything (else) which extends ids can extend a ppnum

			Advance_input(input);
			continue;
		}
		else
		{
			// Otherwise, no dice

			break;
		}
	}

	return true;
}