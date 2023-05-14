#pragma once

#include <stdint.h>
#include <stdbool.h>

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