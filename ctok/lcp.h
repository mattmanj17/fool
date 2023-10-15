#pragma once

#include <stdint.h>

typedef struct //!!!FIXME_typedef_audit
{
	const char * str_begin;
	const char * str_end;
	uint32_t cp;
	int _padding;
} lcp_t; // logical codepoint

bool FTryDecodeLogicalCodePoints(
	const char * pChBegin,
	const char * pChEnd,
	lcp_t ** ppLcpBegin,
	lcp_t ** ppLcpEnd);