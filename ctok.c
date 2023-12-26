
#include <assert.h>
#include <fcntl.h>
#include <io.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <uchar.h>



// Some basic stuff

#define LEN_ARY(x) ((sizeof(x)/sizeof(0[(x)])) / ((size_t)(!(sizeof(x) % sizeof(0[(x)])))))

typedef unsigned char byte_t;



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

DECL_SPAN(bytes, byte_t);
DECL_SPAN(chars, char32_t);
DECL_SPAN(locs, byte_t *);

char32_t chars_lookup_safe(chars_t chars, size_t index)
{
	if (!chars_index_valid(chars, index))
		return U'\0';

	return chars.index[index];
}



// utf8

int Leading_ones(byte_t byte)
{
	int count = 0;
	for (byte_t mask = (1 << 7); mask; mask >>= 1)
	{
		if (!(byte & mask))
			break;

		++count;
	}

	return count;
}

bool Try_decode_utf8(
	bytes_t bytes,
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

	size_t len_bytes = bytes_len(bytes);
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

bool Is_hz_ws(char32_t ch)
{
	return ch == ' ' || ch == '\t' || ch == '\f' || ch == '\v';
}

bool Is_ws(char32_t ch)
{
	return Is_hz_ws(ch) || ch == '\n' || ch == '\r';
}

size_t After_escaped_end_of_line_(
	const char32_t * chars,
	size_t count,
	size_t i)
{
	// Note : returning i implies 'there was no esc eol'

	if (i >= count)
		return i;

	if (chars[i] != '\\')
		return i;

	size_t i_peek = i + 1;
	while (i_peek < count && Is_hz_ws(chars[i_peek]))
	{
		++i_peek;
	}

	if (i_peek >= count)
		return i;

	if (chars[i_peek] == '\n')
	{
		return i_peek + 1;
	}

	if (chars[i_peek] == '\r')
	{
		// BB (matthewd) this is currently never hit, since we scrub
		//  away \r before we call this

		++i_peek;

		if (i_peek < count && chars[i_peek] == '\n')
			return i_peek + 1;

		return i_peek;
	}

	return i;
}

size_t After_escaped_end_of_lines_(
	const char32_t * chars,
	size_t count,
	size_t i)
{
	while (i < count)
	{
		size_t after_esc_eol = After_escaped_end_of_line_(chars, count, i);
		if (after_esc_eol == i)
			break;

		i = after_esc_eol;
	}

	return i;
}

void Move_codepoint_and_locs_(
	char32_t * chars, 
	byte_t ** locs,
	char32_t ch,
	size_t i_from_next,
	size_t * i_from_ref,
	size_t * i_to_ref)
{
	size_t i_from = *i_from_ref;
	size_t i_to = *i_to_ref;

	chars[i_to] = ch;
	locs[i_to] = locs[i_from];

	i_from = i_from_next;
	++i_to;

	locs[i_to] = locs[i_from];

	*i_from_ref = i_from;
	*i_to_ref = i_to;
}

size_t Try_decode_logical_codepoints_(
	bytes_t bytes,
	chars_t * chars_out,
	locs_t * locs_out)
{
	// In the worst case, we will have a codepoint for every byte
	//  in the original span, so allocate enough space for that.

	//  Note that the locations array is one longer than the codepoint array,
	//  since we want to store the begining and end of each codepoint.
	//  for most codepoints, the end is the same as the beggining of the next one,
	//  except for the last one, which must have an explicit extra loc to denote its end

	size_t len_bytes = bytes_len(bytes);

	chars_t chars = chars_alloc(len_bytes);
	locs_t locs = locs_alloc(len_bytes + 1);

	if (chars_empty(chars) || locs_empty(locs))
		return 0;

	// The start of the first char will be the start of the bytes

	locs.index[0] = bytes.index;

	// Chew through the byte span with Try_decode_utf8

	size_t count_chars = 0;
	while (!bytes_empty(bytes))
	{
		char32_t ch;
		size_t len_ch;
		if (Try_decode_utf8(bytes, &ch, &len_ch))
		{
			chars.index[count_chars] = ch;
			locs.index[count_chars + 1] = bytes.index + len_ch;
			bytes.index += len_ch;
		}
		else
		{
			chars.index[count_chars] = UINT32_MAX;
			locs.index[count_chars + 1] = bytes.index + 1;
			++bytes.index;
		}

		++count_chars;
	}

	// \r and \r\n to \n

	size_t i_from = 0;
	size_t i_to = 0;
	
	while (i_from < count_chars)
	{
		char32_t ch = chars.index[i_from];
		bool is_cr = ch == '\r';
		if (is_cr)
			ch = '\n';

		size_t i_from_next = i_from + 1;
		if (is_cr && (i_from + 1 < count_chars) && chars.index[i_from + 1] == '\n')
		{
			i_from_next = i_from + 2;
		}

		char chr = (char)ch;
		(void)chr;
		Move_codepoint_and_locs_(
			chars.index,
			locs.index,
			ch,
			i_from_next,
			&i_from,
			&i_to);
	}
	count_chars = i_to;
	
	// trigraphs

	i_from = 0;
	i_to = 0;

	while (i_from < count_chars)
	{
		if (i_from + 2 < count_chars && 
			chars.index[i_from] == '?' && 
			chars.index[i_from + 1] == '?')
		{
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

			int iPairMatch = -1;

			for (int iPair = 0; iPair < LEN_ARY(pairs); ++iPair)
			{
				char32_t * pair = pairs[iPair];
				if (pair[0] == chars.index[i_from + 2])
				{
					iPairMatch = iPair;
					break;
				}
			}

			if (iPairMatch != -1)
			{
				Move_codepoint_and_locs_(
					chars.index,
					locs.index,
					pairs[iPairMatch][1],
					i_from + 3,
					&i_from,
					&i_to);

				continue;
			}
		}

		Move_codepoint_and_locs_(
			chars.index,
			locs.index,
			chars.index[i_from],
			i_from + 1,
			&i_from,
			&i_to);
	}
	count_chars = i_to;
	
	// escaped line breaks

	i_from = 0;
	i_to = 0;
	
	while (i_from < count_chars)
	{
		size_t after_esc_eol = After_escaped_end_of_lines_(chars.index, chars_len(chars), i_from);
		if (after_esc_eol == i_from)
		{
			Move_codepoint_and_locs_(
				chars.index,
				locs.index,
				chars.index[i_from],
				i_from + 1,
				&i_from,
				&i_to);
		}
		else if (after_esc_eol == count_chars)
		{
			// Drop trailing escaped line break
			//  (do not include it in a character)

			i_from = count_chars;
		}
		else
		{
			Move_codepoint_and_locs_(
				chars.index,
				locs.index,
				chars.index[after_esc_eol],
				after_esc_eol + 1,
				&i_from,
				&i_to);
		}
	}
	count_chars = i_to;
	chars.end = chars.index + count_chars;
	locs.end = locs.index + count_chars + 1;
	
	// return chars + locs

	*chars_out = chars;
	*locs_out = locs;
	
	return count_chars;
}



// Lex

typedef enum TokenKind
{
	TokenKind_unknown = 0,
	TokenKind_eof = 1,
	TokenKind_eod = 2,
	TokenKind_code_completion = 3,
	TokenKind_comment = 4,
	TokenKind_identifier = 5,
	TokenKind_raw_identifier = 6,
	TokenKind_numeric_constant = 7,
	TokenKind_char_constant = 8,
	TokenKind_wide_char_constant = 9,
	TokenKind_utf8_char_constant = 10,
	TokenKind_utf16_char_constant = 11,
	TokenKind_utf32_char_constant = 12,
	TokenKind_string_literal = 13,
	TokenKind_wide_string_literal = 14,
	TokenKind_header_name = 15,
	TokenKind_utf8_string_literal = 16,
	TokenKind_utf16_string_literal = 17,
	TokenKind_utf32_string_literal = 18,
	TokenKind_l_square = 19,
	TokenKind_r_square = 20,
	TokenKind_l_paren = 21,
	TokenKind_r_paren = 22,
	TokenKind_l_brace = 23,
	TokenKind_r_brace = 24,
	TokenKind_period = 25,
	TokenKind_ellipsis = 26,
	TokenKind_amp = 27,
	TokenKind_ampamp = 28,
	TokenKind_ampequal = 29,
	TokenKind_star = 30,
	TokenKind_starequal = 31,
	TokenKind_plus = 32,
	TokenKind_plusplus = 33,
	TokenKind_plusequal = 34,
	TokenKind_minus = 35,
	TokenKind_arrow = 36,
	TokenKind_minusminus = 37,
	TokenKind_minusequal  = 38,
	TokenKind_tilde = 39,
	TokenKind_exclaim = 40,
	TokenKind_exclaimequal = 41,
	TokenKind_slash = 42,
	TokenKind_slashequal = 43,
	TokenKind_percent = 44,
	TokenKind_percentequal = 45,
	TokenKind_less = 46,
	TokenKind_lessless = 47,
	TokenKind_lessequal = 48,
	TokenKind_lesslessequal = 49,
	TokenKind_spaceship = 50,
	TokenKind_greater = 51,
	TokenKind_greatergreater = 52,
	TokenKind_greaterequal = 53,
	TokenKind_greatergreaterequal = 54,
	TokenKind_caret = 55,
	TokenKind_caretequal = 56,
	TokenKind_pipe = 57,
	TokenKind_pipepipe = 58,
	TokenKind_pipeequal = 59,
	TokenKind_question = 60,
	TokenKind_colon = 61,
	TokenKind_semi = 62,
	TokenKind_equal = 63,
	TokenKind_equalequal = 64,
	TokenKind_comma = 65,
	TokenKind_hash = 66,
	TokenKind_hashhash = 67,
	TokenKind_hashat = 68,
	TokenKind_periodstar = 69,
	TokenKind_arrowstar = 70,
	TokenKind_coloncolon = 71,
	TokenKind_at = 72,
	TokenKind_lesslessless = 73,
	TokenKind_greatergreatergreater = 74,
	TokenKind_caretcaret = 75,
	
	TokenKind_kw_auto = 76,
	TokenKind_kw_break = 77,
	TokenKind_kw_case = 78,
	TokenKind_kw_char = 79,
	TokenKind_kw_const = 80,
	TokenKind_kw_continue = 81,
	TokenKind_kw_default = 82,
	TokenKind_kw_do = 83,
	TokenKind_kw_double = 84,
	TokenKind_kw_else = 85,
	TokenKind_kw_enum = 86,
	TokenKind_kw_extern = 87,
	TokenKind_kw_float = 88,
	TokenKind_kw_for = 89,
	TokenKind_kw_goto = 90,
	TokenKind_kw_if = 91,
	TokenKind_kw_int = 92,
	TokenKind_kw__ExtInt = 93,
	TokenKind_kw__BitInt = 94,
	TokenKind_kw_long = 95,
	TokenKind_kw_register = 96,
	TokenKind_kw_return = 97,
	TokenKind_kw_short = 98,
	TokenKind_kw_signed = 99,
	TokenKind_kw_sizeof = 100,
	TokenKind_kw_static = 101,
	TokenKind_kw_struct = 102,
	TokenKind_kw_switch = 103,
	TokenKind_kw_typedef = 104,
	TokenKind_kw_union = 105,
	TokenKind_kw_unsigned = 106,
	TokenKind_kw_void = 107,
	TokenKind_kw_volatile = 108,
	TokenKind_kw_while = 109,
	TokenKind_kw__Alignas = 110,
	TokenKind_kw__Alignof = 111,
	TokenKind_kw__Atomic = 112,
	TokenKind_kw__Bool = 113,
	TokenKind_kw__Complex = 114,
	TokenKind_kw__Generic = 115,
	TokenKind_kw__Imaginary = 116,
	TokenKind_kw__Noreturn = 117,
	TokenKind_kw__Static_assert = 118,
	TokenKind_kw__Thread_local = 119,
	TokenKind_kw___func__ = 120,
	TokenKind_kw___objc_yes = 121,
	TokenKind_kw___objc_no = 122,
	TokenKind_kw_asm = 123,
	TokenKind_kw_bool = 124,
	TokenKind_kw_catch = 125,
	TokenKind_kw_class = 126,
	TokenKind_kw_const_cast = 127,
	TokenKind_kw_delete = 128,
	TokenKind_kw_dynamic_cast = 129,
	TokenKind_kw_explicit = 130,
	TokenKind_kw_export = 131,
	TokenKind_kw_false = 132,
	TokenKind_kw_friend = 133,
	TokenKind_kw_mutable = 134,
	TokenKind_kw_namespace = 135,
	TokenKind_kw_new = 136,
	TokenKind_kw_operator = 137,
	TokenKind_kw_private = 138,
	TokenKind_kw_protected = 139,
	TokenKind_kw_public = 140,
	TokenKind_kw_reinterpret_cast = 141,
	TokenKind_kw_static_cast = 142,
	TokenKind_kw_template = 143,
	TokenKind_kw_this = 144,
	TokenKind_kw_throw = 145,
	TokenKind_kw_true = 146,
	TokenKind_kw_try = 147,
	TokenKind_kw_typename = 148,
	TokenKind_kw_typeid = 149,
	TokenKind_kw_using = 150,
	TokenKind_kw_virtual = 151,
	TokenKind_kw_wchar_t = 152,
	TokenKind_kw_restrict = 153,
	TokenKind_kw_inline = 154,
	TokenKind_kw_alignas = 155,
	TokenKind_kw_alignof = 156,
	TokenKind_kw_char16_t = 157,
	TokenKind_kw_char32_t = 158,
	TokenKind_kw_constexpr = 159,
	TokenKind_kw_decltype = 160,
	TokenKind_kw_noexcept = 161,
	TokenKind_kw_nullptr = 162,
	TokenKind_kw_static_assert = 163,
	TokenKind_kw_thread_local = 164,
	TokenKind_kw_co_await = 165,
	TokenKind_kw_co_return = 166,
	TokenKind_kw_co_yield = 167,
	TokenKind_kw_module = 168,
	TokenKind_kw_import = 169,
	TokenKind_kw_consteval = 170,
	TokenKind_kw_constinit = 171,
	TokenKind_kw_concept = 172,
	TokenKind_kw_requires = 173,
	TokenKind_kw_char8_t = 174,
	TokenKind_kw__Float16 = 175,
	TokenKind_kw_typeof = 176,
	TokenKind_kw_typeof_unqual = 177,
	TokenKind_kw__Accum = 178,
	TokenKind_kw__Fract = 179,
	TokenKind_kw__Sat = 180,
	TokenKind_kw__Decimal32 = 181,
	TokenKind_kw__Decimal64 = 182,
	TokenKind_kw__Decimal128 = 183,
	TokenKind_kw___null = 184,
	TokenKind_kw___alignof = 185,
	TokenKind_kw___attribute = 186,
	TokenKind_kw___builtin_choose_expr = 187,
	TokenKind_kw___builtin_offsetof = 188,
	TokenKind_kw___builtin_FILE = 189,
	TokenKind_kw___builtin_FILE_NAME = 190,
	TokenKind_kw___builtin_FUNCTION = 191,
	TokenKind_kw___builtin_FUNCSIG = 192,
	TokenKind_kw___builtin_LINE = 193,
	TokenKind_kw___builtin_COLUMN = 194,
	TokenKind_kw___builtin_source_location = 195,
	TokenKind_kw___builtin_types_compatible_p = 196,
	TokenKind_kw___builtin_va_arg = 197,
	TokenKind_kw___extension__ = 198,
	TokenKind_kw___float128 = 199,
	TokenKind_kw___ibm128 = 200,
	TokenKind_kw___imag = 201,
	TokenKind_kw___int128 = 202,
	TokenKind_kw___label__ = 203,
	TokenKind_kw___real = 204,
	TokenKind_kw___thread = 205,
	TokenKind_kw___FUNCTION__ = 206,
	TokenKind_kw___PRETTY_FUNCTION__ = 207,
	TokenKind_kw___auto_type = 208,
	TokenKind_kw___FUNCDNAME__ = 209,
	TokenKind_kw___FUNCSIG__ = 210,
	TokenKind_kw_L__FUNCTION__ = 211,
	TokenKind_kw_L__FUNCSIG__ = 212,
	TokenKind_kw___is_interface_class = 213,
	TokenKind_kw___is_sealed = 214,
	TokenKind_kw___is_destructible = 215,
	TokenKind_kw___is_trivially_destructible = 216,
	TokenKind_kw___is_nothrow_destructible = 217,
	TokenKind_kw___is_nothrow_assignable = 218,
	TokenKind_kw___is_constructible = 219,
	TokenKind_kw___is_nothrow_constructible = 220,
	TokenKind_kw___is_assignable = 221,
	TokenKind_kw___has_nothrow_move_assign = 222,
	TokenKind_kw___has_trivial_move_assign = 223,
	TokenKind_kw___has_trivial_move_constructor = 224,
	TokenKind_kw___has_nothrow_assign = 225,
	TokenKind_kw___has_nothrow_copy = 226,
	TokenKind_kw___has_nothrow_constructor = 227,
	TokenKind_kw___has_trivial_assign = 228,
	TokenKind_kw___has_trivial_copy = 229,
	TokenKind_kw___has_trivial_constructor = 230,
	TokenKind_kw___has_trivial_destructor = 231,
	TokenKind_kw___has_virtual_destructor = 232,
	TokenKind_kw___is_abstract = 233,
	TokenKind_kw___is_aggregate = 234,
	TokenKind_kw___is_base_of = 235,
	TokenKind_kw___is_class = 236,
	TokenKind_kw___is_convertible_to = 237,
	TokenKind_kw___is_empty = 238,
	TokenKind_kw___is_enum = 239,
	TokenKind_kw___is_final = 240,
	TokenKind_kw___is_literal = 241,
	TokenKind_kw___is_pod = 242,
	TokenKind_kw___is_polymorphic = 243,
	TokenKind_kw___is_standard_layout = 244,
	TokenKind_kw___is_trivial = 245,
	TokenKind_kw___is_trivially_assignable = 246,
	TokenKind_kw___is_trivially_constructible = 247,
	TokenKind_kw___is_trivially_copyable = 248,
	TokenKind_kw___is_union = 249,
	TokenKind_kw___has_unique_object_representations = 250,
	TokenKind_kw___add_lvalue_reference = 251,
	TokenKind_kw___add_pointer = 252,
	TokenKind_kw___add_rvalue_reference = 253,
	TokenKind_kw___decay = 254,
	TokenKind_kw___make_signed = 255,
	TokenKind_kw___make_unsigned = 256,
	TokenKind_kw___remove_all_extents = 257,
	TokenKind_kw___remove_const = 258,
	TokenKind_kw___remove_cv = 259,
	TokenKind_kw___remove_cvref = 260,
	TokenKind_kw___remove_extent = 261,
	TokenKind_kw___remove_pointer = 262,
	TokenKind_kw___remove_reference_t = 263,
	TokenKind_kw___remove_restrict = 264,
	TokenKind_kw___remove_volatile = 265,
	TokenKind_kw___underlying_type = 266,
	TokenKind_kw___is_trivially_relocatable = 267,
	TokenKind_kw___is_trivially_equality_comparable = 268,
	TokenKind_kw___is_bounded_array = 269,
	TokenKind_kw___is_unbounded_array = 270,
	TokenKind_kw___is_nullptr = 271,
	TokenKind_kw___is_scoped_enum = 272,
	TokenKind_kw___is_referenceable = 273,
	TokenKind_kw___can_pass_in_regs = 274,
	TokenKind_kw___reference_binds_to_temporary = 275,
	TokenKind_kw___is_lvalue_expr = 276,
	TokenKind_kw___is_rvalue_expr = 277,
	TokenKind_kw___is_arithmetic = 278,
	TokenKind_kw___is_floating_point = 279,
	TokenKind_kw___is_integral = 280,
	TokenKind_kw___is_complete_type = 281,
	TokenKind_kw___is_void = 282,
	TokenKind_kw___is_array = 283,
	TokenKind_kw___is_function = 284,
	TokenKind_kw___is_reference = 285,
	TokenKind_kw___is_lvalue_reference = 286,
	TokenKind_kw___is_rvalue_reference = 287,
	TokenKind_kw___is_fundamental = 288,
	TokenKind_kw___is_object = 289,
	TokenKind_kw___is_scalar = 290,
	TokenKind_kw___is_compound = 291,
	TokenKind_kw___is_pointer = 292,
	TokenKind_kw___is_member_object_pointer = 293,
	TokenKind_kw___is_member_function_pointer = 294,
	TokenKind_kw___is_member_pointer = 295,
	TokenKind_kw___is_const = 296,
	TokenKind_kw___is_volatile = 297,
	TokenKind_kw___is_signed = 298,
	TokenKind_kw___is_unsigned = 299,
	TokenKind_kw___is_same = 300,
	TokenKind_kw___is_convertible = 301,
	TokenKind_kw___array_rank = 302,
	TokenKind_kw___array_extent = 303,
	TokenKind_kw___private_extern__ = 304,
	TokenKind_kw___module_private__ = 305,
	TokenKind_kw___declspec = 306,
	TokenKind_kw___cdecl = 307,
	TokenKind_kw___stdcall = 308,
	TokenKind_kw___fastcall = 309,
	TokenKind_kw___thiscall = 310,
	TokenKind_kw___regcall = 311,
	TokenKind_kw___vectorcall = 312,
	TokenKind_kw___forceinline = 313,
	TokenKind_kw___unaligned = 314,
	TokenKind_kw___super = 315,
	TokenKind_kw___global = 316,
	TokenKind_kw___local = 317,
	TokenKind_kw___constant = 318,
	TokenKind_kw___private = 319,
	TokenKind_kw___generic = 320,
	TokenKind_kw___kernel = 321,
	TokenKind_kw___read_only = 322,
	TokenKind_kw___write_only = 323,
	TokenKind_kw___read_write = 324,
	TokenKind_kw___builtin_astype = 325,
	TokenKind_kw_vec_step = 326,
	TokenKind_kw_image1d_t = 327,
	TokenKind_kw_image1d_array_t = 328,
	TokenKind_kw_image1d_buffer_t = 329,
	TokenKind_kw_image2d_t = 330,
	TokenKind_kw_image2d_array_t = 331,
	TokenKind_kw_image2d_depth_t = 332,
	TokenKind_kw_image2d_array_depth_t = 333,
	TokenKind_kw_image2d_msaa_t = 334,
	TokenKind_kw_image2d_array_msaa_t = 335,
	TokenKind_kw_image2d_msaa_depth_t = 336,
	TokenKind_kw_image2d_array_msaa_depth_t = 337,
	TokenKind_kw_image3d_t = 338,
	TokenKind_kw_pipe = 339,
	TokenKind_kw_addrspace_cast = 340,
	TokenKind_kw___noinline__ = 341,
	TokenKind_kw_cbuffer = 342,
	TokenKind_kw_tbuffer = 343,
	TokenKind_kw_groupshared = 344,
	TokenKind_kw___builtin_omp_required_simd_align = 345,
	TokenKind_kw___pascal = 346,
	TokenKind_kw___vector = 347,
	TokenKind_kw___pixel = 348,
	TokenKind_kw___bool = 349,
	TokenKind_kw___bf16 = 350,
	TokenKind_kw_half = 351,
	TokenKind_kw___bridge = 352,
	TokenKind_kw___bridge_transfer = 353,
	TokenKind_kw___bridge_retained = 354,
	TokenKind_kw___bridge_retain = 355,
	TokenKind_kw___covariant = 356,
	TokenKind_kw___contravariant = 357,
	TokenKind_kw___kindof = 358,
	TokenKind_kw__Nonnull = 359,
	TokenKind_kw__Nullable = 360,
	TokenKind_kw__Nullable_result = 361,
	TokenKind_kw__Null_unspecified = 362,
	TokenKind_kw___funcref = 363,
	TokenKind_kw___ptr64 = 364,
	TokenKind_kw___ptr32 = 365,
	TokenKind_kw___sptr = 366,
	TokenKind_kw___uptr = 367,
	TokenKind_kw___w64 = 368,
	TokenKind_kw___uuidof = 369,
	TokenKind_kw___try = 370,
	TokenKind_kw___finally = 371,
	TokenKind_kw___leave = 372,
	TokenKind_kw___int64 = 373,
	TokenKind_kw___if_exists = 374,
	TokenKind_kw___if_not_exists = 375,
	TokenKind_kw___single_inheritance = 376,
	TokenKind_kw___multiple_inheritance = 377,
	TokenKind_kw___virtual_inheritance = 378,
	TokenKind_kw___interface = 379,
	TokenKind_kw___builtin_convertvector = 380,
	TokenKind_kw___builtin_bit_cast = 381,
	TokenKind_kw___builtin_available = 382,
	TokenKind_kw___builtin_sycl_unique_stable_name = 383,
	TokenKind_kw___arm_streaming = 384,
	TokenKind_kw___unknown_anytype = 385,

	TokenKind_annot_cxxscope = 386,
	TokenKind_annot_typename = 387,
	TokenKind_annot_template_id = 388,
	TokenKind_annot_non_type = 389,
	TokenKind_annot_non_type_undeclared = 390,
	TokenKind_annot_non_type_dependent = 391,
	TokenKind_annot_overload_set = 392,
	TokenKind_annot_primary_expr = 393,
	TokenKind_annot_decltype = 394,
	TokenKind_annot_pragma_unused = 395,
	TokenKind_annot_pragma_vis = 396,
	TokenKind_annot_pragma_pack = 397,
	TokenKind_annot_pragma_parser_crash = 398,
	TokenKind_annot_pragma_captured = 399,
	TokenKind_annot_pragma_dump = 400,
	TokenKind_annot_pragma_msstruct = 401,
	TokenKind_annot_pragma_align = 402,
	TokenKind_annot_pragma_weak = 403,
	TokenKind_annot_pragma_weakalias = 404,
	TokenKind_annot_pragma_redefine_extname = 405,
	TokenKind_annot_pragma_fp_contract = 406,
	TokenKind_annot_pragma_fenv_access = 407,
	TokenKind_annot_pragma_fenv_access_ms = 408,
	TokenKind_annot_pragma_fenv_round = 409,
	TokenKind_annot_pragma_float_control = 410,
	TokenKind_annot_pragma_ms_pointers_to_members = 411,
	TokenKind_annot_pragma_ms_vtordisp = 412,
	TokenKind_annot_pragma_ms_pragma = 413,
	TokenKind_annot_pragma_opencl_extension = 414,
	TokenKind_annot_attr_openmp = 415,
	TokenKind_annot_pragma_openmp = 416,
	TokenKind_annot_pragma_openmp_end = 417,
	TokenKind_annot_pragma_loop_hint = 418,
	TokenKind_annot_pragma_fp = 419,
	TokenKind_annot_pragma_attribute = 420,
	TokenKind_annot_pragma_riscv = 421,
	TokenKind_annot_module_include = 422,
	TokenKind_annot_module_begin = 423,
	TokenKind_annot_module_end = 424,
	TokenKind_annot_header_unit = 425,
	TokenKind_annot_repl_input_end = 426,
} TokenKind;

void Lex_punctuation(
	const char32_t * chars,
	size_t count_chars,
	size_t i,
	TokenKind * tokk_out,
	size_t * i_after_out)
{
	typedef struct
	{
		const char * str;
		TokenKind tokk;
		int _padding;
	} punctution_t;

	// "::" is included to match clang
	// https://github.com/llvm/llvm-project/commit/874217f99b99ab3c9026dc3b7bd84cd2beebde6e

	static punctution_t puctuations[] =
	{
		{"%:%:", TokenKind_hashhash},
		{">>=", TokenKind_greatergreaterequal},
		{"<<=", TokenKind_lesslessequal},
		{"...", TokenKind_ellipsis},
		{"|=", TokenKind_pipeequal},
		{"||", TokenKind_pipepipe},
		{"^=", TokenKind_caretequal},
		{"==", TokenKind_equalequal},
		{"::", TokenKind_coloncolon},
		{":>", TokenKind_r_square},
		{"-=", TokenKind_minusequal},
		{"--", TokenKind_minusminus},
		{"->", TokenKind_arrow},
		{"+=", TokenKind_plusequal},
		{"++", TokenKind_plusplus},
		{"*=", TokenKind_starequal},
		{"&=", TokenKind_ampequal},
		{"&&", TokenKind_ampamp},
		{"##", TokenKind_hashhash},
		{"!=", TokenKind_exclaimequal},
		{">=", TokenKind_greaterequal},
		{">>", TokenKind_greatergreater},
		{"<=", TokenKind_lessequal},
		{"<:", TokenKind_l_square},
		{"<%", TokenKind_l_brace},
		{"<<", TokenKind_lessless},
		{"%>", TokenKind_r_brace},
		{"%=", TokenKind_percentequal},
		{"%:", TokenKind_hash},
		{"/=", TokenKind_slashequal},
		{"~", TokenKind_tilde},
		{"}", TokenKind_r_brace},
		{"{", TokenKind_l_brace},
		{"]", TokenKind_r_square},
		{"[", TokenKind_l_square},
		{"?", TokenKind_question},
		{";", TokenKind_semi},
		{",", TokenKind_comma},
		{")", TokenKind_r_paren},
		{"(", TokenKind_l_paren},
		{"|", TokenKind_pipe},
		{"^", TokenKind_caret},
		{"=", TokenKind_equal},
		{":", TokenKind_colon},
		{"-", TokenKind_minus},
		{"+", TokenKind_plus},
		{"*", TokenKind_star},
		{"&", TokenKind_amp},
		{"#", TokenKind_hash},
		{"!", TokenKind_exclaim},
		{">", TokenKind_greater},
		{"<", TokenKind_less},
		{"%", TokenKind_percent},
		{".", TokenKind_period},
		{"/", TokenKind_slash},
	};

	for (int i_puctuation = 0; i_puctuation < LEN_ARY(puctuations); ++i_puctuation)
	{
		punctution_t punctuation = puctuations[i_puctuation];
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
	*tokk_out = TokenKind_unknown;
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

	for (int i = 0; i < LEN_ARY(no); ++i)
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

	for (int i = 0; i < LEN_ARY(yes); ++i)
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

	for (int i = 0; i < LEN_ARY(ws); ++i)
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
	TokenKind * tokk_out)
{
	TokenKind tokk = TokenKind_unknown;

	while (i < count_chars)
	{
		char32_t ch0 = chars[i];
		++i;

		if (i < count_chars)
		{
			char32_t ch1 = chars[i];
			if (ch0 == '*' && ch1 == '/')
			{
				tokk = TokenKind_comment;
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
	TokenKind * tokk_out)
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

		*tokk_out = (valid) ? TokenKind_utf8_string_literal : TokenKind_unknown;
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
			*tokk_out = TokenKind_unknown;
			return;
		}
		
		TokenKind tokk;
		switch (ch_0)
		{
		case 'u':
			tokk = (ch_1 == '"') ? TokenKind_utf16_string_literal : TokenKind_utf16_char_constant;
			break;
		case 'U':
			tokk = (ch_1 == '"') ? TokenKind_utf32_string_literal : TokenKind_utf32_char_constant;
			break;
		default: // 'L'
			tokk = (ch_1 == '"') ? TokenKind_wide_string_literal : TokenKind_wide_char_constant;
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
			*tokk_out = TokenKind_unknown;
			return;
		}

		*tokk_out = (ch_0 == '"') ? TokenKind_string_literal : TokenKind_char_constant;
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
		*tokk_out = TokenKind_comment;
	}
	else if (ch_0 == '.' && ch_1 >= '0' && ch_1 <= '9')
	{
		i += 2;
		*i_after_out = After_rest_of_ppnum(chars, count_chars, i);
		*tokk_out = TokenKind_numeric_constant;
	}
	else if (Starts_id(ch_0))
	{
		++i;
		*i_after_out = After_rest_of_id(chars, count_chars, i);
		*tokk_out = TokenKind_raw_identifier;
	}
	else if (ch_0 >= '0' && ch_0 <= '9')
	{
		++i;
		*i_after_out = After_rest_of_ppnum(chars, count_chars, i);
		*tokk_out = TokenKind_numeric_constant;
	}
	else if (Is_ws(ch_0) || ch_0 == '\0')
	{
		++i;
		*i_after_out = After_whitespace(chars, count_chars, i);
		*tokk_out = TokenKind_unknown;
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
				*tokk_out = TokenKind_raw_identifier;
			}
			else
			{
				// Bogus UCN, return it as an unknown token

				*i_after_out = i_after;
				*tokk_out = TokenKind_unknown;
			}
		}
		else
		{
			// Stray backslash, return as unknown token
			
			*i_after_out = i + 1;
			*tokk_out = TokenKind_unknown;
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
	TokenKind tokk,
	int line,
	long long col,
	byte_t * loc_begin,
	byte_t * loc_end)
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

int Len_eol(byte_t * str)
{
	byte_t ch = str[0];

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
	byte_t * pChBegin,
	byte_t * pChEnd,
	int * pCLine,
	byte_t ** ppStartOfLine)
{
	// BUG if we care about having random access to line info, 
	//  we would need a smarter answer than InspectSpanForEol...

	int cLine = 0;
	byte_t * pStartOfLine = NULL;

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

void PrintRawTokens(bytes_t bytes)
{
	// Munch bytes to logical characters

	chars_t chars;
	locs_t locs;
	size_t count_chars = Try_decode_logical_codepoints_(bytes, &chars, &locs);
	if (!count_chars)
		return;

	//

	for (size_t i = 0; i < count_chars; ++i)
	{
		assert(locs.index[i + 1] > locs.index[i + 0]);
	}

	// Keep track of line info

	byte_t * line_start = bytes.index;
	int line = 1;

	// Lex!

	size_t i = 0;
	while (i < count_chars)
	{
		size_t i_after;
		TokenKind tokk;
		Lex(chars.index, chars_len(chars), i, &i_after, &tokk);

		byte_t * loc_begin = locs.index[i];
		byte_t * loc_end = locs.index[i_after];

		Print_token(
			tokk,
			line,
			loc_begin - line_start + 1,
			loc_begin,
			loc_end);

		// Handle eol

		int cLine;
		byte_t * pStartOfLine;
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

	byte_t * file_bytes;
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

		file_bytes = (byte_t *)calloc(file_length, 1);
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

	bytes_t bytes;
	bytes.index = file_bytes;
	bytes.end = file_bytes + file_length;

	PrintRawTokens(bytes);
}