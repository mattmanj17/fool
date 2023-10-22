
#include <assert.h>
#include <stdlib.h>

#include "lcp.h"
#include "common.h"
#include "unicode.h"



// Forward declares

typedef enum
{
	COLLAPSEK_Trigraph,
	COLLAPSEK_EscapedLineBreaks,
	COLLAPSEK_CarriageReturn,
} COLLAPSEK;

static void Collapse(
	COLLAPSEK collapsek,
	lcp_t * pLcpBegin,
	lcp_t ** ppLcpEnd);

static bool FPeekCollapse(
	COLLAPSEK collapsek,
	lcp_t * pLcpBegin,
	lcp_t * pLcpEnd,
	uint32_t * pU32Peek,
	lcp_t ** ppLcpEndPeek);

static bool FPeekTrigraph(
	lcp_t * pLcpBegin,
	lcp_t * pLcpEnd,
	uint32_t * pU32Peek,
	lcp_t ** ppLcpEndPeek);

static bool FPeekCarriageReturn(
	lcp_t * pLcpBegin,
	lcp_t * pLcpEnd,
	uint32_t * pU32Peek,
	lcp_t ** ppLcpEndPeek);

static bool FPeekEscapedLineBreaks(
	lcp_t * pLcpBegin,
	lcp_t * pLcpEnd,
	uint32_t * pU32Peek,
	lcp_t ** ppLcpEndPeek);

static int Len_escaped_end_of_lines(
	const lcp_t * mic);

static int Len_escaped_end_of_line(
	const lcp_t * cursor);



// 'public' function

bool FTryDecodeLogicalCodePoints(
	const char * pChBegin,
	const char * pChEnd,
	lcp_t ** ppLcpBegin,
	lcp_t ** ppLcpEnd)
{
	// In the worst case, we will have a codepoint for every byte
	//  in the original span, so allocate enough space for that.
	//  Note that we include room for a trailing '\0' codepoint

	int cCh = (int)(pChEnd - pChBegin);
	int cAlloc = cCh + 1;

	lcp_t * pLcpBegin = (lcp_t *)calloc(sizeof(lcp_t) * cAlloc, 1);
	if (!pLcpBegin)
		return false;

	// Chew through the byte span with Try_decode_utf8

	int cLcp = 0;
	while (pChBegin < pChEnd)
	{
		uint32_t cp;
		const char * pChEndCp;
		if (Try_decode_utf8(pChBegin, pChEnd, &cp, &pChEndCp))
		{
			pLcpBegin[cLcp].cp = cp;
			pLcpBegin[cLcp].str_begin = pChBegin;
			pLcpBegin[cLcp].str_end = pChEndCp;
			pLcpBegin[cLcp].fIsDirty = false;
			pChBegin = pChEndCp;
		}
		else
		{
			pLcpBegin[cLcp].cp = UINT32_MAX;
			pLcpBegin[cLcp].str_begin = pChBegin;
			pLcpBegin[cLcp].str_end = pChBegin + 1;
			pLcpBegin[cLcp].fIsDirty = false;
			++pChBegin;
		}

		++cLcp;
	}
	assert(pChBegin == pChEnd);

	// Append a final '\0'

	pLcpBegin[cLcp].cp = '\0';
	pLcpBegin[cLcp].str_begin = pChEnd;
	pLcpBegin[cLcp].str_end = pChEnd;
	pLcpBegin[cLcp].fIsDirty = false;

	// Set ppLcpEnd

	*ppLcpEnd = pLcpBegin + cLcp;

	// Collapse trigraphs/etc

	Collapse(COLLAPSEK_CarriageReturn, pLcpBegin, ppLcpEnd);
	Collapse(COLLAPSEK_Trigraph, pLcpBegin, ppLcpEnd);
	Collapse(COLLAPSEK_EscapedLineBreaks, pLcpBegin, ppLcpEnd);

	// Set ppLcpBegin and return

	*ppLcpBegin = pLcpBegin;
	return true;
}



// Collapse impl

static void Collapse(
	COLLAPSEK collapsek,
	lcp_t * pLcpBegin,
	lcp_t ** ppLcpEnd)
{
	lcp_t * pLcpEnd = *ppLcpEnd;
	lcp_t * pLcpDest = pLcpBegin;

	while (pLcpBegin < pLcpEnd)
	{
		uint32_t u32Peek;
		lcp_t * pLcpEndPeek;
		bool fPeek = FPeekCollapse(
			collapsek,
			pLcpBegin,
			pLcpEnd,
			&u32Peek,
			&pLcpEndPeek);

		bool fIsDirty = (collapsek == COLLAPSEK_Trigraph) || (collapsek == COLLAPSEK_EscapedLineBreaks);

		if (!fPeek)
		{
			u32Peek = pLcpBegin->cp;
			pLcpEndPeek = pLcpBegin + 1;
			fIsDirty = pLcpBegin->fIsDirty;

			assert(pLcpBegin->str_end == pLcpEndPeek->str_begin);
		}

		pLcpDest->cp = u32Peek;
		pLcpDest->str_begin = pLcpBegin->str_begin;
		pLcpDest->str_end = pLcpEndPeek->str_begin;
		pLcpDest->fIsDirty = fIsDirty;

		pLcpBegin = pLcpEndPeek;
		++pLcpDest;
	}
	assert(pLcpBegin == pLcpEnd);

	// Copy trailing '\0'

	*pLcpDest = *pLcpEnd;

	// Write out new end

	*ppLcpEnd = pLcpDest;
}

