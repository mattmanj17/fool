
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> // strlen only in Lex_punctuation...
#include <uchar.h>

// fuck windows

#include <fcntl.h>
#include <io.h>



// Some basic stuff

#define ARY_LEN(x) ((sizeof(x)/sizeof(0[(x)])) / ((size_t)(!(sizeof(x) % sizeof(0[(x)])))))

typedef unsigned char Byte_t;

bool Is_hz_ws(char32_t ch)
{
	return ch == ' ' || ch == '\t' || ch == '\f' || ch == '\v';
}

bool Is_ws(char32_t ch)
{
	return Is_hz_ws(ch) || ch == '\n' || ch == '\r';
}

int Leading_ones(Byte_t byte)
{
	int count = 0;
	for (Byte_t mask = (1 << 7); mask; mask >>= 1)
	{
		if (!(byte & mask))
			break;

		++count;
	}

	return count;
}



// spans

#define DECL_SPAN(name, type) \
typedef struct name##_t \
{ \
	type * index; \
	type * end; \
} name##_t; \
\
name##_t name##_alloc(size_t len) \
{ \
	name##_t span; \
	span.index = (type *)calloc(len, sizeof(type)); \
	span.end = (span.index) ? span.index + len : NULL; \
	return span; \
} \
\
size_t name##_len(name##_t span) \
{ \
	if (!span.index) \
		return 0; \
\
	long long count = span.end - span.index; \
	if (count < 0) \
		return 0; \
\
	return (size_t)count; \
} \
\
bool name##_empty(name##_t span) \
{ \
	return name##_len(span) == 0; \
} \
\
bool name##_index_valid(name##_t span, size_t index) \
{ \
	return index < name##_len(span); \
}

DECL_SPAN(Bytes, Byte_t);
DECL_SPAN(Chars, char32_t);
DECL_SPAN(Locs, Byte_t *);

char32_t Char_at_index_safe(Chars_t chars, size_t index)
{
	if (!Chars_index_valid(chars, index))
		return U'\0';

	return chars.index[index];
}



// utf8

bool Try_decode_utf8_ch(
	Bytes_t bytes,
	char32_t * ch_out,
	size_t * len_ch_out)
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

	size_t len_bytes = Bytes_len(bytes);
	if (!len_bytes)
		return false;

	// Check if first byte is a trailing byte (1 leading 1),
	//  or if too many leading ones.

	int leading_ones = Leading_ones(bytes.index[0]);

	if (leading_ones == 1)
		return false;

	if (leading_ones > 4)
		return false;

	// Compute len from leading_ones

	size_t len_ch;
	switch (leading_ones)
	{
	case 0: len_ch = 1; break;
	case 2: len_ch = 2; break;
	case 3: len_ch = 3; break;
	default: len_ch = 4; break;
	}

	// Check if we do not have enough bytes

	if (len_ch > len_bytes)
		return false;

	// Check if any trailing bytes are invalid

	for (size_t i = 1; i < len_ch; ++i)
	{
		if (Leading_ones(bytes.index[i]) != 1)
			return false;
	}

	// Get the significant bits from the first byte

	char32_t ch = bytes.index[0];
	switch (len_ch)
	{
	case 2: ch &= 0x1F; break;
	case 3: ch &= 0x0F; break;
	case 4: ch &= 0x07; break;
	}

	// Get bits from the trailing bytes

	for (size_t i = 1; i < len_ch; ++i)
	{
		ch <<= 6;
		ch |= (bytes.index[i] & 0x3F);
	}

	// Check for illegal codepoints

	if (ch >= 0xD800 && ch <= 0xDFFF)
		return false;

	if (ch > 0x10FFFF)
		return false;

	// Check for 'overlong encodings'

	if (ch <= 0x7f && len_ch > 1)
		return false;

	if (ch <= 0x7ff && len_ch > 2)
		return false;

	if (ch <= 0xffff && len_ch > 3)
		return false;

	// We did it, return ch and len

	*ch_out = ch;
	*len_ch_out = len_ch;
	return true;
}

void Decode_utf8(
	Bytes_t bytes,
	Chars_t * chars_r,
	Locs_t * locs_r)
{
	assert(Bytes_len(bytes) <= Chars_len(*chars_r));
	assert(Bytes_len(bytes) + 1 <= Locs_len(*locs_r));

	// The start of the first char will be the start of the bytes

	locs_r->index[0] = bytes.index;

	// Chew through the byte span with Try_decode_utf8

	size_t count_chars = 0;
	while (!Bytes_empty(bytes))
	{
		char32_t ch;
		size_t len_ch;
		if (Try_decode_utf8_ch(bytes, &ch, &len_ch))
		{
			chars_r->index[count_chars] = ch;
			locs_r->index[count_chars + 1] = bytes.index + len_ch;
			bytes.index += len_ch;
		}
		else
		{
			chars_r->index[count_chars] = UINT32_MAX;
			locs_r->index[count_chars + 1] = bytes.index + 1;
			++bytes.index;
		}

		++count_chars;
	}
	chars_r->end = chars_r->index + count_chars;
	locs_r->end = locs_r->index + count_chars + 1;
}



// 'scrub' : replace patterns in chars

typedef struct Scrub_t
{
	Chars_t chars;
	Locs_t locs;
	
	char32_t * ch_from;
	Byte_t ** loc_from;
} Scrub_t;

Scrub_t Start_scrub(
	Chars_t chars,
	Locs_t locs)
{
	Scrub_t scrub;
	scrub.chars = chars;
	scrub.locs = locs;

	scrub.ch_from = chars.index;
	scrub.loc_from = locs.index;

	return scrub;
}

bool Is_scrub_done(Scrub_t scrub)
{
	return scrub.ch_from >= scrub.chars.end;
}

char32_t Scrub_ch(Scrub_t scrub, size_t index)
{
	Chars_t chars_from;
	chars_from.index = scrub.ch_from;
	chars_from.end = scrub.chars.end;
	return Char_at_index_safe(chars_from, index);
}

void Advance_scrub(
	Scrub_t * scrub_r,
	char32_t ch,
	size_t len_scrub)
{
	assert(scrub_r->chars.index < scrub_r->chars.end);
	assert(scrub_r->locs.index < scrub_r->locs.end);
	
	assert(scrub_r->ch_from < scrub_r->chars.end);
	assert(scrub_r->loc_from < scrub_r->locs.end);

	// Set next 'to' char to ch, and copy starting loc.

	scrub_r->chars.index[0] = ch;
	scrub_r->locs.index[0] = scrub_r->loc_from[0];

	// Inc from/to

	scrub_r->ch_from += len_scrub;
	scrub_r->loc_from += len_scrub;

	scrub_r->chars.index += 1;
	scrub_r->locs.index += 1;
}

