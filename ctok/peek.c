
#include <assert.h>
#include <stdbool.h>

#include "peek.h"

#include "unicode.h"



// trigraphs

cp_len_t Peek_trigraph(cp_len_str_t * cursor)
{
	uint32_t cp0 = cursor[0].cp;
	if (cp0 != '?')
		return {UINT32_MAX, 0};

	uint32_t cp1 = cursor[1].cp;
	if (cp1 != '?')
		return {UINT32_MAX, 0};

	uint32_t cp2 = cursor[2].cp;
	switch (cp2)
	{
	case '<': return {'{', 3};
	case '>': return {'}', 3};
	case '(': return {'[', 3};
	case ')': return {']', 3};
	case '=': return {'#', 3};
	case '/': return {'\\', 3};
	case '\'': return {'^', 3};
	case '!': return {'|', 3};
	case '-': return {'~', 3};
	}

	return {UINT32_MAX, 0};
}



// Escaped line breaks

int Len_escaped_end_of_line(cp_len_str_t * cursor)
{
	if (cursor[0].cp != '\\')
		return 0;

	int len = 1;
	while (Is_cp_ascii_horizontal_white_space(cursor[len].cp))
	{
		++len;
	}

	// BUG clang does a horrifying thing where it slurps
	//  up a \n\r as a single line break when measuring
	//  a line continue, EVEN THOUGH it only defines
	//  "physical line breaks" as \n, \r, and \r\n.
	//  It has been like that since the very first
	//  version of the tokenizer, go figure.

	if (cursor[len].cp == '\n')
	{
		if (cursor[len + 1].cp == '\r')
			return len + 2; // :(

		return len + 1;
	}

	if (cursor[len].cp == '\r')
	{
		if (cursor[len + 1].cp == '\n')
			return len + 2;

		return len + 1;
	}

	return 0;
}

int Len_escaped_end_of_lines(cp_len_str_t * mic)
{
	int len = 0;

	while (true)
	{
		int len_esc_eol = Len_escaped_end_of_line(mic + len);
		if (len_esc_eol == 0)
			break;

		len += len_esc_eol;
	}

	return len;
}

static cp_len_t Peek_escaped_line_breaks(cp_len_str_t * cursor)
{
	int len_esc_eol = Len_escaped_end_of_lines(cursor);
	if (!len_esc_eol)
		return {UINT32_MAX, 0};

	return {cursor[len_esc_eol].cp, len_esc_eol + 1};
}



// Convert '\r' and "\r\n" to '\n'

static cp_len_t Peek_line_break(cp_len_str_t * cursor)
{
	if (cursor[0].cp == '\r')
	{
		if (cursor[1].cp == '\n')
		{
			return {'\n', 2};
		}
		else
		{
			return {'\n', 1};
		}
	}
	
	return {UINT32_MAX, 0};
}





// Hex UCNs

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

	if (cp < 0xA0 && cp != 0x24 && cp != 0x40 && cp != 0x60)
		return false;

	if (!Is_cp_valid(cp))
		return false;

	return true;
}

static cp_len_t Peek_hex_ucn(cp_len_str_t * cursor)
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

	while (hex_digits_read < num_hex_digits)
	{
		// Check if valid hex digit

		uint32_t hex_digit_value = Hex_digit_value_from_cp(cursor[len].cp);
		if (hex_digit_value == UINT32_MAX)
			break;

		// Fold hex digit into cp

		cp_result <<= 4;
		cp_result |= hex_digit_value;

		// Keep track of how many digits we have read

		++hex_digits_read;

		// Advance to next digit

		++len;
	}

	// If we did not read the correct number of digits after the 'u',
	//  just treat this as a stray '\\'

	if (hex_digits_read < num_hex_digits)
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



// Generic driver function to run a collapse function over a span

typedef cp_len_t (*collapse_fn_t)(cp_len_str_t * cursor);

void Collapse(collapse_fn_t collapse_fn, cp_span_t * cp_span)
{
	cp_len_str_t * cursor_from = cp_span->mic;
	cp_len_str_t * cursor_to = cp_span->mic;

	while (cursor_from < cp_span->mac)
	{
		cp_len_t cp_len = collapse_fn(cursor_from);
		if (cp_len.len)
		{
			cursor_to->cp = cp_len.cp;
			cursor_to->str = cursor_from->str;

			int len = 0;
			for (int i_cp = 0; i_cp < cp_len.len; ++i_cp)
			{
				len += cursor_from[i_cp].len;
			}
			cursor_to->len = len;

			cursor_from += cp_len.len;

			assert(cursor_to->str + cursor_to->len == cursor_from->str);
			++cursor_to;
		}
		else
		{
			*cursor_to = *cursor_from;
			++cursor_to;
			++cursor_from;
		}
	}
	assert(cursor_from == cp_span->mac);

	// Copy trailing '\0'

	*cursor_to = *cursor_from;
	cp_span->mac = cursor_to;
}



// Exposed function

void Collapse_cp_span(cp_span_t * cp_span)
{
	// BUG ostensibly the standard says Collapse(Peek_line_break)
	//  should happen first, but it has to happen
	//  after Collapse(Peek_escaped_line_breaks) because of a hack
	//  we do there to match clang
	
	Collapse(Peek_trigraph, cp_span);
	Collapse(Peek_escaped_line_breaks, cp_span);
	Collapse(Peek_line_break, cp_span);
	Collapse(Peek_hex_ucn, cp_span);
}