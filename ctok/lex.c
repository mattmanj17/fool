
#include <stdio.h>
#include <string.h>

#include "lex.h"

#include "count_of.h"
#include "peek.h"
#include "unicode.h"



static int Len_horizontal_whitespace(lcp_t * cursor);
static int Len_whitespace(lcp_t * cursor);
static int Len_after_u(lcp_t * cursor, lcp_t * terminator);
static int Len_after_L_or_U(lcp_t * cursor, lcp_t * terminator);
static int Len_rest_of_str_lit(uint32_t cp_sential, lcp_t * cursor, lcp_t * terminator);
static int Len_after_fslash(lcp_t * cursor, lcp_t * terminator);
static int Len_rest_of_block_comment(lcp_t * cursor, lcp_t * terminator);
static int Len_rest_of_line_comment(lcp_t * cursor, lcp_t * terminator);
static int Len_after_dot(lcp_t * cursor);
static bool May_cp_start_id(uint32_t cp);
static int Len_rest_of_id(lcp_t * cursor);
static bool Does_cp_extend_id(uint32_t cp);
static cp_len_t Peek_ucn(lcp_t * cursor);
static cp_len_t Peek_hex_ucn(lcp_t * cursor);
static uint32_t Hex_digit_value_from_cp(uint32_t cp);
static bool Is_cp_valid_ucn(uint32_t cp);
static cp_len_t Peek_named_ucn(lcp_t * cursor);
static int Len_rest_of_operator(uint32_t cp_leading, lcp_t * cursor);
static int Len_rest_of_ppnum(lcp_t * cursor);

int Len_leading_token(lcp_t * cursor, lcp_t * terminator)
{
	// Special handling of horizontal WS to match clang ... :(

	if (Is_ch_horizontal_white_space(cursor[0].str[0]))
	{
		return 1 + Len_horizontal_whitespace(cursor + 1);
	}

	// Advance input and decide what to do

	const lcp_t * cursor_orig = cursor;
	uint32_t cp = cursor->cp;

	if (cp == 'u')
	{
		++cursor;
		cursor += Len_after_u(cursor, terminator);
	}
	else if (cp == 'U' || cp == 'L')
	{
		++cursor;
		cursor += Len_after_L_or_U(cursor, terminator);
	}
	else if (cp == '"' || cp == '\'')
	{
		++cursor;
		cursor += Len_rest_of_str_lit(cp, cursor, terminator);
	}
	else if (cp == '/')
	{
		++cursor;
		cursor += Len_after_fslash(cursor, terminator);
	}
	else if (cp == '.')
	{
		++cursor;
		cursor += Len_after_dot(cursor);
	}
	else if (May_cp_start_id(cp))
	{
		++cursor;
		cursor += Len_rest_of_id(cursor);
	}
	else if (Is_cp_ascii_digit(cp))
	{
		++cursor;
		cursor += Len_rest_of_ppnum(cursor);
	}
	else if (Is_cp_ascii_white_space(cp))
	{
		++cursor;
		cursor += Len_whitespace(cursor);
	}
	else if (cp == '\0')
	{
		++cursor;

		// skip whitespace after a '\0'

		cursor += Len_whitespace(cursor);
	}
	else if (cp =='\\')
	{
		cp_len_t cp_len = Peek_ucn(cursor);
		if (cp_len.len)
		{
			cursor += cp_len.len;

			if (May_cp_start_id(cp_len.cp))
			{
				cursor += Len_rest_of_id(cursor);
			}
			else
			{
				// Bogus UCN, return it as an unknown token
			}
		}
		else
		{
			// Stray backslash, return as unknown token

			++cursor;
		}
	}
	else
	{
		++cursor; // BUG fix this, one arg to Len_rest_of_operator
		cursor += Len_rest_of_operator(cp, cursor);
	}

	return (int)(cursor - cursor_orig);
}

static int Len_horizontal_whitespace(lcp_t * cursor)
{
	int len = 0;

	while (true)
	{
		if (!Is_cp_ascii_horizontal_white_space(cursor[len].cp))
			break;

		// We only want to skip raw whitespace, not whitesapce after
		//  escaped new lines. This is a gross hack to hatch clang.

		if (cursor[len].num_ch > 1)
			break;

		++len;
	}

	return len;
}

