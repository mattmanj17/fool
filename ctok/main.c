
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "common.h"
#include "lex.h"
#include "unicode.h"
#include "lcp.h"

typedef struct
{
	lcp_t * lcp_mic;
	lcp_t * lcp_mac;
} lcp_span_t;



typedef struct
{
	char * cursor;
	const char * terminator;
} bounded_c_str_t;

static bool Try_read_file_to_buffer(FILE * file, bounded_c_str_t * bstr)
{
	bstr->cursor = NULL;
	bstr->terminator = NULL;

	int err = fseek(file, 0, SEEK_END);
	if (err)
	{
		printf("fseek failed");
		exit(1);
	}

	long len_file = ftell(file);
	if (len_file < 0)
	{
		printf("ftell failed");
		exit(1);
	}

	err = fseek(file, 0, SEEK_SET);
	if (err)
	{
		printf("fseek failed");
		exit(1);
	}

	void * allocation = calloc((size_t)(len_file + 1), 1);
	if (!allocation)
	{
		printf("ALLOCATION FAILED");
		exit(1);
	}

	size_t bytes_read = fread(allocation, 1, (size_t)len_file, file);
	if (bytes_read != (size_t)len_file)
	{
		printf("fread failed");
		exit(1);
	}

	bstr->cursor = (char *)allocation;
	bstr->terminator = bstr->cursor + len_file;

	return true;
}

static bool Try_read_file_at_path_to_buffer(const wchar_t * fpath, bounded_c_str_t * bstr)
{
	bstr->cursor = NULL;
	bstr->terminator = NULL;

	FILE * file = _wfopen(fpath, L"rb");
	if (!file)
		return false;

	bool success = Try_read_file_to_buffer(file, bstr);

	fclose(file); // BUG (matthewd) ignoring return value?

	return success;
}


static void Try_print_tokens_in_file(const wchar_t * fpath, bool raw);



int wmain(int argc, wchar_t *argv[])
{
	if (argc == 3)
	{
		if (wcscmp(argv[1], L"-raw") != 0)
		{
			printf("bad argumens, expected '-raw <file path>'\n");
			return 1;
		}
		else
		{
			Try_print_tokens_in_file(argv[2], true);
			return 0;
		}
	}

	else if (argc != 2)
	{
		printf("wrong number of arguments, only expected a file path\n");
		return 1;
	}
	else
	{
		Try_print_tokens_in_file(argv[1], false);
		return 0;
	}
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

	keyword__asm,
	keyword__asm__,
	keyword__volatile__,

	keyword_auto,

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

		"__asm",
		"__asm__",
		"__volatile__",

		"auto",
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

	switch (keyword)
	{
	case keyword__inline:
		return "inline";

	case keyword__attribute__:
		return "__attribute";

	case keyword__asm:
	case keyword__asm__:
		return "asm";

	case keyword__volatile__:
		return "volatile";

	default:
		return str_from_keyword(keyword);
	}	
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

static void Print_token(
	token_kind_t tokk,
	const char * str_tok,
	int num_ch_tok,
	int line,
	const char * line_start)
{
	// Skip certain tokens

	switch (tokk)
	{
	case tokk_bogus_ucn:
	case tokk_unterminated_quote:
	case tokk_zero_length_char_lit:
	case tokk_unterminated_block_comment:
	case tokk_whitespace:
	case tokk_comment:
		return;
	}

	// token kind

	if (tokk == tokk_raw_identifier)
	{
		Print_id_kind(str_tok, num_ch_tok);
	}
	else if (tokk == tokk_unknown_byte || tokk == tokk_stray_backslash)
	{
		printf("unknown");
	}
	else
	{
		printf("%s", str_from_tokk(tokk));
	}

	// token text

	printf(" \"");
	for (int i = 0; i < num_ch_tok; ++i)
	{
		Clean_and_print_ch(str_tok[i]);
	}
	printf("\"");

	// token loc

	int col = (int)(str_tok - line_start + 1);

	printf(
		" Loc=<%d:%d>\n",
		line,
		col);
}

static void Print_token_raw(
	token_kind_t tokk,
	const char * str_tok,
	int num_ch_tok,
	int line,
	const char * line_start)
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
	for (int i = 0; i < num_ch_tok; ++i)
	{
		Clean_and_print_ch(str_tok[i]);
	}
	printf("\"");

	// token loc

	int col = (int)(str_tok - line_start + 1);

	printf(
		" Loc=<%d:%d>\n",
		line,
		col);
}

