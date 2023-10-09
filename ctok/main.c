
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "count_of.h"
#include "lex.h"
#include "file.h"
#include "unicode.h"
#include "peek.h"

#define CASSERT(x) static_assert((x), #x)



static void Try_print_tokens_in_file(const wchar_t * fpath);



int wmain(int argc, wchar_t *argv[])
{
	if (argc != 2)
	{
		printf("wrong number of arguments, only expected a file path\n");
		return 1;
	}
	
	Try_print_tokens_in_file(argv[1]);
	return 0;
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

typedef enum 
{
	keyword_return,
	keyword_int,
	keyword_if,
	keyword_else,
	keyword_while,
	keyword_for,
	keyword_do,
	keyword_goto,
	keyword_struct,
	keyword_typedef,
	keyword_char,
	keyword_break,
	keyword_continue,
	keyword_sizeof,
	keyword_void,
	keyword_unsigned,
	keyword__Bool,
	keyword_short,
	keyword_extern,
	keyword_const,
	keyword_long,
	keyword_restrict,
	keyword_double,
	keyword_float,
	keyword_union,
	keyword_switch,
	keyword_case,
	keyword_default,
	keyword_enum,
	keyword_static,
	keyword_signed,
	keyword_volatile,
	keyword_generic,
	keyword_static_assert,

	keyword__cdecl,
	keyword__inline,
	keyword__builtin_va_arg,
	keyword__attribute__,

	keyword_max,
	keyword_min = 0,

	keyword_nil = -1,
} keyword_t;

static const char * str_from_keyword(keyword_t keyword)
{
	/*
		misc not hit

		auto
		inline
		register

		_Alignas
		_Alignof
		_Atomic
		_Complex
		_Imaginary
		_Noreturn
		_Thread_local
	*/

	static const char * keywords[] =
	{
		"return",
		"int",
		"if",
		"else",
		"while",
		"for",
		"do",
		"goto",
		"struct",
		"typedef",
		"char",
		"break",
		"continue",
		"sizeof",
		"void",
		"unsigned",
		"_Bool",
		"short",
		"extern",
		"const",
		"long",
		"restrict",
		"double",
		"float",
		"union",
		"switch",
		"case",
		"default",
		"enum",
		"static",
		"signed",
		"volatile",
		"_Generic",
		"_Static_assert",

		"__cdecl",
		"__inline",
		"__builtin_va_arg",
		"__attribute__",
	};
	CASSERT(COUNT_OF(keywords) == keyword_max);

	assert(keyword >= keyword_min);
	assert(keyword < keyword_max);

	return keywords[keyword];
}

static keyword_t keyword_from_str(
	const char * str,
	int len)
{
	for (keyword_t keyword = keyword_min;
		keyword < keyword_max;
		keyword = (keyword_t)(keyword + 1))
	{
		const char * str_kw = str_from_keyword(keyword);

		if (strlen(str_kw) != (size_t)len)
			continue;

		if (strncmp(str_kw, str, (size_t)len) == 0)
			return keyword;
	}

	return keyword_nil;
}

static const char * str_display_from_keyword(keyword_t keyword)
{
	// blek

	if (keyword == keyword__inline)
		return "inline";

	if (keyword == keyword__attribute__)
		return "__attribute";
	
	return str_from_keyword(keyword);
}

static void Print_id_kind(
	const char * str,
	int len)
{
	keyword_t keyword = keyword_from_str(str, len);
	if (keyword != keyword_nil)
	{
		printf("%s", str_display_from_keyword(keyword));
	}
	else
	{
		printf("identifier");
	}
}

static void Print_token(
	tok_t tok,
	const char * str,
	int len,
	int line,
	int col)
{
	// convert raw ids to keywords

	if (tok == tok_raw_identifier)
	{
		Print_id_kind(str, len);
	}
	else
	{
		printf("%s", str_from_tok(tok));
	}

	// token text

	printf(" \"");
	for (int i = 0; i < len; ++i)
	{
		Clean_and_print_ch(str[i]);
	}
	printf("\"");

	// token loc

	printf(
		" Loc=<%d:%d>\n",
		line,
		col);
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

typedef struct
{
	int num_eol;
	int offset_to_new_line_start;
} eol_info_t;

static eol_info_t Inspect_span_for_eol(const char * mic, const char * mac)
{
	const char * mic_orig = mic;

	eol_info_t eol_info;
	eol_info.num_eol = 0;
	eol_info.offset_to_new_line_start = 0;

	while (mic < mac)
	{
		int len_eol = Len_eol(mic);

		if (len_eol)
		{
			mic += len_eol;
			++eol_info.num_eol;
			eol_info.offset_to_new_line_start = (int)(mic - mic_orig);
		}
		else
		{
			++mic;
		}
	}

	return eol_info;
}

static void Print_toks_in_ch_range(const bounded_c_str_t * bstr)
{
	const char * str_mic = bstr->cursor;
	const char * str_mac = bstr->terminator;
	const char * line_start = str_mic;
	int line = 1;

	// Deal with potential UTF-8 BOM

	// Note that we leave line_start pointed at the original cursor.
	//  This means anything on the first line will have their
	//  col num bumped by 3, but that is what clang does, so whatever

	//??? is this worth filing a bug about?

	int num_ch = (int)(str_mac - str_mic);
	if (num_ch >= 3 &&
		str_mic[0] == '\xEF' &&
		str_mic[1] == '\xBB' &&
		str_mic[2] == '\xBF')
	{
		str_mic += 3;
	}

	// Munch bytes to a cp_span

	lcp_span_t lcp_span;
	Decode_utf8_to_lcp_span(str_mic, str_mac, &lcp_span);
	Collapse_lcp_span(&lcp_span);

	// Lex!

	while (lcp_span.lcp_mic < lcp_span.lcp_mac)
	{
		// Peek

		lex_t lex = Lex_leading_token(lcp_span.lcp_mic, lcp_span.lcp_mac);
		lcp_t * lcp_mic_next = lex.lcp_max;

		// Get token bounds

		const char * str_tok = lcp_span.lcp_mic[0].str;
		const char * str_tok_mac = lcp_mic_next->str;
		int num_ch_tok = (int)(str_tok_mac - str_tok);

		// Turbo hack to not print trailing escaped new lines
		// (blarg trigrpahs)

		bool is_trailing_line_escape =
			(str_tok_mac == lcp_span.lcp_mac->str) &&
			((str_tok[0] == '\\') || ((str_tok[0] == '?') && (str_tok[1] == '?') && (str_tok[2] == '/'))) &&
			(lcp_span.lcp_mic[0].cp == '\0') &&
			((lcp_mic_next - lcp_span.lcp_mic) == 1);

		int col = (int)(str_tok - line_start + 1);

		if (!is_trailing_line_escape)
		{
			if (lex.tok == tok_whitespace)
			{
				// scuffed eof handling...

				if (str_tok_mac == lcp_span.lcp_mac->str)
				{
					printf("eof \"\" Loc=<%d:%d>\n", line, col);
				}
			}
			else
			{
				Print_token(
					lex.tok,
					str_tok,
					num_ch_tok,
					line,
					col);
			}
		}

		// Handle eol

		eol_info_t eol_info = Inspect_span_for_eol(str_tok, str_tok + num_ch_tok);
		if (eol_info.num_eol)
		{
			line += eol_info.num_eol;
			line_start = str_tok + eol_info.offset_to_new_line_start;
		}

		// Advance

		lcp_span.lcp_mic = lcp_mic_next;
	}
}

static bool Starts_with_invalid_BOM(const bounded_c_str_t * bstr)
{
	typedef struct
	{
		const char * name;
		const char * bytes;
		int len;

		int _padding;
	} bom_t;

	static bom_t s_boms[10] =
	{
		{"UTF-32 (BE)", "\x00\x00\xFE\xFF", 4},
		{"UTF-32 (LE)", "\xFF\xFE\x00\x00", 4},
		{"UTF-16 (BE)", "\xFE\xFF", 2},
		{"UTF-16 (LE)", "\xFF\xFE", 2},
		{"UTF-7", "\x2B\x2F\x76", 3},
		{"UTF-1", "\xF7\x64\x4C", 3},
		{"UTF-EBCDIC", "\xDD\x73\x66\x73", 4},
		{"SCSU", "\x0E\xFE\xFF", 3},
		{"BOCU-1", "\xFB\xEE\x28", 3},
		{"GB-18030", "\x84\x31\x95\x33", 4},
	};

	int cCh = (int)(bstr->terminator - bstr->cursor);

	for (int iBom = 0; iBom < 10; ++iBom)
	{
		bom_t * bom = &s_boms[iBom];
		if (bom->len > cCh)
			continue;

		bool match = true;
		for (int iCh = 0; iCh < bom->len; ++iCh)
		{
			if (bstr->cursor[iCh] != bom->bytes[iCh])
			{
				match = false;
				break;
			}
		}

		if (match)
		{
			printf(
				"File started with invalid byte order mark. "
				"This file seems to be encoded in %s, which is not supported.",
				bom->name);

			return true;
		}
	}

	return false;
}

static void Try_print_tokens_in_file(const wchar_t * fpath)
{
	bounded_c_str_t bstr;
	bool success = Try_read_file_at_path_to_buffer(fpath, &bstr);
	if (!success)
	{
		printf("Failed to read file '%ls'.\n", fpath);
		return;
	}

	// Check for invalid BOM

	if (Starts_with_invalid_BOM(&bstr))
	{
		// Err msg printed in Starts_with_invalid_BOM...

		return;
	}

	// Print tokens

	Print_toks_in_ch_range(&bstr);
}