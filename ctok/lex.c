
#include <stdio.h>

#include "lex.h"

#include "ch.h"
#include "peek.h"
#include "utf8.h"
#include "unicode_ids.h"



// input_t

void Init_input(input_t * input, const char * cursor, const char * terminator)
{
	input->cursor = cursor;
	input->terminator = terminator;
	input->line_start = cursor;
	input->line = 1;

	// Deal with potential UTF-8 BOM

	// Note that we leave line_start pointed at the original cursor.
	//  This means anything on the first line will have their
	//  col num bumped by 3, but that is what clang does, so whatever

	//??? is this worth filing a bug about?

	int num_ch = (int)(terminator - cursor);
	if (num_ch >= 3 && 
		cursor[0] == '\xEF' && 
		cursor[1] == '\xBB' && 
		cursor[2] == '\xBF')
	{
		input->cursor = cursor + 3;
	}
}

static bool Does_input_physically_start_with_horizontal_whitespace(input_t * input)
{
	return Is_ch_horizontal_white_space(input->cursor[0]);
}

static void Skip_horizontal_whitespace(input_t * input)
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

static peek_cp_t Peek_input(input_t * input)
{
	return Peek_cp(input->cursor, input->terminator);
}

static void Advance_input(input_t * input, peek_cp_t peek_cp)
{
	input->cursor += peek_cp.len;
	if (peek_cp.after_last_eol)
	{
		input->line_start = peek_cp.after_last_eol;
	}
	input->line += peek_cp.num_eol;
}

static void Advance_past_line_continues(input_t * input)
{
	// BUG the fact that this function is needed is awful

	peek_cp_t peek_cp;
	Peek_escaped_end_of_lines(input->cursor, input->terminator, &peek_cp);
	Advance_input(input, peek_cp);
}

static void Skip_whitespace(input_t * input)
{
	while (Is_ch_white_space(input->cursor[0]))
	{
		Advance_input(input, Peek_cp(input->cursor, input->terminator));
	}
}



// Main lex function (and helpers)

static void Lex_rest_of_ppnum(input_t * input);
static void Lex_after_dot(input_t * input);

static void Lex_after_fslash(input_t * input);
static void Lex_rest_of_block_comment(input_t * input);
static void Lex_rest_of_line_comment(input_t * input);

static void Lex_after_u(input_t * input);
static void Lex_after_L_or_U(input_t * input);
static void Lex_rest_of_str_lit(uint32_t cp_sential, input_t * input);

static void Lex_rest_of_id(input_t * input);

static void Lex_after_percent(input_t * input);
static void Lex_after_percent_colon(input_t * input);
static void Lex_after_lt(input_t * input);
static void Lex_after_gt(input_t * input);
static void Lex_after_bang(input_t * input);
static void Lex_after_htag(input_t * input);
static void Lex_after_amp(input_t * input);
static void Lex_after_star(input_t * input);
static void Lex_after_plus(input_t * input);
static void Lex_after_minus(input_t * input);
static void Lex_after_colon(input_t * input);
static void Lex_after_eq(input_t * input);
static void Lex_after_caret(input_t * input);
static void Lex_after_vbar(input_t * input);