// clang goo...
// If the file ends with a newline, form the EOF token on the newline itself,
// rather than "on the line following it", which doesn't exist.  This makes
// diagnostics relating to the end of file include the last line that the user
// actually typed, which is better.
static const char * eof_pos(const char * mic, const char * mac)
{
	if (mac != mic &&
		(mac[-1] == '\n' || mac[-1] == '\r'))
	{
		--mac;

		// Handle \n\r and \r\n:

		if (mac != mic &&
			(mac[-1] == '\n' || mac[-1] == '\r') &&
			mac[-1] != mac[0])
		{
			--mac;
		}
	}

	return mac;
}

// trigraphs

static cp_len_t Peek_trigraph(lcp_span_t span)
{
	const lcp_t * cursor = span.lcp_mic;

	uint32_t cp0 = cursor[0].cp;
	if (cp0 != '?')
		return {UINT32_MAX, 0};

	uint32_t cp1 = cursor[1].cp;
	if (cp1 != '?')
		return {UINT32_MAX, 0};

	uint32_t cp2 = cursor[2].cp;
	switch (cp2)
	{
	case '<':
		return {'{', 3};
	case '>':
		return {'}', 3};
	case '(':
		return {'[', 3};
	case ')':
		return {']', 3};
	case '=':
		return {'#', 3};
	case '/':
		return {'\\', 3};
	case '\'':
		return {'^', 3};
	case '!':
		return {'|', 3};
	case '-':
		return {'~', 3};
	}

	return {UINT32_MAX, 0};
}



// Escaped line breaks