static int Len_whitespace(lcp_t * cursor)
{
	int len = 0;

	while (true)
	{
		// We look at the raw char starting the cp,
		//  in order to avoid including stuff like "\\\n\n",
		//  but still being able to include "\r\n"
		//  This is a gross hack to match clang

		char ch = cursor[len].str[0];
		if (!Is_ch_white_space(ch))
			break;

		++len;
	}

	return len;
}

static int Len_after_u(lcp_t * cursor, lcp_t * terminator)
{
	const lcp_t * cursor_orig = cursor;

	uint32_t cp = cursor->cp;

	if (cp == '8')
	{
		++cursor;

		// In c11 (ostensibly what we are targeting),
		//  you can only have u8 before double quotes

		// Bug commonize with Lex_after_L_or_U

		uint32_t cp_quote = cursor->cp;
		if (cp_quote == '"')
		{
			++cursor;
			cursor += Len_rest_of_str_lit('"', cursor, terminator);
		}
		else
		{
			cursor += Len_rest_of_id(cursor);
		}
	}
	else
	{
		cursor += Len_after_L_or_U(cursor, terminator);
	}

	return (int)(cursor - cursor_orig);
}

static int Len_after_L_or_U(lcp_t * cursor, lcp_t * terminator)
{
	const lcp_t * cursor_orig = cursor;

	uint32_t cp = cursor->cp;
	if (cp == '"' || cp == '\'')
	{
		++cursor;
		cursor += Len_rest_of_str_lit(cp, cursor, terminator);
	}
	else
	{
		cursor += Len_rest_of_id(cursor);
	}

	return (int)(cursor - cursor_orig);
}

static void Do_escaped_line_break_hack(lcp_t * cursor)
{
	// BUG hack to match clang. 
	//  We want to include any escaped line breaks at the end of this
	//  un-terminated string literal. we do this by shuffling
	//  everything except the trailing (physical) '\n', onto the last logical character
	//  in the string, cursor[-1]. This is awful.

	if (cursor[0].str[0] == '\\')
	{
		int len_logical_new_line = cursor[0].num_ch;
		int i_ch_most = len_logical_new_line - 1;

		int len_physical_new_line;
		if (cursor[0].str[i_ch_most] == '\n' &&
			cursor[0].str[i_ch_most - 1] == '\r')
		{
			len_physical_new_line = 2;
		}
		else
		{
			len_physical_new_line = 1;
		}

		int len_escaped_line_break = len_logical_new_line - len_physical_new_line;

		cursor[-1].num_ch += len_escaped_line_break;
		cursor[0].str += len_escaped_line_break;
		cursor[0].num_ch -= len_escaped_line_break;
	}
}

static int Len_rest_of_str_lit(uint32_t cp_sential, lcp_t * cursor, lcp_t * terminator)
{
	const lcp_t * cursor_orig = cursor;

	while (cursor < terminator)
	{
		uint32_t cp = cursor->cp;

		// String without closing quote (which we support in raw lexing mode..)

		if (cp == '\n')
		{
			Do_escaped_line_break_hack(cursor);

			break;
		}

		// Anything else will be part of the str lit

		++cursor;

		// Closing quote

		if (cp == cp_sential)
			break;

		// Deal with back slash

		if (cp == '\\')
		{
			// Check if escaped char is '\"', '\'', or '\\',
			//  the only escapes we need to handle in raw mode

			cp = cursor->cp;
			if (cp == cp_sential || cp == '\\')
			{
				++cursor;
			}
		}
	}

	return (int)(cursor - cursor_orig);
}

static int Len_after_fslash(lcp_t * cursor, lcp_t * terminator)
{
	const lcp_t * cursor_orig = cursor;

	uint32_t cp = cursor->cp;

	switch (cp)
	{
	case '*':
		++cursor;
		cursor += Len_rest_of_block_comment(cursor, terminator);
		break;
	case '/':
		++cursor;
		cursor += Len_rest_of_line_comment(cursor, terminator);
		break;
	case '=':
		++cursor;
		break;
	}

	return (int)(cursor - cursor_orig);
}

static int Len_rest_of_block_comment(lcp_t * cursor, lcp_t * terminator)
{
	const lcp_t * cursor_orig = cursor;

	while (cursor < terminator)
	{
		uint32_t cp0 = cursor->cp;
		++cursor;

		uint32_t cp1 = cursor->cp;
		if (cp0 == '*' && cp1 == '/')
		{
			++cursor;
			break;
		}
	}

	return (int)(cursor - cursor_orig);
}