void Skip_next_token(input_t * input)
{
	// Special handling of horizontal WS to match clang ... :(

	if (Does_input_physically_start_with_horizontal_whitespace(input))
	{
		Skip_horizontal_whitespace(input);
		return;
	}

	// Advance input and decide what to do

	peek_cp_t peek_cp = Peek_cp(input->cursor, input->terminator);
	Advance_input(input, peek_cp);

	switch (peek_cp.cp)
	{
	case '\0': // Clang skips ws after a bogus \0
	case ' ': // Will likely only see horz ws in this switch if it is after an escaped newline
	case '\t':
	case '\f':
	case '\v':
	case '\n':
		Skip_whitespace(input);
		break;

	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		Lex_rest_of_ppnum(input);
		break;

	case '.':
		Lex_after_dot(input);
		break;

	case '/':
		Lex_after_fslash(input);
		break;

	case 'u':
		Lex_after_u(input);
		break;

	case 'U':
		Lex_after_L_or_U(input);
		break;

	case 'L':
		Lex_after_L_or_U(input);
		break;

	case '"':
		Lex_rest_of_str_lit('"', input);
		break;

	case '\'':
		Lex_rest_of_str_lit('\'', input);
		break;

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
		Lex_rest_of_id(input);
		break;

	case '%':
		Lex_after_percent(input);
		break;

	case '<':
		Lex_after_lt(input);
		break;

	case '>':
		Lex_after_gt(input);
		break;

	case '!':
		Lex_after_bang(input);
		break;

	case '#':
		Lex_after_htag(input);
		break;

	case '&':
		Lex_after_amp(input);
		break;

	case '*':
		Lex_after_star(input);
		break;

	case '+':
		Lex_after_plus(input);
		break;

	case '-':
		Lex_after_minus(input);
		break;
		
	case ':':
		Lex_after_colon(input);
		break;

	case '=':
		Lex_after_eq(input);
		break;

	case '^':
		Lex_after_caret(input);
		break;

	case '|':
		Lex_after_vbar(input);
		break;

	case '(':
	case ')':
	case ',':
	case ';':
	case '?':
	case '[':
	case ']':
	case '{':
	case '}':
	case '~':
		break;

	case '`':
	case '@':
	case '\\':
	case '\r': // Peek_input should never return '\r', but here for completeness 
		break;

	case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
	case 0x08: /* '\t' */ /* '\n' */ /* '\v' */ /* '\f' */ /* '\r' */ case 0x0E:
	case 0x0F: case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15:
	case 0x16: case 0x17: case 0x18: case 0x19: case 0x1A: case 0x1B: case 0x1C:
	case 0x1D: case 0x1E: case 0x1F:
	case 0x7f:
		break;

	default:
		{
			if (May_non_ascii_codepoint_start_id(lang_ver_c11, peek_cp.cp))
			{
				Lex_rest_of_id(input);
			}
		}
		break;
	}
}

void Lex(input_t* input, token_t* tok)
{
	tok->str = input->cursor;
	tok->line = input->line;
	tok->col = (int)(input->cursor - input->line_start + 1);
	
	Skip_next_token(input);

	tok->len = (int)(input->cursor - tok->str);
}

static void Lex_after_dot(input_t * input)
{
	peek_cp_t peek_cp = Peek_input(input);
	
	if (Is_ascii_digit(peek_cp.cp))
	{
		Advance_input(input, peek_cp);
		Lex_rest_of_ppnum(input);
		return;
	}

	if (peek_cp.cp == '.')
	{
		input_t input_peek = *input;
		Advance_input(&input_peek, peek_cp);

		peek_cp = Peek_input(&input_peek);
		if (peek_cp.cp == '.')
		{
			Advance_input(&input_peek, peek_cp);
			*input = input_peek;
		}
	}
}

static void Lex_after_fslash(input_t * input)
{
	peek_cp_t peek = Peek_input(input);

	switch (peek.cp)
	{
	case '*':
		Advance_input(input, peek);
		Lex_rest_of_block_comment(input);
		break;
	case '/':
		Advance_input(input, peek);
		Lex_rest_of_line_comment(input);
		break;
	case '=':
		Advance_input(input, peek);
		break;
	}
}

static void Lex_rest_of_block_comment(input_t * input)
{
	while (!Is_input_exhausted(input))
	{
		peek_cp_t peek_cp0 = Peek_input(input);
		Advance_input(input, peek_cp0);

		peek_cp_t peek_cp1 = Peek_input(input);
		if (peek_cp0.cp == '*' && peek_cp1.cp == '/')
		{
			Advance_input(input, peek_cp1);
			break;
		}
	}
}

static void Lex_rest_of_line_comment(input_t * input)
{
	while (!Is_input_exhausted(input))
	{
		peek_cp_t peek_cp = Peek_input(input);

		if (peek_cp.cp == '\n')
			break;

		Advance_input(input, peek_cp);
	}
}