static int Len_escaped_end_of_line(const lcp_t * cursor)
{
	if (cursor[0].cp != '\\')
		return 0;

	int len = 1;
	while (Is_cp_ascii_horizontal_white_space(cursor[len].cp))
	{
		++len;
	}

	// BUG clang does a horrifying thing where it slurps
	//  up a \n\r as a single line break when measuring
	//  a line continue, EVEN THOUGH it only defines
	//  "physical line breaks" as \n, \r, and \r\n.
	//  It has been like that since the very first
	//  version of the tokenizer, go figure.

	if (cursor[len].cp == '\n')
	{
		if (cursor[len + 1].cp == '\r')
			return len + 2; // :(

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

static cp_len_t Peek_escaped_line_breaks(lcp_span_t span)
{
	const lcp_t * cursor = span.lcp_mic;

	int len_esc_eol = Len_escaped_end_of_lines(cursor);
	if (!len_esc_eol)
		return {UINT32_MAX, 0};

	if ((cursor + len_esc_eol) == span.lcp_mac)
	{
		// Treat trailing escaped eol as a '\0'

		return {'\0', len_esc_eol};
	}
	else
	{
		return {cursor[len_esc_eol].cp, len_esc_eol + 1};
	}
}



// Convert '\r' and "\r\n" to '\n'

static cp_len_t Peek_line_break(lcp_span_t span)
{
	const lcp_t * cursor = span.lcp_mic;

	if (cursor[0].cp == '\r')
	{
		// Need to check .num_ch because we do this after
		//  Collapse(Peek_escaped_line_breaks) :/

		if ((cursor[1].cp == '\n') && (cursor[1].num_ch == 1))
		{
			return {'\n', 2};
		}
		else
		{
			return {'\n', 1};
		}
	}

	return {UINT32_MAX, 0};
}



// Generic driver function to run a collapse function over a span

typedef cp_len_t(*collapse_fn_t)(lcp_span_t span);

static void Collapse(collapse_fn_t collapse_fn, lcp_span_t * span)
{
	lcp_t * cursor_from = span->lcp_mic;
	lcp_t * cursor_to = span->lcp_mic;

	while (cursor_from < span->lcp_mac)
	{
		// BUG the 'len' in cp_len here is not characters, 
		//  but lcps ('logical' characters). This distinction
		//  has confused/bit me before, so it is probably bad.

		cp_len_t cp_len = collapse_fn({cursor_from, span->lcp_mac});

		if (cp_len.len)
		{
			cursor_to->cp = cp_len.cp;
			cursor_to->str = cursor_from->str;

			int num_ch = 0;
			for (int i_lcp = 0; i_lcp < cp_len.len; ++i_lcp)
			{
				num_ch += cursor_from[i_lcp].num_ch;
			}
			cursor_to->num_ch = num_ch;

			cursor_from += cp_len.len;

			assert(cursor_to->str + cursor_to->num_ch == cursor_from->str);
			++cursor_to;
		}
		else
		{
			*cursor_to = *cursor_from;
			++cursor_to;
			++cursor_from;
		}
	}
	assert(cursor_from == span->lcp_mac);

	// Copy trailing '\0'

	*cursor_to = *cursor_from;
	span->lcp_mac = cursor_to;
}


static void Decode_utf8_to_lcp_span(
	const char * mic,
	const char * mac,
	lcp_span_t * lcp_span_out)
{
	// In the worst case, we will have a codepoint for every byte
	//  in the original span, so allocate enough spae for that.
	//  Note that we include room for a trailing '\0' codepoint

	int num_ch = (int)(mac - mic);
	int num_alloc = num_ch + 1;

	lcp_span_out->lcp_mic = (lcp_t *)calloc(sizeof(lcp_t) * num_alloc, 1);
	if (!lcp_span_out->lcp_mic)
	{
		printf("ALLOCATION FAILED");
		exit(1);
	}

	// Chew through the byte span with Try_decode_utf8

	int num_cp = 0;
	while (mic < mac)
	{
		cp_len_t cp_len;
		if (Try_decode_utf8((const uint8_t *)mic, (const uint8_t *)mac, &cp_len))
		{
			lcp_span_out->lcp_mic[num_cp].cp = cp_len.cp;
			lcp_span_out->lcp_mic[num_cp].num_ch = cp_len.len;
			lcp_span_out->lcp_mic[num_cp].str = mic;
			mic += cp_len.len;
		}
		else
		{
			lcp_span_out->lcp_mic[num_cp].cp = UINT32_MAX;
			lcp_span_out->lcp_mic[num_cp].num_ch = 1;
			lcp_span_out->lcp_mic[num_cp].str = mic;
			++mic;
		}

		++num_cp;
	}
	assert(mic == mac);

	// Append a final '\0'

	lcp_span_out->lcp_mic[num_cp].cp = '\0';
	lcp_span_out->lcp_mic[num_cp].num_ch = 0;
	lcp_span_out->lcp_mic[num_cp].str = mic;

	// Set lcp_mac

	lcp_span_out->lcp_mac = lcp_span_out->lcp_mic + num_cp;
}

static void Collapse_lcp_span(lcp_span_t * lcp_span)
{
	// BUG ostensibly the standard says Collapse(Peek_line_break)
	//  should happen first, but it has to happen
	//  after Collapse(Peek_escaped_line_breaks) because of a hack
	//  we do there to match clang

	Collapse(Peek_trigraph, lcp_span);
	Collapse(Peek_escaped_line_breaks, lcp_span);
	Collapse(Peek_line_break, lcp_span);
}

static lcp_span_t Decode_logical_code_points(const char * mic, const char * mac)
{
	lcp_span_t span;
	Decode_utf8_to_lcp_span(mic, mac, &span);
	Collapse_lcp_span(&span);
	return span;
}

static void Print_toks_in_ch_range(const bounded_c_str_t * bstr, bool raw)
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

	lcp_span_t lcp_span = Decode_logical_code_points(str_mic, str_mac);

	// Lex!

	while (true)
	{
		// Peek

		lex_t lex = Lex_leading_token(lcp_span.lcp_mic, lcp_span.lcp_mac);

		lcp_t * lcp_mic_next = lex.lcp_max;

		const char * str_tok = lcp_span.lcp_mic[0].str;
		const char * str_tok_mac = lcp_mic_next->str;
		int num_ch_tok = (int)(str_tok_mac - str_tok);
		bool is_last_tok = (str_tok_mac == lcp_span.lcp_mac->str);

		// print token

		if (raw)
		{
			// Turbo hack to not print trailing escaped new lines

			bool is_trailing_line_escape =
				is_last_tok &&
				((str_tok[0] == '\\') || ((str_tok[0] == '?') && (str_tok[1] == '?') && (str_tok[2] == '/'))) &&
				(lcp_span.lcp_mic[0].cp == '\0');

			if (!is_trailing_line_escape)
			{
				Print_token_raw(
					lex.tokk,
					str_tok,
					num_ch_tok,
					line,
					line_start);
			}
		}
		else
		{
			Print_token(
				lex.tokk,
				str_tok,
				num_ch_tok,
				line,
				line_start);
		}

		// Cache stuff

		if (is_last_tok)
		{
			if (!raw)
			{
				const char * end_pos = eof_pos(str_tok, str_tok_mac);

				eol_info_t eol_info = Inspect_span_for_eol(str_tok, end_pos);
				if (eol_info.num_eol)
				{
					line += eol_info.num_eol;
					line_start = str_tok + eol_info.offset_to_new_line_start;
				}

				int col = (int)(end_pos - line_start + 1);
				printf("eof \"\" Loc=<%d:%d>\n", line, col);
			}

			break;
		}
		else
		{
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

static void Try_print_tokens_in_file(const wchar_t * fpath, bool raw)
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

	Print_toks_in_ch_range(&bstr, raw);
}