static int Len_rest_of_line_comment(lcp_t * cursor, lcp_t * terminator)
{
	const lcp_t * cursor_orig = cursor;

	while (cursor < terminator)
	{
		uint32_t cp = cursor->cp;
		if (cp == '\n')
		{
			Do_escaped_line_break_hack(cursor);

			break;
		}

		++cursor;
	}

	return (int)(cursor - cursor_orig);
}

static int Len_after_dot(lcp_t * cursor)
{
	const lcp_t * cursor_orig = cursor;

	uint32_t cp = cursor->cp;

	if (Is_cp_ascii_digit(cp))
	{
		++cursor;
		cursor += Len_rest_of_ppnum(cursor);
	}
	else if (cp == '.')
	{
		lcp_t * cursor_peek = cursor;
		++cursor_peek;

		cp = cursor_peek->cp;
		if (cp == '.')
		{
			++cursor_peek;
			cursor = cursor_peek;
		}
	}

	return (int)(cursor - cursor_orig);
}

static bool May_cp_start_id(uint32_t cp)
{
	// Letters

	if (Is_cp_ascii_lowercase(cp))
		return true;

	if (Is_cp_ascii_uppercase(cp))
		return true;

	// Underscore

	if (cp == '_')
		return true;

	// '$' allowed as an extension :/

	if (cp == '$')
		return true;

	// All other ascii does not start ids

	if (Is_cp_ascii(cp))
		return false;

	// Bogus utf8 does not start ids

	if (cp == UINT32_MAX)
		return false;

	// These codepoints are not allowed as the start of an id

	static const cp_min_most_t c11_disallowed_initial[] =
	{
		{ 0x0300, 0x036F },
		{ 0x1DC0, 0x1DFF },
		{ 0x20D0, 0x20FF },
		{ 0xFE20, 0xFE2F }
	};

	if (Is_cp_in_ranges(cp, c11_disallowed_initial, COUNT_OF(c11_disallowed_initial)))
		return false;

	// These code points are allowed to start an id (minus ones from c11_disallowed_initial)

	static const cp_min_most_t c11_allowed[] =
	{
		// 1
		{ 0x00A8, 0x00A8 }, { 0x00AA, 0x00AA }, { 0x00AD, 0x00AD },
		{ 0x00AF, 0x00AF }, { 0x00B2, 0x00B5 }, { 0x00B7, 0x00BA },
		{ 0x00BC, 0x00BE }, { 0x00C0, 0x00D6 }, { 0x00D8, 0x00F6 },
		{ 0x00F8, 0x00FF },

		// 2
		{ 0x0100, 0x167F }, { 0x1681, 0x180D }, { 0x180F, 0x1FFF },

		// 3
		{ 0x200B, 0x200D }, { 0x202A, 0x202E }, { 0x203F, 0x2040 },
		{ 0x2054, 0x2054 }, { 0x2060, 0x206F },

		// 4
		{ 0x2070, 0x218F }, { 0x2460, 0x24FF }, { 0x2776, 0x2793 },
		{ 0x2C00, 0x2DFF }, { 0x2E80, 0x2FFF },

		// 5
		{ 0x3004, 0x3007 }, { 0x3021, 0x302F }, { 0x3031, 0x303F },

		// 6
		{ 0x3040, 0xD7FF },

		// 7
		{ 0xF900, 0xFD3D }, { 0xFD40, 0xFDCF }, { 0xFDF0, 0xFE44 },
		{ 0xFE47, 0xFFFD },

		// 8
		{ 0x10000, 0x1FFFD }, { 0x20000, 0x2FFFD }, { 0x30000, 0x3FFFD },
		{ 0x40000, 0x4FFFD }, { 0x50000, 0x5FFFD }, { 0x60000, 0x6FFFD },
		{ 0x70000, 0x7FFFD }, { 0x80000, 0x8FFFD }, { 0x90000, 0x9FFFD },
		{ 0xA0000, 0xAFFFD }, { 0xB0000, 0xBFFFD }, { 0xC0000, 0xCFFFD },
		{ 0xD0000, 0xDFFFD }, { 0xE0000, 0xEFFFD }
	};

	return Is_cp_in_ranges(cp, c11_allowed, COUNT_OF(c11_allowed));
}

