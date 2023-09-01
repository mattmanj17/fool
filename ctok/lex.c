
#include <stdlib.h>

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

typedef struct
{
	const char * new_line_start;
	uint32_t cp;
	int length;
	int num_lines;

	int _padding;
} peek_t;

static peek_t Peek_logical(input_t * input, bool discard_unicode_after_line_escape)
{
	peek_t peek;
	peek.length = 0;
	peek.num_lines = 0;
	peek.new_line_start = input->line_start;

	const char * str = input->cursor;
	if (str[0] == '\\')
	{
		int num_lines;
		int len_line_continue = Len_line_continues(str, &num_lines);
		if (len_line_continue)
		{
			str += len_line_continue;

			peek.length = len_line_continue;
			peek.num_lines = num_lines;
			peek.new_line_start = str;
		}
	}

	if (str == input->terminator)
	{
		peek.cp = '\0';
		return peek;
	}

	unsigned char ch = (unsigned char)str[0];
	if (ch == '\n' || ch == '\r')
	{
		peek.cp = '\n';
		++peek.length;

		peek.new_line_start = str + 1;
		++peek.num_lines;

		if (ch == '\r' && str[1] == '\n')
		{
			++peek.new_line_start;
			++peek.length;
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

			if (peek.length && discard_unicode_after_line_escape)
			{
				peek.cp = UINT32_MAX;
			}
			else
			{
				peek.cp = cp;
			}

			peek.length += (int)((const char *)span.cursor - str);
		}
		else
		{
			peek.cp = UINT32_MAX;
			peek.length += 1;
		}
	}
	else
	{
		peek.cp = ch;
		++peek.length;
	}

	return peek;
}

static void Advance_logical(input_t * input)
{
	peek_t peek = Peek_logical(input, false);

	input->cursor += peek.length;
	input->line_start = peek.new_line_start;
	input->line += peek.num_lines;
}

static uint32_t Hex_digit_value_from_cp(uint32_t cp)
{
	if (!Is_cp_ascii(cp))
		return UINT32_MAX;

	if (Is_ascii_digit(cp))
		return cp - '0';

	if (cp < 'A')
		return UINT32_MAX;

	if (cp > 'f')
		return UINT32_MAX;

	if (cp <= 'F')
		return cp - 'A' + 10;

	if (cp >= 'a')
		return cp - 'a' + 10;

	return UINT32_MAX;
}

static bool Is_cp_valid_ucn(uint32_t cp)
{
	// Comment cribbed from clang
	// C99 6.4.3p2: A universal character name shall not specify a character whose
	//   short identifier is less than 00A0 other than 0024 ($), 0040 (@), or
	//   0060 (`), nor one in the range D800 through DFFF inclusive.)

	if (cp < 0xA0 && cp != 0x24 && cp != 0x40 && cp != 0x60)
		return false;

	if (cp >= 0xD800 && cp <= 0xDFFF)
		return false;

	if (cp > cp_most)
		return false;

	return true;
}

static peek_t Peek_handle_ucn(input_t * input, bool discard_unicode_after_line_escape)
{
	// Check for leading '\\'

	peek_t peek_bslash = Peek_logical(input, discard_unicode_after_line_escape);
	if (peek_bslash.cp != '\\')
		return peek_bslash;

	// The 'discard_unicode_after_line_escape' only matters at the start

	discard_unicode_after_line_escape = false;

	// 'fork' input into input_peek, so we can look ahead
	//  and see if we have a valid UCN

	input_t input_peek = *input;
	Advance_logical(&input_peek);

	// Look for 'u' or 'U' after '\\'

	peek_t peek_u = Peek_logical(&input_peek, discard_unicode_after_line_escape);
	if (peek_u.cp != 'u' && peek_u.cp != 'U')
		return peek_bslash;

	// Set up 'scract' peek struct to accumulate
	//  the individual 'peeks' we do to read the UCN

	peek_t peek_result;
	peek_result.cp = 0;
	peek_result.length = peek_bslash.length + peek_u.length;
	peek_result.num_lines = peek_bslash.num_lines + peek_u.num_lines;
	peek_result.new_line_start = peek_u.new_line_start;

	// Advance past u/U

	Advance_logical(&input_peek);

	// Look for 4 or 8 hex digits, based on u vs U

	int num_hex_digits;
	if (peek_u.cp == 'u')
	{
		num_hex_digits = 4;
	}
	else
	{
		num_hex_digits = 8;
	}

	// Look for correct number of hex digits

	int hex_digits_read = 0;
	while (hex_digits_read < num_hex_digits)
	{
		// Peek next digit

		peek_t peek_digit = Peek_logical(&input_peek, discard_unicode_after_line_escape);

		// Check if valid hex digit

		uint32_t hex_digit_value = Hex_digit_value_from_cp(peek_digit.cp);
		if (hex_digit_value == UINT32_MAX)
			break;

		// Fold hex digit into cp

		peek_result.cp <<= 4;
		peek_result.cp |= hex_digit_value;

		// Keep track of 'how far we have peeked'

		peek_result.length += peek_digit.length;
		peek_result.num_lines += peek_digit.num_lines;
		peek_result.new_line_start = peek_digit.new_line_start;
		
		// Keep track of how many digits we have read

		++hex_digits_read;

		// Advance to next digit

		Advance_logical(&input_peek);
	}

	// If we did not read the correct number of digits after the 'u',
	//  just treat this as a stray '\\'

	if (hex_digits_read < num_hex_digits)
		return peek_bslash;

	// Sanity check that people are not trying to encode
	//  something particularly weird with a UCN.
	//  Convert any weird inputs to the error value UINT32_MAX

	if (!Is_cp_valid_ucn(peek_result.cp))
	{
		peek_result.cp = UINT32_MAX;
	}

	// Otherwise, we read a valid UCN, and the info
	//  we need to return in in peek_result

	return peek_result;
}

