
#include <stdio.h>

#include "print_tokens.h"
#include "lcp.h"
#include "lex.h"


// forward declares

static void Print_token(
	token_kind_t tokk,
	lcp_t * lcp_tok,
	lcp_t * lcp_tok_end,
	const wchar_t * file_path,
	bool fIsAtStartOfLine,
	int line,
	int col);

static void InspectSpanForEol(
	const char * pChBegin,
	const char * pChEnd,
	int * pCLine,
	const char ** ppStartOfLine);

static int Len_eol(
	const char * str);



// 'public' functions

void PrintRawTokens(const wchar_t * file_path, const char * pChBegin, const char * pChEnd)
{
	// Munch bytes to logical characters

	lcp_t * pLcpBegin;
	lcp_t * pLcpEnd;
	if (!FTryDecodeLogicalCodePoints(pChBegin, pChEnd, &pLcpBegin, &pLcpEnd))
		return;

	// Keep track of line info

	const char * line_start = pChBegin;
	int line = 1;

	bool fIsAtStartOfLine = true;

	// Lex!

	while (pLcpBegin < pLcpEnd)
	{
		lcp_t * pLcpTokEnd;
		token_kind_t tokk = TokkPeek(pLcpBegin, pLcpEnd, &pLcpTokEnd);

		// Turbo hack to not print trailing escaped new lines

		const char * pChTokBegin = pLcpBegin->str_begin;
		const char * pChTokEnd = pLcpTokEnd->str_begin;

		bool fIsLastToken = (pLcpTokEnd == pLcpEnd);
		bool fIsDirty = pLcpBegin->fIsDirty;
		bool fIsLogicalNull = (pLcpBegin->cp == '\0');

		bool is_trailing_line_escape =
			fIsLastToken &&
			fIsDirty &&
			fIsLogicalNull;

		if (!is_trailing_line_escape)
		{
			Print_token(
				tokk,
				pLcpBegin,
				pLcpTokEnd,
				file_path,
				fIsAtStartOfLine,
				line,
				(int)(pChTokBegin - line_start + 1));
		}

		// fIsAtStartOfLine

		switch (tokk)
		{
		case tokk_hz_whitespace:
			break;

		case tokk_multi_line_whitespace:
			fIsAtStartOfLine = true;
			break;

		//case tokk_line_comment:
		//case tokk_block_comment:
		default:
			fIsAtStartOfLine = false;
			break;
		}

		// Handle eol

		int cLine;
		const char * pStartOfLine;
		InspectSpanForEol(pChTokBegin, pChTokEnd, &cLine, &pStartOfLine);

		if (cLine)
		{
			line += cLine;
			line_start = pStartOfLine;
		}

		// Advance

		pLcpBegin = pLcpTokEnd;
	}
}



// Impl

void CP_to_utf8(
	uint32_t cp,
	char * ach,
	int * len)
{
	if (cp <= 0x7F)
	{
		// 0xxxxxxx

		*len = 1;
		ach[0] = (char)cp;
	}
	else if (cp <= 0x7FF)
	{
		// 110xxxxx 10xxxxxx

		*len = 2;
		ach[0] = (char)(0b11000000 | ((cp & 0x7FF) >> 6));
		ach[1] = (char)(0b10000000 | ((cp & 0x03F) >> 0));
	}
	else if (cp <= 0xFFFF)
	{
		// 1110xxxx 10xxxxxx 10xxxxxx

		*len = 3;
		ach[0] = (char)(0b10000000 | ((cp & 0xFFFF) >> 12));
		ach[1] = (char)(0b10000000 | ((cp & 0x0FFF) >> 6));
		ach[2] = (char)(0b10000000 | ((cp & 0x003F) >> 0));
	}
	else
	{
		// 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx

		*len = 4;

		ach[0] = (char)(0b11110000 | ((cp & 0x1FFFFF) >> 18));
		ach[1] = (char)(0b10000000 | ((cp & 0x03FFFF) >> 12));
		ach[2] = (char)(0b10000000 | ((cp & 0x000FFF) >> 6));
		ach[3] = (char)(0b10000000 | ((cp & 0x00003F) >> 0));
	}
}

static void Print_token(
	token_kind_t tokk,
	lcp_t * lcp_tok,
	lcp_t * lcp_tok_end,
	const wchar_t * file_path,
	bool fIsAtStartOfLine,
	int line,
	int col)
{
	// Token Kind

	switch (tokk)
	{
	case tokk_bogus_ucn:
	case tokk_stray_backslash:
	case tokk_hz_whitespace:
	case tokk_multi_line_whitespace:
	case tokk_unterminated_quote:
	case tokk_zero_length_char_lit:
	case tokk_unterminated_block_comment:
	case tokk_unknown_byte:
		printf("unknown");
		break;

	case tokk_line_comment:
	case tokk_block_comment:
		printf("comment");
		break;

	default:
		printf("%s", str_from_tokk(tokk));
		break;
	}

	// token text

	bool fIsUnclean = false;

	printf(" '");
	for (lcp_t * lcp = lcp_tok; lcp < lcp_tok_end; ++lcp)
	{
		if (lcp->fIsDirty)
		{
			fIsUnclean = true;
		}

		char ach[4];
		int len;
		CP_to_utf8(lcp->cp, ach, &len);

		for (int i = 0; i < len; ++i)
		{
			putchar(ach[i]);
		}
	}
	printf("'");

	printf("\t");

	if (fIsAtStartOfLine)
	{
		printf(" [StartOfLine]");
	}

	if (fIsUnclean)
	{
		printf(" [UnClean='");

		const char * str_tok = lcp_tok->str_begin;
		const char * str_tok_end = lcp_tok_end->str_begin;

		for (const char * pCh = str_tok; pCh < str_tok_end; ++pCh)
		{
			putchar(*pCh);
		}

		printf("']");
	}

	// token loc

	printf(
		"\tLoc=<%ls:%d:%d>\n",
		file_path,
		line,
		col);
}

static void InspectSpanForEol(
	const char * pChBegin,
	const char * pChEnd,
	int * pCLine,
	const char ** ppStartOfLine)
{
	// BUG if we care about having random access to line info, 
	//  we would need a smarter answer than InspectSpanForEol...

	int cLine = 0;
	const char * pStartOfLine = NULL;

	while (pChBegin < pChEnd)
	{
		// BUG what if pChEnd straddles an EOL?

		int len_eol = Len_eol(pChBegin);

		if (len_eol)
		{
			pChBegin += len_eol;

			++cLine;
			pStartOfLine = pChBegin;
		}
		else
		{
			++pChBegin;
		}
	}

	*pCLine = cLine;
	*ppStartOfLine = pStartOfLine;
}

static int Len_eol(const char * str)
{
	char ch = str[0];

	if (ch == '\n')
	{
		return 1;
	}
	else if (ch == '\r')
	{
		if (str[1] == '\n')
		{
			return 2;
		}
		else
		{
			return 1;
		}
	}
	else
	{
		return 0;
	}
}