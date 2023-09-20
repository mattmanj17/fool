
#include <assert.h>
#include <stdbool.h>

#include "peek.h"

#include "unicode.h"



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
		cp_len_t cp_len;
		if (Try_decode_utf8((const uint8_t *)mic, (const uint8_t *)mac, &cp_len))
			return cp_len;
		
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

static int Len_escaped_end_of_line(
	const char * mic, 
	const char * mac)
{
	cp_len_t cp_len_bslash = Peek_handle_utf8_eol_trigraph(mic, mac);
	if (cp_len_bslash.cp != '\\')
		return 0;

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
			return len + 2; // :(

		return len + 1;
	}

	if (mic[len] == '\r')
	{
		if (mic[len + 1] == '\n')
			return len + 2;

		return len + 1;
	}

	return 0;
}

int Len_escaped_end_of_lines(const char * mic, const char * mac)
{
	int len = 0;

	while (true)
	{
		int len_esc_eol = Len_escaped_end_of_line(mic + len, mac);
		if (len_esc_eol == 0)
			break;

		len += len_esc_eol;
	}

	return len;
}

static cp_len_t Peek_handle_escaped_eol(const char * mic, const char * mac)
{
	int len_esc_eol = Len_escaped_end_of_lines(mic, mac);
	mic += len_esc_eol;

	// Look at cp after line escapes

	cp_len_t cp_len = Peek_handle_utf8_eol_trigraph(mic, mac);

	// Add length of escaped eols

	cp_len.len += len_esc_eol;

	// Return

	return cp_len;
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

	if (cp < 0xA0 && cp != 0x24 && cp != 0x40 && cp != 0x60)
		return false;

	if (!Is_cp_valid(cp))
		return false;

	return true;
}

static cp_len_t Peek_handle_ucn(const char * mic, const char * mac)
{
	// Check for leading '\\'

	cp_len_t cp_len_bslash = Peek_handle_escaped_eol(mic, mac);
	if (cp_len_bslash.cp != '\\')
		return cp_len_bslash;

	// Look past the leading '\\'

	mic += cp_len_bslash.len;

	// Look for 'u' or 'U' after '\\'

	cp_len_t cp_len_u = Peek_handle_escaped_eol(mic, mac);
	if (cp_len_u.cp != 'u' && cp_len_u.cp != 'U')
		return cp_len_bslash;

	// Set up 'scratch' peek struct to accumulate
	//  the individual 'peeks' we do to read the UCN

	cp_len_t cp_len_result;
	cp_len_result.cp = 0;
	cp_len_result.len = cp_len_bslash.len + cp_len_u.len;

	// Advance past u/U

	mic += cp_len_u.len;

	// Look for 4 or 8 hex digits, based on u vs U

	int num_hex_digits;
	if (cp_len_u.cp == 'u')
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

		cp_len_t cp_len_digit = Peek_handle_escaped_eol(mic, mac);

		// Check if valid hex digit

		uint32_t hex_digit_value = Hex_digit_value_from_cp(cp_len_digit.cp);
		if (hex_digit_value == UINT32_MAX)
			break;

		// Fold hex digit into cp

		cp_len_result.cp <<= 4;
		cp_len_result.cp |= hex_digit_value;

		// Keep track of 'how far we have peeked'

		cp_len_result.len += cp_len_digit.len;

		// Keep track of how many digits we have read

		++hex_digits_read;

		// Advance to next digit

		mic += cp_len_digit.len;
	}

	// If we did not read the correct number of digits after the 'u',
	//  just treat this as a stray '\\'

	if (hex_digits_read < num_hex_digits)
		return cp_len_bslash;

	// Sanity check that people are not trying to encode
	//  something particularly weird with a UCN.
	//  Convert any weird inputs to the error value UINT32_MAX

	if (!Is_cp_valid_ucn(cp_len_result.cp))
	{
		cp_len_result.cp = UINT32_MAX;
	}

	// Otherwise, we read a valid UCN, and the info
	//  we need to return is in peek_result

	return cp_len_result;
}

cp_len_t Peek_cp(const char * mic, const char * mac)
{
	return Peek_handle_ucn(mic, mac);
}