static bool FPeekCollapse(
	COLLAPSEK collapsek,
	lcp_t * pLcpBegin,
	lcp_t * pLcpEnd,
	uint32_t * pU32Peek,
	lcp_t ** ppLcpEndPeek)
{
	switch (collapsek)
	{
	case COLLAPSEK_Trigraph:
		return FPeekTrigraph(pLcpBegin, pLcpEnd, pU32Peek, ppLcpEndPeek);

	case COLLAPSEK_EscapedLineBreaks:
		return FPeekEscapedLineBreaks(pLcpBegin, pLcpEnd, pU32Peek, ppLcpEndPeek);

	case COLLAPSEK_CarriageReturn:
		return FPeekCarriageReturn(pLcpBegin, pLcpEnd, pU32Peek, ppLcpEndPeek);

	default:
		return false;
	}
}

static bool FPeekTrigraph(
	lcp_t * pLcpBegin,
	lcp_t * pLcpEnd,
	uint32_t * pU32Peek,
	lcp_t ** ppLcpEndPeek)
{
	int cLcp = (int)(pLcpEnd - pLcpBegin);
	(void)cLcp;

	assert(cLcp > 0);
	if (pLcpBegin[0].cp != '?')
		return false;

	assert(cLcp > 1);
	if (pLcpBegin[1].cp != '?')
		return false;

	uint32_t pairs[][2] =
	{
		{ '<', '{' },
		{ '>', '}' },
		{ '(', '[' },
		{ ')', ']' },
		{ '=', '#' },
		{ '/', '\\' },
		{ '\'', '^' },
		{ '!', '|' },
		{ '-', '~' },
	};

	assert(cLcp > 2);

	for (int iPair = 0; iPair < COUNT_OF(pairs); ++iPair)
	{
		uint32_t * pair = pairs[iPair];
		if (pair[0] == pLcpBegin[2].cp)
		{
			*pU32Peek = pair[1];
			*ppLcpEndPeek = pLcpBegin + 3;
			return true;
		}
	}

	return false;
}

static bool FPeekCarriageReturn(
	lcp_t * pLcpBegin,
	lcp_t * pLcpEnd,
	uint32_t * pU32Peek,
	lcp_t ** ppLcpEndPeek)
{
	(void)pLcpEnd;

	assert((pLcpEnd - pLcpBegin) > 0);
	if (pLcpBegin[0].cp == '\r')
	{
		*pU32Peek = '\n';

		assert((pLcpEnd - pLcpBegin) > 1);
		if (pLcpBegin[1].cp == '\n')
		{
			*ppLcpEndPeek = pLcpBegin + 2;
		}
		else
		{
			*ppLcpEndPeek = pLcpBegin + 1;
		}

		return true;
	}

	return false;
}

static bool FPeekEscapedLineBreaks(
	lcp_t * pLcpBegin,
	lcp_t * pLcpEnd,
	uint32_t * pU32Peek,
	lcp_t ** ppLcpEndPeek)
{
	int len_esc_eol = Len_escaped_end_of_lines(pLcpBegin);
	if (!len_esc_eol)
		return false;

	if ((pLcpBegin + len_esc_eol) == pLcpEnd)
	{
		// Treat trailing escaped eol as a '\0'

		*pU32Peek = '\0';
		*ppLcpEndPeek = pLcpBegin + len_esc_eol;
	}
	else
	{
		*pU32Peek = pLcpBegin[len_esc_eol].cp;
		*ppLcpEndPeek = pLcpBegin + len_esc_eol + 1;
	}

	return true;
}

static int Len_escaped_end_of_lines(const lcp_t * mic)
{
	int len = 0;

	while (true)
	{
		int len_esc_eol = Len_escaped_end_of_line(mic + len);
		if (len_esc_eol == 0)
			break;

		len += len_esc_eol;
	}

	return len;
}

static int Len_escaped_end_of_line(const lcp_t * cursor)
{
	if (cursor[0].cp != '\\')
		return 0;

	int len = 1;
	while (Is_cp_ascii_horizontal_white_space(cursor[len].cp))
	{
		++len;
	}

	if (cursor[len].cp == '\n')
	{
		return len + 1;
	}

	if (cursor[len].cp == '\r')
	{
		if (cursor[len + 1].cp == '\n')
			return len + 2;

		return len + 1;
	}

	return 0;
}