void End_scrub(
	Scrub_t scrub, 
	Chars_t * chars_r,
	Locs_t * locs_r)
{
	// Move last loc (the end of the last token)

	assert(scrub.locs.index < scrub.locs.end);
	assert(scrub.loc_from < scrub.locs.end);

	scrub.locs.index[0] = scrub.loc_from[0];

	// Set end ptrs

	chars_r->end = scrub.chars.index;
	locs_r->end = scrub.locs.index + 1;
}

void Scrub_carriage_returns(
	Chars_t * chars_r,
	Locs_t * locs_r)
{
	Scrub_t scrub = Start_scrub(*chars_r, *locs_r);
	while (!Is_scrub_done(scrub))
	{
		char32_t ch = Scrub_ch(scrub, 0);
		bool is_cr = ch == '\r';
		if (is_cr)
			ch = '\n';

		size_t len_scrub = 1;
		if (is_cr && Scrub_ch(scrub, 1) == '\n')
		{
			len_scrub = 2;
		}

		Advance_scrub(&scrub, ch, len_scrub);
	}
	End_scrub(scrub, chars_r, locs_r);
}

void Scrub_trigraphs(
	Chars_t * chars_r,
	Locs_t * locs_r)
{
	Scrub_t scrub = Start_scrub(*chars_r, *locs_r);
	while (!Is_scrub_done(scrub))
	{
		char32_t ch0 = Scrub_ch(scrub, 0);

		if (ch0 == '?' && Scrub_ch(scrub, 1) == '?')
		{
			char32_t ch2 = Scrub_ch(scrub, 2);

			char32_t pairs[][2] =
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

			int i_match = -1;
			for (int i = 0; i < ARY_LEN(pairs); ++i)
			{
				if (pairs[i][0] == ch2)
				{
					i_match = i;
					break;
				}
			}

			if (i_match != -1)
			{
				char32_t ch_replace = pairs[i_match][1];
				Advance_scrub(&scrub, ch_replace, 3);

				continue;
			}
		}

		Advance_scrub(&scrub, ch0, 1);
	}
	End_scrub(scrub, chars_r, locs_r);
}

void Scrub_leading_escaped_line_breaks(Scrub_t * scrub_r)
{
	// Get length of leading esc eols

	size_t len = 0;
	while (true)
	{
		size_t i = len;
		if (Scrub_ch(*scrub_r, i) != '\\')
			break;

		++i;

		while (Is_hz_ws(Scrub_ch(*scrub_r, i)))
		{
			++i;
		}

		// Do not need to check for \r. since that 
		//  has already been scrubbed.

		if (Scrub_ch(*scrub_r, i) != '\n')
			break;

		len = i + 1;
	}

	// Advance now that we have the length

	if (len == 0)
	{
		Advance_scrub(scrub_r, Scrub_ch(*scrub_r, 0), 1);
	}
	else if (scrub_r->ch_from + len == scrub_r->chars.end)
	{
		// Drop trailing escaped line break
		//  (do not include it in a character)
		
		scrub_r->ch_from = scrub_r->chars.end;
	}
	else
	{
		Advance_scrub(
			scrub_r, 
			Scrub_ch(*scrub_r, len), 
			len + 1);
	}
}

void Scrub_escaped_line_breaks(
	Chars_t * chars_r,
	Locs_t * locs_r)
{
	Scrub_t scrub = Start_scrub(*chars_r, *locs_r);
	while (!Is_scrub_done(scrub))
	{
		Scrub_leading_escaped_line_breaks(&scrub);
	}
	End_scrub(scrub, chars_r, locs_r);
}



// Lex

