
#include <assert.h>
#include <fcntl.h>
#include <io.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define LEN(x) ((sizeof(x)/sizeof(0[(x)])) / ((size_t)(!(sizeof(x) % sizeof(0[(x)])))))

// utf8

int Leading_ones(uint8_t byte)
{
	int count = 0;
	for (uint8_t mask = (1 << 7); mask; mask >>= 1)
	{
		if (!(byte & mask))
			break;

		++count;
	}

	return count;
}

bool Try_decode_utf8(
	const uint8_t * bytes,
	const uint8_t * bytes_end,
	uint32_t * cp_out,
	int * len_cp_out)
{
	// Try_decode_utf8 Roughly based on Table 3.1B in unicode Corrigendum #1
	//  Special care is taken to reject 'overlong encodings'
	//  that is, using more bytes than necessary/allowed for a given code point

	/*
		utf8 can encode up to 21 bits like this

		1-byte = 0 to 7 bits : 		0xxxxxxx
		2-byte = 8 to 11 bits :		110xxxxx 10xxxxxx
		3-byte = 12 to 16 bits :	1110xxxx 10xxxxxx 10xxxxxx
		4-byte = 17 to 21 bits :	11110xxx 10xxxxxx 10xxxxxx 10xxxxxx

		so
		1-byte if		<= 	0b 0111 1111, 			(0x7f)
		2-byte if		<= 	0b 0111 1111 1111,		(0x7ff)
		3-byte if		<= 	0b 1111 1111 1111 1111,	(0xffff)
		4 byte otherwise

		The only other details, are that you are not allowed to encode
		values in [0xD800, 0xDFFF] (the utf16 surogates),
		or anything > 0x10FFFF (0x10FFFF is the largest valid code point).

		Also note that, 2/3/4 byte values start with a byte with 2/3/4 leading ones.
		That is how you decode them later. (the trailing bytes all start with '10')
	*/

	// Check if we have no bytes at all

	int64_t len_bytes = bytes_end - bytes;
	if (len_bytes <= 0)
		return false;

	// Check if first byte is a trailing byte (1 leading 1),
	//  or if too many leading ones.

	int leading_ones = Leading_ones(bytes[0]);

	if (leading_ones == 1)
		return false;

	if (leading_ones > 4)
		return false;

	// Compute len_cp from leading_ones

	int len_cp;
	switch (leading_ones)
	{
	case 0: len_cp = 1; break;
	case 2: len_cp = 2; break;
	case 3: len_cp = 3; break;
	default: len_cp = 4; break;
	}

	// Check if we do not have enough bytes

	if (len_cp > len_bytes)
		return false;

	// Check if any trailing bytes are invalid

	for (int i = 1; i < len_cp; ++i)
	{
		if (Leading_ones(bytes[i]) != 1)
			return false;
	}

	// Get the significant bits from the first byte

	uint32_t cp = bytes[0];
	switch (len_cp)
	{
	case 2: cp &= 0x1F; break;
	case 3: cp &= 0x0F; break;
	case 4: cp &= 0x07; break;
	}

	// Get bits from the trailing bytes

	for (int i = 1; i < len_cp; ++i)
	{
		cp <<= 6;
		cp |= (bytes[i] & 0x3F);
	}

	// Check for illegal codepoints

	if (cp >= 0xD800 && cp <= 0xDFFF)
		return false;

	if (cp > 0x10FFFF)
		return false;

	// Check for 'overlong encodings'

	if (cp <= 0x7f && len_cp > 1)
		return false;

	if (cp <= 0x7ff && len_cp > 2)
		return false;

	if (cp <= 0xffff && len_cp > 3)
		return false;

	// We did it, return cp and len

	*cp_out = cp;
	*len_cp_out = len_cp;
	return true;
}

// 'logical' code points : munge raw codepoints to...
//  1. convert /r/n and /r to /n
//  2. convert trigraphs
//  3. skip 'escaped newlines' (\\\n)

typedef struct //!!!FIXME_typedef_audit
{
	const uint8_t * bytes;
	uint32_t cp;
} lcp_t; // logical codepoint

bool Is_hz_ws(uint32_t cp)
{
	return cp == ' ' || cp == '\t' || cp == '\f' || cp == '\v';
}

bool Is_ws(uint32_t cp)
{
	return Is_hz_ws(cp) || cp == '\n' || cp == '\r';
}

