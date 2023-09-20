
#include <stdio.h>
#include <string.h>

#include "lex.h"

#include "count_of.h"
#include "peek.h"
#include "unicode.h"



static int Len_horizontal_whitespace(const char * cursor);
static int Len_whitespace(const char * cursor);
static int Len_after_u(const char * cursor, const char * terminator);
static int Len_after_L_or_U(const char * cursor, const char * terminator);
static int Len_rest_of_str_lit(uint32_t cp_sential, const char * cursor, const char * terminator);
static int Len_after_fslash(const char * cursor, const char * terminator);
static int Len_rest_of_block_comment(const char * cursor, const char * terminator);
static int Len_rest_of_line_comment(const char * cursor, const char * terminator);
static int Len_after_dot(const char * cursor, const char * terminator);
static bool May_cp_start_id(uint32_t cp);
static int Len_rest_of_id(const char * cursor, const char * terminator);
static bool Does_cp_extend_id(uint32_t cp);
static int Len_rest_of_operator(uint32_t cp_leading, const char * cursor, const char * terminator);
static int Len_rest_of_ppnum(const char * cursor, const char * terminator);

int Len_leading_token(const char * cursor, const char * terminator)
{
	// Special handling of horizontal WS to match clang ... :(

	if (Is_ch_horizontal_white_space(cursor[0]))
	{
		return Len_horizontal_whitespace(cursor);
	}

	// Advance input and decide what to do

	const char * cursor_orig = cursor;

	cp_len_t cp_len = Peek_cp(cursor, terminator);
	cursor += cp_len.len;

	if (cp_len.cp == 'u')
	{
		cursor += Len_after_u(cursor, terminator);
	}
	else if (cp_len.cp == 'U' || cp_len.cp == 'L')
	{
		cursor += Len_after_L_or_U(cursor, terminator);
	}
	else if (cp_len.cp == '"' || cp_len.cp == '\'')
	{
		cursor += Len_rest_of_str_lit(cp_len.cp, cursor, terminator);
	}
	else if (cp_len.cp == '/')
	{
		cursor += Len_after_fslash(cursor, terminator);
	}
	else if (cp_len.cp == '.')
	{
		cursor += Len_after_dot(cursor, terminator);
	}
	else if (May_cp_start_id(cp_len.cp))
	{
		cursor += Len_rest_of_id(cursor, terminator);
	}
	else if (Is_cp_ascii_digit(cp_len.cp))
	{
		cursor += Len_rest_of_ppnum(cursor, terminator);
	}
	else if (Is_cp_ascii_white_space(cp_len.cp))
	{
		cursor += Len_whitespace(cursor);
	}
	else if (cp_len.cp == '\0')
	{
		// skip whitespace after a '\0'

		cursor += Len_whitespace(cursor);
	}
	else
	{
		cursor += Len_rest_of_operator(cp_len.cp, cursor, terminator);
	}

	return (int)(cursor - cursor_orig);
}

static int Len_horizontal_whitespace(const char * cursor)
{
	int len = 0;

	while (Is_ch_horizontal_white_space(cursor[len]))
	{
		++len;
	}

	return len;
}

static int Len_whitespace(const char * cursor)
{
	int len = 0;

	while (Is_ch_white_space(cursor[len]))
	{
		++len;
	}

	return len;
}

static int Len_after_u(const char * cursor, const char * terminator)
{
	const char * cursor_orig = cursor;

	cp_len_t cp_len = Peek_cp(cursor, terminator);

	if (cp_len.cp == '8')
	{
		cursor += cp_len.len;

		// In c11 (ostensibly what we are targeting),
		//  you can only have u8 before double quotes

		// Bug commonize with Lex_after_L_or_U

		cp_len_t cp_len_quote = Peek_cp(cursor, terminator);
		if (cp_len_quote.cp == '"')
		{
			cursor += cp_len_quote.len;
			cursor += Len_rest_of_str_lit('"', cursor, terminator);
		}
		else
		{
			cursor += Len_rest_of_id(cursor, terminator);
		}
	}
	else
	{
		cursor += Len_after_L_or_U(cursor, terminator);
	}

	return (int)(cursor - cursor_orig);
}

static int Len_after_L_or_U(const char * cursor, const char * terminator)
{
	const char * cursor_orig = cursor;

	cp_len_t cp_len = Peek_cp(cursor, terminator);
	if (cp_len.cp == '"' || cp_len.cp == '\'')
	{
		cursor += cp_len.len;
		cursor += Len_rest_of_str_lit(cp_len.cp, cursor, terminator);
	}
	else
	{
		cursor += Len_rest_of_id(cursor, terminator);
	}

	return (int)(cursor - cursor_orig);
}

