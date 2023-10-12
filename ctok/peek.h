#pragma once

#include <stdint.h>

#include "unicode.h"

typedef struct
{
	uint32_t cp;
	int num_ch;
	const char * str;
} lcp_t; // logical codepoint

typedef struct
{
	lcp_t * lcp_mic;
	lcp_t * lcp_mac;
} lcp_span_t;

lcp_span_t Decode_logical_code_points(const char * mic, const char * mac);