static uint32_t Peek_input(input_t * input)
{
	return Peek_handle_ucn(input, false).cp;
}

static uint32_t Peek_input_after_id_start_hack(input_t * input)
{
	return Peek_handle_ucn(input, true).cp;
}

static void Advance_input(input_t * input)
{
	peek_t peek = Peek_handle_ucn(input, false);

	input->cursor += peek.length;
	input->line_start = peek.new_line_start;
	input->line += peek.num_lines;
}

static void Advance_past_line_continue(input_t * input)
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

static void Skip_whitespace(input_t * input)
{
	while (Is_ch_white_space(input->cursor[0]))
	{
		Advance_input(input);
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

void Lex(input_t * input)
{
	// Special handling of horizontal WS to match clang ... :(

	if (Does_input_physically_start_with_horizontal_whitespace(input))
	{
		Skip_horizontal_whitespace(input);
		return;
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
			if (May_non_ascii_codepoint_start_id(lang_ver_c11, cp))
			{
				Lex_rest_of_id(input);
			}
		}
		break;
	}
}

static void Lex_after_dot(input_t * input)
{
	uint32_t cp = Peek_input(input);
	
	if (Is_ascii_digit(cp))
	{
		Advance_input(input);
		Lex_rest_of_ppnum(input);
		return;
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
}

static void Lex_after_fslash(input_t * input)
{
	switch (Peek_input(input))
	{
	case '*':
		Advance_input(input);
		Lex_rest_of_block_comment(input);
		break;
	case '/':
		Advance_input(input);
		Lex_rest_of_line_comment(input);
		break;
	case '=':
		Advance_input(input);
		break;
	}
}

static void Lex_rest_of_block_comment(input_t * input)
{
	while (!Is_input_exhausted(input))
	{
		uint32_t cp = Peek_input(input);
		Advance_input(input);

		if (cp == '*' && Peek_input(input) == '/')
		{
			Advance_input(input);
			break;
		}
	}
}

static void Lex_rest_of_line_comment(input_t * input)
{
	while (!Is_input_exhausted(input))
	{
		uint32_t cp = Peek_input(input);

		if (cp == '\n')
			break;

		Advance_input(input);
	}
}

static void Lex_after_u(input_t * input)
{
	if (Peek_input(input) == '8')
	{
		Advance_input(input);

		// In c11 (ostensibly what we are targeting),
		//  you can only have u8 before double quotes

		// Bug commonize with Lex_after_L_or_U

		if (Peek_input(input) == '"')
		{
			Advance_input(input);
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
	uint32_t cp = Peek_input(input);
	if (cp == '"' || cp == '\'')
	{
		Advance_input(input);
		Lex_rest_of_str_lit(cp, input);
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
		uint32_t cp = Peek_input_after_id_start_hack(input);
		if (Does_cp_extend_id(cp))
		{
			Advance_input(input);
			continue;
		}

		break;
	}
}

static void Lex_after_percent(input_t * input)
{
	switch (Peek_input(input))
	{
	case ':':
		Advance_input(input);
		Lex_after_percent_colon(input);
		break;
	case '=':
		Advance_input(input);
		break;
	case '>':
		Advance_input(input);
		break;
	}
}

static void Lex_after_percent_colon(input_t * input)
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
}

static void Lex_after_lt(input_t * input)
{
	switch (Peek_input(input))
	{
	case '<':
		Advance_input(input);
		if (Peek_input(input) == '=')
		{
			Advance_input(input);
		}
		break;

	case '%':
		Advance_input(input);
		break;

	case ':':
		Advance_input(input);
		break;

	case '=':
		Advance_input(input);
		break;
	}
}

static void Lex_after_gt(input_t * input)
{
	switch (Peek_input(input))
	{
	case '>':
		Advance_input(input);
		if (Peek_input(input) == '=')
		{
			Advance_input(input);
		}
		break;

	case '=':
		Advance_input(input);
		break;
	}
}

static void Lex_after_bang(input_t * input)
{
	if (Peek_input(input) == '=')
	{
		Advance_input(input);
	}
}

static void Lex_after_htag(input_t * input)
{
	if (Peek_input(input) == '#')
	{
		Advance_input(input);
	}
}

static void Lex_after_amp(input_t * input)
{
	switch (Peek_input(input))
	{
	case '&':
		Advance_input(input);
		break;
	case '=':
		Advance_input(input);
		break;
	}
}

static void Lex_after_star(input_t * input)
{
	if (Peek_input(input) == '=')
	{
		Advance_input(input);
	}
}

static void Lex_after_plus(input_t * input)
{
	switch (Peek_input(input))
	{
	case '+':
		Advance_input(input);
		break;
	case '=':
		Advance_input(input);
		break;
	}
}

static void Lex_after_minus(input_t * input)
{
	switch (Peek_input(input))
	{
	case '>':
		Advance_input(input);
		break;
	case '-':
		Advance_input(input);
		break;
	case '=':
		Advance_input(input);
		break;
	}
}

static void Lex_after_colon(input_t * input)
{
	if (Peek_input(input) == '>')
	{
		Advance_input(input);
	}
}

static void Lex_after_eq(input_t * input)
{
	if (Peek_input(input) == '=')
	{
		Advance_input(input);
	}
}

static void Lex_after_caret(input_t * input)
{
	if (Peek_input(input) == '=')
	{
		Advance_input(input);
	}
}

static void Lex_after_vbar(input_t * input)
{
	switch (Peek_input(input))
	{
	case '|':
		Advance_input(input);
		break;
	case '=':
		Advance_input(input);
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
}