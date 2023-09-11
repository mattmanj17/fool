#include "utf8.h"

static const uint32_t cp_surrogate_min = 0xD800;
static const uint32_t cp_surrogate_most = 0xDFFF;
const uint32_t cp_most = 0x10FFFF;

utf8_encode_error_t Try_encode_utf8(
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
			return utf8_encode_illegal_surrogate;

		bytes_to_write = 3;
	}
	else if (cp <= cp_most)
	{
		bytes_to_write = 4;
	}
	else
	{
		return utf8_encode_illegal_cp_too_high;
	}

	// Check for space in dest

	int bytes_available = (int)(dest_span->max - dest_span->cursor);
	if (bytes_available < bytes_to_write)
		return utf8_encode_no_space_in_dest;

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
	return utf8_encode_ok;
}

// Try_decode_utf8 Roughly based on Table 3.1B in unicode Corrigendum #1
//  Special care is taken to reject 'overlong encodings'
//  that is, using more bytes than necessary/allowed for a given code point

// BUG Try_decode_utf8 is rather spaghetti-y, but is being very literal
//  about the checks it is doing, so it is easy to debug + verify its correctness

utf8_decode_error_t Try_decode_utf8(
	input_byte_span_t * source_span, 
	cp_len_t * cp_len_out)
{
	// Figure out how many bytes we have to work with in source_span

	int bytes_available = (int)(source_span->max - source_span->cursor);

	// If source_span is empty, we can not even look at the
	//  'first' byte to figure anything out, so just
	//  return utf8_decode_source_too_short right away

	if (bytes_available == 0)
		return utf8_decode_source_too_short;

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

		return utf8_decode_first_marked_as_trailing;
	}
	else if (first_byte == 0xC0 || first_byte == 0xC1)
	{
		// if first == 0xC0, we are only encoding (at most) 6 significant bits,
		//  and if first == 0xC1, we are only encoding (at most) 7 bits.
		//  using more than one byte to encode a codepoint <= 7f
		//  is illegal

		return utf8_decode_overlong_2_byte;
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
			return utf8_decode_source_too_short;

		uint8_t second = source_span->cursor[1];

		// Must have first bit set

		// NOTE this is duplicating the normal <= 0x7f check below.
		//  We ALSO do it here to return the most correct error code.

		if (second <= 0x7f)
			return utf8_decode_invalid_trailing_byte;

		// Check for 'overlong-ness'

		if (second < 0xA0)
		{
			// If less than '0b10100000',
			//  the 2nd byte only has 5 significant bits, 
			//  so we are encoding a max of 11 bits (5 + 6 in the 3rd byte), 
			//  which could have been represented with only two bytes, 
			//  so this is 'overlong'

			return utf8_decode_overlong_3_byte;
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
			return utf8_decode_source_too_short;

		uint8_t second = source_span->cursor[1];

		// Must have first bit set

		// NOTE this is duplicating the normal <= 0x7f check below.
		//  We ALSO do it here to return the most correct error code.

		if (second <= 0x7f)
			return utf8_decode_invalid_trailing_byte;

		// Check for 'overlong-ness'

		if (second < 0x90)
		{
			// If less than '0b10010000',
			//  the 2nd byte only has 4 significant bits, 
			//  so we are encoding a max of 11 bits (4 + 12 in the 3rd + 4th bytes), 
			//  which could have been represented with only three bytes, 
			//  so this is 'overlong'

			return utf8_decode_overlong_4_byte;
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

		return utf8_decode_cp_too_high;
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
			return utf8_decode_source_too_short;

		uint8_t trailing_byte = source_span->cursor[i];

		// need first two bits to be exactly '10'

		if (trailing_byte < 0x80 || trailing_byte > 0xBF)
			return utf8_decode_invalid_trailing_byte;
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
		return utf8_decode_illegal_surrogate;

	// Make sure below cp_most

	if (cp > cp_most)
		return utf8_decode_cp_too_high;

	// We did it, copy to dest_cp and return OK

	cp_len_out->cp = cp;
	cp_len_out->len = bytes_to_read;

	source_span->cursor += bytes_to_read;
	return utf8_decode_ok;
}