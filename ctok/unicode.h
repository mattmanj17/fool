#pragma once

bool Is_ch_ascii(char ch);
bool Is_cp_ascii(uint32_t cp);
bool May_non_ascii_codepoint_start_id(uint32_t cp);
bool Does_non_ascii_codepoint_extend_id(uint32_t cp);
bool Is_unicode_whitespace(uint32_t cp);



extern const uint32_t cp_most;

typedef struct
{
	const uint8_t * cursor;
	const uint8_t * max;
} input_byte_span_t;

typedef struct
{
	uint8_t * cursor;
	const uint8_t * max;
} output_byte_span_t;

bool Try_encode_utf8(
	uint32_t cp,
	output_byte_span_t * dest_span);

// BUG it is possible to fail to decode for multiple reasons...
//  i.e., we did not have enough bytes to decode, AND
//  the bytes we did have indicated an invalid encoding.
//  There is an argument to be made that we should actually
//  return error FLAGS instead of a single 'error code',
//  to encode this more accuratly...

typedef struct
{
	uint32_t cp;
	int len;
} cp_len_t;

bool Try_decode_utf8(
	input_byte_span_t * source_span,
	cp_len_t * cp_len_out);