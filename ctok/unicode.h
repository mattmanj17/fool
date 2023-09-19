#pragma once

typedef struct
{
	uint32_t cp_min;
	uint32_t cp_most;
} cp_min_most_t;

bool Is_cp_in_range(
	uint32_t cp,
	cp_min_most_t range);

bool Is_cp_in_ranges(
	uint32_t cp,
	const cp_min_most_t * ranges,
	int num_ranges);



typedef struct
{
	uint32_t cp;
	int len;
} cp_len_t;

bool Try_decode_utf8(
	const uint8_t * mic,
	const uint8_t * mac,
	cp_len_t * cp_len_out);



bool Is_cp_valid(uint32_t cp);
bool Is_cp_surrogate(uint32_t cp);

bool Is_cp_ascii(uint32_t cp);
bool Is_cp_ascii_digit(uint32_t cp);
bool Is_cp_ascii_lowercase(uint32_t cp);
bool Is_cp_ascii_uppercase(uint32_t cp);

bool Is_cp_ascii_horizontal_white_space(uint32_t cp);
bool Is_cp_ascii_white_space(uint32_t cp);

bool Is_ch_horizontal_white_space(char ch);
bool Is_ch_white_space(char ch);

bool Is_cp_unicode_whitespace(uint32_t cp);

