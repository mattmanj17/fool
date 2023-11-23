
#include <assert.h>
#include <fcntl.h>
#include <io.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

// ------

#define COUNT_OF(x) ((sizeof(x)/sizeof(0[(x)])) / ((size_t)(!(sizeof(x) % sizeof(0[(x)])))))
#define STATIC_ASSERT(x) static_assert((x), #x)

// ------

bool Is_cp_ascii_horizontal_white_space(uint32_t cp)
{
	return cp == ' ' || cp == '\t' || cp == '\f' || cp == '\v';
}

bool Is_cp_ascii_white_space(uint32_t cp)
{
	return Is_cp_ascii_horizontal_white_space(cp) || cp == '\n' || cp == '\r';
}

bool Is_cp_in_ranges(
	uint32_t cp,
	const uint32_t(*ranges)[2],
	int num_ranges)
{
	for (int i = 0; i < num_ranges; ++i)
	{
		if (cp >= ranges[i][0] && cp <= ranges[i][1])
			return true;
	}

	return false;
}

bool Is_cp_unicode_whitespace(uint32_t cp)
{
	static const uint32_t unicode_whitespace[][2] =
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

	return Is_cp_in_ranges(cp, unicode_whitespace, COUNT_OF(unicode_whitespace));
}

// ------

static int Num_bytes_to_encode_cp(uint32_t cp)
{
	if (cp <= 0x7f)
		return 1;

	if (cp <= 0x7ff)
		return 2;

	if (cp <= 0xffff)
		return 3;

	return 4;
}

static int Num_bytes_from_first_byte(uint8_t first_byte)
{
	if (first_byte <= 0x7f)
		return 1;

	if (first_byte <= 0xDF)
		return 2;

	if (first_byte <= 0xEF)
		return 3;

	return 4;
}

static bool Is_valid_trailing_byte(uint8_t byte)
{
	// Trailing bytes start with '10'

	if (!(byte & 0b1000'0000))
		return false;

	if (byte & 0b0100'0000)
		return false;

	return true;
}

bool Try_decode_utf8(
	const char * pChBegin,
	const char * pChEnd,
	uint32_t * pCp,
	const char ** ppChEndCp)
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

	int bytes_available = (int)(pChEnd - pChBegin);
	if (bytes_available == 0)
		return false;

	// Check if first byte is too high

	uint8_t first_byte = (uint8_t)pChBegin[0];
	if (first_byte >= 0b1111'1000)
		return false;

	// Check if first byte is a trailing byte

	if (Is_valid_trailing_byte(first_byte))
		return false;

	// Check if we do not have enough bytes

	int bytes_to_read = Num_bytes_from_first_byte(first_byte);
	if (bytes_to_read > bytes_available)
		return false;

	// Check if any trailing bytes are invalid

	for (int i = 1; i < bytes_to_read; ++i)
	{
		uint8_t trailing_byte = (uint8_t)pChBegin[i];
		if (!Is_valid_trailing_byte(trailing_byte))
			return false;
	}

	// Get the significant bits from the first byte

	uint32_t cp = first_byte;
	switch (bytes_to_read)
	{
	case 2: cp &= 0b0001'1111; break;
	case 3: cp &= 0b0000'1111; break;
	case 4: cp &= 0b0000'0111; break;
	}

	// Get bits from the trailing bytes

	for (int i = 1; i < bytes_to_read; ++i)
	{
		uint8_t trailing_bits = (uint8_t)pChBegin[i];
		trailing_bits &= 0b0011'1111;

		cp <<= 6;
		cp |= trailing_bits;
	}

	// Check for illegal codepoints

	if (cp >= 0xD800 && cp <= 0xDFFF)
		return false;

	if (cp > 0x10FFFF)
		return false;

	// Check for 'overlong encodings'

	if (Num_bytes_to_encode_cp(cp) != bytes_to_read)
		return false;

	// We did it, copy to cp_len_out and return true

	*pCp = cp;
	*ppChEndCp = pChBegin + bytes_to_read;
	return true;
}

// ------

typedef struct //!!!FIXME_typedef_audit
{
	const char * str_begin;
	const char * str_end;
	uint32_t cp;
	bool _padding[4];
} lcp_t; // logical codepoint

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

static bool FPeekTrigraph(
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

typedef enum
{
	COLLAPSEK_Trigraph,
	COLLAPSEK_EscapedLineBreaks,
	COLLAPSEK_CarriageReturn,
} COLLAPSEK;

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

		if (!fPeek)
		{
			u32Peek = pLcpBegin->cp;
			pLcpEndPeek = pLcpBegin + 1;

			assert(pLcpBegin->str_end == pLcpEndPeek->str_begin);
		}

		pLcpDest->cp = u32Peek;
		pLcpDest->str_begin = pLcpBegin->str_begin;
		pLcpDest->str_end = pLcpEndPeek->str_begin;

		pLcpBegin = pLcpEndPeek;
		++pLcpDest;
	}
	assert(pLcpBegin == pLcpEnd);

	// Copy trailing '\0'

	*pLcpDest = *pLcpEnd;

	// Write out new end

	*ppLcpEnd = pLcpDest;
}

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
			pChBegin = pChEndCp;
		}
		else
		{
			pLcpBegin[cLcp].cp = UINT32_MAX;
			pLcpBegin[cLcp].str_begin = pChBegin;
			pLcpBegin[cLcp].str_end = pChBegin + 1;
			++pChBegin;
		}

		++cLcp;
	}
	assert(pChBegin == pChEnd);

	// Append a final '\0'

	pLcpBegin[cLcp].cp = '\0';
	pLcpBegin[cLcp].str_begin = pChEnd;
	pLcpBegin[cLcp].str_end = pChEnd;

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

