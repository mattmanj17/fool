#pragma once

#include <stdbool.h>

#include "unicode.h"


typedef struct
{
	lcp_t * lcp_max;
} lex_t;

lex_t Lex_leading_token(lcp_t * cursor, lcp_t * terminator);