static int Len_rest_of_id(lcp_t * cursor)
{
	const lcp_t * cursor_orig = cursor;

	while (true)
	{
		uint32_t cp = cursor->cp;
		if (Does_cp_extend_id(cp))
		{
			++cursor;
			continue;
		}

		if (cp == '\\')
		{
			cp_len_t cp_len = Peek_ucn(cursor);
			if (cp_len.len && Does_cp_extend_id(cp_len.cp))
			{
				cursor += cp_len.len;
				continue;
			}
		}

		break;
	}

	return (int)(cursor - cursor_orig);
}

static bool Does_cp_extend_id(uint32_t cp)
{
	if (Is_cp_ascii_lowercase(cp))
		return true;

	if (Is_cp_ascii_uppercase(cp))
		return true;

	if (cp == '_')
		return true;

	if (Is_cp_ascii_digit(cp))
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

	// BUG I do not like this. the right thing to do is check c11_allowed from May_cp_start_id.
	//  Clang seems to do the wrong thing here,
	//  and produce an invalid pp token. I suspect no one
	//  actually cares, since dump_raw_tokens is only for debugging...

	if (!Is_cp_unicode_whitespace(cp))
		return true;

	return false;
}

static cp_len_t Peek_ucn(lcp_t * cursor)
{
	cp_len_t cp_len = Peek_hex_ucn(cursor);
	if (cp_len.len)
		return cp_len;

	return Peek_named_ucn(cursor);
}

static cp_len_t Peek_hex_ucn(lcp_t * cursor)
{
	int len = 0;

	// Check for leading '\\'

	if (cursor[len].cp != '\\')
		return {UINT32_MAX, 0};

	// Advance past '\\'

	++len;

	// Look for 'u' or 'U' after '\\'

	if (cursor[len].cp != 'u' && cursor[len].cp != 'U')
		return {UINT32_MAX, 0};

	// Look for 4 or 8 hex digits, based on u vs U

	int num_hex_digits;
	if (cursor[len].cp == 'u')
	{
		num_hex_digits = 4;
	}
	else
	{
		num_hex_digits = 8;
	}

	// Advance past u/U

	++len;

	// Look for correct number of hex digits

	uint32_t cp_result = 0;
	int hex_digits_read = 0;
	bool delimited = false;
	bool found_end_delimiter = false;

	while ((hex_digits_read < num_hex_digits) || delimited)
	{
		uint32_t cp = cursor[len].cp;

		// Check for '{' (delimited ucns)

		if (!delimited && hex_digits_read == 0 && cp == '{')
		{
			delimited = true;
			++len;
			continue;
		}

		// Check for '}' (delimited ucns)

		if (delimited && cp == '}')
		{
			found_end_delimiter = true;
			++len;
			break;
		}

		// Check if valid hex digit

		uint32_t hex_digit_value = Hex_digit_value_from_cp(cp);
		if (hex_digit_value == UINT32_MAX)
		{
			if (delimited)
			{
				return {UINT32_MAX, 0};
			}
			else
			{
				break;
			}
		}

		/*
		if (cp_result & 0xF000'0000) return {UINT32_MAX, 0}; //from clang, do we need it?
		*/

		// Fold hex digit into cp

		cp_result <<= 4;
		cp_result |= hex_digit_value;

		// Keep track of how many digits we have read

		++hex_digits_read;

		// Advance to next digit

		++len;
	}

	// No digits read?

	if (hex_digits_read == 0)
		return {UINT32_MAX, 0};

	// Delimited 'U' is not allowed (find somthing in clang to explain this?)

	if (delimited && num_hex_digits == 8)
		return {UINT32_MAX, 0};

	// Read wrong number of digits?

	if (!delimited && hex_digits_read != num_hex_digits)
		return {UINT32_MAX, 0};

	// Sanity check that people are not trying to encode
	//  something particularly weird with a UCN.
	//  Convert any weird inputs to the error value UINT32_MAX

	if (!Is_cp_valid_ucn(cp_result))
	{
		cp_result = UINT32_MAX;
	}

	// Return result

	return {cp_result, len};
}

static uint32_t Hex_digit_value_from_cp(uint32_t cp)
{
	if (!Is_cp_ascii(cp))
		return UINT32_MAX;

	if (Is_cp_ascii_digit(cp))
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

	// BUG 
	// should be
	//  if (cp < 0xA0 && cp != 0x24 && cp != 0x40 && cp != 0x60)
	// but clang does somthing else :/

	if (cp < 0xA0)
		return false;

	// BUG matthewd you would expect this to be if !Is_cp_valid,
	//  but that is not what clang does...

	if (Is_cp_surrogate(cp))
		return false;

	return true;
}