typedef enum Tokk_t
{
	Tokk_unknown = 0,
	Tokk_eof = 1,
	Tokk_eod = 2,
	Tokk_code_completion = 3,
	Tokk_comment = 4,
	Tokk_identifier = 5,
	Tokk_raw_identifier = 6,
	Tokk_numeric_constant = 7,
	Tokk_char_constant = 8,
	Tokk_wide_char_constant = 9,
	Tokk_utf8_char_constant = 10,
	Tokk_utf16_char_constant = 11,
	Tokk_utf32_char_constant = 12,
	Tokk_string_literal = 13,
	Tokk_wide_string_literal = 14,
	Tokk_header_name = 15,
	Tokk_utf8_string_literal = 16,
	Tokk_utf16_string_literal = 17,
	Tokk_utf32_string_literal = 18,
	Tokk_l_square = 19,
	Tokk_r_square = 20,
	Tokk_l_paren = 21,
	Tokk_r_paren = 22,
	Tokk_l_brace = 23,
	Tokk_r_brace = 24,
	Tokk_period = 25,
	Tokk_ellipsis = 26,
	Tokk_amp = 27,
	Tokk_ampamp = 28,
	Tokk_ampequal = 29,
	Tokk_star = 30,
	Tokk_starequal = 31,
	Tokk_plus = 32,
	Tokk_plusplus = 33,
	Tokk_plusequal = 34,
	Tokk_minus = 35,
	Tokk_arrow = 36,
	Tokk_minusminus = 37,
	Tokk_minusequal  = 38,
	Tokk_tilde = 39,
	Tokk_exclaim = 40,
	Tokk_exclaimequal = 41,
	Tokk_slash = 42,
	Tokk_slashequal = 43,
	Tokk_percent = 44,
	Tokk_percentequal = 45,
	Tokk_less = 46,
	Tokk_lessless = 47,
	Tokk_lessequal = 48,
	Tokk_lesslessequal = 49,
	Tokk_spaceship = 50,
	Tokk_greater = 51,
	Tokk_greatergreater = 52,
	Tokk_greaterequal = 53,
	Tokk_greatergreaterequal = 54,
	Tokk_caret = 55,
	Tokk_caretequal = 56,
	Tokk_pipe = 57,
	Tokk_pipepipe = 58,
	Tokk_pipeequal = 59,
	Tokk_question = 60,
	Tokk_colon = 61,
	Tokk_semi = 62,
	Tokk_equal = 63,
	Tokk_equalequal = 64,
	Tokk_comma = 65,
	Tokk_hash = 66,
	Tokk_hashhash = 67,
	Tokk_hashat = 68,
	Tokk_periodstar = 69,
	Tokk_arrowstar = 70,
	Tokk_coloncolon = 71,
	Tokk_at = 72,
	Tokk_lesslessless = 73,
	Tokk_greatergreatergreater = 74,
	Tokk_caretcaret = 75,
	
	Tokk_kw_auto = 76,
	Tokk_kw_break = 77,
	Tokk_kw_case = 78,
	Tokk_kw_char = 79,
	Tokk_kw_const = 80,
	Tokk_kw_continue = 81,
	Tokk_kw_default = 82,
	Tokk_kw_do = 83,
	Tokk_kw_double = 84,
	Tokk_kw_else = 85,
	Tokk_kw_enum = 86,
	Tokk_kw_extern = 87,
	Tokk_kw_float = 88,
	Tokk_kw_for = 89,
	Tokk_kw_goto = 90,
	Tokk_kw_if = 91,
	Tokk_kw_int = 92,
	Tokk_kw__ExtInt = 93,
	Tokk_kw__BitInt = 94,
	Tokk_kw_long = 95,
	Tokk_kw_register = 96,
	Tokk_kw_return = 97,
	Tokk_kw_short = 98,
	Tokk_kw_signed = 99,
	Tokk_kw_sizeof = 100,
	Tokk_kw_static = 101,
	Tokk_kw_struct = 102,
	Tokk_kw_switch = 103,
	Tokk_kw_typedef = 104,
	Tokk_kw_union = 105,
	Tokk_kw_unsigned = 106,
	Tokk_kw_void = 107,
	Tokk_kw_volatile = 108,
	Tokk_kw_while = 109,
	Tokk_kw__Alignas = 110,
	Tokk_kw__Alignof = 111,
	Tokk_kw__Atomic = 112,
	Tokk_kw__Bool = 113,
	Tokk_kw__Complex = 114,
	Tokk_kw__Generic = 115,
	Tokk_kw__Imaginary = 116,
	Tokk_kw__Noreturn = 117,
	Tokk_kw__Static_assert = 118,
	Tokk_kw__Thread_local = 119,
	Tokk_kw___func__ = 120,
	Tokk_kw___objc_yes = 121,
	Tokk_kw___objc_no = 122,
	Tokk_kw_asm = 123,
	Tokk_kw_bool = 124,
	Tokk_kw_catch = 125,
	Tokk_kw_class = 126,
	Tokk_kw_const_cast = 127,
	Tokk_kw_delete = 128,
	Tokk_kw_dynamic_cast = 129,
	Tokk_kw_explicit = 130,
	Tokk_kw_export = 131,
	Tokk_kw_false = 132,
	Tokk_kw_friend = 133,
	Tokk_kw_mutable = 134,
	Tokk_kw_namespace = 135,
	Tokk_kw_new = 136,
	Tokk_kw_operator = 137,
	Tokk_kw_private = 138,
	Tokk_kw_protected = 139,
	Tokk_kw_public = 140,
	Tokk_kw_reinterpret_cast = 141,
	Tokk_kw_static_cast = 142,
	Tokk_kw_template = 143,
	Tokk_kw_this = 144,
	Tokk_kw_throw = 145,
	Tokk_kw_true = 146,
	Tokk_kw_try = 147,
	Tokk_kw_typename = 148,
	Tokk_kw_typeid = 149,
	Tokk_kw_using = 150,
	Tokk_kw_virtual = 151,
	Tokk_kw_wchar_t = 152,
	Tokk_kw_restrict = 153,
	Tokk_kw_inline = 154,
	Tokk_kw_alignas = 155,
	Tokk_kw_alignof = 156,
	Tokk_kw_char16_t = 157,
	Tokk_kw_char32_t = 158,
	Tokk_kw_constexpr = 159,
	Tokk_kw_decltype = 160,
	Tokk_kw_noexcept = 161,
	Tokk_kw_nullptr = 162,
	Tokk_kw_static_assert = 163,
	Tokk_kw_thread_local = 164,
	Tokk_kw_co_await = 165,
	Tokk_kw_co_return = 166,
	Tokk_kw_co_yield = 167,
	Tokk_kw_module = 168,
	Tokk_kw_import = 169,
	Tokk_kw_consteval = 170,
	Tokk_kw_constinit = 171,
	Tokk_kw_concept = 172,
	Tokk_kw_requires = 173,
	Tokk_kw_char8_t = 174,
	Tokk_kw__Float16 = 175,
	Tokk_kw_typeof = 176,
	Tokk_kw_typeof_unqual = 177,
	Tokk_kw__Accum = 178,
	Tokk_kw__Fract = 179,
	Tokk_kw__Sat = 180,
	Tokk_kw__Decimal32 = 181,
	Tokk_kw__Decimal64 = 182,
	Tokk_kw__Decimal128 = 183,
	Tokk_kw___null = 184,
	Tokk_kw___alignof = 185,
	Tokk_kw___attribute = 186,
	Tokk_kw___builtin_choose_expr = 187,
	Tokk_kw___builtin_offsetof = 188,
	Tokk_kw___builtin_FILE = 189,
	Tokk_kw___builtin_FILE_NAME = 190,
	Tokk_kw___builtin_FUNCTION = 191,
	Tokk_kw___builtin_FUNCSIG = 192,
	Tokk_kw___builtin_LINE = 193,
	Tokk_kw___builtin_COLUMN = 194,
	Tokk_kw___builtin_source_location = 195,
	Tokk_kw___builtin_types_compatible_p = 196,
	Tokk_kw___builtin_va_arg = 197,
	Tokk_kw___extension__ = 198,
	Tokk_kw___float128 = 199,
	Tokk_kw___ibm128 = 200,
	Tokk_kw___imag = 201,
	Tokk_kw___int128 = 202,
	Tokk_kw___label__ = 203,
	Tokk_kw___real = 204,
	Tokk_kw___thread = 205,
	Tokk_kw___FUNCTION__ = 206,
	Tokk_kw___PRETTY_FUNCTION__ = 207,
	Tokk_kw___auto_type = 208,
	Tokk_kw___FUNCDNAME__ = 209,
	Tokk_kw___FUNCSIG__ = 210,
	Tokk_kw_L__FUNCTION__ = 211,
	Tokk_kw_L__FUNCSIG__ = 212,
	Tokk_kw___is_interface_class = 213,
	Tokk_kw___is_sealed = 214,
	Tokk_kw___is_destructible = 215,
	Tokk_kw___is_trivially_destructible = 216,
	Tokk_kw___is_nothrow_destructible = 217,
	Tokk_kw___is_nothrow_assignable = 218,
	Tokk_kw___is_constructible = 219,
	Tokk_kw___is_nothrow_constructible = 220,
	Tokk_kw___is_assignable = 221,
	Tokk_kw___has_nothrow_move_assign = 222,
	Tokk_kw___has_trivial_move_assign = 223,
	Tokk_kw___has_trivial_move_constructor = 224,
	Tokk_kw___has_nothrow_assign = 225,
	Tokk_kw___has_nothrow_copy = 226,
	Tokk_kw___has_nothrow_constructor = 227,
	Tokk_kw___has_trivial_assign = 228,
	Tokk_kw___has_trivial_copy = 229,
	Tokk_kw___has_trivial_constructor = 230,
	Tokk_kw___has_trivial_destructor = 231,
	Tokk_kw___has_virtual_destructor = 232,
	Tokk_kw___is_abstract = 233,
	Tokk_kw___is_aggregate = 234,
	Tokk_kw___is_base_of = 235,
	Tokk_kw___is_class = 236,
	Tokk_kw___is_convertible_to = 237,
	Tokk_kw___is_empty = 238,
	Tokk_kw___is_enum = 239,
	Tokk_kw___is_final = 240,
	Tokk_kw___is_literal = 241,
	Tokk_kw___is_pod = 242,
	Tokk_kw___is_polymorphic = 243,
	Tokk_kw___is_standard_layout = 244,
	Tokk_kw___is_trivial = 245,
	Tokk_kw___is_trivially_assignable = 246,
	Tokk_kw___is_trivially_constructible = 247,
	Tokk_kw___is_trivially_copyable = 248,
	Tokk_kw___is_union = 249,
	Tokk_kw___has_unique_object_representations = 250,
	Tokk_kw___add_lvalue_reference = 251,
	Tokk_kw___add_pointer = 252,
	Tokk_kw___add_rvalue_reference = 253,
	Tokk_kw___decay = 254,
	Tokk_kw___make_signed = 255,
	Tokk_kw___make_unsigned = 256,
	Tokk_kw___remove_all_extents = 257,
	Tokk_kw___remove_const = 258,
	Tokk_kw___remove_cv = 259,
	Tokk_kw___remove_cvref = 260,
	Tokk_kw___remove_extent = 261,
	Tokk_kw___remove_pointer = 262,
	Tokk_kw___remove_reference_t = 263,
	Tokk_kw___remove_restrict = 264,
	Tokk_kw___remove_volatile = 265,
	Tokk_kw___underlying_type = 266,
	Tokk_kw___is_trivially_relocatable = 267,
	Tokk_kw___is_trivially_equality_comparable = 268,
	Tokk_kw___is_bounded_array = 269,
	Tokk_kw___is_unbounded_array = 270,
	Tokk_kw___is_nullptr = 271,
	Tokk_kw___is_scoped_enum = 272,
	Tokk_kw___is_referenceable = 273,
	Tokk_kw___can_pass_in_regs = 274,
	Tokk_kw___reference_binds_to_temporary = 275,
	Tokk_kw___is_lvalue_expr = 276,
	Tokk_kw___is_rvalue_expr = 277,
	Tokk_kw___is_arithmetic = 278,
	Tokk_kw___is_floating_point = 279,
	Tokk_kw___is_integral = 280,
	Tokk_kw___is_complete_type = 281,
	Tokk_kw___is_void = 282,
	Tokk_kw___is_array = 283,
	Tokk_kw___is_function = 284,
	Tokk_kw___is_reference = 285,
	Tokk_kw___is_lvalue_reference = 286,
	Tokk_kw___is_rvalue_reference = 287,
	Tokk_kw___is_fundamental = 288,
	Tokk_kw___is_object = 289,
	Tokk_kw___is_scalar = 290,
	Tokk_kw___is_compound = 291,
	Tokk_kw___is_pointer = 292,
	Tokk_kw___is_member_object_pointer = 293,
	Tokk_kw___is_member_function_pointer = 294,
	Tokk_kw___is_member_pointer = 295,
	Tokk_kw___is_const = 296,
	Tokk_kw___is_volatile = 297,
	Tokk_kw___is_signed = 298,
	Tokk_kw___is_unsigned = 299,
	Tokk_kw___is_same = 300,
	Tokk_kw___is_convertible = 301,
	Tokk_kw___array_rank = 302,
	Tokk_kw___array_extent = 303,
	Tokk_kw___private_extern__ = 304,
	Tokk_kw___module_private__ = 305,
	Tokk_kw___declspec = 306,
	Tokk_kw___cdecl = 307,
	Tokk_kw___stdcall = 308,
	Tokk_kw___fastcall = 309,
	Tokk_kw___thiscall = 310,
	Tokk_kw___regcall = 311,
	Tokk_kw___vectorcall = 312,
	Tokk_kw___forceinline = 313,
	Tokk_kw___unaligned = 314,
	Tokk_kw___super = 315,
	Tokk_kw___global = 316,
	Tokk_kw___local = 317,
	Tokk_kw___constant = 318,
	Tokk_kw___private = 319,
	Tokk_kw___generic = 320,
	Tokk_kw___kernel = 321,
	Tokk_kw___read_only = 322,
	Tokk_kw___write_only = 323,
	Tokk_kw___read_write = 324,
	Tokk_kw___builtin_astype = 325,
	Tokk_kw_vec_step = 326,
	Tokk_kw_image1d_t = 327,
	Tokk_kw_image1d_array_t = 328,
	Tokk_kw_image1d_buffer_t = 329,
	Tokk_kw_image2d_t = 330,
	Tokk_kw_image2d_array_t = 331,
	Tokk_kw_image2d_depth_t = 332,
	Tokk_kw_image2d_array_depth_t = 333,
	Tokk_kw_image2d_msaa_t = 334,
	Tokk_kw_image2d_array_msaa_t = 335,
	Tokk_kw_image2d_msaa_depth_t = 336,
	Tokk_kw_image2d_array_msaa_depth_t = 337,
	Tokk_kw_image3d_t = 338,
	Tokk_kw_pipe = 339,
	Tokk_kw_addrspace_cast = 340,
	Tokk_kw___noinline__ = 341,
	Tokk_kw_cbuffer = 342,
	Tokk_kw_tbuffer = 343,
	Tokk_kw_groupshared = 344,
	Tokk_kw___builtin_omp_required_simd_align = 345,
	Tokk_kw___pascal = 346,
	Tokk_kw___vector = 347,
	Tokk_kw___pixel = 348,
	Tokk_kw___bool = 349,
	Tokk_kw___bf16 = 350,
	Tokk_kw_half = 351,
	Tokk_kw___bridge = 352,
	Tokk_kw___bridge_transfer = 353,
	Tokk_kw___bridge_retained = 354,
	Tokk_kw___bridge_retain = 355,
	Tokk_kw___covariant = 356,
	Tokk_kw___contravariant = 357,
	Tokk_kw___kindof = 358,
	Tokk_kw__Nonnull = 359,
	Tokk_kw__Nullable = 360,
	Tokk_kw__Nullable_result = 361,
	Tokk_kw__Null_unspecified = 362,
	Tokk_kw___funcref = 363,
	Tokk_kw___ptr64 = 364,
	Tokk_kw___ptr32 = 365,
	Tokk_kw___sptr = 366,
	Tokk_kw___uptr = 367,
	Tokk_kw___w64 = 368,
	Tokk_kw___uuidof = 369,
	Tokk_kw___try = 370,
	Tokk_kw___finally = 371,
	Tokk_kw___leave = 372,
	Tokk_kw___int64 = 373,
	Tokk_kw___if_exists = 374,
	Tokk_kw___if_not_exists = 375,
	Tokk_kw___single_inheritance = 376,
	Tokk_kw___multiple_inheritance = 377,
	Tokk_kw___virtual_inheritance = 378,
	Tokk_kw___interface = 379,
	Tokk_kw___builtin_convertvector = 380,
	Tokk_kw___builtin_bit_cast = 381,
	Tokk_kw___builtin_available = 382,
	Tokk_kw___builtin_sycl_unique_stable_name = 383,
	Tokk_kw___arm_streaming = 384,
	Tokk_kw___unknown_anytype = 385,

	Tokk_annot_cxxscope = 386,
	Tokk_annot_typename = 387,
	Tokk_annot_template_id = 388,
	Tokk_annot_non_type = 389,
	Tokk_annot_non_type_undeclared = 390,
	Tokk_annot_non_type_dependent = 391,
	Tokk_annot_overload_set = 392,
	Tokk_annot_primary_expr = 393,
	Tokk_annot_decltype = 394,
	Tokk_annot_pragma_unused = 395,
	Tokk_annot_pragma_vis = 396,
	Tokk_annot_pragma_pack = 397,
	Tokk_annot_pragma_parser_crash = 398,
	Tokk_annot_pragma_captured = 399,
	Tokk_annot_pragma_dump = 400,
	Tokk_annot_pragma_msstruct = 401,
	Tokk_annot_pragma_align = 402,
	Tokk_annot_pragma_weak = 403,
	Tokk_annot_pragma_weakalias = 404,
	Tokk_annot_pragma_redefine_extname = 405,
	Tokk_annot_pragma_fp_contract = 406,
	Tokk_annot_pragma_fenv_access = 407,
	Tokk_annot_pragma_fenv_access_ms = 408,
	Tokk_annot_pragma_fenv_round = 409,
	Tokk_annot_pragma_float_control = 410,
	Tokk_annot_pragma_ms_pointers_to_members = 411,
	Tokk_annot_pragma_ms_vtordisp = 412,
	Tokk_annot_pragma_ms_pragma = 413,
	Tokk_annot_pragma_opencl_extension = 414,
	Tokk_annot_attr_openmp = 415,
	Tokk_annot_pragma_openmp = 416,
	Tokk_annot_pragma_openmp_end = 417,
	Tokk_annot_pragma_loop_hint = 418,
	Tokk_annot_pragma_fp = 419,
	Tokk_annot_pragma_attribute = 420,
	Tokk_annot_pragma_riscv = 421,
	Tokk_annot_module_include = 422,
	Tokk_annot_module_begin = 423,
	Tokk_annot_module_end = 424,
	Tokk_annot_header_unit = 425,
	Tokk_annot_repl_input_end = 426,
} Tokk_t;