// ------

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
	STATIC_ASSERT(COUNT_OF(s_mpTokkStr) == tokk_max);

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

static token_kind_t Lex_punctuation(
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

	for (int i_puctuation = 0; i_puctuation < COUNT_OF(puctuations); ++i_puctuation)
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

static bool May_cp_start_id(uint32_t cp)
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

	static const uint32_t c11_disallowed_initial[][2] =
	{
		{ 0x0300, 0x036F },
		{ 0x1DC0, 0x1DFF },
		{ 0x20D0, 0x20FF },
		{ 0xFE20, 0xFE2F }
	};

	if (Is_cp_in_ranges(cp, c11_disallowed_initial, COUNT_OF(c11_disallowed_initial)))
		return false;

	// These code points are allowed to start an id (minus ones from c11_disallowed_initial)

	static const uint32_t c11_allowed[][2] =
	{
		// 1
		{ 0x00A8, 0x00A8 }, { 0x00AA, 0x00AA }, { 0x00AD, 0x00AD },
		{ 0x00AF, 0x00AF }, { 0x00B2, 0x00B5 }, { 0x00B7, 0x00BA },
		{ 0x00BC, 0x00BE }, { 0x00C0, 0x00D6 }, { 0x00D8, 0x00F6 },
		{ 0x00F8, 0x00FF },

		// 2
		{ 0x0100, 0x167F }, { 0x1681, 0x180D }, { 0x180F, 0x1FFF },

		// 3
		{ 0x200B, 0x200D }, { 0x202A, 0x202E }, { 0x203F, 0x2040 },
		{ 0x2054, 0x2054 }, { 0x2060, 0x206F },

		// 4
		{ 0x2070, 0x218F }, { 0x2460, 0x24FF }, { 0x2776, 0x2793 },
		{ 0x2C00, 0x2DFF }, { 0x2E80, 0x2FFF },

		// 5
		{ 0x3004, 0x3007 }, { 0x3021, 0x302F }, { 0x3031, 0x303F },

		// 6
		{ 0x3040, 0xD7FF },

		// 7
		{ 0xF900, 0xFD3D }, { 0xFD40, 0xFDCF }, { 0xFDF0, 0xFE44 },
		{ 0xFE47, 0xFFFD },

		// 8
		{ 0x10000, 0x1FFFD }, { 0x20000, 0x2FFFD }, { 0x30000, 0x3FFFD },
		{ 0x40000, 0x4FFFD }, { 0x50000, 0x5FFFD }, { 0x60000, 0x6FFFD },
		{ 0x70000, 0x7FFFD }, { 0x80000, 0x8FFFD }, { 0x90000, 0x9FFFD },
		{ 0xA0000, 0xAFFFD }, { 0xB0000, 0xBFFFD }, { 0xC0000, 0xCFFFD },
		{ 0xD0000, 0xDFFFD }, { 0xE0000, 0xEFFFD }
	};

	return Is_cp_in_ranges(cp, c11_allowed, COUNT_OF(c11_allowed));
}

static bool Is_cp_valid_ucn(uint32_t cp)
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

static uint32_t Hex_digit_value_from_cp(uint32_t cp)
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

static void Peek_ucn(
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

static bool Does_cp_extend_id(uint32_t cp)
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

	if (!Is_cp_unicode_whitespace(cp))
		return true;

	return false;
}

static token_kind_t Lex_after_rest_of_id(
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

static token_kind_t Lex_after_rest_of_ppnum(
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

static token_kind_t Lex_after_rest_of_line_comment(
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

static token_kind_t Lex_after_rest_of_block_comment(
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

static token_kind_t Lex_after_rest_of_str_lit(
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

static token_kind_t Lex_after_whitespace(token_kind_t tokk, lcp_t * cursor, lcp_t ** ppLcpTokEnd)
{
	while (true)
	{
		if (!Is_cp_ascii_white_space(cursor->cp))
			break;

		if (!Is_cp_ascii_horizontal_white_space(cursor->cp))
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
	else if (Is_cp_ascii_white_space(cp_0))
	{
		// this +1 is important, in case
		//  cp_0 came after a line continuation,
		//  because Lex_after_whitespace only skips 
		//  physical whitespace (blek)

		token_kind_t tokk = (Is_cp_ascii_horizontal_white_space(cp_0)) ? 
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

// ------

static void clean_and_print_char(char ch)
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

static void Print_token(
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

	const char * str_tok = lcp_tok->str_begin;
	const char * str_tok_end = lcp_tok_end->str_begin;

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

		const char * pChTokBegin = pLcpBegin->str_begin;
		const char * pChTokEnd = pLcpTokEnd->str_begin;

		Print_token(
			tokk,
			pLcpBegin,
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

		pLcpBegin = pLcpTokEnd;
	}
}

// ------

static bool FTryReadWholeFile(
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

static bool FTryReadWholeFileAtPath(
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

// ------

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
		PrintRawTokens(pChBegin, pChEnd);
	}
	else
	{
		printf("PrintTokens Not yet implemented!!!\n");
		//PrintTokens(pChBegin, pChEnd);
	}
}