static void Lex_after_u(input_t * input)
{
	peek_cp_t peek_cp = Peek_input(input);

	if (peek_cp.cp == '8')
	{
		Advance_input(input, peek_cp);

		// In c11 (ostensibly what we are targeting),
		//  you can only have u8 before double quotes

		// Bug commonize with Lex_after_L_or_U

		peek_cp_t peek_cp_quote = Peek_input(input);
		if (peek_cp_quote.cp == '"')
		{
			Advance_input(input, peek_cp_quote);
			Lex_rest_of_str_lit('"', input);
		}
		else
		{
			Lex_rest_of_id(input);
		}
	}
	else
	{
		Lex_after_L_or_U(input);
	}
}

static void Lex_after_L_or_U(input_t * input)
{
	peek_cp_t peek_cp = Peek_input(input);
	if (peek_cp.cp == '"' || peek_cp.cp == '\'')
	{
		Advance_input(input, peek_cp);
		Lex_rest_of_str_lit(peek_cp.cp, input);
		return;
	}

	Lex_rest_of_id(input);
}

static void Lex_rest_of_str_lit(uint32_t cp_sential, input_t * input)
{
	//??? TODO support utf8 chars? or do we get that for free?
	//  should probably at least check for mal-formed utf8, instead
	//  of just accepting all the bytes between the quotes

	while (!Is_input_exhausted(input))
	{
		peek_cp_t peek_cp = Peek_input(input);

		// String without closing quote (which we support in raw lexing mode..)

		if (peek_cp.cp == '\n')
		{
			// BUG hack to match clang

			Advance_past_line_continues(input);
			break;
		}

		// Anything else will be part of the str lit

		Advance_input(input, peek_cp);

		// Closing quote

		if (peek_cp.cp == cp_sential)
			break;

		// Deal with back slash

		if (peek_cp.cp == '\\')
		{
			// Check if escaped char is '\"', '\'', or '\\',
			//  the only escapes we need to handle in raw mode

			peek_cp = Peek_input(input);
			if (peek_cp.cp == cp_sential || peek_cp.cp == '\\')
			{
				Advance_input(input, peek_cp);
			}
		}
	}
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

static void Lex_rest_of_id(input_t * input)
{
	while (true)
	{
		peek_cp_t peek_cp = Peek_input(input);
		if (Does_cp_extend_id(peek_cp.cp))
		{
			Advance_input(input, peek_cp);
			continue;
		}

		break;
	}
}

static void Lex_after_percent(input_t * input)
{
	peek_cp_t peek_cp = Peek_input(input);

	switch (peek_cp.cp)
	{
	case ':':
		Advance_input(input, peek_cp);
		Lex_after_percent_colon(input);
		break;
	case '=':
		Advance_input(input, peek_cp);
		break;
	case '>':
		Advance_input(input, peek_cp);
		break;
	}
}

static void Lex_after_percent_colon(input_t * input)
{
	peek_cp_t peek_cp = Peek_input(input);

	if (peek_cp.cp == '%')
	{
		input_t input_peek = *input;
		Advance_input(&input_peek, peek_cp);

		peek_cp = Peek_input(&input_peek);
		if (peek_cp.cp == ':')
		{
			Advance_input(&input_peek, peek_cp);
			*input = input_peek;
		}
	}
}

static void Lex_after_lt(input_t * input)
{
	peek_cp_t peek_cp = Peek_input(input);

	switch (peek_cp.cp)
	{
	case '<':
		Advance_input(input, peek_cp);
		peek_cp = Peek_input(input);
		if (peek_cp.cp == '=')
		{
			Advance_input(input, peek_cp);
		}
		break;

	case '%':
		Advance_input(input, peek_cp);
		break;

	case ':':
		Advance_input(input, peek_cp);
		break;

	case '=':
		Advance_input(input, peek_cp);
		break;
	}
}

static void Lex_after_gt(input_t * input)
{
	peek_cp_t peek_cp = Peek_input(input);

	switch (peek_cp.cp)
	{
	case '>':
		Advance_input(input, peek_cp);
		peek_cp = Peek_input(input);
		if (peek_cp.cp == '=')
		{
			Advance_input(input, peek_cp);
		}
		break;

	case '=':
		Advance_input(input, peek_cp);
		break;
	}
}

static void Lex_after_bang(input_t * input)
{
	peek_cp_t peek_cp = Peek_input(input);

	if (peek_cp.cp == '=')
	{
		Advance_input(input, peek_cp);
	}
}

static void Lex_after_htag(input_t * input)
{
	peek_cp_t peek_cp = Peek_input(input);

	if (peek_cp.cp == '#')
	{
		Advance_input(input, peek_cp);
	}
}

static void Lex_after_amp(input_t * input)
{
	peek_cp_t peek_cp = Peek_input(input);

	switch (peek_cp.cp)
	{
	case '&':
		Advance_input(input, peek_cp);
		break;
	case '=':
		Advance_input(input, peek_cp);
		break;
	}
}

static void Lex_after_star(input_t * input)
{
	peek_cp_t peek_cp = Peek_input(input);

	if (peek_cp.cp == '=')
	{
		Advance_input(input, peek_cp);
	}
}

static void Lex_after_plus(input_t * input)
{
	peek_cp_t peek_cp = Peek_input(input);

	switch (peek_cp.cp)
	{
	case '+':
		Advance_input(input, peek_cp);
		break;
	case '=':
		Advance_input(input, peek_cp);
		break;
	}
}

static void Lex_after_minus(input_t * input)
{
	peek_cp_t peek_cp = Peek_input(input);

	switch (peek_cp.cp)
	{
	case '>':
		Advance_input(input, peek_cp);
		break;
	case '-':
		Advance_input(input, peek_cp);
		break;
	case '=':
		Advance_input(input, peek_cp);
		break;
	}
}

static void Lex_after_colon(input_t * input)
{
	peek_cp_t peek_cp = Peek_input(input);

	if (peek_cp.cp == '>')
	{
		Advance_input(input, peek_cp);
	}
	else if (peek_cp.cp == ':')
	{
		// https://github.com/llvm/llvm-project/commit/874217f99b99ab3c9026dc3b7bd84cd2beebde6e

		Advance_input(input, peek_cp);
	}
}

static void Lex_after_eq(input_t * input)
{
	peek_cp_t peek_cp = Peek_input(input);

	if (peek_cp.cp == '=')
	{
		Advance_input(input, peek_cp);
	}
}

static void Lex_after_caret(input_t * input)
{
	peek_cp_t peek_cp = Peek_input(input);

	if (peek_cp.cp == '=')
	{
		Advance_input(input, peek_cp);
	}
}

static void Lex_after_vbar(input_t * input)
{
	peek_cp_t peek_cp = Peek_input(input);

	switch (peek_cp.cp)
	{
	case '|':
		Advance_input(input, peek_cp);
		break;
	case '=':
		Advance_input(input, peek_cp);
		break;
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

static void Lex_rest_of_ppnum(input_t * input)
{
	while (true)
	{
		peek_cp_t peek_cp = Peek_input(input);

		if (peek_cp.cp == '.')
		{
			Advance_input(input, peek_cp);
			continue;
		}
		else if (peek_cp.cp == 'e' || peek_cp.cp == 'E' || peek_cp.cp == 'p' || peek_cp.cp == 'P')
		{
			Advance_input(input, peek_cp);
		
			peek_cp = Peek_input(input);
			if (peek_cp.cp == '+' || peek_cp.cp == '-')
			{
				Advance_input(input, peek_cp);
			}

			continue;
		}
		else if (peek_cp.cp == '$')
		{
			// Clang does not allow '$' in ppnums, 
			//  even though the spec would seem to suggest that 
			//  implimentation defined id chars should be included in PP nums...

			break;
		}
		else if (Does_cp_extend_id(peek_cp.cp))
		{
			// Everything (else) which extends ids can extend a ppnum

			Advance_input(input, peek_cp);
			continue;
		}
		else
		{
			// Otherwise, no dice

			break;
		}
	}
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

void Print_token(const token_t * tok)
{
	printf("\"");
	for (int i = 0; i < tok->len; ++i)
	{
		Clean_and_print_ch(tok->str[i]);
	}
	printf("\"");

	printf(
		" %d:%d\n",
		tok->line,
		tok->col);
}