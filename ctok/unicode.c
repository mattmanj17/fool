
#include <stdint.h>

#include "unicode.h"



static const uint32_t cp_surrogate_min = 0xD800;
static const uint32_t cp_surrogate_most = 0xDFFF;
const uint32_t cp_most = 0x10FFFF;

bool Try_encode_utf8(
	uint32_t cp,
	output_byte_span_t * dest_span)
{
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

	// figure out how many bytes we need to encode this code point
	// (or if this codepoint is too large)

	int bytes_to_write;
	if (cp <= 0x7f)
	{
		bytes_to_write = 1;
	}
	else if (cp <= 0x7ff)
	{
		bytes_to_write = 2;
	}
	else if (cp <= 0xffff)
	{
		// Check for illegal surrogate values

		if (cp >= cp_surrogate_min && cp <= cp_surrogate_most)
			return false;

		bytes_to_write = 3;
	}
	else if (cp <= cp_most)
	{
		bytes_to_write = 4;
	}
	else
	{
		return false;
	}

	// Check for space in dest

	int bytes_available = (int)(dest_span->max - dest_span->cursor);
	if (bytes_available < bytes_to_write)
		return false;

	// write least significant bits in 6 bit chunks.
	// we write from right to left, 
	// so we can >>= 6 to get the next 6 bits to write.

	int i_byte_write = bytes_to_write - 1;
	while (i_byte_write > 0)
	{
		uint8_t byte = (uint8_t)(cp & 0b111111);
		byte |= 0b10000000; // trailing byte mark

		dest_span->cursor[i_byte_write] = byte;

		cp >>= 6;
		--i_byte_write;
	}

	// Write remaining most significant bits

	uint8_t first_byte_mark[5] =
	{
		0x00, // 0 bytes (unused, bytes_to_write always > 0)
		0x00, // 1 byte  (leave cp unchanged)
		0xC0, // 2 bytes (0b11000000)
		0xE0, // 3 bytes (0b11100000)
		0xF0, // 4 bytes (0b11110000)
	};

	*dest_span->cursor = (uint8_t)(cp | first_byte_mark[bytes_to_write]);

	// advance dest_span->begin and return

	dest_span->cursor += bytes_to_write;
	return true;
}

// Try_decode_utf8 Roughly based on Table 3.1B in unicode Corrigendum #1
//  Special care is taken to reject 'overlong encodings'
//  that is, using more bytes than necessary/allowed for a given code point

// BUG Try_decode_utf8 is rather spaghetti-y, but is being very literal
//  about the checks it is doing, so it is easy to debug + verify its correctness

bool Try_decode_utf8(
	input_byte_span_t * source_span,
	cp_len_t * cp_len_out)
{
	// Figure out how many bytes we have to work with in source_span

	int bytes_available = (int)(source_span->max - source_span->cursor);

	// If source_span is empty, we can not even look at the
	//  'first' byte to figure anything out, so just
	//  return utf8_decode_source_too_short right away

	if (bytes_available == 0)
		return false;

	// Figure out how many bytes to read by looking at the first byte
	//  (and also do some initial validation...)

	uint8_t first_byte = source_span->cursor[0];

	int bytes_to_read;
	if (first_byte <= 0x7f)
	{
		bytes_to_read = 1;
	}
	else if (first_byte < 0xC0)
	{
		// Invalid first byte if first two bits not set

		return false;
	}
	else if (first_byte == 0xC0 || first_byte == 0xC1)
	{
		// if first == 0xC0, we are only encoding (at most) 6 significant bits,
		//  and if first == 0xC1, we are only encoding (at most) 7 bits.
		//  using more than one byte to encode a codepoint <= 7f
		//  is illegal

		return false;
	}
	else if (first_byte <= 0xDF)
	{
		// There are enough significant bits in the first byte,
		//  so we could be a valid 2 byte sequence

		bytes_to_read = 2;
	}
	else if (first_byte == 0xE0)
	{
		// Going to use 3 bytes, but there are no significant bits
		//  in the first byte (0b11100000) so we need to check 
		//  the 2nd byte for 'overlong-ness'

		if (bytes_available < 2)
			return false;

		uint8_t second = source_span->cursor[1];

		// Must have first bit set

		// NOTE this is duplicating the normal <= 0x7f check below.
		//  We ALSO do it here to return the most correct error code.

		if (second <= 0x7f)
			return false;

		// Check for 'overlong-ness'

		if (second < 0xA0)
		{
			// If less than '0b10100000',
			//  the 2nd byte only has 5 significant bits, 
			//  so we are encoding a max of 11 bits (5 + 6 in the 3rd byte), 
			//  which could have been represented with only two bytes, 
			//  so this is 'overlong'

			return false;
		}

		// NOTE we check for <= BF in the normal case below

		bytes_to_read = 3;
	}
	else if (first_byte <= 0xEF)
	{
		// There are enough significant bits in the first byte,
		//  so we could be a valid 3 byte sequence

		bytes_to_read = 3;
	}
	else if (first_byte == 0xF0)
	{
		// BUG this code is very similar to the (first_byte == 0xE0) case...

		// Going to use 4 bytes, but there are no significant bits
		//  in the first byte (0b11110000) so we need to check 
		//  the 2nd byte for 'overlong-ness'

		if (bytes_available < 2)
			return false;

		uint8_t second = source_span->cursor[1];

		// Must have first bit set

		// NOTE this is duplicating the normal <= 0x7f check below.
		//  We ALSO do it here to return the most correct error code.

		if (second <= 0x7f)
			return false;

		// Check for 'overlong-ness'

		if (second < 0x90)
		{
			// If less than '0b10010000',
			//  the 2nd byte only has 4 significant bits, 
			//  so we are encoding a max of 11 bits (4 + 12 in the 3rd + 4th bytes), 
			//  which could have been represented with only three bytes, 
			//  so this is 'overlong'

			return false;
		}

		// NOTE we check for <= BF in the normal case below

		bytes_to_read = 4;
	}
	else if (first_byte <= 0xF7)
	{
		// There are enough significant bits in the first byte,
		//  so we could be a valid 3 byte sequence

		// NOTE we deal with checking cp_most below

		bytes_to_read = 4;
	}
	else
	{
		// more than 4 leading ones

		return false;
	}

	// Check that each trailing byte is valid 

	for (int i = 1; i < bytes_to_read; ++i)
	{
		// ... but if we run out of input, return THAT error code

		// BUG we do it this weird way so that we return 
		//  utf8_decode_invalid_trailing_byte even
		//  if bytes_available < bytes_to_read...
		//  unclear if this is important...

		if (i > bytes_available - 1)
			return false;

		uint8_t trailing_byte = source_span->cursor[i];

		// need first two bits to be exactly '10'

		if (trailing_byte < 0x80 || trailing_byte > 0xBF)
			return false;
	}

	// Ok, we have validated that this is legit utf8, 
	//  so decode into a 21 bit codepoint

	// Get the significant bits from the first byte

	uint32_t cp = 0;
	switch (bytes_to_read)
	{
	case 1:
		cp = first_byte;
		break;
	case 2:
		cp = (uint8_t)(first_byte & 0b11111);
		break;
	case 3:
		cp = (uint8_t)(first_byte & 0b1111);
		break;
	case 4:
		cp = (uint8_t)(first_byte & 0b111);
		break;
	}

	// Get the bits from the trailing bytes

	for (int i = 1; i < bytes_to_read; ++i)
	{
		uint8_t trailing_byte = source_span->cursor[i];
		cp <<= 6;
		cp |= (uint8_t)(trailing_byte & 0b111111);
	}

	// Check for illegal surrogate values

	if (cp >= cp_surrogate_min && cp <= cp_surrogate_most)
		return false;

	// Make sure below cp_most

	if (cp > cp_most)
		return false;

	// We did it, copy to dest_cp and return OK

	cp_len_out->cp = cp;
	cp_len_out->len = bytes_to_read;

	source_span->cursor += bytes_to_read;
	return true;
}



