
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "unicode.h"

#include "common.h"



bool Is_cp_in_ranges(
	uint32_t cp,
	const uint32_t(*ranges)[2],
	int num_ranges)
{
	for (int i = 0; i < num_ranges; ++i)
	{
		if (cp >= ranges[i][0] && cp <= ranges[i][1])
			return true;
	}

	return false;
}



// Try_decode_utf8 Roughly based on Table 3.1B in unicode Corrigendum #1
//  Special care is taken to reject 'overlong encodings'
//  that is, using more bytes than necessary/allowed for a given code point

/*
	utf8 can encode up to 21 bits like this

	1-byte = 0 to 7 bits : 		0xxxxxxx
	2-byte = 8 to 11 bits :		110xxxxx 10xxxxxx
	3-byte = 12 to 16 bits :	1110xxxx 10xxxxxx 10xxxxxx
	4-byte = 17 to 21 bits :	11110xxx 10xxxxxx 10xxxxxx 10xxxxxx

	so
	1-byte if		<= 	0b 0111 1111, 			(0x7f)
	2-byte if		<= 	0b 0111 1111 1111,		(0x7ff)
	3-byte if		<= 	0b 1111 1111 1111 1111,	(0xffff)
	4 byte otherwise

	The only other details, are that you are not allowed to encode
	values in [0xD800, 0xDFFF] (the utf16 surogates),
	or anything > 0x10FFFF (0x10FFFF is the largest valid code point).

	Also note that, 2/3/4 byte values start with a byte with 2/3/4 leading ones.
	That is how you decode them later. (the trailing bytes all start with '10')
*/

static int Num_bytes_from_first_byte(uint8_t first_byte);
static bool Is_valid_trailing_byte(uint8_t byte);
static int Num_bytes_to_encode_cp(uint32_t cp);

bool Try_decode_utf8(
	const char * pChBegin,
	const char * pChEnd,
	uint32_t * pCp,
	const char ** ppChEndCp)
{
	// Check if we have no bytes at all

	int bytes_available = (int)(pChEnd - pChBegin);
	if (bytes_available == 0)
		return false;

	// Check if first byte is too high

	uint8_t first_byte = (uint8_t)pChBegin[0];
	if (first_byte >= 0b1111'1000)
		return false;

	// Check if first byte is a trailing byte

	if (Is_valid_trailing_byte(first_byte))
		return false;

	// Check if we do not have enough bytes

	int bytes_to_read = Num_bytes_from_first_byte(first_byte);
	if (bytes_to_read > bytes_available)
		return false;

	// Check if any trailing bytes are invalid

	for (int i = 1; i < bytes_to_read; ++i)
	{
		uint8_t trailing_byte = (uint8_t)pChBegin[i];
		if (!Is_valid_trailing_byte(trailing_byte))
			return false;
	}

	// Get the significant bits from the first byte

	uint32_t cp = first_byte;
	switch (bytes_to_read)
	{
	case 2: cp &= 0b0001'1111; break;
	case 3: cp &= 0b0000'1111; break;
	case 4: cp &= 0b0000'0111; break;
	}

	// Get bits from the trailing bytes

	for (int i = 1; i < bytes_to_read; ++i)
	{
		uint8_t trailing_bits = (uint8_t)pChBegin[i];
		trailing_bits &= 0b0011'1111;

		cp <<= 6;
		cp |= trailing_bits;
	}

	// Check for illegal codepoints

	if (!Is_cp_valid(cp))
		return false;

	// Check for 'overlong encodings'

	if (Num_bytes_to_encode_cp(cp) != bytes_to_read)
		return false;

	// We did it, copy to cp_len_out and return true

	*pCp = cp;
	*ppChEndCp = pChBegin + bytes_to_read;
	return true;
}



// Queries about code points

bool Is_cp_valid(uint32_t cp)
{
	if (Is_cp_surrogate(cp))
		return false;

	const uint32_t cp_most = 0x10FFFF;
	if (cp > cp_most)
		return false;

	return true;
}

bool Is_cp_surrogate(uint32_t cp)
{
	static const uint32_t cp_surrogate_min = 0xD800;
	if (cp < cp_surrogate_min)
		return false;

	static const uint32_t cp_surrogate_most = 0xDFFF;
	if (cp > cp_surrogate_most)
		return false;

	return true;
}

bool Is_cp_ascii(uint32_t cp)
{
	return cp <= 0x7F;
}

bool Is_cp_ascii_digit(uint32_t cp)
{
	return cp >= '0' && cp <= '9';
}

bool Is_cp_ascii_lowercase(uint32_t cp)
{
	return cp >= 'a' && cp <= 'z';
}

bool Is_cp_ascii_uppercase(uint32_t cp)
{
	return cp >= 'A' && cp <= 'Z';
}

bool Is_cp_ascii_horizontal_white_space(uint32_t cp)
{
	return cp == ' ' || cp == '\t' || cp == '\f' || cp == '\v';
}

bool Is_cp_ascii_white_space(uint32_t cp)
{
	return Is_cp_ascii_horizontal_white_space(cp) || cp == '\n' || cp == '\r';
}

bool Is_ch_horizontal_white_space(char ch)
{
	return Is_cp_ascii_horizontal_white_space((uint32_t)ch);
}

bool Is_ch_white_space(char ch)
{
	return Is_cp_ascii_white_space((uint32_t)ch);
}

bool Is_cp_unicode_whitespace(uint32_t cp)
{
	static const uint32_t unicode_whitespace[][2] =
	{
		{ 0x0085, 0x0085 }, 
		{ 0x00A0, 0x00A0 }, 
		{ 0x1680, 0x1680 },
		{ 0x180E, 0x180E }, 
		{ 0x2000, 0x200A }, 
		{ 0x2028, 0x2029 },
		{ 0x202F, 0x202F }, 
		{ 0x205F, 0x205F }, 
		{ 0x3000, 0x3000 }
	};

	return Is_cp_in_ranges(cp, unicode_whitespace, COUNT_OF(unicode_whitespace));
}



// Local helper functions

static int Num_bytes_from_first_byte(uint8_t first_byte)
{
	if (first_byte <= 0x7f)
		return 1;

	if (first_byte <= 0xDF)
		return 2;

	if (first_byte <= 0xEF)
		return 3;

	return 4;
}

static bool Is_valid_trailing_byte(uint8_t byte)
{
	// Trailing bytes start with '10'

	if (!(byte & 0b1000'0000))
		return false;

	if (byte & 0b0100'0000)
		return false;

	return true;
}

static int Num_bytes_to_encode_cp(uint32_t cp)
{
	if (cp <= 0x7f)
		return 1;

	if (cp <= 0x7ff)
		return 2;

	if (cp <= 0xffff)
		return 3;

	return 4;
}