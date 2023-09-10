
#include "peek.h"

#include "ch.h"
#include "utf8.h"
#include "unicode_ids.h"



static peek_t Peek_logical(const char * mic, const char * mac)
{
	peek_t peek;
	peek.length = 0;
	peek.num_lines = 0;
	peek.new_line_start = nullptr;

	if (mic[0] == '\\')
	{
		int num_lines;
		int len_line_continue = Len_line_continues(mic, &num_lines);
		if (len_line_continue)
		{
			mic += len_line_continue;

			peek.length = len_line_continue;
			peek.num_lines = num_lines;
			peek.new_line_start = mic;
		}
	}

	if (mic == mac)
	{
		peek.cp = '\0';
		return peek;
	}

	unsigned char ch = (unsigned char)mic[0];
	if (ch == '\n' || ch == '\r')
	{
		peek.cp = '\n';
		++peek.length;

		peek.new_line_start = mic + 1;
		++peek.num_lines;

		if (ch == '\r' && mic[1] == '\n')
		{
			++peek.new_line_start;
			++peek.length;
		}
	}
	else if (ch > 0x7f)
	{
		input_byte_span_t span;
		span.cursor = (const uint8_t*)mic;
		span.max = (const uint8_t*)mac;

		uint32_t cp;
		utf8_decode_error_t err = Try_decode_utf8(&span, &cp);

		if (err == utf8_decode_ok)
		{
			peek.cp = cp;
			peek.length += (int)((const char*)span.cursor - mic);
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

static peek_t Peek_handle_ucn(const char * mic_, const char * mac)
{
	// Check for leading '\\'

	peek_t peek_bslash = Peek_logical(mic_, mac);
	if (peek_bslash.cp != '\\')
		return peek_bslash;

	//

	const char * mic_peek = mic_ + peek_bslash.length;

	// Look for 'u' or 'U' after '\\'

	peek_t peek_u = Peek_logical(mic_peek, mac);
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

	mic_peek += peek_u.length;

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

		peek_t peek_digit = Peek_logical(mic_peek, mac);

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

		mic_peek += peek_digit.length;
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

peek_t Peek(const char * mic, const char * mac)
{
	return Peek_handle_ucn(mic, mac);
}