// Cribbed from LLVM UnicodeCharRanges.h/etc
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

typedef struct
{
	uint32_t min;
	uint32_t most;
} codepoint_range_t;

// C11 D.1 [charname.allowed]

static const codepoint_range_t c11_allowed[] =
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

// C11 D.2 [charname.disallowed]

static const codepoint_range_t c11_disallowed_initial[] =
{
	{ 0x0300, 0x036F }, { 0x1DC0, 0x1DFF }, { 0x20D0, 0x20FF },
	{ 0xFE20, 0xFE2F }
};

// Unicode v6.2, chapter 6.2, table 6-2.
static const codepoint_range_t unicode_whitespace[] =
{
	{ 0x0085, 0x0085 }, { 0x00A0, 0x00A0 }, { 0x1680, 0x1680 },
	{ 0x180E, 0x180E }, { 0x2000, 0x200A }, { 0x2028, 0x2029 },
	{ 0x202F, 0x202F }, { 0x205F, 0x205F }, { 0x3000, 0x3000 }
};

static bool Is_codepoint_in_range(
	uint32_t cp,
	codepoint_range_t range)
{
	return cp >= range.min && cp <= range.most;
}

static bool Is_codepoint_in_ranges(
	uint32_t cp,
	const codepoint_range_t * ranges,
	int num_ranges)
{
	for (int i = 0; i < num_ranges; ++i)
	{
		if (Is_codepoint_in_range(cp, ranges[i]))
			return true;
	}

	return false;
}

#define COUNT_OF(x) ((sizeof(x)/sizeof(0[(x)])) / ((size_t)(!(sizeof(x) % sizeof(0[(x)])))))

bool Is_ch_ascii(char ch)
{
	return Is_cp_ascii((unsigned char)ch);
}

bool Is_cp_ascii(uint32_t cp)
{
	return cp <= 0x7F;
}

bool May_non_ascii_codepoint_start_id(uint32_t cp)
{
	if (Is_cp_ascii(cp))
		return false;
	
	if (!Does_non_ascii_codepoint_extend_id(cp))
		return false;
	
	return !Is_codepoint_in_ranges(cp, c11_disallowed_initial, COUNT_OF(c11_disallowed_initial));
}

bool Does_non_ascii_codepoint_extend_id(uint32_t cp)
{
	if (Is_cp_ascii(cp))
		return false;
	
	return Is_codepoint_in_ranges(cp, c11_allowed, COUNT_OF(c11_allowed));
}

bool Is_unicode_whitespace(uint32_t cp)
{
	return Is_codepoint_in_ranges(cp, unicode_whitespace, COUNT_OF(unicode_whitespace));
}