
#include <stdio.h>

#include "print_tokens.h"
#include "lcp.h"
#include "lex.h"


// forward declares

static void Print_token(
	token_kind_t tokk,
	const char * str_tok,
	const char * str_tok_end,
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
				pChTokBegin,
				pChTokEnd,
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

void CP_to_utf16(
	uint32_t cp,
	wchar_t * awch)
{
	if (cp <= 0xFFFF)
	{
		awch[0] = (wchar_t)cp;
		awch[1] = 0;
	}
	else
	{
		uint32_t u_prime = cp - 0x10000;
		wchar_t w1 = (wchar_t)(0xD800 | (u_prime >> 10));
		wchar_t w2 = (wchar_t)(0xDC00 | (u_prime & 0x3FF));
		awch[0] = w1;
		awch[1] = w2;
		awch[2] = 0;
	}
}

static void Print_token(
	token_kind_t tokk,
	const char * str_tok,
	const char * str_tok_end,
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

	printf(" '");
	for (const char * pCh = str_tok; pCh < str_tok_end; ++pCh)
	{
		putchar(*pCh);
	}
	printf("'");

	printf("\t");

	if (fIsAtStartOfLine)
	{
		printf(" [StartOfLine]");
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