static cp_len_t Peek_named_ucn(lcp_t * cursor)
{
	int len = 0;

	// Check for leading '\\'

	if (cursor[len].cp != '\\')
		return {UINT32_MAX, 0};

	// Advance past '\\'

	++len;

	// Look for 'N' after '\\'

	if (cursor[len].cp != 'N')
		return {UINT32_MAX, 0};

	// Advance past 'N'

	++len;

	// Look for '{'

	if (cursor[len].cp != '{')
		return {UINT32_MAX, 0};

	// Advance

	++len;

	// Look for closing '}'

	int len_name_start = len;
	bool found_closing_brace = false;

	while (uint32_t cp = cursor[len].cp)
	{
		++len;
		if (cp == '}')
		{
			found_closing_brace = true;
			break;
		}

		if (cp == '\n')
			break;
	}

	// Check if we found '}'

	if (!found_closing_brace)
		return {UINT32_MAX, 0};

	// Check if we actually got any chars between '{' and '}'

	int len_name = len - len_name_start - 1;
	if (!len_name)
		return {UINT32_MAX, 0};

	// Ok, we have \N{...}
	//  For now, we just shrug and return it as an unknown codepoint.
	//  Later on we are going to have to do all the name look up :/

	return {UINT32_MAX, len};
}

static int Len_rest_of_operator(uint32_t cp_leading, lcp_t * cursor)
{
	const lcp_t * cursor_orig = cursor;

	// "::" is included to match clang
	// https://github.com/llvm/llvm-project/commit/874217f99b99ab3c9026dc3b7bd84cd2beebde6e

	static const char * operators[] =
	{
		"%:%:",

		">>=", "<<=", "...",

		"|=", "||", "^=", "==", "::", ":>", "-=", "--", "->", "+=", "++", "*=",
		"&=", "&&", "##", "!=", ">=", ">>", "<=", "<:", "<%", "<<", "%>", "%=",
		"%:", "/=",

		"~", "}", "{", "]", "[", "?", ";", ",", ")", "(", "|", "^", "=", ":",
		"-", "+", "*", "&", "#", "!", ">", "<", "%", ".", "/",
	};

	for (int i_operator = 0; i_operator < COUNT_OF(operators); ++i_operator)
	{
		const char * str_operator = operators[i_operator];
		size_t len = strlen(str_operator);

		lcp_t * cursor_peek = cursor;
		bool found_match = true;

		for (size_t i_ch = 0; i_ch < len; ++i_ch)
		{
			char ch = str_operator[i_ch];
			uint32_t cp_ch = (uint32_t)ch;

			if (i_ch == 0)
			{
				if (cp_leading != cp_ch)
				{
					found_match = false;
					break;
				}
			}
			else
			{
				uint32_t cp = cursor_peek->cp;
				if (cp != cp_ch)
				{
					found_match = false;
					break;
				}

				++cursor_peek;
			}
		}

		if (found_match)
		{
			cursor = cursor_peek;
			break;
		}
	}

	return (int)(cursor - cursor_orig);
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
*/

// Len_rest_of_pp_num is called after we see ( '.'? [0-9] ), that is, pp_num_start
// 'rest_of_pp_num' is equivalent to pp_num_continue*

static int Len_rest_of_ppnum(lcp_t * cursor)
{
	const lcp_t * cursor_orig = cursor;

	while (true)
	{
		uint32_t cp = cursor->cp;

		if (cp == '.')
		{
			++cursor;
			continue;
		}
		else if (cp == 'e' || cp == 'E' || cp == 'p' || cp == 'P')
		{
			++cursor;

			cp = cursor->cp;
			if (cp == '+' || cp == '-')
			{
				++cursor;
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

			++cursor;
			continue;
		}
		else if (cp == '\\')
		{
			cp_len_t cp_len = Peek_ucn(cursor);
			if (cp_len.len && Does_cp_extend_id(cp_len.cp))
			{
				cursor += cp_len.len;
				continue;
			}
			else
			{
				break;
			}
		}
		else
		{
			// Otherwise, no dice

			break;
		}
	}

	return (int)(cursor - cursor_orig);
}