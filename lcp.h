#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct //!!!FIXME_typedef_audit
{
	const char * str_begin;
	const char * str_end;
	uint32_t cp;
	bool fIsDirty;
	bool _padding[3];
} lcp_t; // logical codepoint

bool FTryDecodeLogicalCodePoints(
	const char * pChBegin,
	const char * pChEnd,
	lcp_t ** ppLcpBegin,
	lcp_t ** ppLcpEnd);