static int Len_rest_of_str_lit(uint32_t cp_sential, const char * cursor, const char * terminator)
{
	//??? TODO support utf8 chars? or do we get that for free?
	//  should probably at least check for mal-formed utf8, instead
	//  of just accepting all the bytes between the quotes

	const char * cursor_orig = cursor;

	while (cursor < terminator)
	{
		cp_len_t cp_len = Peek_cp(cursor, terminator);

		// String without closing quote (which we support in raw lexing mode..)

		if (cp_len.cp == '\n')
		{
			// BUG hack to match clang

			cursor += Len_escaped_end_of_lines(cursor, terminator);
			break;
		}

		// Anything else will be part of the str lit

		cursor += cp_len.len;

		// Closing quote

		if (cp_len.cp == cp_sential)
			break;

		// Deal with back slash

		if (cp_len.cp == '\\')
		{
			// Check if escaped char is '\"', '\'', or '\\',
			//  the only escapes we need to handle in raw mode

			cp_len = Peek_cp(cursor, terminator);
			if (cp_len.cp == cp_sential || cp_len.cp == '\\')
			{
				cursor += cp_len.len;
			}
		}
	}

	return (int)(cursor - cursor_orig);
}

static int Len_after_fslash(const char * cursor, const char * terminator)
{
	const char * cursor_orig = cursor;

	cp_len_t cp_len = Peek_cp(cursor, terminator);

	switch (cp_len.cp)
	{
	case '*':
		cursor += cp_len.len;
		cursor += Len_rest_of_block_comment(cursor, terminator);
		break;
	case '/':
		cursor += cp_len.len;
		cursor += Len_rest_of_line_comment(cursor, terminator);
		break;
	case '=':
		cursor += cp_len.len;
		break;
	}

	return (int)(cursor - cursor_orig);
}

static int Len_rest_of_block_comment(const char * cursor, const char * terminator)
{
	const char * cursor_orig = cursor;

	while (cursor < terminator)
	{
		cp_len_t cp_len0 = Peek_cp(cursor, terminator);
		cursor += cp_len0.len;

		cp_len_t cp_len1 = Peek_cp(cursor, terminator);
		if (cp_len0.cp == '*' && cp_len1.cp == '/')
		{
			cursor += cp_len1.len;
			break;
		}
	}
	
	return (int)(cursor - cursor_orig);
}

static int Len_rest_of_line_comment(const char * cursor, const char * terminator)
{
	const char * cursor_orig = cursor;

	while (cursor < terminator)
	{
		cp_len_t cp_len = Peek_cp(cursor, terminator);

		if (cp_len.cp == '\n')
			break;

		cursor += cp_len.len;
	}

	return (int)(cursor - cursor_orig);
}

static int Len_after_dot(const char * cursor, const char * terminator)
{
	const char * cursor_orig = cursor;

	cp_len_t cp_len = Peek_cp(cursor, terminator);

	if (Is_cp_ascii_digit(cp_len.cp))
	{
		cursor += cp_len.len;
		cursor += Len_rest_of_ppnum(cursor, terminator);
	}
	else if (cp_len.cp == '.')
	{
		const char * cursor_peek = cursor;
		cursor_peek += cp_len.len;

		cp_len = Peek_cp(cursor_peek, terminator);
		if (cp_len.cp == '.')
		{
			cursor_peek += cp_len.len;
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

static int Len_rest_of_id(const char * cursor, const char * terminator)
{
	const char * cursor_orig = cursor;

	while (true)
	{
		cp_len_t cp_len = Peek_cp(cursor, terminator);
		if (Does_cp_extend_id(cp_len.cp))
		{
			cursor += cp_len.len;
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

static int Len_rest_of_operator(uint32_t cp_leading, const char * cursor, const char * terminator)
{
	const char * cursor_orig = cursor;

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

		const char * cursor_peek = cursor;
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
				cp_len_t cp_len = Peek_cp(cursor_peek, terminator);
				if (cp_len.cp != cp_ch)
				{
					found_match = false;
					break;
				}

				cursor_peek += cp_len.len;
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

static int Len_rest_of_ppnum(const char * cursor, const char * terminator)
{
	const char * cursor_orig = cursor;

	while (true)
	{
		cp_len_t cp_len = Peek_cp(cursor, terminator);

		if (cp_len.cp == '.')
		{
			cursor += cp_len.len;
			continue;
		}
		else if (cp_len.cp == 'e' || cp_len.cp == 'E' || cp_len.cp == 'p' || cp_len.cp == 'P')
		{
			cursor += cp_len.len;

			cp_len = Peek_cp(cursor, terminator);
			if (cp_len.cp == '+' || cp_len.cp == '-')
			{
				cursor += cp_len.len;
			}

			continue;
		}
		else if (cp_len.cp == '$')
		{
			// Clang does not allow '$' in ppnums, 
			//  even though the spec would seem to suggest that 
			//  implimentation defined id chars should be included in PP nums...

			break;
		}
		else if (Does_cp_extend_id(cp_len.cp))
		{
			// Everything (else) which extends ids can extend a ppnum

			cursor += cp_len.len;
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
