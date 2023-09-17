#pragma once

extern const uint32_t cp_most;

typedef struct
{
	uint32_t cp;
	int len;
} cp_len_t;

bool Try_decode_utf8(
	const uint8_t * mic,
	const uint8_t * mac,
	cp_len_t * cp_len_out);

bool Is_ch_ascii(char ch);
bool Is_cp_ascii(uint32_t cp);
bool May_non_ascii_codepoint_start_id(uint32_t cp);
bool Does_non_ascii_codepoint_extend_id(uint32_t cp);
bool Is_unicode_whitespace(uint32_t cp);