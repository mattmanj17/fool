
#include <assert.h>
#include <stdbool.h>

#include "peek.h"

#include "ch.h"
#include "utf8.h"
#include "unicode_ids.h"



static cp_len_t Peek_handle_utf8_eol_trigraph(const char * mic, const char * mac)
{
	// Check for 'EOF'

	// NOTE that we expect *mac == '\0'. the str pointed to
	//  by mic may contain other nulls, but we expect there
	//  to be a final null at the end. This means is it is always safe to read
	//  another ch from mic, if reading after non-null.

	if (mic == mac)
		return {'\0', 0};

	// Cast mic[0] to unsiged, so we can stuff it in a uint32_t

	unsigned char ch = (unsigned char)mic[0];

	// Utf8

	if (ch > 0x7f)
	{
		input_byte_span_t span;
		span.cursor = (const uint8_t *)mic;
		span.max = (const uint8_t *)mac;

		cp_len_t cp_len;
		utf8_decode_error_t err = Try_decode_utf8(&span, &cp_len);

		if (err == utf8_decode_ok)
		{
			return cp_len;
		}
		
		return {UINT32_MAX, 1};
	}

	// End of line

	if (ch == '\r')
	{
		if (mic[1] == '\n')
		{
			return {'\n', 2};
		}
		
		return {'\n', 1};
	}
	
	// Trigraphs

	if (ch == '?' && mic[1] == '?')
	{
		switch (mic[2])
		{
		case '<':
			return {'{', 3};

		case '>':
			return {'}', 3};

		case '(':
			return {'[', 3};

		case ')':
			return {']', 3};

		case '=':
			return {'#', 3};

		case '/':
			return {'\\', 3};

		case '\'':
			return {'^', 3};

		case '!':
			return {'|', 3};

		case '-':
			return {'~', 3};
		}
	}
	
	// Normal ascii

	return {ch, 1};
}

typedef struct
{
	int len;
	int num_eol;
} esc_eol_len_t;

static esc_eol_len_t Len_escaped_eol(
	const char * mic, 
	const char * mac)
{
	cp_len_t cp_len_bslash = Peek_handle_utf8_eol_trigraph(mic, mac);
	if (cp_len_bslash.cp != '\\')
		return {0, 0};

	// Skip whitespace after the backslash as an extension

	int len = cp_len_bslash.len;

	while (Is_ch_horizontal_white_space(mic[len]))
	{
		++len;
	}

	// BUG clang does a horrifying thing where it slurps
	//  up a \n\r as a single line break when measuring
	//  a line continue, EVEN THOUGH it only defines
	//  "physical line breaks" as \n, \r, and \r\n.
	//  It has been like that since the very first
	//  version of the tokenizer, go figure.

	if (mic[len] == '\n')
	{
		if (mic[len + 1] == '\r')
			return {len + 2, 2}; // :(

		return {len + 1, 1};
	}

	if (mic[len] == '\r')
	{
		if (mic[len + 1] == '\n')
			return {len + 2, 1};

		return {len + 1, 1};
	}

	return {0, 0};
}

void Peek_escaped_end_of_lines(const char * mic, const char * mac, peek_cp_t * peek_cp_out)
{
	peek_cp_t peek_cp;
	peek_cp.cp = UINT32_MAX;
	peek_cp.len = 0;
	peek_cp.num_eol = 0;
	peek_cp.after_last_eol = nullptr;

	while (true)
	{
		esc_eol_len_t esc_eol_len = Len_escaped_eol(mic, mac);
		if (esc_eol_len.len == 0)
			break;

		mic += esc_eol_len.len;

		peek_cp.len += esc_eol_len.len;
		peek_cp.num_eol += esc_eol_len.num_eol;
		peek_cp.after_last_eol = mic;
	}

	*peek_cp_out = peek_cp;
}

static void Peek_handle_escaped_eol(const char * mic, const char * mac, peek_cp_t * peek_cp_out)
{
	peek_cp_t peek_cp;
	Peek_escaped_end_of_lines(mic, mac, &peek_cp);
	mic += peek_cp.len;

	// Look at cp after line escapes

	cp_len_t cp_len = Peek_handle_utf8_eol_trigraph(mic, mac);

	peek_cp.cp = cp_len.cp;
	peek_cp.len += cp_len.len;

	// Track end of lines

	if (peek_cp.cp == '\n')
	{
		++peek_cp.num_eol;
		peek_cp.after_last_eol = mic + cp_len.len;
	}

	// Return

	*peek_cp_out = peek_cp;
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

static void Accumulate_peeks(peek_cp_t * peek_cp_accumulate, peek_cp_t peek_cp)
{
	peek_cp_accumulate->len += peek_cp.len;
	peek_cp_accumulate->num_eol += peek_cp.num_eol;

	if (peek_cp.num_eol)
	{
		peek_cp_accumulate->after_last_eol = peek_cp.after_last_eol;
	}
}

static peek_cp_t Peek_handle_ucn(const char * mic, const char * mac)
{
	// Check for leading '\\'

	peek_cp_t peek_cp_bslash;
	Peek_handle_escaped_eol(mic, mac, &peek_cp_bslash);
	if (peek_cp_bslash.cp != '\\')
		return peek_cp_bslash;

	// Look past the leading '\\'

	const char * mic_peek = mic + peek_cp_bslash.len;

	// Look for 'u' or 'U' after '\\'

	peek_cp_t peek_cp_u;
	Peek_handle_escaped_eol(mic_peek, mac, &peek_cp_u);
	if (peek_cp_u.cp != 'u' && peek_cp_u.cp != 'U')
		return peek_cp_bslash;

	// Set up 'scratch' peek struct to accumulate
	//  the individual 'peeks' we do to read the UCN

	peek_cp_t peek_result;
	peek_result.cp = 0;
	peek_result.len = 0;
	peek_result.num_eol = 0;
	peek_result.after_last_eol = nullptr;
	
	Accumulate_peeks(&peek_result, peek_cp_bslash);
	Accumulate_peeks(&peek_result, peek_cp_u);

	// Advance past u/U

	mic_peek += peek_cp_u.len;

	// Look for 4 or 8 hex digits, based on u vs U

	int num_hex_digits;
	if (peek_cp_u.cp == 'u')
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

		peek_cp_t peek_cp_digit;
		Peek_handle_escaped_eol(mic_peek, mac, &peek_cp_digit);

		// Check if valid hex digit

		uint32_t hex_digit_value = Hex_digit_value_from_cp(peek_cp_digit.cp);
		if (hex_digit_value == UINT32_MAX)
			break;

		// Fold hex digit into cp

		peek_result.cp <<= 4;
		peek_result.cp |= hex_digit_value;

		// Keep track of 'how far we have peeked'

		Accumulate_peeks(&peek_result, peek_cp_digit);

		// Keep track of how many digits we have read

		++hex_digits_read;

		// Advance to next digit

		mic_peek += peek_cp_digit.len;
	}

	// If we did not read the correct number of digits after the 'u',
	//  just treat this as a stray '\\'

	if (hex_digits_read < num_hex_digits)
		return peek_cp_bslash;

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

peek_cp_t Peek_cp(const char * mic, const char * mac)
{
	return Peek_handle_ucn(mic, mac);
}