void Lex_punctuation(
	const char32_t * chars,
	size_t count_chars,
	size_t i,
	Tokk_t * tokk_out,
	size_t * i_after_out)
{
	typedef struct
	{
		const char * str;
		Tokk_t tokk;
		int _padding;
	} Punctution_t;

	// "::" is included to match clang
	// https://github.com/llvm/llvm-project/commit/874217f99b99ab3c9026dc3b7bd84cd2beebde6e

	static Punctution_t puctuations[] =
	{
		{"%:%:", Tokk_hashhash},
		{">>=", Tokk_greatergreaterequal},
		{"<<=", Tokk_lesslessequal},
		{"...", Tokk_ellipsis},
		{"|=", Tokk_pipeequal},
		{"||", Tokk_pipepipe},
		{"^=", Tokk_caretequal},
		{"==", Tokk_equalequal},
		{"::", Tokk_coloncolon},
		{":>", Tokk_r_square},
		{"-=", Tokk_minusequal},
		{"--", Tokk_minusminus},
		{"->", Tokk_arrow},
		{"+=", Tokk_plusequal},
		{"++", Tokk_plusplus},
		{"*=", Tokk_starequal},
		{"&=", Tokk_ampequal},
		{"&&", Tokk_ampamp},
		{"##", Tokk_hashhash},
		{"!=", Tokk_exclaimequal},
		{">=", Tokk_greaterequal},
		{">>", Tokk_greatergreater},
		{"<=", Tokk_lessequal},
		{"<:", Tokk_l_square},
		{"<%", Tokk_l_brace},
		{"<<", Tokk_lessless},
		{"%>", Tokk_r_brace},
		{"%=", Tokk_percentequal},
		{"%:", Tokk_hash},
		{"/=", Tokk_slashequal},
		{"~", Tokk_tilde},
		{"}", Tokk_r_brace},
		{"{", Tokk_l_brace},
		{"]", Tokk_r_square},
		{"[", Tokk_l_square},
		{"?", Tokk_question},
		{";", Tokk_semi},
		{",", Tokk_comma},
		{")", Tokk_r_paren},
		{"(", Tokk_l_paren},
		{"|", Tokk_pipe},
		{"^", Tokk_caret},
		{"=", Tokk_equal},
		{":", Tokk_colon},
		{"-", Tokk_minus},
		{"+", Tokk_plus},
		{"*", Tokk_star},
		{"&", Tokk_amp},
		{"#", Tokk_hash},
		{"!", Tokk_exclaim},
		{">", Tokk_greater},
		{"<", Tokk_less},
		{"%", Tokk_percent},
		{".", Tokk_period},
		{"/", Tokk_slash},
	};

	for (int i_puctuation = 0; i_puctuation < ARY_LEN(puctuations); ++i_puctuation)
	{
		Punctution_t punctuation = puctuations[i_puctuation];
		const char * str_puctuation = punctuation.str;
		size_t len = strlen(str_puctuation);

		if (i + len > count_chars)
			continue;

		size_t i_peek = i;
		bool found_match = true;

		for (size_t i_ch = 0; i_ch < len; ++i_ch)
		{
			// This code is messy + has bad var names...

			char ch = str_puctuation[i_ch];
			char32_t cp_ch = (char32_t)ch;

			char32_t cp = chars[i_peek];
			++i_peek;

			if (cp != cp_ch)
			{
				found_match = false;
				break;
			}
		}

		if (found_match)
		{
			*i_after_out = i_peek;
			*tokk_out = punctuation.tokk;
			return;
		}
	}

	*i_after_out = i + 1;
	*tokk_out = Tokk_unknown;
}

