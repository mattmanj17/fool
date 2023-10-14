#pragma once

#include <stdint.h>

bool Is_cp_in_ranges(
	uint32_t cp,
	const uint32_t (*ranges)[2],
	int num_ranges);

bool Try_decode_utf8(
	const uint8_t * mic,
	const uint8_t * mac,
	uint32_t * pCp,
	int * pLen);

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

