#pragma once

#include <stdint.h>

#include "unicode.h"

typedef struct
{
	uint32_t cp;
	int num_ch;
	const char * str;
} lcp_t; // logical codepoint