bool Starts_id(char32_t ch)
{
	// Letters

	if (ch >= 'a' && ch <= 'z')
		return true;

	if (ch >= 'A' && ch <= 'Z')
		return true;

	// Underscore

	if (ch == '_')
		return true;

	// '$' allowed as an extension :/

	if (ch == '$')
		return true;

	// All other ascii does not start ids

	if (ch <= 0x7F)
		return false;

	// Bogus utf8 does not start ids

	if (ch == UINT32_MAX)
		return false;

	// These codepoints are not allowed as the start of an id

	static const char32_t no[][2] =
	{
		{ 0x0300, 0x036F },
		{ 0x1DC0, 0x1DFF },
		{ 0x20D0, 0x20FF },
		{ 0xFE20, 0xFE2F }
	};

	for (int i = 0; i < ARY_LEN(no); ++i)
	{
		char32_t first = no[i][0];
		char32_t last = no[i][1];
		if (ch >= first && ch <= last)
			return false;
	}

	// These code points are allowed to start an id (minus ones from 'no')

	static const char32_t yes[][2] =
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

	for (int i = 0; i < ARY_LEN(yes); ++i)
	{
		char32_t first = yes[i][0];
		char32_t last = yes[i][1];
		if (ch >= first && ch <= last)
			return true;
	}

	return false;
}

