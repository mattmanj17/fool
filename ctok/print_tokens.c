
#include <stdio.h>

#include "print_tokens.h"
#include "lcp.h"
#include "lex.h"


// forward declares

static void Print_token(
	token_kind_t tokk,
	const char * str_tok,
	const char * str_tok_end,
	int line,
	int col);

static void Clean_and_print_ch(
	char ch);

static void InspectSpanForEol(
	const char * pChBegin,
	const char * pChEnd,
	int * pCLine,
	const char ** ppStartOfLine);

static int Len_eol(
	const char * str);



// 'public' functions

void PrintRawTokens(const char * pChBegin, const char * pChEnd)
{
	// Munch bytes to logical characters

	lcp_t * pLcpBegin;
	lcp_t * pLcpEnd;
	if (!FTryDecodeLogicalCodePoints(pChBegin, pChEnd, &pLcpBegin, &pLcpEnd))
		return;

	// Keep track of line info

	const char * line_start = pChBegin;
	int line = 1;

	// Lex!

	while (pLcpBegin < pLcpEnd)
	{
		lcp_t * pLcpTokEnd;
		token_kind_t tokk = TokkPeek(pLcpBegin, pLcpEnd, &pLcpTokEnd);

		// Turbo hack to not print trailing escaped new lines

		const char * pChTokBegin = pLcpBegin->str_begin;
		const char * pChTokEnd = pLcpTokEnd->str_begin;

		bool fIsLastToken = (pLcpTokEnd == pLcpEnd);

		bool fStartsWithPhysicalBackslash = (pChTokBegin[0] == '\\');
		bool fStartsWithTrigraphBackslash = (pChTokBegin[0] == '?') && (pChTokBegin[1] == '?') && (pChTokBegin[2] == '/');
		bool fStartsWithBackslash = fStartsWithPhysicalBackslash || fStartsWithTrigraphBackslash;

		bool fIsLogicalNull = (pLcpBegin->cp == '\0');

		bool is_trailing_line_escape =
			fIsLastToken &&
			fStartsWithBackslash &&
			fIsLogicalNull;

		if (!is_trailing_line_escape)
		{
			Print_token(
				tokk,
				pChTokBegin,
				pChTokEnd,
				line,
				(int)(pChTokBegin - line_start + 1));
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

static void Print_token(
	token_kind_t tokk,
	const char * str_tok,
	const char * str_tok_end,
	int line,
	int col)
{
	// Token Kind

	switch (tokk)
	{
	case tokk_bogus_ucn:
	case tokk_stray_backslash:
	case tokk_whitespace:
	case tokk_unterminated_quote:
	case tokk_zero_length_char_lit:
	case tokk_unterminated_block_comment:
	case tokk_unknown_byte:
		printf("unknown");
		break;

	default:
		printf("%s", str_from_tokk(tokk));
		break;
	}

	// token text

	printf(" \"");
	for (const char * pCh = str_tok; pCh < str_tok_end; ++pCh)
	{
		Clean_and_print_ch(*pCh);
	}
	printf("\"");

	// token loc

	printf(
		" Loc=<%d:%d>\n",
		line,
		col);
}

static void Clean_and_print_ch(char ch)
{
	// NOTE we print ' ' as \x20 so that we can split output on spaces

	switch (ch)
	{
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
	case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
	case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u':
	case 'v': case 'w': case 'x': case 'y': case 'z':
	case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
	case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
	case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
	case 'V': case 'W': case 'X': case 'Y': case 'Z':
	case '_': case '$':
	case '!': case '#': case '%': case '&': case '\'': case '(': case ')':
	case '*': case '+': case ',': case '-': case '.': case '/': case ':':
	case ';': case '<': case '=': case '>': case '?': case '@': case '[':
	case ']': case '^': case '`': case '{': case '|': case '}': case '~':
		putchar(ch);
		break;
	case '\a':
		printf("\\a");
		break;
	case '\b':
		printf("\\b");
		break;
	case '\f':
		printf("\\f");
		break;
	case '\n':
		printf("\\n");
		break;
	case '\r':
		printf("\\r");
		break;
	case '\t':
		printf("\\t");
		break;
	case '\v':
		printf("\\v");
		break;
	case '"':
		printf("\\\"");
		break;
	case '\\':
		printf("\\\\");
		break;
	default:
		printf("\\x%02hhx", ch);
		break;
	}
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