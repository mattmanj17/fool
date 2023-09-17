#pragma once

typedef enum
{
	lang_ver_c99,
	lang_ver_c11,
	lang_ver_c2x,
} lang_ver_t;

bool Is_ch_ascii(char ch);
bool Is_cp_ascii(uint32_t cp);
bool May_non_ascii_codepoint_start_id(lang_ver_t lang_ver, uint32_t cp);
bool Does_non_ascii_codepoint_extend_id(lang_ver_t lang_ver, uint32_t cp);
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

typedef enum
{
	utf8_encode_ok,
	utf8_encode_illegal_surrogate,
	utf8_encode_illegal_cp_too_high,
	utf8_encode_no_space_in_dest,
} utf8_encode_error_t;

utf8_encode_error_t Try_encode_utf8(
	uint32_t cp,
	output_byte_span_t * dest_span);

typedef enum
{
	utf8_decode_ok,
	utf8_decode_source_too_short,
	utf8_decode_first_marked_as_trailing,
	utf8_decode_overlong_2_byte,
	utf8_decode_invalid_trailing_byte,
	utf8_decode_overlong_3_byte,
	utf8_decode_overlong_4_byte,
	utf8_decode_illegal_surrogate,
	utf8_decode_cp_too_high,
} utf8_decode_error_t;

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

utf8_decode_error_t Try_decode_utf8(
	input_byte_span_t * source_span,
	cp_len_t * cp_len_out);