bool Is_valid_ucn(char32_t ch)
{
	// A universal character name shall not specify a character whose
	//  short identifier is less than 00A0, nor one in the range 
	//  D800 through DFFF inclusive.

	if (ch < 0xA0)
		return false;

	if (ch >= 0xD800 && ch <= 0xDFFF)
		return false;

	return true;
}

uint32_t Hex_val_from_ch(char32_t ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';

	if (ch < 'A')
		return UINT32_MAX;

	if (ch > 'f')
		return UINT32_MAX;

	if (ch <= 'F')
		return ch - 'A' + 10;

	if (ch >= 'a')
		return ch - 'a' + 10;

	return UINT32_MAX;
}

void Peek_ucn(
	const char32_t * chars,
	size_t count_chars,
	size_t i,
	char32_t * ch_out,
	size_t * i_after_out)
{
	*ch_out = UINT32_MAX;
	*i_after_out = i;

	if (i >= count_chars)
		return;

	// Check for leading '\\'

	if (chars[i] != '\\')
		return;

	// Advance past '\\'

	++i;
	if (i >= count_chars)
		return;

	// Look for 'u' or 'U' after '\\'

	char32_t ch_u = chars[i];
	if (ch_u != 'u' && ch_u != 'U')
		return;

	// Look for 4 or 8 hex digits, based on u vs U

	int digits_need;
	if (ch_u == 'u')
	{
		digits_need = 4;
	}
	else
	{
		digits_need = 8;
	}

	// Advance past u/U

	++i;
	if (i >= count_chars)
		return;

	// Look for correct number of hex digits

	char32_t ch_result = 0;
	int digits_read = 0;
	bool delimited = false;
	bool got_end_delim = false;

	while (i < count_chars && ((digits_read < digits_need) || delimited))
	{
		char32_t ch = chars[i];

		// Check for '{' (delimited ucns)

		if (!delimited && digits_read == 0 && ch == '{')
		{
			delimited = true;
			++i;
			continue;
		}

		// Check for '}' (delimited ucns)

		if (delimited && ch == '}')
		{
			got_end_delim = true;
			++i;
			break;
		}

		// Check if valid hex digit

		char32_t digit_val = Hex_val_from_ch(ch);
		if (digit_val == UINT32_MAX)
		{
			if (delimited)
				return;

			break;
		}

		// Bail out if we are about to overflow

		if (ch_result & 0xF000'0000)
			return;

		// Fold hex digit into cp

		ch_result <<= 4;
		ch_result |= digit_val;

		// Keep track of how many digits we have read

		++digits_read;

		// Advance to next digit

		++i;
	}

	// No digits read?

	if (digits_read == 0)
		return;

	// Delimited 'U' is not allowed (find somthing in clang to explain this??)

	if (delimited && digits_need == 8)
		return;

	// Read wrong number of digits?

	if (!delimited && digits_read != digits_need)
		return;

	// Sanity check that people are not trying to encode
	//  something particularly weird with a UCN.
	//  Convert any weird inputs to the error value UINT32_MAX

	if (!Is_valid_ucn(ch_result))
	{
		ch_result = UINT32_MAX;
	}

	// Return result

	*ch_out = ch_result;
	*i_after_out = i;
}

bool Extends_id(char32_t ch)
{
	if (ch >= 'a' && ch <= 'z')
		return true;

	if (ch >= 'A' && ch <= 'Z')
		return true;

	if (ch == '_')
		return true;

	if (ch >= '0' && ch <= '9')
		return true;

	if (ch == '$') // '$' allowed as an extension :/
		return true;

	if (ch <= 0x7F) // All other ascii is invalid
		return false;

	if (ch == UINT32_MAX) // Bogus utf8 does not extend ids
		return false;

	// We are lexing in 'raw mode', and to match clang, 
	//  once we are parsing an id, we just slurp up all
	//  valid non-ascii-non-whitespace utf8...

	// BUG I do not like this. the right thing to do is check c11_allowed from May_cp_start_id.
	//  Clang seems to do the wrong thing here,
	//  and produce an invalid pp token. I suspect no one
	//  actually cares, since dump_raw_tokens is only for debugging...

	static const char32_t ws[][2] =
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

	for (int i = 0; i < ARY_LEN(ws); ++i)
	{
		char32_t first = ws[i][0];
		char32_t last = ws[i][1];
		if (ch >= first && ch <= last)
			return false;
	}

	return true;
}

size_t After_rest_of_id(
	const char32_t * chars,
	size_t count_chars,
	size_t i)
{
	while (i < count_chars)
	{
		if (Extends_id(chars[i]))
		{
			++i;
			continue;
		}

		if (chars[i] == '\\')
		{
			char32_t ch;
			size_t i_after;
			Peek_ucn(chars, count_chars, i, &ch, &i_after);
			if (i_after != i && Extends_id(ch))
			{
				i = i_after;
				continue;
			}
		}

		break;
	}

	return i;
}

size_t After_rest_of_ppnum(
	const char32_t * chars,
	size_t count_chars,
	size_t i)
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

	while (i < count_chars)
	{
		char32_t ch = chars[i];

		if (ch == '.')
		{
			++i;
			continue;
		}
		else if (ch == 'e' || ch == 'E' || ch == 'p' || ch == 'P')
		{
			++i;

			if (i < count_chars)
			{
				ch = chars[i];
				if (ch == '+' || ch == '-')
				{
					++i;
				}
			}

			continue;
		}
		else if (ch == '$')
		{
			// Clang does not allow '$' in ppnums, 
			//  even though the spec would seem to suggest that 
			//  implimentation defined id chars should be included in PP nums...

			break;
		}
		else if (Extends_id(ch))
		{
			// Everything (else) which extends ids can extend a ppnum

			++i;
			continue;
		}
		else if (ch == '\\')
		{
			char32_t ch_ucn;
			size_t i_after;

			Peek_ucn(chars, count_chars, i, &ch_ucn, &i_after);
			if (i_after != i && Extends_id(ch_ucn))
			{
				i = i_after;
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

	return i;
}

size_t After_rest_of_line_comment(
	const char32_t * chars,
	size_t count_chars,
	size_t i)
{
	while (i < count_chars)
	{
		if (chars[i] == '\n')
			break;

		++i;
	}

	return i;
}

void Lex_rest_of_block_comment(
	const char32_t * chars,
	size_t count_chars,
	size_t i,
	size_t * i_after_out,
	Tokk_t * tokk_out)
{
	Tokk_t tokk = Tokk_unknown;

	while (i < count_chars)
	{
		char32_t ch0 = chars[i];
		++i;

		if (i < count_chars)
		{
			char32_t ch1 = chars[i];
			if (ch0 == '*' && ch1 == '/')
			{
				tokk = Tokk_comment;
				++i;
				break;
			}
		}
	}

	*i_after_out = i;
	*tokk_out = tokk;
}

void Lex_rest_of_str_lit_(
	bool use_dquote,
	const char32_t * chars,
	size_t count_chars,
	size_t i,
	size_t * i_after_out,
	bool * valid_out)
{
	char32_t ch_close = (use_dquote) ? U'"' : U'\'';

	bool closed = false;
	int len = 0;

	while (i < count_chars)
	{
		char32_t ch = chars[i];
		char chr = (char)ch;
		(void)chr;

		// String without closing quote (which we support in raw lexing mode..)

		if (ch == '\n')
			break;

		// Anything else will be part of the str lit

		++i;

		// Closing quote

		if (ch == ch_close)
		{
			closed = true;
			break;
		}

		// Found somthing other than open/close quote, inc len

		++len;

		// Deal with back slash

		if (ch == '\\' && i < count_chars)
		{
			// Check if escaped char is '\"', '\'', or '\\',
			//  the only escapes we need to handle in raw mode

			ch = chars[i];
			if (ch == ch_close || ch == '\\')
			{
				++len;
				++i;
			}
		}
	}

	// zero length char lits are invalid

	*valid_out = closed && (use_dquote || len > 0);
	*i_after_out = i;
}

size_t After_whitespace(
	const char32_t * chars,
	size_t count_chars,
	size_t i)
{
	while (i < count_chars)
	{
		if (!Is_ws(chars[i]))
			break;

		++i;
	}

	return i;
}

void Lex(
	const char32_t * chars,
	size_t count_chars,
	size_t i,
	size_t * i_after_out,
	Tokk_t * tokk_out)
{
	char32_t ch_0 = chars[i + 0];
	char32_t ch_1 = (count_chars - i >= 2) ? chars[i + 1] : '\0';
	char32_t ch_2 = (count_chars - i >= 3) ? chars[i + 2] : '\0';

	// Decide what to do

	if (ch_0 == 'u' && ch_1 == '8' && ch_2 == '"')
	{
		i += 3;

		bool valid;
		Lex_rest_of_str_lit_(
			true,
			chars,
			count_chars,
			i,
			i_after_out,
			&valid);

		*tokk_out = (valid) ? Tokk_utf8_string_literal : Tokk_unknown;
	}
	else if ((ch_0 == 'u' || ch_0 == 'U' || ch_0 == 'L') &&
			 (ch_1 == '"' || ch_1 == '\''))
	{
		i += 2;

		bool valid;
		Lex_rest_of_str_lit_(
			ch_1 == '"', 
			chars, 
			count_chars, 
			i, 
			i_after_out, 
			&valid);

		if (!valid)
		{
			*tokk_out = Tokk_unknown;
			return;
		}
		
		Tokk_t tokk;
		switch (ch_0)
		{
		case 'u':
			tokk = (ch_1 == '"') ? Tokk_utf16_string_literal : Tokk_utf16_char_constant;
			break;
		case 'U':
			tokk = (ch_1 == '"') ? Tokk_utf32_string_literal : Tokk_utf32_char_constant;
			break;
		default: // 'L'
			tokk = (ch_1 == '"') ? Tokk_wide_string_literal : Tokk_wide_char_constant;
			break;
		}
		
		*tokk_out = tokk;
	}
	else if (ch_0 == '"' || ch_0 == '\'')
	{
		++i;

		bool valid;
		Lex_rest_of_str_lit_(
			ch_0 == '"', 
			chars, 
			count_chars, 
			i, 
			i_after_out, 
			&valid);

		if (!valid)
		{
			*tokk_out = Tokk_unknown;
			return;
		}

		*tokk_out = (ch_0 == '"') ? Tokk_string_literal : Tokk_char_constant;
	}
	else if (ch_0 == '/' && ch_1 == '*')
	{
		i += 2;
		Lex_rest_of_block_comment(chars, count_chars, i, i_after_out, tokk_out);
	}
	else if (ch_0 == '/' && ch_1 == '/')
	{
		i += 2;
		*i_after_out = After_rest_of_line_comment(chars, count_chars, i);
		*tokk_out = Tokk_comment;
	}
	else if (ch_0 == '.' && ch_1 >= '0' && ch_1 <= '9')
	{
		i += 2;
		*i_after_out = After_rest_of_ppnum(chars, count_chars, i);
		*tokk_out = Tokk_numeric_constant;
	}
	else if (Starts_id(ch_0))
	{
		++i;
		*i_after_out = After_rest_of_id(chars, count_chars, i);
		*tokk_out = Tokk_raw_identifier;
	}
	else if (ch_0 >= '0' && ch_0 <= '9')
	{
		++i;
		*i_after_out = After_rest_of_ppnum(chars, count_chars, i);
		*tokk_out = Tokk_numeric_constant;
	}
	else if (Is_ws(ch_0) || ch_0 == '\0')
	{
		++i;
		*i_after_out = After_whitespace(chars, count_chars, i);
		*tokk_out = Tokk_unknown;
	}
	else if (ch_0 =='\\')
	{
		char32_t ch;
		size_t i_after;
		Peek_ucn(chars, count_chars, i, &ch, &i_after);
		if (i_after != i)
		{
			if (Starts_id(ch))
			{
				i = i_after;
				*i_after_out = After_rest_of_id(chars, count_chars, i);
				*tokk_out = Tokk_raw_identifier;
			}
			else
			{
				// Bogus UCN, return it as an unknown token

				*i_after_out = i_after;
				*tokk_out = Tokk_unknown;
			}
		}
		else
		{
			// Stray backslash, return as unknown token
			
			*i_after_out = i + 1;
			*tokk_out = Tokk_unknown;
		}
	}
	else
	{
		Lex_punctuation(chars, count_chars, i, tokk_out, i_after_out);
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
    unsigned char highNibble = (unsigned char)((ch >> 4) & 0xF);
    unsigned char lowNibble = (unsigned char)(ch & 0xF);

    char highNibbleChar = highNibble < 10 ? '0' + highNibble : 'A' + (highNibble - 10);
    char lowNibbleChar = lowNibble < 10 ? '0' + lowNibble : 'A' + (lowNibble - 10);

    printf("\\x");
    printf("%c", highNibbleChar);
	printf("%c", lowNibbleChar);
    break;
  }
}

void Print_token(
	Tokk_t tokk,
	int line,
	long long col,
	Byte_t * loc_begin,
	Byte_t * loc_end)
{
	// Token Kind

	printf("%d", tokk);

	// token loc

	printf(
		":%d:%lld",
		line,
		col);

#if 0
	// token text

	printf("  \"");

	for (; tok_begin < tok_end; ++tok_begin)
	{
		clean_and_print_char(*tok_begin);
	}

	printf("\" ");
#else
	(void)loc_begin;
	(void)loc_end;
#endif

	printf("\n");
}

int Len_eol(Byte_t * str)
{
	Byte_t ch = str[0];

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
	Byte_t * pChBegin,
	Byte_t * pChEnd,
	int * pCLine,
	Byte_t ** ppStartOfLine)
{
	// BUG if we care about having random access to line info, 
	//  we would need a smarter answer than InspectSpanForEol...

	int cLine = 0;
	Byte_t * pStartOfLine = NULL;

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

void Print_raw_tokens(Bytes_t bytes)
{
	// In the worst case, we will have a codepoint for every byte
	//  in the original span, so allocate enough space for that.

	//  Note that the locations array is one longer than the character array,
	//  since we want to store the begining and end of each character.
	//  for most chars, the end is the same as the beginning of the next one,
	//  except for the last one, which must have an explicit extra loc to denote its end

	size_t len_bytes = Bytes_len(bytes);

	Chars_t chars = Chars_alloc(len_bytes);
	Locs_t locs = Locs_alloc(len_bytes + 1);

	if (Chars_empty(chars) || Locs_empty(locs))
		return;

	// Decode + scrub

	Decode_utf8(bytes, &chars, &locs);

	Scrub_carriage_returns(&chars, &locs);
	Scrub_trigraphs(&chars, &locs);
	Scrub_escaped_line_breaks(&chars, &locs);

	// Keep track of line info

	Byte_t * line_start = bytes.index;
	int line = 1;

	// Lex!

	size_t i = 0;
	while (chars.index + i < chars.end)
	{
		size_t i_after;
		Tokk_t tokk;
		Lex(chars.index, Chars_len(chars), i, &i_after, &tokk);

		Byte_t * loc_begin = locs.index[i];
		Byte_t * loc_end = locs.index[i_after];

		Print_token(
			tokk,
			line,
			loc_begin - line_start + 1,
			loc_begin,
			loc_end);

		// Handle eol

		int cLine;
		Byte_t * pStartOfLine;
		InspectSpanForEol(loc_begin, loc_end, &cLine, &pStartOfLine);

		if (cLine)
		{
			line += cLine;
			line_start = pStartOfLine;
		}

		// Advance

		i = i_after;
	}
}



// main

int wmain(int argc, wchar_t *argv[])
{
	// Windows crap

	(void) _setmode(_fileno(stdin), _O_BINARY);

	// Get file path

	if (argc != 2)
	{
		printf(
			"wrong number of arguments, "
			"only expected a file path\n");

		return 1;
	}
	wchar_t * path = argv[1];

	// Read file

	Byte_t * file_bytes;
	size_t file_length;
	{
		FILE * file = _wfopen(path, L"rb");
		if (!file)
		{
			printf(
				"Failed to open file '%ls'.\n", 
				path);
			
			return 1;
		}

		// get file length 
		//  (seek to end, ftell, seek back to start)

		if (fseek(file, 0, SEEK_END))
		{
			printf(
				"fseek '%ls' SEEK_END failed.\n", 
				path);
			
			return 1;
		}

		long signed_file_length = ftell(file);
		if (signed_file_length < 0)
		{
			printf(
				"ftell '%ls' failed.\n", 
				path);
			
			return 1;
		}

		file_length = (size_t)signed_file_length;

		if (fseek(file, 0, SEEK_SET))
		{
			printf(
				"fseek '%ls' SEEK_SET failed.\n", 
				path);
			
			return 1;
		}

		// Allocate space to read file

		file_bytes = (Byte_t *)calloc(file_length, 1);
		if (!file_bytes)
		{
			printf(
				"failed to allocate %zu bytes "
				"to read '%ls'.\n", 
				file_length, 
				path);
			
			return 1;
		}

		// Actually read

		size_t bytes_read = fread(
								file_bytes, 
								1, 
								file_length,
								file);

		if (bytes_read != file_length)
		{
			printf(
				"failed to read %zu bytes from '%ls', "
				"only read %zu bytes.\n",
				file_length, 
				path,
				bytes_read);

			return 1;
		}

		// close file

		// BUG (matthewd) ignoring return value?

		fclose(file);
	}

	// Deal with potential UTF-8 BOM

	if (file_length >= 3 &&
		file_bytes[0] == '\xEF' &&
		file_bytes[1] == '\xBB' &&
		file_bytes[2] == '\xBF')
	{
		file_bytes += 3;
		file_length -= 3;
	}

	// Print tokens

	Bytes_t bytes;
	bytes.index = file_bytes;
	bytes.end = file_bytes + file_length;

	Print_raw_tokens(bytes);
}