int Len_escaped_end_of_line(const lcp_t * cursor)
{
	if (cursor[0].cp != '\\')
		return 0;

	int len = 1;
	while (Is_hz_ws(cursor[len].cp))
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

int Len_escaped_end_of_lines(const lcp_t * mic)
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

bool FPeekCarriageReturn(
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

bool FPeekTrigraph(
	lcp_t * pLcpBegin,
	lcp_t * pLcpEnd,
	uint32_t * pU32Peek,
	lcp_t ** ppLcpEndPeek)
{
	int cLcp = (int)(pLcpEnd - pLcpBegin);
	(void)cLcp;

	if (cLcp < 3)
		return false;

	if (pLcpBegin[0].cp != '?')
		return false;

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

	for (int iPair = 0; iPair < LEN(pairs); ++iPair)
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

bool FPeekEscapedLineBreaks(
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

typedef enum
{
	COLLAPSEK_Trigraph,
	COLLAPSEK_EscapedLineBreaks,
	COLLAPSEK_CarriageReturn,
} COLLAPSEK;

bool FPeekCollapse(
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

void Collapse(
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

		if (!fPeek)
		{
			u32Peek = pLcpBegin->cp;
			pLcpEndPeek = pLcpBegin + 1;
		}

		pLcpDest->cp = u32Peek;
		pLcpDest->bytes = pLcpBegin->bytes;

		pLcpBegin = pLcpEndPeek;
		++pLcpDest;
	}
	assert(pLcpBegin == pLcpEnd);

	// Copy trailing '\0'

	*pLcpDest = *pLcpEnd;

	// Write out new end

	*ppLcpEnd = pLcpDest;
}

lcp_t * Try_decode_logical_codepoints(
	const uint8_t * bytes,
	const uint8_t * bytes_end,
	int * len_lcps_ref)
{
	// In the worst case, we will have a codepoint for every byte
	//  in the original span, so allocate enough space for that.
	//  Note that we include room for a trailing '\0' codepoint

	size_t len_bytes;
	{
		int64_t len_bytes_signed = bytes_end - bytes;
		if (len_bytes_signed <= 0)
			return NULL;

		len_bytes = (size_t)len_bytes_signed;
	}

	lcp_t * lcps = (lcp_t *)calloc(sizeof(lcp_t) * (len_bytes + 1), 1);
	if (!lcps)
		return NULL;

	// Chew through the byte span with Try_decode_utf8

	int len_lcps = 0;
	while (bytes < bytes_end)
	{
		uint32_t cp;
		int len_cp;
		if (Try_decode_utf8(bytes, bytes_end, &cp, &len_cp))
		{
			lcps[len_lcps].cp = cp;
			lcps[len_lcps].bytes = bytes;
			bytes += len_cp;
		}
		else
		{
			lcps[len_lcps].cp = UINT32_MAX;
			lcps[len_lcps].bytes = bytes;
			++bytes;
		}

		++len_lcps;
	}
	assert(bytes == bytes_end);

	// Append a final '\0'

	lcps[len_lcps].cp = '\0';
	lcps[len_lcps].bytes = bytes_end;

	// Collapse trigraphs/etc

	lcp_t * lcps_end = lcps + len_lcps;
	Collapse(COLLAPSEK_CarriageReturn, lcps, &lcps_end);
	Collapse(COLLAPSEK_Trigraph, lcps, &lcps_end);
	Collapse(COLLAPSEK_EscapedLineBreaks, lcps, &lcps_end);
	len_lcps = (int)(lcps_end - lcps);

	// return lcps and len

	*len_lcps_ref = len_lcps;
	return lcps;
}

// Lex

typedef enum//!!!FIXME_typedef_audit
{
	tokk_raw_identifier,
	tokk_l_paren,
	tokk_r_paren,
	tokk_l_brace,
	tokk_r_brace,
	tokk_l_square,
	tokk_r_square,
	tokk_semi,
	tokk_star,
	tokk_equal,
	tokk_amp,
	tokk_plusplus,
	tokk_exclaimequal,
	tokk_numeric_constant,
	tokk_colon,
	tokk_minus,
	tokk_period,
	tokk_slash,
	tokk_comma,
	tokk_arrow,
	tokk_plus,
	tokk_string_literal,
	tokk_minusminus,
	tokk_percent,
	tokk_pipe,
	tokk_caret,
	tokk_greater,
	tokk_greaterequal,
	tokk_equalequal,
	tokk_less,
	tokk_lessequal,
	tokk_ampamp,
	tokk_pipepipe,
	tokk_exclaim,
	tokk_plusequal,
	tokk_minusequal,
	tokk_starequal,
	tokk_ellipsis,
	tokk_char_constant,
	tokk_lessless,
	tokk_question,
	tokk_wide_char_constant,
	tokk_tilde,
	tokk_greatergreater,
	tokk_slashequal,
	tokk_wide_string_literal,
	tokk_hash,
	
	tokk_line_comment,
	tokk_block_comment,
	
	tokk_hashhash,
	tokk_pipeequal,
	tokk_lesslessequal,
	tokk_ampequal,
	tokk_greatergreaterequal,
	tokk_percentequal,
	tokk_caretequal,
	tokk_coloncolon,
	tokk_utf16_string_literal,
	tokk_utf16_char_constant,
	tokk_utf32_string_literal,
	tokk_utf32_char_constant,
	tokk_utf8_string_literal,

	tokk_bogus_ucn,
	tokk_stray_backslash,
	tokk_hz_whitespace,
	tokk_multi_line_whitespace,
	tokk_unterminated_quote,
	tokk_zero_length_char_lit,
	tokk_unterminated_block_comment,
	tokk_unknown_byte,

	tokk_max
} token_kind_t;

const char * str_from_tokk(token_kind_t tokk)
{
	static const char * s_mpTokkStr[] =
	{
		"raw_identifier",				// tok_raw_identifier
		"l_paren",						// tok_l_paren
		"r_paren",						// tok_r_paren
		"l_brace",						// tok_l_brace
		"r_brace",						// tok_r_brace
		"l_square",						// tok_l_square
		"r_square",						// tok_r_square
		"semi",							// tok_semi
		"star",							// tok_star
		"equal",						// tok_equal
		"amp",							// tok_amp
		"plusplus",						// tok_plusplus
		"exclaimequal",					// tok_exclaimequal
		"numeric_constant",				// tok_numeric_constant
		"colon",						// tok_colon
		"minus",						// tok_minus
		"period",						// tok_period
		"slash",						// tok_slash
		"comma",						// tok_comma
		"arrow",						// tok_arrow
		"plus",							// tok_plus
		"string_literal",				// tok_string_literal
		"minusminus",					// tok_minusminus
		"percent",						// tok_percent
		"pipe",							// tok_pipe
		"caret",						// tok_caret
		"greater",						// tok_greater
		"greaterequal",					// tok_greaterequal
		"equalequal",					// tok_equalequal
		"less",							// tok_less
		"lessequal",					// tok_lesequal
		"ampamp",						// tok_ampamp
		"pipepipe",						// tok_pipepipe
		"exclaim",						// tok_exclaim
		"plusequal",					// tok_plusequal
		"minusequal",					// tok_minusequal
		"starequal",					// tok_starequal
		"ellipsis",						// tok_ellipsis
		"char_constant",				// tok_char_constant
		"lessless",						// tok_lessless
		"question",						// tok_question
		"wide_char_constant",			// tok_wide_char_constant
		"tilde",						// tok_tilde
		"greatergreater",				// tok_greatergreater
		"slashequal",					// tok_slashequal
		"wide_string_literal",			// tok_wide_string_literal
		"hash",							// tok_hash
		
		"line_comment",					// tokk_line_comment
		"block_comment",				// tokk_block_comment
		
		"hashhash",						// tok_hashhash
		"pipeequal",					// tok_pipeequal
		"lesslessequal",				// tok_lesslessequal
		"ampequal",						// tok_ampequal
		"greatergreaterequal",			// tok_greatergreaterequal
		"percentequal",					// tok_percentequal
		"caretequal",					// tok_caretequal
		"coloncolon",					// tok_coloncolon
		"utf16_string_literal",			// tok_utf16_string_literal
		"utf16_char_constant",			// tok_utf16_char_constant
		"utf32_string_literal",			// tok_utf32_string_literal
		"utf32_char_constant",			// tok_utf32_char_constant
		"utf8_string_literal",			// tok_utf8_string_literal

		"bogus_ucn",					// tok_bogus_ucn
		"stray_backslash",				// tok_stray_backslash
		
		"hz_whitespace",				// tokk_hz_whitespace
		"multi_line_whitespace",		// tokk_multi_line_whitespace
		
		"unterminated_quote",			// tok_unterminated_quote
		"zero_length_char_lit",			// tok_zero_length_char_lit
		"unterminated_block_comment",	// tok_unterminated_block_comment
		"unknown_byte",					// tok_unknown_byte
	};
	static_assert(LEN(s_mpTokkStr) == tokk_max, "each tokk needs a string");

	assert(tokk >= 0);
	assert(tokk < tokk_max);

	return s_mpTokkStr[tokk];
}

typedef struct//!!!FIXME_typedef_audit
{
	const char * str;
	token_kind_t tokk;
	int _padding;
} punctution_t;

token_kind_t Lex_punctuation(
	lcp_t * cursor, 
	lcp_t ** ppLcpTokEnd)
{
	// "::" is included to match clang
	// https://github.com/llvm/llvm-project/commit/874217f99b99ab3c9026dc3b7bd84cd2beebde6e

	static punctution_t puctuations[] =
	{
		{"%:%:", tokk_hashhash},
		{">>=", tokk_greatergreaterequal},
		{"<<=", tokk_lesslessequal},
		{"...", tokk_ellipsis},
		{"|=", tokk_pipeequal},
		{"||", tokk_pipepipe},
		{"^=", tokk_caretequal},
		{"==", tokk_equalequal},
		{"::", tokk_coloncolon},
		{":>", tokk_r_square},
		{"-=", tokk_minusequal},
		{"--", tokk_minusminus},
		{"->", tokk_arrow},
		{"+=", tokk_plusequal},
		{"++", tokk_plusplus},
		{"*=", tokk_starequal},
		{"&=", tokk_ampequal},
		{"&&", tokk_ampamp},
		{"##", tokk_hashhash},
		{"!=", tokk_exclaimequal},
		{">=", tokk_greaterequal},
		{">>", tokk_greatergreater},
		{"<=", tokk_lessequal},
		{"<:", tokk_l_square},
		{"<%", tokk_l_brace},
		{"<<", tokk_lessless},
		{"%>", tokk_r_brace},
		{"%=", tokk_percentequal},
		{"%:", tokk_hash},
		{"/=", tokk_slashequal},
		{"~", tokk_tilde},
		{"}", tokk_r_brace},
		{"{", tokk_l_brace},
		{"]", tokk_r_square},
		{"[", tokk_l_square},
		{"?", tokk_question},
		{";", tokk_semi},
		{",", tokk_comma},
		{")", tokk_r_paren},
		{"(", tokk_l_paren},
		{"|", tokk_pipe},
		{"^", tokk_caret},
		{"=", tokk_equal},
		{":", tokk_colon},
		{"-", tokk_minus},
		{"+", tokk_plus},
		{"*", tokk_star},
		{"&", tokk_amp},
		{"#", tokk_hash},
		{"!", tokk_exclaim},
		{">", tokk_greater},
		{"<", tokk_less},
		{"%", tokk_percent},
		{".", tokk_period},
		{"/", tokk_slash},
	};

	for (int i_puctuation = 0; i_puctuation < LEN(puctuations); ++i_puctuation)
	{
		punctution_t punctuation = puctuations[i_puctuation];
		const char * str_puctuation = punctuation.str;
		size_t len = strlen(str_puctuation);

		lcp_t * cursor_peek = cursor;
		bool found_match = true;

		for (size_t i_ch = 0; i_ch < len; ++i_ch)
		{
			char ch = str_puctuation[i_ch];
			uint32_t cp_ch = (uint32_t)ch;

			uint32_t cp = cursor_peek->cp;
			++cursor_peek;

			if (cp != cp_ch)
			{
				found_match = false;
				break;
			}
		}

		if (found_match)
		{
			*ppLcpTokEnd = cursor_peek;
			return punctuation.tokk;
		}
	}

	*ppLcpTokEnd = cursor + 1;
	return tokk_unknown_byte;
}

bool May_cp_start_id(uint32_t cp)
{
	// Letters

	if (cp >= 'a' && cp <= 'z')
		return true;

	if (cp >= 'A' && cp <= 'Z')
		return true;

	// Underscore

	if (cp == '_')
		return true;

	// '$' allowed as an extension :/

	if (cp == '$')
		return true;

	// All other ascii does not start ids

	if (cp <= 0x7F)
		return false;

	// Bogus utf8 does not start ids

	if (cp == UINT32_MAX)
		return false;

	// These codepoints are not allowed as the start of an id

	static const uint32_t no[][2] =
	{
		{ 0x0300, 0x036F },
		{ 0x1DC0, 0x1DFF },
		{ 0x20D0, 0x20FF },
		{ 0xFE20, 0xFE2F }
	};

	for (int i = 0; i < LEN(no); ++i)
	{
		uint32_t first = no[i][0];
		uint32_t last = no[i][1];
		if (cp >= first && cp <= last)
			return false;
	}

	// These code points are allowed to start an id (minus ones from 'no')

	static const uint32_t yes[][2] =
	{
		{ 0x00A8, 0x00A8 }, { 0x00AA, 0x00AA }, { 0x00AD, 0x00AD },
		{ 0x00AF, 0x00AF }, { 0x00B2, 0x00B5 }, { 0x00B7, 0x00BA },
		{ 0x00BC, 0x00BE }, { 0x00C0, 0x00D6 }, { 0x00D8, 0x00F6 },
		{ 0x00F8, 0x00FF },

		{ 0x0100, 0x167F }, { 0x1681, 0x180D }, { 0x180F, 0x1FFF },

		{ 0x200B, 0x200D }, { 0x202A, 0x202E }, { 0x203F, 0x2040 },
		{ 0x2054, 0x2054 }, { 0x2060, 0x206F },

		{ 0x2070, 0x218F }, { 0x2460, 0x24FF }, { 0x2776, 0x2793 },
		{ 0x2C00, 0x2DFF }, { 0x2E80, 0x2FFF },

		{ 0x3004, 0x3007 }, { 0x3021, 0x302F }, { 0x3031, 0x303F },

		{ 0x3040, 0xD7FF },

		{ 0xF900, 0xFD3D }, { 0xFD40, 0xFDCF }, { 0xFDF0, 0xFE44 },
		{ 0xFE47, 0xFFFD },

		{ 0x10000, 0x1FFFD }, { 0x20000, 0x2FFFD }, { 0x30000, 0x3FFFD },
		{ 0x40000, 0x4FFFD }, { 0x50000, 0x5FFFD }, { 0x60000, 0x6FFFD },
		{ 0x70000, 0x7FFFD }, { 0x80000, 0x8FFFD }, { 0x90000, 0x9FFFD },
		{ 0xA0000, 0xAFFFD }, { 0xB0000, 0xBFFFD }, { 0xC0000, 0xCFFFD },
		{ 0xD0000, 0xDFFFD }, { 0xE0000, 0xEFFFD }
	};

	for (int i = 0; i < LEN(yes); ++i)
	{
		uint32_t first = yes[i][0];
		uint32_t last = yes[i][1];
		if (cp >= first && cp <= last)
			return true;
	}

	return false;
}

bool Is_cp_valid_ucn(uint32_t cp)
{
	// A universal character name shall not specify a character whose
	//  short identifier is less than 00A0, nor one in the range 
	//  D800 through DFFF inclusive.

	if (cp < 0xA0)
		return false;

	if (cp >= 0xD800 && cp <= 0xDFFF)
		return false;

	return true;
}

uint32_t Hex_digit_value_from_cp(uint32_t cp)
{
	if (cp >= '0' && cp <= '9')
		return cp - '0';

	if (cp < 'A')
		return UINT32_MAX;

	if (cp > 'f')
		return UINT32_MAX;

	if (cp <= 'F')
		return cp - 'A' + 10;

	if (cp >= 'a')
		return cp - 'a' + 10;

	return UINT32_MAX;
}

void Peek_ucn(
	lcp_t * cursor,
	uint32_t * pCp,
	int * pLen)
{
	*pCp = UINT32_MAX;
	*pLen = 0;

	int len = 0;

	// Check for leading '\\'

	if (cursor[len].cp != '\\')
		return;

	// Advance past '\\'

	++len;

	// Look for 'u' or 'U' after '\\'

	if (cursor[len].cp != 'u' && cursor[len].cp != 'U')
		return;

	// Look for 4 or 8 hex digits, based on u vs U

	int num_hex_digits;
	if (cursor[len].cp == 'u')
	{
		num_hex_digits = 4;
	}
	else
	{
		num_hex_digits = 8;
	}

	// Advance past u/U

	++len;

	// Look for correct number of hex digits

	uint32_t cp_result = 0;
	int hex_digits_read = 0;
	bool delimited = false;
	bool found_end_delimiter = false;

	while ((hex_digits_read < num_hex_digits) || delimited)
	{
		uint32_t cp = cursor[len].cp;

		// Check for '{' (delimited ucns)

		if (!delimited && hex_digits_read == 0 && cp == '{')
		{
			delimited = true;
			++len;
			continue;
		}

		// Check for '}' (delimited ucns)

		if (delimited && cp == '}')
		{
			found_end_delimiter = true;
			++len;
			break;
		}

		// Check if valid hex digit

		uint32_t hex_digit_value = Hex_digit_value_from_cp(cp);
		if (hex_digit_value == UINT32_MAX)
		{
			if (delimited)
			{
				return;
			}
			else
			{
				break;
			}
		}

		// Bail out if we are about to overflow

		if (cp_result & 0xF000'0000)
			return;

		// Fold hex digit into cp

		cp_result <<= 4;
		cp_result |= hex_digit_value;

		// Keep track of how many digits we have read

		++hex_digits_read;

		// Advance to next digit

		++len;
	}

	// No digits read?

	if (hex_digits_read == 0)
		return;

	// Delimited 'U' is not allowed (find somthing in clang to explain this??)

	if (delimited && num_hex_digits == 8)
		return;

	// Read wrong number of digits?

	if (!delimited && hex_digits_read != num_hex_digits)
		return;

	// Sanity check that people are not trying to encode
	//  something particularly weird with a UCN.
	//  Convert any weird inputs to the error value UINT32_MAX

	if (!Is_cp_valid_ucn(cp_result))
	{
		cp_result = UINT32_MAX;
	}

	// Return result

	*pCp = cp_result;
	*pLen = len;
}

bool Does_cp_extend_id(uint32_t cp)
{
	if (cp >= 'a' && cp <= 'z')
		return true;

	if (cp >= 'A' && cp <= 'Z')
		return true;

	if (cp == '_')
		return true;

	if (cp >= '0' && cp <= '9')
		return true;

	if (cp == '$') // '$' allowed as an extension :/
		return true;

	if (cp <= 0x7F) // All other ascii is invalid
		return false;

	if (cp == UINT32_MAX) // Bogus utf8 does not extend ids
		return false;

	// We are lexing in 'raw mode', and to match clang, 
	//  once we are parsing an id, we just slurp up all
	//  valid non-ascii-non-whitespace utf8...

	// BUG I do not like this. the right thing to do is check c11_allowed from May_cp_start_id.
	//  Clang seems to do the wrong thing here,
	//  and produce an invalid pp token. I suspect no one
	//  actually cares, since dump_raw_tokens is only for debugging...

	static const uint32_t ws[][2] =
	{
		{ 0x0085, 0x0085 },
		{ 0x00A0, 0x00A0 },
		{ 0x1680, 0x1680 },
		{ 0x180E, 0x180E },
		{ 0x2000, 0x200A },
		{ 0x2028, 0x2029 },
		{ 0x202F, 0x202F },
		{ 0x205F, 0x205F },
		{ 0x3000, 0x3000 }
	};

	for (int i = 0; i < LEN(ws); ++i)
	{
		uint32_t first = ws[i][0];
		uint32_t last = ws[i][1];
		if (cp >= first && cp <= last)
			return false;
	}

	return true;
}

token_kind_t Lex_after_rest_of_id(
	lcp_t * cursor, 
	lcp_t ** ppLcpTokEnd)
{
	while (true)
	{
		if (Does_cp_extend_id(cursor->cp))
		{
			++cursor;
			continue;
		}

		if (cursor->cp == '\\')
		{
			uint32_t cp;
			int len;
			Peek_ucn(cursor, &cp, &len);
			if (len && Does_cp_extend_id(cp))
			{
				cursor += len;
				continue;
			}
		}

		break;
	}

	*ppLcpTokEnd = cursor;
	return tokk_raw_identifier;
}

token_kind_t Lex_after_rest_of_ppnum(
	lcp_t * cursor,
	lcp_t ** ppLcpTokEnd)
{
	/* NOTE (matthewd)
		preprocesor numbers are a bit unintuative,
		since they can match things that are not valid tokens
		in later translation phases.

		so, here is a quick antlr style definition of pp-num

		pp_num
			: pp_num_start pp_num_continue*
			;

		pp_num_start
			: [0-9]
			| '.' [0-9]
			;

		pp_num_continue
			: '.'
			| [eEpP][+-]
			| [0-9a-zA-Z_]
			;

		also note that, this is not how this is defined in the standard.
		That standard defines it with left recursion, so I factored out
		the left recursion so it would be more obvious what the code was doing

		the original definition is

		pp_num
			: [0-9]
			| '.' [0-9]
			| pp_num [0-9]
			| pp_num identifier_nondigit
			| pp_num 'e' [+-]
			| pp_num 'E' [+-]
			| pp_num 'p' [+-]
			| pp_num 'P' [+-]
			| pp_num '.'
			;
	*/

	// Len_rest_of_pp_num is called after we see ( '.'? [0-9] ), that is, pp_num_start
	// 'rest_of_pp_num' is equivalent to pp_num_continue*

	while (true)
	{
		uint32_t cp = cursor->cp;

		if (cp == '.')
		{
			++cursor;
			continue;
		}
		else if (cp == 'e' || cp == 'E' || cp == 'p' || cp == 'P')
		{
			++cursor;

			cp = cursor->cp;
			if (cp == '+' || cp == '-')
			{
				++cursor;
			}

			continue;
		}
		else if (cp == '$')
		{
			// Clang does not allow '$' in ppnums, 
			//  even though the spec would seem to suggest that 
			//  implimentation defined id chars should be included in PP nums...

			break;
		}
		else if (Does_cp_extend_id(cp))
		{
			// Everything (else) which extends ids can extend a ppnum

			++cursor;
			continue;
		}
		else if (cp == '\\')
		{
			uint32_t cpUcn;
			int len;
			Peek_ucn(cursor, &cpUcn, &len);
			if (len && Does_cp_extend_id(cpUcn))
			{
				cursor += len;
				continue;
			}
			else
			{
				break;
			}
		}
		else
		{
			// Otherwise, no dice

			break;
		}
	}

	*ppLcpTokEnd = cursor;
	return tokk_numeric_constant;
}

token_kind_t Lex_after_rest_of_line_comment(
	lcp_t * cursor, 
	lcp_t * terminator, 
	lcp_t ** ppLcpTokEnd)
{
	while (cursor < terminator)
	{
		uint32_t cp = cursor->cp;
		if (cp == '\n')
			break;

		++cursor;
	}

	*ppLcpTokEnd = cursor;
	return tokk_line_comment;
}

token_kind_t Lex_after_rest_of_block_comment(
	lcp_t * cursor, 
	lcp_t * terminator, 
	lcp_t ** ppLcpTokEnd)
{
	token_kind_t tokk = tokk_unterminated_block_comment;

	while (cursor < terminator)
	{
		uint32_t cp0 = cursor->cp;
		++cursor;

		uint32_t cp1 = cursor->cp;
		if (cp0 == '*' && cp1 == '/')
		{
			tokk = tokk_block_comment;
			++cursor;
			break;
		}
	}

	*ppLcpTokEnd = cursor;
	return tokk;
}

token_kind_t Lex_after_rest_of_str_lit(
	token_kind_t tokk,
	uint32_t cp_sential,
	lcp_t * cursor,
	lcp_t * terminator,
	lcp_t ** ppLcpTokEnd)
{
	int len = 0;
	bool found_end = false;

	while (cursor < terminator)
	{
		uint32_t cp = cursor->cp;

		// String without closing quote (which we support in raw lexing mode..)

		if (cp == '\n')
			break;

		// Anything else will be part of the str lit

		++cursor;

		// Closing quote

		if (cp == cp_sential)
		{
			found_end = true;
			break;
		}

		// Found somthing other than open/close quote, in len

		++len;

		// Deal with back slash

		if (cp == '\\')
		{
			// Check if escaped char is '\"', '\'', or '\\',
			//  the only escapes we need to handle in raw mode

			cp = cursor->cp;
			if (cp == cp_sential || cp == '\\')
			{
				++cursor;
			}
		}
	}

	// Unterminated lits are invalid

	if (!found_end)
	{
		tokk = tokk_unterminated_quote;
	}

	// zero length char lits are invalid

	if (cp_sential == '\'' && len == 0)
	{
		tokk = tokk_zero_length_char_lit;
	}

	*ppLcpTokEnd = cursor;
	return tokk;
}

token_kind_t Lex_after_whitespace(token_kind_t tokk, lcp_t * cursor, lcp_t ** ppLcpTokEnd)
{
	while (true)
	{
		if (!Is_ws(cursor->cp))
			break;

		if (!Is_hz_ws(cursor->cp))
		{
			tokk = tokk_multi_line_whitespace;
		}

		++cursor;
	}

	*ppLcpTokEnd = cursor;
	return tokk;
}

token_kind_t TokkPeek(
	lcp_t * pLcpBegin,
	lcp_t * pLcpEnd,
	lcp_t ** ppLcpTokEnd)
{
	// Decide what to do

	uint32_t cp_0 = pLcpBegin[0].cp;
	uint32_t cp_1 = (cp_0) ? pLcpBegin[1].cp : '\0';
	uint32_t cp_2 = (cp_1) ? pLcpBegin[2].cp : '\0';

	if (cp_0 == 'u' && cp_1 == '8' && cp_2 == '"')
	{
		pLcpBegin += 3;
		return Lex_after_rest_of_str_lit(tokk_utf8_string_literal, '"', pLcpBegin, pLcpEnd, ppLcpTokEnd);
	}
	else if ((cp_0 == 'u' || cp_0 == 'U' || cp_0 == 'L') &&
			 (cp_1 == '"' || cp_1 == '\''))
	{
		pLcpBegin += 2;

		token_kind_t tokk;
		
		switch (cp_0)
		{
		case 'u':
			tokk = (cp_1 == '"') ? tokk_utf16_string_literal : tokk_utf16_char_constant;
			break;
		case 'U':
			tokk = (cp_1 == '"') ? tokk_utf32_string_literal : tokk_utf32_char_constant;
			break;
		default: // 'L'
			tokk = (cp_1 == '"') ? tokk_wide_string_literal : tokk_wide_char_constant;
			break;
		}
		
		return Lex_after_rest_of_str_lit(tokk, cp_1, pLcpBegin, pLcpEnd, ppLcpTokEnd);
	}
	else if (cp_0 == '"' || cp_0 == '\'')
	{
		++pLcpBegin;
		token_kind_t tokk = (cp_0 == '"') ? tokk_string_literal : tokk_char_constant;
		return Lex_after_rest_of_str_lit(tokk, cp_0, pLcpBegin, pLcpEnd, ppLcpTokEnd);
	}
	else if (cp_0 == '/' && cp_1 == '*')
	{
		pLcpBegin += 2;
		return Lex_after_rest_of_block_comment(pLcpBegin, pLcpEnd, ppLcpTokEnd);
	}
	else if (cp_0 == '/' && cp_1 == '/')
	{
		pLcpBegin += 2;
		return Lex_after_rest_of_line_comment(pLcpBegin, pLcpEnd, ppLcpTokEnd);
	}
	else if (cp_0 == '.' && cp_1 >= '0' && cp_1 <= '9')
	{
		pLcpBegin += 2;
		return Lex_after_rest_of_ppnum(pLcpBegin, ppLcpTokEnd);
	}
	else if (May_cp_start_id(cp_0))
	{
		return Lex_after_rest_of_id(pLcpBegin + 1, ppLcpTokEnd);
	}
	else if (cp_0 >= '0' && cp_0 <= '9')
	{
		return Lex_after_rest_of_ppnum(pLcpBegin + 1, ppLcpTokEnd);
	}
	else if (Is_ws(cp_0))
	{
		// this +1 is important, in case
		//  cp_0 came after a line continuation,
		//  because Lex_after_whitespace only skips 
		//  physical whitespace (blek)

		token_kind_t tokk = (Is_hz_ws(cp_0)) ? 
								tokk_hz_whitespace : 
								tokk_multi_line_whitespace;

		return Lex_after_whitespace(tokk, pLcpBegin + 1, ppLcpTokEnd);
	}
	else if (cp_0 == '\0')
	{
		return Lex_after_whitespace(tokk_hz_whitespace, pLcpBegin + 1, ppLcpTokEnd);
	}
	else if (cp_0 =='\\')
	{
		uint32_t cp;
		int len;
		Peek_ucn(pLcpBegin, &cp, &len);
		if (len)
		{
			if (May_cp_start_id(cp))
			{
				return Lex_after_rest_of_id(pLcpBegin + len, ppLcpTokEnd);
			}
			else
			{
				// Bogus UCN, return it as an unknown token

				*ppLcpTokEnd = pLcpBegin + len;
				return tokk_bogus_ucn;
			}
		}
		else
		{
			// Stray backslash, return as unknown token

			*ppLcpTokEnd = pLcpBegin + 1;
			return tokk_stray_backslash;
		}
	}
	else
	{
		return Lex_punctuation(pLcpBegin, ppLcpTokEnd);
	}
}

// printing tokens

void clean_and_print_char(char ch)
{
	switch (ch)
	{ 
	case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': case 'i': case 'j':
	case 'k': case 'l': case 'm': case 'n': case 'o': case 'p': case 'q': case 'r': case 's': case 't':
	case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
	case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': case 'I': case 'J':
	case 'K': case 'L': case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T':
	case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
	case '!': case '\'': case '#': case '$': case '%': case '&': case '(': case ')': case '*': case '+':
	case ',': case '-': case '.': case '/': case ':': case ';': case '<': case '=': case '>': case '?':
	case '@': case '[': case ']': case '^': case '_': case '`': case '{': case '|': case '}': case '~':
		printf("%c", ch);
		break;

	case '"':
		printf("\\\"");
		break;

	case '\\':
		printf("\\\\");
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

	default:
		{
			unsigned char high = (unsigned char)((ch >> 4) & 0xF);
			unsigned char low = (unsigned char)(ch & 0xF);

			char highCh = high < 10 ? '0' + high : 'A' + (high - 10);
			char lowCh = low < 10 ? '0' + low : 'A' + (low - 10);

			printf("\\x%c%c", highCh, lowCh);
		}
		break;
	}
}

void Print_token(
	token_kind_t tokk,
	lcp_t * lcp_tok,
	lcp_t * lcp_tok_end,
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

	printf(" \"");

	const char * str_tok = (const char *)lcp_tok->bytes;
	const char * str_tok_end = (const char *)lcp_tok_end->bytes;

	for (const char * pCh = str_tok; pCh < str_tok_end; ++pCh)
	{
		clean_and_print_char(*pCh);
	}
	
	printf("\" ");

	// token loc

	printf(
		"%d:%d\n",
		line,
		col);
}

int Len_eol(const char * str)
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

void InspectSpanForEol(
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

void PrintRawTokens(const uint8_t * bytes, const uint8_t * bytes_end)
{
	// Munch bytes to logical characters

	int len_lcps;
	lcp_t * lcps = Try_decode_logical_codepoints(bytes, bytes_end, &len_lcps);
	if (!lcps)
		return;

	lcp_t * lcps_end = lcps + len_lcps;

	// Keep track of line info

	const char * line_start = (const char *)bytes;
	int line = 1;

	// Lex!

	while (lcps < lcps_end)
	{
		lcp_t * pLcpTokEnd;
		token_kind_t tokk = TokkPeek(lcps, lcps_end, &pLcpTokEnd);

		const char * pChTokBegin = (const char *)lcps->bytes;
		const char * pChTokEnd = (const char *)pLcpTokEnd->bytes;

		Print_token(
			tokk,
			lcps,
			pLcpTokEnd,
			line,
			(int)(pChTokBegin - line_start + 1));

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

		lcps = pLcpTokEnd;
	}
}

// files

bool FTryReadWholeFile(
	FILE * file,
	const char ** ppChBegin,
	const char ** ppChEnd)
{
	int err = fseek(file, 0, SEEK_END);
	if (err)
		return false;

	long len_file = ftell(file);
	if (len_file < 0)
		return false;

	err = fseek(file, 0, SEEK_SET);
	if (err)
		return false;

	// We allocated a trailing '\0' for sanity

	char * pChAlloc = (char *)calloc((size_t)(len_file + 1), 1);
	if (!pChAlloc)
		return false;

	size_t bytes_read = fread(pChAlloc, 1, (size_t)len_file, file);
	if (bytes_read != (size_t)len_file)
	{
		free(pChAlloc);
		return false;
	}

	*ppChBegin = pChAlloc;
	*ppChEnd = pChAlloc + len_file;

	return true;
}

bool FTryReadWholeFileAtPath(
	const wchar_t * fpath,
	const char ** ppChBegin,
	const char ** ppChEnd)
{
	FILE * file = _wfopen(fpath, L"rb");
	if (!file)
		return false;

	bool fRead = FTryReadWholeFile(file, ppChBegin, ppChEnd);

	fclose(file); // BUG (matthewd) ignoring return value?

	return fRead;
}

// main

int wmain(int argc, wchar_t *argv[])
{
	(void) _setmode(_fileno(stdin), _O_BINARY);

	// BUG do real argument parsing....

	wchar_t * pWChFile = NULL;
	bool fRaw = false;
	if (argc == 3)
	{
		if (wcscmp(argv[1], L"-raw") != 0)
		{
			printf("bad argumens, expected '-raw <file path>'\n");
			return 1;
		}
		else
		{
			pWChFile = argv[2];
			fRaw = true;
		}
	}
	else if (argc != 2)
	{
		printf("wrong number of arguments, only expected a file path\n");
		return 1;
	}
	else
	{
		pWChFile = argv[1];
	}

	// Read file

	const char * pChBegin;
	const char * pChEnd;
	bool success = FTryReadWholeFileAtPath(pWChFile, &pChBegin, &pChEnd);
	if (!success)
	{
		printf("Failed to read file '%ls'.\n", pWChFile);
		return 1;
	}

	// Deal with potential UTF-8 BOM

	int num_ch = (int)(pChEnd - pChBegin);
	if (num_ch >= 3 &&
		pChBegin[0] == '\xEF' &&
		pChBegin[1] == '\xBB' &&
		pChBegin[2] == '\xBF')
	{
		pChBegin += 3;
	}

	// Print tokens

	if (fRaw)
	{
		PrintRawTokens((const uint8_t *)pChBegin, (const uint8_t *)pChEnd);
	}
	else
	{
		printf("PrintTokens Not yet implemented!!!\n");
		//PrintTokens(pChBegin, pChEnd);
	}
}