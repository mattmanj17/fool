
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

int Len_escaped_end_of_line(
	const lcp_t * begin,
	const lcp_t * end)
{
	if (end <= begin)
		return 0;

	if (begin->cp != '\\')
		return 0;

	++begin;

	int len = 1;
	while (begin < end && Is_hz_ws(begin->cp))
	{
		++len;
		++begin;
	}

	if (end <= begin)
		return 0;

	if (begin->cp == '\n')
	{
		return len + 1;
	}

	if (begin->cp == '\r')
	{
		// BB (matthewd) this is currently never hit, since we scrub
		//  away \r before we call this

		++begin;

		if (end <= begin && begin->cp == '\n')
			return len + 2;

		return len + 1;
	}

	return 0;
}

int Len_escaped_end_of_lines(
	const lcp_t * begin,
	const lcp_t * end)
{
	int len = 0;

	while (begin < end)
	{
		int len_esc_eol = Len_escaped_end_of_line(begin, end);
		if (len_esc_eol == 0)
			break;

		len += len_esc_eol;
		begin += len_esc_eol;
	}

	return len;
}

lcp_t * Try_decode_logical_codepoints(
	const uint8_t * bytes,
	const uint8_t * bytes_end,
	int * len_lcps_ref)
{
	// In the worst case, we will have a codepoint for every byte
	//  in the original span, so allocate enough space for that.

	size_t len_bytes;
	{
		int64_t len_bytes_signed = bytes_end - bytes;
		if (len_bytes_signed <= 0)
			return NULL;

		len_bytes = (size_t)len_bytes_signed;
	}

	lcp_t * lcps = (lcp_t *)calloc(sizeof(lcp_t) * len_bytes, 1);
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

	// \r and \r\n to \n

	lcp_t * lcps_end = lcps + len_lcps;

	lcp_t * lcps_from = lcps;
	lcp_t * lcps_to = lcps;
	
	while (lcps_from < lcps_end)
	{
		if (lcps_from[0].cp == '\r')
		{
			lcps_to->cp = '\n';
			lcps_to->bytes = lcps_from->bytes;
			++lcps_to;

			if ((lcps_from + 1) < lcps_end &&
				lcps_from[1].cp == '\n')
			{
				lcps_from += 2;
			}
			else
			{
				++lcps_from;
			}
		}
		else
		{
			lcps_to->cp = lcps_from->cp;
			lcps_to->bytes = lcps_from->bytes;
			++lcps_to;
			++lcps_from;
		}
	}

	lcps_end = lcps_to;
	lcps_from = lcps;
	lcps_to = lcps;
	
	// trigraphs

	while (lcps_from < lcps_end)
	{
		if ((lcps_from + 2) < lcps_end && 
			lcps_from[0].cp == '?' && 
			lcps_from[1].cp == '?')
		{
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

			int iPairMatch = -1;

			for (int iPair = 0; iPair < LEN(pairs); ++iPair)
			{
				uint32_t * pair = pairs[iPair];
				if (pair[0] == lcps_from[2].cp)
				{
					iPairMatch = iPair;
					break;
				}
			}

			if (iPairMatch != -1)
			{
				lcps_to->cp = pairs[iPairMatch][1];
				lcps_to->bytes = lcps_from->bytes;
				++lcps_to;
				lcps_from += 3;

				continue;
			}
		}

		lcps_to->cp = lcps_from->cp;
		lcps_to->bytes = lcps_from->bytes;
		++lcps_to;
		++lcps_from;
	}

	lcps_end = lcps_to;
	lcps_from = lcps;
	lcps_to = lcps;
	
	// escaped line breaks
	
	while (lcps_from < lcps_end)
	{
		int len_esc_eol = Len_escaped_end_of_lines(lcps_from, lcps_end);
		if (!len_esc_eol)
		{
			lcps_to->cp = lcps_from->cp;
			lcps_to->bytes = lcps_from->bytes;
			++lcps_to;
			++lcps_from;
		}
		else if ((lcps_from + len_esc_eol) == lcps_end)
		{
			// Drop trailing escaped line break

			lcps_from = lcps_end;
			bytes_end -= len_esc_eol;
		}
		else
		{
			lcps_to->cp = lcps_from[len_esc_eol].cp;
			lcps_to->bytes = lcps_from->bytes;
			++lcps_to;
			lcps_from += len_esc_eol + 1;
		}
	}
	
	// return len and lcps

	lcps_end = lcps_to;
	len_lcps = (int)(lcps_end - lcps);

	*len_lcps_ref = len_lcps;
	return lcps;
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

typedef struct//!!!FIXME_typedef_audit
{
	const char * str;
	TokenKind tokk;
	int _padding;
} punctution_t;

TokenKind Lex_punctuation(
	lcp_t * pLcpBegin,
	lcp_t * pLcpEnd, 
	lcp_t ** ppLcpTokEnd)
{
	long long signed_cLcp = pLcpEnd - pLcpBegin;
	assert(signed_cLcp >= 1);

	size_t cLcp = (size_t)signed_cLcp;

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

	for (int i_puctuation = 0; i_puctuation < LEN(puctuations); ++i_puctuation)
	{
		punctution_t punctuation = puctuations[i_puctuation];
		const char * str_puctuation = punctuation.str;
		size_t len = strlen(str_puctuation);

		if (cLcp < len)
			continue;

		lcp_t * cursor_peek = pLcpBegin;
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

	*ppLcpTokEnd = pLcpBegin + 1;
	return TokenKind_unknown;
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
	lcp_t * pLcpBegin,
	lcp_t * pLcpEnd,
	uint32_t * pCp,
	int * pLen)
{
	// BUG need to take another crack at this, rewrite
	//  it to handle pLcpEnd nicer

	*pCp = UINT32_MAX;
	*pLen = 0;

	if (pLcpBegin >= pLcpEnd)
		return;

	int len = 0;

	// Check for leading '\\'

	if (pLcpBegin->cp != '\\')
		return;

	// Advance past '\\'

	++len;
	++pLcpBegin;
	if (pLcpBegin >= pLcpEnd)
		return;

	// Look for 'u' or 'U' after '\\'

	if (pLcpBegin->cp != 'u' && pLcpBegin->cp != 'U')
		return;

	// Look for 4 or 8 hex digits, based on u vs U

	int num_hex_digits;
	if (pLcpBegin->cp == 'u')
	{
		num_hex_digits = 4;
	}
	else
	{
		num_hex_digits = 8;
	}

	// Advance past u/U

	++len;
	++pLcpBegin;
	if (pLcpBegin >= pLcpEnd)
		return;

	// Look for correct number of hex digits

	uint32_t cp_result = 0;
	int hex_digits_read = 0;
	bool delimited = false;
	bool found_end_delimiter = false;

	while ((hex_digits_read < num_hex_digits) || delimited)
	{
		uint32_t cp = pLcpBegin->cp;

		// Check for '{' (delimited ucns)

		if (!delimited && hex_digits_read == 0 && cp == '{')
		{
			delimited = true;
			++len;
			++pLcpBegin;
			if (pLcpBegin >= pLcpEnd)
				break;
			continue;
		}

		// Check for '}' (delimited ucns)

		if (delimited && cp == '}')
		{
			found_end_delimiter = true;
			++len;
			++pLcpBegin;
			if (pLcpBegin >= pLcpEnd)
				break;
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

		++pLcpBegin;
		if (pLcpBegin >= pLcpEnd)
			break;
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

TokenKind Lex_after_rest_of_id(
	lcp_t * pLcpBegin,
	lcp_t * pLcpEnd,
	lcp_t ** ppLcpTokEnd)
{
	while (pLcpBegin < pLcpEnd)
	{
		if (Does_cp_extend_id(pLcpBegin->cp))
		{
			++pLcpBegin;
			continue;
		}

		if (pLcpBegin->cp == '\\')
		{
			uint32_t cp;
			int len;
			Peek_ucn(pLcpBegin, pLcpEnd, &cp, &len);
			if (len && Does_cp_extend_id(cp))
			{
				pLcpBegin += len;
				continue;
			}
		}

		break;
	}

	*ppLcpTokEnd = pLcpBegin;
	return TokenKind_raw_identifier;
}

TokenKind Lex_after_rest_of_ppnum(
	lcp_t * pLcpBegin,
	lcp_t * pLcpEnd,
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

	while (pLcpBegin < pLcpEnd)
	{
		uint32_t cp = pLcpBegin->cp;

		if (cp == '.')
		{
			++pLcpBegin;
			continue;
		}
		else if (cp == 'e' || cp == 'E' || cp == 'p' || cp == 'P')
		{
			++pLcpBegin;

			if (pLcpBegin < pLcpEnd)
			{
				cp = pLcpBegin->cp;
				if (cp == '+' || cp == '-')
				{
					++pLcpBegin;
				}
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

			++pLcpBegin;
			continue;
		}
		else if (cp == '\\')
		{
			uint32_t cpUcn;
			int len;
			Peek_ucn(pLcpBegin, pLcpEnd, &cpUcn, &len);
			if (len && Does_cp_extend_id(cpUcn))
			{
				pLcpBegin += len;
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

	*ppLcpTokEnd = pLcpBegin;
	return TokenKind_numeric_constant;
}

TokenKind Lex_after_rest_of_line_comment(
	lcp_t * pLcpBegin,
	lcp_t * pLcpEnd,
	lcp_t ** ppLcpTokEnd)
{
	while (pLcpBegin < pLcpEnd)
	{
		uint32_t cp = pLcpBegin->cp;
		if (cp == '\n')
			break;

		++pLcpBegin;
	}

	*ppLcpTokEnd = pLcpBegin;
	return TokenKind_comment;
}

TokenKind Lex_after_rest_of_block_comment(
	lcp_t * pLcpBegin,
	lcp_t * pLcpEnd,
	lcp_t ** ppLcpTokEnd)
{
	TokenKind tokk = TokenKind_unknown;

	while (pLcpBegin < pLcpEnd)
	{
		uint32_t cp0 = pLcpBegin->cp;
		++pLcpBegin;

		if (pLcpBegin < pLcpEnd)
		{
			uint32_t cp1 = pLcpBegin->cp;
			if (cp0 == '*' && cp1 == '/')
			{
				tokk = TokenKind_comment;
				++pLcpBegin;
				break;
			}
		}
	}

	*ppLcpTokEnd = pLcpBegin;
	return tokk;
}

TokenKind Lex_after_rest_of_str_lit(
	TokenKind tokk,
	uint32_t cp_sential,
	lcp_t * pLcpBegin,
	lcp_t * pLcpEnd,
	lcp_t ** ppLcpTokEnd)
{
	int len = 0;
	bool found_end = false;

	while (pLcpBegin < pLcpEnd)
	{
		uint32_t cp = pLcpBegin->cp;

		// String without closing quote (which we support in raw lexing mode..)

		if (cp == '\n')
			break;

		// Anything else will be part of the str lit

		++pLcpBegin;

		// Closing quote

		if (cp == cp_sential)
		{
			found_end = true;
			break;
		}

		// Found somthing other than open/close quote, inc len

		++len;

		// Deal with back slash

		if (cp == '\\' && pLcpBegin < pLcpEnd)
		{
			// Check if escaped char is '\"', '\'', or '\\',
			//  the only escapes we need to handle in raw mode

			cp = pLcpBegin->cp;
			if (cp == cp_sential || cp == '\\')
			{
				++pLcpBegin;
			}
		}
	}

	// Unterminated lits are invalid

	if (!found_end)
	{
		tokk = TokenKind_unknown;
	}

	// zero length char lits are invalid

	if (cp_sential == '\'' && len == 0)
	{
		tokk = TokenKind_unknown;
	}

	*ppLcpTokEnd = pLcpBegin;
	return tokk;
}

TokenKind Lex_after_whitespace(
	lcp_t * pLcpBegin,
	lcp_t * pLcpEnd, 
	lcp_t ** ppLcpTokEnd)
{
	while (pLcpBegin < pLcpEnd)
	{
		if (!Is_ws(pLcpBegin->cp))
			break;

		++pLcpBegin;
	}

	*ppLcpTokEnd = pLcpBegin;
	return TokenKind_unknown;
}

TokenKind TokkPeek(
	lcp_t * pLcpBegin,
	lcp_t * pLcpEnd,
	lcp_t ** ppLcpTokEnd)
{
	long long cLcp = pLcpEnd - pLcpBegin;

	assert(cLcp >= 1);

	uint32_t cp_0 = pLcpBegin[0].cp;
	uint32_t cp_1 = (cLcp >= 2) ? pLcpBegin[1].cp : '\0';
	uint32_t cp_2 = (cLcp >= 3) ? pLcpBegin[2].cp : '\0';

	// Decide what to do

	if (cp_0 == 'u' && cp_1 == '8' && cp_2 == '"')
	{
		pLcpBegin += 3;
		return Lex_after_rest_of_str_lit(TokenKind_utf8_string_literal, '"', pLcpBegin, pLcpEnd, ppLcpTokEnd);
	}
	else if ((cp_0 == 'u' || cp_0 == 'U' || cp_0 == 'L') &&
			 (cp_1 == '"' || cp_1 == '\''))
	{
		pLcpBegin += 2;

		TokenKind tokk;
		
		switch (cp_0)
		{
		case 'u':
			tokk = (cp_1 == '"') ? TokenKind_utf16_string_literal : TokenKind_utf16_char_constant;
			break;
		case 'U':
			tokk = (cp_1 == '"') ? TokenKind_utf32_string_literal : TokenKind_utf32_char_constant;
			break;
		default: // 'L'
			tokk = (cp_1 == '"') ? TokenKind_wide_string_literal : TokenKind_wide_char_constant;
			break;
		}
		
		return Lex_after_rest_of_str_lit(tokk, cp_1, pLcpBegin, pLcpEnd, ppLcpTokEnd);
	}
	else if (cp_0 == '"' || cp_0 == '\'')
	{
		++pLcpBegin;
		TokenKind tokk = (cp_0 == '"') ? TokenKind_string_literal : TokenKind_char_constant;
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
		return Lex_after_rest_of_ppnum(pLcpBegin, pLcpEnd, ppLcpTokEnd);
	}
	else if (May_cp_start_id(cp_0))
	{
		return Lex_after_rest_of_id(pLcpBegin + 1, pLcpEnd, ppLcpTokEnd);
	}
	else if (cp_0 >= '0' && cp_0 <= '9')
	{
		return Lex_after_rest_of_ppnum(pLcpBegin + 1, pLcpEnd, ppLcpTokEnd);
	}
	else if (Is_ws(cp_0))
	{
		// this +1 is important, in case
		//  cp_0 came after a line continuation,
		//  because Lex_after_whitespace only skips 
		//  physical whitespace (blek)

		// BB (matthewd) that might not be true anymore...

		return Lex_after_whitespace(pLcpBegin + 1, pLcpEnd, ppLcpTokEnd);
	}
	else if (cp_0 == '\0')
	{
		return Lex_after_whitespace(pLcpBegin + 1, pLcpEnd, ppLcpTokEnd);
	}
	else if (cp_0 =='\\')
	{
		uint32_t cp;
		int len;
		Peek_ucn(pLcpBegin, pLcpEnd, &cp, &len);
		if (len)
		{
			if (May_cp_start_id(cp))
			{
				return Lex_after_rest_of_id(pLcpBegin + len, pLcpEnd, ppLcpTokEnd);
			}
			else
			{
				// Bogus UCN, return it as an unknown token

				*ppLcpTokEnd = pLcpBegin + len;
				return TokenKind_unknown;
			}
		}
		else
		{
			// Stray backslash, return as unknown token

			*ppLcpTokEnd = pLcpBegin + 1;
			return TokenKind_unknown;
		}
	}
	else
	{
		return Lex_punctuation(pLcpBegin, pLcpEnd, ppLcpTokEnd);
	}
}

// printing tokens

void Print_token(
	TokenKind tokk,
	int line,
	int col)
{
	// Token Kind

	printf("%d", tokk);

	// token loc

	printf(
		":%d:%d\n",
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
	lcp_t * lcps = Try_decode_logical_codepoints(
						bytes, 
						bytes_end, 
						&len_lcps);
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
		TokenKind tokk = TokkPeek(
								lcps, 
								lcps_end, 
								&pLcpTokEnd);

		assert(pLcpTokEnd <= lcps_end);

		const char * pChTokBegin = 
						(const char *)lcps->bytes;
		
		const char * pChTokEnd = 
						(pLcpTokEnd == lcps_end) ? 
							(const char *)bytes_end : 
							(const char *)pLcpTokEnd->bytes;

		Print_token(
			tokk,
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

	uint8_t * file_bytes;
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

		file_bytes = (uint8_t *)calloc(file_length, 1);
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

	PrintRawTokens(
		file_bytes, 
		file_bytes + file_length);
}