
#include <stdio.h>
#include <string.h>

#include "lex.h"

#include "count_of.h"
#include "peek.h"
#include "unicode.h"



static int Len_horizontal_whitespace(cp_len_str_t * cursor);
static int Len_whitespace(cp_len_str_t * cursor);
static int Len_after_u(cp_len_str_t * cursor, cp_len_str_t * terminator);
static int Len_after_L_or_U(cp_len_str_t * cursor, cp_len_str_t * terminator);
static int Len_rest_of_str_lit(uint32_t cp_sential, cp_len_str_t * cursor, cp_len_str_t * terminator);
static int Len_after_fslash(cp_len_str_t * cursor, cp_len_str_t * terminator);
static int Len_rest_of_block_comment(cp_len_str_t * cursor, cp_len_str_t * terminator);
static int Len_rest_of_line_comment(cp_len_str_t * cursor, cp_len_str_t * terminator);
static int Len_after_dot(cp_len_str_t * cursor);
static bool May_cp_start_id(uint32_t cp);
static int Len_rest_of_id(cp_len_str_t * cursor);
static bool Does_cp_extend_id(uint32_t cp);
static int Len_rest_of_operator(uint32_t cp_leading, cp_len_str_t * cursor);
static int Len_rest_of_ppnum(cp_len_str_t * cursor);

int Len_leading_token(cp_len_str_t * cursor, cp_len_str_t * terminator)
{
	// Special handling of horizontal WS to match clang ... :(

	if (Is_ch_horizontal_white_space(cursor[0].str[0]))
	{
		return 1 + Len_horizontal_whitespace(cursor + 1);
	}

	// Advance input and decide what to do

	const cp_len_str_t * cursor_orig = cursor;
	uint32_t cp = cursor->cp;
	++cursor;

	if (cp == 'u')
	{
		cursor += Len_after_u(cursor, terminator);
	}
	else if (cp == 'U' || cp == 'L')
	{
		cursor += Len_after_L_or_U(cursor, terminator);
	}
	else if (cp == '"' || cp == '\'')
	{
		cursor += Len_rest_of_str_lit(cp, cursor, terminator);
	}
	else if (cp == '/')
	{
		cursor += Len_after_fslash(cursor, terminator);
	}
	else if (cp == '.')
	{
		cursor += Len_after_dot(cursor);
	}
	else if (May_cp_start_id(cp))
	{
		cursor += Len_rest_of_id(cursor);
	}
	else if (Is_cp_ascii_digit(cp))
	{
		cursor += Len_rest_of_ppnum(cursor);
	}
	else if (Is_cp_ascii_white_space(cp))
	{
		cursor += Len_whitespace(cursor);
	}
	else if (cp == '\0')
	{
		// skip whitespace after a '\0'

		cursor += Len_whitespace(cursor);
	}
	else
	{
		cursor += Len_rest_of_operator(cp, cursor);
	}

	return (int)(cursor - cursor_orig);
}

static int Len_horizontal_whitespace(cp_len_str_t * cursor)
{
	int len = 0;

	while (true)
	{
		if (!Is_cp_ascii_horizontal_white_space(cursor[len].cp))
			break;

		// We only want to skip raw whitespace, not whitesapce after
		//  escaped new lines. This is a gross hack to hatch clang.

		if (cursor[len].len > 1)
			break;

		++len;
	}

	return len;
}

static int Len_whitespace(cp_len_str_t * cursor)
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

static int Len_after_u(cp_len_str_t * cursor, cp_len_str_t * terminator)
{
	const cp_len_str_t * cursor_orig = cursor;

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

static int Len_after_L_or_U(cp_len_str_t * cursor, cp_len_str_t * terminator)
{
	const cp_len_str_t * cursor_orig = cursor;

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

static void Do_escaped_line_break_hack(cp_len_str_t * cursor)
{
	// BUG hack to match clang. 
	//  We want to include any escaped line breaks at the end of this
	//  un-terminated string literal. we do this by shuffling
	//  everything except the trailing (physical) '\n', onto the last logical character
	//  in the string, cursor[-1]. This is awful.

	if (cursor[0].str[0] == '\\')
	{
		int len_logical_new_line = cursor[0].len;
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

		cursor[-1].len += len_escaped_line_break;
		cursor[0].str += len_escaped_line_break;
		cursor[0].len -= len_escaped_line_break;
	}
}

static int Len_rest_of_str_lit(uint32_t cp_sential, cp_len_str_t * cursor, cp_len_str_t * terminator)
{
	const cp_len_str_t * cursor_orig = cursor;

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

static int Len_after_fslash(cp_len_str_t * cursor, cp_len_str_t * terminator)
{
	const cp_len_str_t * cursor_orig = cursor;

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

static int Len_rest_of_block_comment(cp_len_str_t * cursor, cp_len_str_t * terminator)
{
	const cp_len_str_t * cursor_orig = cursor;

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

static int Len_rest_of_line_comment(cp_len_str_t * cursor, cp_len_str_t * terminator)
{
	const cp_len_str_t * cursor_orig = cursor;

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

static int Len_after_dot(cp_len_str_t * cursor)
{
	const cp_len_str_t * cursor_orig = cursor;

	uint32_t cp = cursor->cp;

	if (Is_cp_ascii_digit(cp))
	{
		++cursor;
		cursor += Len_rest_of_ppnum(cursor);
	}
	else if (cp == '.')
	{
		cp_len_str_t * cursor_peek = cursor;
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

static int Len_rest_of_id(cp_len_str_t * cursor)
{
	const cp_len_str_t * cursor_orig = cursor;

	while (true)
	{
		uint32_t cp = cursor->cp;
		if (Does_cp_extend_id(cp))
		{
			++cursor;
			continue;
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

static int Len_rest_of_operator(uint32_t cp_leading, cp_len_str_t * cursor)
{
	const cp_len_str_t * cursor_orig = cursor;

	// "::" is included to match clang
	// https://github.com/llvm/llvm-project/commit/874217f99b99ab3c9026dc3b7bd84cd2beebde6e

	static const char * operators[] =
	{
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

		cp_len_str_t * cursor_peek = cursor;
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

static int Len_rest_of_ppnum(cp_len_str_t * cursor)
{
	const cp_len_str_t * cursor_orig = cursor;

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
		else
		{
			// Otherwise, no dice

			break;
		}
	}

	return (int)(cursor - cursor_orig);
}