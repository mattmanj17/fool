
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <uchar.h>



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

size_t Leading_ones(Byte_t byte)
{
	size_t count = 0;
	for (Byte_t mask = (1 << 7); mask; mask >>= 1)
	{
		if (!(byte & mask))
			break;

		++count;
	}

	return count;
}



// byte_span

typedef struct Byte_span_t
{
	Byte_t * begin;
	Byte_t * end;
} Byte_span_t;

size_t Byte_span_len(Byte_span_t span)
{
	if (!span.begin)
		return 0;

	long long count = span.end - span.begin;
	if (count < 0)
		return 0;

	return (size_t)count;
}



// utf8 decode : convert a byte_span into a Ch_loc_ary

typedef struct Ch_len_t // ch + len
{
	char32_t ch;
	size_t len;
} Ch_len_t;

typedef struct Ch_loc_t // ch + loc
{
	char32_t ch;
	Byte_t * loc;
} Ch_loc_t;

typedef enum Mch_k // Meta ch
{
	Mch_invalid = 0x110000, // 0x10FFFF + 1, signifies an encoding error
	Mch_end,				// signifies the end of input bytes
} Mch_k;

Ch_len_t Decode_leading_ch(Byte_span_t span)
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

	size_t span_len = Byte_span_len(span);
	if (span_len == 0)
		return {Mch_end, 0};

	// Value we return on error

	Ch_len_t invalid = {Mch_invalid, 1};

	// Check if first byte is a trailing byte (1 leading 1),
	//  or if too many leading ones.

	size_t leading_ones = Leading_ones(*span.begin);
	if (leading_ones == 1)
		return invalid;

	if (leading_ones > 4)
		return invalid;

	// Compute len from leading_ones

	size_t len;
	switch (leading_ones)
	{
	case 0: len = 1; break;
	case 2: len = 2; break;
	case 3: len = 3; break;
	default: len = 4; break;
	}

	// Check if we do not have enough bytes

	if (len > span_len)
		return invalid;

	// Check if any trailing bytes are invalid

	for (size_t i = 1; i < len; ++i)
	{
		if (Leading_ones(span.begin[i]) != 1)
			return invalid;
	}

	// Get the significant bits from the first byte

	char32_t ch = *span.begin;
	switch (len)
	{
	case 2: ch &= 0x1F; break;
	case 3: ch &= 0x0F; break;
	case 4: ch &= 0x07; break;
	}

	// Get bits from the trailing bytes

	for (size_t i = 1; i < len; ++i)
	{
		ch <<= 6;
		ch |= (span.begin[i] & 0x3F);
	}

	// Check for illegal codepoints

	if (ch >= 0xD800 && ch <= 0xDFFF)
		return invalid;

	if (ch > 0x10FFFF)
		return invalid;

	// Check for 'overlong encodings'

	if (ch <= 0x7f && len > 1)
		return invalid;

	if (ch <= 0x7ff && len > 2)
		return invalid;

	if (ch <= 0xffff && len > 3)
		return invalid;

	// We did it, return ch + len

	return {ch, len};
}

Ch_loc_t * Decode_byte_span(Byte_span_t span)
{
	// Deal with potential UTF-8 BOM

	size_t span_len = Byte_span_len(span);

	if (span_len >= 3 &&
		span.begin[0] == 0xEF &&
		span.begin[1] == 0xBB &&
		span.begin[2] == 0xBF)
	{
		span.begin += 3;
		span_len -= 3;
	}

	// Allocate space for ch_locs
	//  In the worst case, we will have a ch_loc for every byte
	//  in byte_span. Note that we allocate 1 extra ch_loc, so
	//  we have room for a trailing Mch_end

	Ch_loc_t * ary;
	{
		size_t len = span_len + 1;
		size_t size = sizeof(Ch_loc_t);
		ary = (Ch_loc_t *)calloc(len, size);
	}

	// Chew through the byte span with Decode_leading_ch,
	//  until we get to Mch_end.

	Ch_loc_t * it = ary;
	while (true)
	{
		// Decode
		
		Ch_len_t ch_len = Decode_leading_ch(span);

		// Add to ary

		*it = {ch_len.ch, span.begin};

		// Check for Mch_end

		if (ch_len.ch == Mch_end)
			break;

		// Advance

		++it;
		span.begin += ch_len.len;
	}

	// Return the ary

	return ary;
}



// 'scrub' : replace patterns in ch_loc_ary

typedef struct Scrub_t
{
	Ch_loc_t * it_from;
	Ch_loc_t * it_to;
} Scrub_t;

Scrub_t Start_scrub(
	Ch_loc_t * ary)
{
	Scrub_t scrub;
	scrub.it_from = ary;
	scrub.it_to = ary;

	return scrub;
}

bool Is_scrub_done(Scrub_t scrub)
{
	return scrub.it_from->ch == Mch_end;
}

char32_t Scrub_peek_ch(Scrub_t scrub, size_t i)
{
	return scrub.it_from[i].ch;
}

void Advance_scrub(
	Scrub_t * scrub_r,
	char32_t ch,
	size_t len)
{
	scrub_r->it_to->ch = ch;
	scrub_r->it_to->loc = scrub_r->it_from->loc;

	++scrub_r->it_to;
	scrub_r->it_from += len;
}

void End_scrub(Scrub_t scrub)
{
	scrub.it_to->ch = Mch_end;
	scrub.it_to->loc = scrub.it_from->loc;
}

void Scrub_carriage_returns(
	Ch_loc_t * ary)
{
	Scrub_t scrub = Start_scrub(ary);
	while (!Is_scrub_done(scrub))
	{
		char32_t ch = Scrub_peek_ch(scrub, 0);
		bool is_cr = ch == '\r';
		if (is_cr)
			ch = '\n';

		size_t len = 1;
		if (is_cr && Scrub_peek_ch(scrub, 1) == '\n')
		{
			len = 2;
		}

		Advance_scrub(&scrub, ch, len);
	}
	End_scrub(scrub);
}

void Scrub_trigraphs(
	Ch_loc_t * ary)
{
	Scrub_t scrub = Start_scrub(ary);
	while (!Is_scrub_done(scrub))
	{
		char32_t ch0 = Scrub_peek_ch(scrub, 0);

		if (ch0 == '?' && Scrub_peek_ch(scrub, 1) == '?')
		{
			char32_t ch2 = Scrub_peek_ch(scrub, 2);

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
	End_scrub(scrub);
}

void Scrub_leading_escaped_line_breaks(Scrub_t * scrub_r)
{
	// Get length of leading esc eols

	size_t len = 0;
	while (true)
	{
		size_t i = len;
		if (Scrub_peek_ch(*scrub_r, i) != '\\')
			break;

		++i;

		while (Is_hz_ws(Scrub_peek_ch(*scrub_r, i)))
		{
			++i;
		}

		// Do not need to check for \r. since that 
		//  has already been scrubbed.

		if (Scrub_peek_ch(*scrub_r, i) != '\n')
			break;

		len = i + 1;
	}

	// Advance now that we have the length

	if (len == 0)
	{
		Advance_scrub(scrub_r, Scrub_peek_ch(*scrub_r, 0), 1);
	}
	else if (scrub_r->it_from[len].ch == Mch_end)
	{
		// Drop trailing escaped line break
		//  (do not include it in a character)
		
		scrub_r->it_from += len;
	}
	else
	{
		Advance_scrub(
			scrub_r,
			Scrub_peek_ch(*scrub_r, len),
			len + 1);
	}
}

void Scrub_escaped_line_breaks(
	Ch_loc_t * ary)
{
	Scrub_t scrub = Start_scrub(ary);
	while (!Is_scrub_done(scrub))
	{
		Scrub_leading_escaped_line_breaks(&scrub);
	}
	End_scrub(scrub);
}



// Lex

// TODO TokkLen_t !!!!!!

#define TOKEN_KINDS()\
	X(unknown)\
	X(eof)\
	X(eod)\
	X(code_completion)\
	X(comment)\
	X(identifier)\
	X(raw_identifier)\
	X(numeric_constant)\
	X(char_constant)\
	X(wide_char_constant)\
	X(utf8_char_constant)\
	X(utf16_char_constant)\
	X(utf32_char_constant)\
	X(string_literal)\
	X(wide_string_literal)\
	X(header_name)\
	X(utf8_string_literal)\
	X(utf16_string_literal)\
	X(utf32_string_literal)\
	X(l_square)\
	X(r_square)\
	X(l_paren)\
	X(r_paren)\
	X(l_brace)\
	X(r_brace)\
	X(period)\
	X(ellipsis)\
	X(amp)\
	X(ampamp)\
	X(ampequal)\
	X(star)\
	X(starequal)\
	X(plus)\
	X(plusplus)\
	X(plusequal)\
	X(minus)\
	X(arrow)\
	X(minusminus)\
	X(minusequal )\
	X(tilde)\
	X(exclaim)\
	X(exclaimequal)\
	X(slash)\
	X(slashequal)\
	X(percent)\
	X(percentequal)\
	X(less)\
	X(lessless)\
	X(lessequal)\
	X(lesslessequal)\
	X(spaceship)\
	X(greater)\
	X(greatergreater)\
	X(greaterequal)\
	X(greatergreaterequal)\
	X(caret)\
	X(caretequal)\
	X(pipe)\
	X(pipepipe)\
	X(pipeequal)\
	X(question)\
	X(colon)\
	X(semi)\
	X(equal)\
	X(equalequal)\
	X(comma)\
	X(hash)\
	X(hashhash)\
	X(hashat)\
	X(periodstar)\
	X(arrowstar)\
	X(coloncolon)\
	X(at)\
	X(lesslessless)\
	X(greatergreatergreater)\
	X(caretcaret)\
\
	X(kw_auto)\
	X(kw_break)\
	X(kw_case)\
	X(kw_char)\
	X(kw_const)\
	X(kw_continue)\
	X(kw_default)\
	X(kw_do)\
	X(kw_double)\
	X(kw_else)\
	X(kw_enum)\
	X(kw_extern)\
	X(kw_float)\
	X(kw_for)\
	X(kw_goto)\
	X(kw_if)\
	X(kw_int)\
	X(kw__ExtInt)\
	X(kw__BitInt)\
	X(kw_long)\
	X(kw_register)\
	X(kw_return)\
	X(kw_short)\
	X(kw_signed)\
	X(kw_sizeof)\
	X(kw_static)\
	X(kw_struct)\
	X(kw_switch)\
	X(kw_typedef)\
	X(kw_union)\
	X(kw_unsigned)\
	X(kw_void)\
	X(kw_volatile)\
	X(kw_while)\
	X(kw__Alignas)\
	X(kw__Alignof)\
	X(kw__Atomic)\
	X(kw__Bool)\
	X(kw__Complex)\
	X(kw__Generic)\
	X(kw__Imaginary)\
	X(kw__Noreturn)\
	X(kw__Static_assert)\
	X(kw__Thread_local)\
	X(kw___func__)\
	X(kw___objc_yes)\
	X(kw___objc_no)\
	X(kw_asm)\
	X(kw_bool)\
	X(kw_catch)\
	X(kw_class)\
	X(kw_const_cast)\
	X(kw_delete)\
	X(kw_dynamic_cast)\
	X(kw_explicit)\
	X(kw_export)\
	X(kw_false)\
	X(kw_friend)\
	X(kw_mutable)\
	X(kw_namespace)\
	X(kw_new)\
	X(kw_operator)\
	X(kw_private)\
	X(kw_protected)\
	X(kw_public)\
	X(kw_reinterpret_cast)\
	X(kw_static_cast)\
	X(kw_template)\
	X(kw_this)\
	X(kw_throw)\
	X(kw_true)\
	X(kw_try)\
	X(kw_typename)\
	X(kw_typeid)\
	X(kw_using)\
	X(kw_virtual)\
	X(kw_wchar_t)\
	X(kw_restrict)\
	X(kw_inline)\
	X(kw_alignas)\
	X(kw_alignof)\
	X(kw_char16_t)\
	X(kw_char32_t)\
	X(kw_constexpr)\
	X(kw_decltype)\
	X(kw_noexcept)\
	X(kw_nullptr)\
	X(kw_static_assert)\
	X(kw_thread_local)\
	X(kw_co_await)\
	X(kw_co_return)\
	X(kw_co_yield)\
	X(kw_module)\
	X(kw_import)\
	X(kw_consteval)\
	X(kw_constinit)\
	X(kw_concept)\
	X(kw_requires)\
	X(kw_char8_t)\
	X(kw__Float16)\
	X(kw_typeof)\
	X(kw_typeof_unqual)\
	X(kw__Accum)\
	X(kw__Fract)\
	X(kw__Sat)\
	X(kw__Decimal32)\
	X(kw__Decimal64)\
	X(kw__Decimal128)\
	X(kw___null)\
	X(kw___alignof)\
	X(kw___attribute)\
	X(kw___builtin_choose_expr)\
	X(kw___builtin_offsetof)\
	X(kw___builtin_FILE)\
	X(kw___builtin_FILE_NAME)\
	X(kw___builtin_FUNCTION)\
	X(kw___builtin_FUNCSIG)\
	X(kw___builtin_LINE)\
	X(kw___builtin_COLUMN)\
	X(kw___builtin_source_location)\
	X(kw___builtin_types_compatible_p)\
	X(kw___builtin_va_arg)\
	X(kw___extension__)\
	X(kw___float128)\
	X(kw___ibm128)\
	X(kw___imag)\
	X(kw___int128)\
	X(kw___label__)\
	X(kw___real)\
	X(kw___thread)\
	X(kw___FUNCTION__)\
	X(kw___PRETTY_FUNCTION__)\
	X(kw___auto_type)\
	X(kw___FUNCDNAME__)\
	X(kw___FUNCSIG__)\
	X(kw_L__FUNCTION__)\
	X(kw_L__FUNCSIG__)\
	X(kw___is_interface_class)\
	X(kw___is_sealed)\
	X(kw___is_destructible)\
	X(kw___is_trivially_destructible)\
	X(kw___is_nothrow_destructible)\
	X(kw___is_nothrow_assignable)\
	X(kw___is_constructible)\
	X(kw___is_nothrow_constructible)\
	X(kw___is_assignable)\
	X(kw___has_nothrow_move_assign)\
	X(kw___has_trivial_move_assign)\
	X(kw___has_trivial_move_constructor)\
	X(kw___has_nothrow_assign)\
	X(kw___has_nothrow_copy)\
	X(kw___has_nothrow_constructor)\
	X(kw___has_trivial_assign)\
	X(kw___has_trivial_copy)\
	X(kw___has_trivial_constructor)\
	X(kw___has_trivial_destructor)\
	X(kw___has_virtual_destructor)\
	X(kw___is_abstract)\
	X(kw___is_aggregate)\
	X(kw___is_base_of)\
	X(kw___is_class)\
	X(kw___is_convertible_to)\
	X(kw___is_empty)\
	X(kw___is_enum)\
	X(kw___is_final)\
	X(kw___is_literal)\
	X(kw___is_pod)\
	X(kw___is_polymorphic)\
	X(kw___is_standard_layout)\
	X(kw___is_trivial)\
	X(kw___is_trivially_assignable)\
	X(kw___is_trivially_constructible)\
	X(kw___is_trivially_copyable)\
	X(kw___is_union)\
	X(kw___has_unique_object_representations)\
	X(kw___add_lvalue_reference)\
	X(kw___add_pointer)\
	X(kw___add_rvalue_reference)\
	X(kw___decay)\
	X(kw___make_signed)\
	X(kw___make_unsigned)\
	X(kw___remove_all_extents)\
	X(kw___remove_const)\
	X(kw___remove_cv)\
	X(kw___remove_cvref)\
	X(kw___remove_extent)\
	X(kw___remove_pointer)\
	X(kw___remove_reference_t)\
	X(kw___remove_restrict)\
	X(kw___remove_volatile)\
	X(kw___underlying_type)\
	X(kw___is_trivially_relocatable)\
	X(kw___is_trivially_equality_comparable)\
	X(kw___is_bounded_array)\
	X(kw___is_unbounded_array)\
	X(kw___is_nullptr)\
	X(kw___is_scoped_enum)\
	X(kw___is_referenceable)\
	X(kw___can_pass_in_regs)\
	X(kw___reference_binds_to_temporary)\
	X(kw___is_lvalue_expr)\
	X(kw___is_rvalue_expr)\
	X(kw___is_arithmetic)\
	X(kw___is_floating_point)\
	X(kw___is_integral)\
	X(kw___is_complete_type)\
	X(kw___is_void)\
	X(kw___is_array)\
	X(kw___is_function)\
	X(kw___is_reference)\
	X(kw___is_lvalue_reference)\
	X(kw___is_rvalue_reference)\
	X(kw___is_fundamental)\
	X(kw___is_object)\
	X(kw___is_scalar)\
	X(kw___is_compound)\
	X(kw___is_pointer)\
	X(kw___is_member_object_pointer)\
	X(kw___is_member_function_pointer)\
	X(kw___is_member_pointer)\
	X(kw___is_const)\
	X(kw___is_volatile)\
	X(kw___is_signed)\
	X(kw___is_unsigned)\
	X(kw___is_same)\
	X(kw___is_convertible)\
	X(kw___array_rank)\
	X(kw___array_extent)\
	X(kw___private_extern__)\
	X(kw___module_private__)\
	X(kw___declspec)\
	X(kw___cdecl)\
	X(kw___stdcall)\
	X(kw___fastcall)\
	X(kw___thiscall)\
	X(kw___regcall)\
	X(kw___vectorcall)\
	X(kw___forceinline)\
	X(kw___unaligned)\
	X(kw___super)\
	X(kw___global)\
	X(kw___local)\
	X(kw___constant)\
	X(kw___private)\
	X(kw___generic)\
	X(kw___kernel)\
	X(kw___read_only)\
	X(kw___write_only)\
	X(kw___read_write)\
	X(kw___builtin_astype)\
	X(kw_vec_step)\
	X(kw_image1d_t)\
	X(kw_image1d_array_t)\
	X(kw_image1d_buffer_t)\
	X(kw_image2d_t)\
	X(kw_image2d_array_t)\
	X(kw_image2d_depth_t)\
	X(kw_image2d_array_depth_t)\
	X(kw_image2d_msaa_t)\
	X(kw_image2d_array_msaa_t)\
	X(kw_image2d_msaa_depth_t)\
	X(kw_image2d_array_msaa_depth_t)\
	X(kw_image3d_t)\
	X(kw_pipe)\
	X(kw_addrspace_cast)\
	X(kw___noinline__)\
	X(kw_cbuffer)\
	X(kw_tbuffer)\
	X(kw_groupshared)\
	X(kw___builtin_omp_required_simd_align)\
	X(kw___pascal)\
	X(kw___vector)\
	X(kw___pixel)\
	X(kw___bool)\
	X(kw___bf16)\
	X(kw_half)\
	X(kw___bridge)\
	X(kw___bridge_transfer)\
	X(kw___bridge_retained)\
	X(kw___bridge_retain)\
	X(kw___covariant)\
	X(kw___contravariant)\
	X(kw___kindof)\
	X(kw__Nonnull)\
	X(kw__Nullable)\
	X(kw__Nullable_result)\
	X(kw__Null_unspecified)\
	X(kw___funcref)\
	X(kw___ptr64)\
	X(kw___ptr32)\
	X(kw___sptr)\
	X(kw___uptr)\
	X(kw___w64)\
	X(kw___uuidof)\
	X(kw___try)\
	X(kw___finally)\
	X(kw___leave)\
	X(kw___int64)\
	X(kw___if_exists)\
	X(kw___if_not_exists)\
	X(kw___single_inheritance)\
	X(kw___multiple_inheritance)\
	X(kw___virtual_inheritance)\
	X(kw___interface)\
	X(kw___builtin_convertvector)\
	X(kw___builtin_bit_cast)\
	X(kw___builtin_available)\
	X(kw___builtin_sycl_unique_stable_name)\
	X(kw___arm_streaming)\
	X(kw___unknown_anytype)\
\
	X(annot_cxxscope)\
	X(annot_typename)\
	X(annot_template_id)\
	X(annot_non_type)\
	X(annot_non_type_undeclared)\
	X(annot_non_type_dependent)\
	X(annot_overload_set)\
	X(annot_primary_expr)\
	X(annot_decltype)\
	X(annot_pragma_unused)\
	X(annot_pragma_vis)\
	X(annot_pragma_pack)\
	X(annot_pragma_parser_crash)\
	X(annot_pragma_captured)\
	X(annot_pragma_dump)\
	X(annot_pragma_msstruct)\
	X(annot_pragma_align)\
	X(annot_pragma_weak)\
	X(annot_pragma_weakalias)\
	X(annot_pragma_redefine_extname)\
	X(annot_pragma_fp_contract)\
	X(annot_pragma_fenv_access)\
	X(annot_pragma_fenv_access_ms)\
	X(annot_pragma_fenv_round)\
	X(annot_pragma_float_control)\
	X(annot_pragma_ms_pointers_to_members)\
	X(annot_pragma_ms_vtordisp)\
	X(annot_pragma_ms_pragma)\
	X(annot_pragma_opencl_extension)\
	X(annot_attr_openmp)\
	X(annot_pragma_openmp)\
	X(annot_pragma_openmp_end)\
	X(annot_pragma_loop_hint)\
	X(annot_pragma_fp)\
	X(annot_pragma_attribute)\
	X(annot_pragma_riscv)\
	X(annot_module_include)\
	X(annot_module_begin)\
	X(annot_module_end)\
	X(annot_header_unit)\
	X(annot_repl_input_end)

typedef enum Tokk_t
{
	#define X(id) Tokk_##id,
	TOKEN_KINDS()
	#undef X
} Tokk_t;

const char * Str_from_tokk(Tokk_t tokk)
{
	static const char *  str_for_tokk[] =
	{
		#define X(id) #id,
		TOKEN_KINDS()	
		#undef X
	};

	return str_for_tokk[tokk];
}

typedef struct Tokk_end_t
{
	Tokk_t tokk;
	Ch_loc_t * end;
} Tokk_len_t;

Tokk_end_t Lex_punctuation(
	Ch_loc_t * ary)
{
	// Punctuation sorted by length

	// "::" is included to match clang
	// https://github.com/llvm/llvm-project/commit/874217f99b99ab3c9026dc3b7bd84cd2beebde6e

	typedef struct
	{
		const char32_t * str;
		Tokk_t tokk;
	} Str_tokk_t;

	static Str_tokk_t puncts[] =
	{
		{U"%:%:", Tokk_hashhash},
		{U">>=", Tokk_greatergreaterequal},
		{U"<<=", Tokk_lesslessequal},
		{U"...", Tokk_ellipsis},
		{U"|=", Tokk_pipeequal},
		{U"||", Tokk_pipepipe},
		{U"^=", Tokk_caretequal},
		{U"==", Tokk_equalequal},
		{U"::", Tokk_coloncolon},
		{U":>", Tokk_r_square},
		{U"-=", Tokk_minusequal},
		{U"--", Tokk_minusminus},
		{U"->", Tokk_arrow},
		{U"+=", Tokk_plusequal},
		{U"++", Tokk_plusplus},
		{U"*=", Tokk_starequal},
		{U"&=", Tokk_ampequal},
		{U"&&", Tokk_ampamp},
		{U"##", Tokk_hashhash},
		{U"!=", Tokk_exclaimequal},
		{U">=", Tokk_greaterequal},
		{U">>", Tokk_greatergreater},
		{U"<=", Tokk_lessequal},
		{U"<:", Tokk_l_square},
		{U"<%", Tokk_l_brace},
		{U"<<", Tokk_lessless},
		{U"%>", Tokk_r_brace},
		{U"%=", Tokk_percentequal},
		{U"%:", Tokk_hash},
		{U"/=", Tokk_slashequal},
		{U"~", Tokk_tilde},
		{U"}", Tokk_r_brace},
		{U"{", Tokk_l_brace},
		{U"]", Tokk_r_square},
		{U"[", Tokk_l_square},
		{U"?", Tokk_question},
		{U";", Tokk_semi},
		{U",", Tokk_comma},
		{U")", Tokk_r_paren},
		{U"(", Tokk_l_paren},
		{U"|", Tokk_pipe},
		{U"^", Tokk_caret},
		{U"=", Tokk_equal},
		{U":", Tokk_colon},
		{U"-", Tokk_minus},
		{U"+", Tokk_plus},
		{U"*", Tokk_star},
		{U"&", Tokk_amp},
		{U"#", Tokk_hash},
		{U"!", Tokk_exclaim},
		{U">", Tokk_greater},
		{U"<", Tokk_less},
		{U"%", Tokk_percent},
		{U".", Tokk_period},
		{U"/", Tokk_slash},
	};

	for (int i_punct = 0; i_punct < ARY_LEN(puncts); ++i_punct)
	{
		Str_tokk_t punct = puncts[i_punct];
		const char32_t * str = punct.str;

		// Empty str in puncts? skip it

		if (str[0] == '\0')
			continue;

		// Check if ary starts with str

		bool found_match = false;

		Ch_loc_t * it = ary;
		while (true)
		{
			if (it->ch != str[0])
				break;

			++it;
			++str;

			if (str[0] == '\0')
			{
				// Reached end of str, so we have a match

				found_match = true;
				break;
			}
		}

		// Found a match, return tokk + end

		if (found_match)
			return {punct.tokk, it};
	}

	// Just return leading char as an unknown token

	return {Tokk_unknown, ary + 1};
}

bool Extends_id(char32_t ch)
{
	// Letters

	if (ch >= 'a' && ch <= 'z')
		return true;

	if (ch >= 'A' && ch <= 'Z')
		return true;

	// Underscore

	if (ch == '_')
		return true;

	// Digits

	if (ch >= '0' && ch <= '9')
		return true;

	// '$' allowed as an extension :/

	if (ch == '$')
		return true;

	// All other ascii is invalid
	
	if (ch <= 0x7F) 
		return false;

	// Bogus utf8 does not extend ids

	if (ch > 0x10ffff)
		return false;

	// These code points are allowed to extend an id

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

bool Starts_id(char32_t ch)
{
	// Does not extend ids? then does not start them either

	if (!Extends_id(ch))
		return false;

	// Digits don't start ids

	if (ch >= '0' && ch <= '9') 
		return false;

	// Any ascii that gets past Extends_id starts ids

	if (ch <= 0x7F) 
		return true;

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

	return true;
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
		return Mch_invalid;

	if (ch > 'f')
		return Mch_invalid;

	if (ch <= 'F')
		return ch - 'A' + 10;

	if (ch >= 'a')
		return ch - 'a' + 10;

	return Mch_invalid;
}

typedef struct Ch_end_t
{
	char32_t ch;
	Ch_loc_t * end;
} Ch_end_t;

Ch_end_t Lex_ucn(
	Ch_loc_t * it)
{
	Ch_end_t invalid = {Mch_invalid, NULL};

	// Leading '\\'

	if (it->ch != '\\')
		return invalid;

	++it;

	// U\u

	if (it->ch != 'u' && it->ch != 'U')
		return invalid;

	int digits_need;
	if (it->ch == 'u')
	{
		digits_need = 4;
	}
	else
	{
		digits_need = 8;
	}

	++it;

	// String of hex digits, possibly between {}

	char32_t result = 0;
	int digits_read = 0;
	bool delimited = false;
	bool got_end_delim = false;

	while (digits_read < digits_need || delimited)
	{
		// Handle {}

		if (!delimited && digits_read == 0 && it->ch == '{')
		{
			delimited = true;
			++it;
			
			continue;
		}

		if (delimited && it->ch == '}')
		{
			got_end_delim = true;
			++it;
			
			break;
		}

		// Check if valid hex digit

		char32_t val = Hex_val_from_ch(it->ch);
		if (val == Mch_invalid)
		{
			if (delimited)
				return invalid;

			break;
		}

		// Avoid overflow

		if (result & 0xF000'0000)
			return invalid;

		// Single hex digit

		result <<= 4;
		result |= val;

		++digits_read;

		++it;
	}

	// Error checks

	if (digits_read == 0)
		return invalid;

	if (!delimited && digits_read != digits_need)
		return invalid;

	// Note that we still return a valid end ptr if !Is_valid_ucn.
	//  This lets us tell the difference between a syntactically invalid
	//  ucn, and a 'semanticaly' invalid one.

	if (!Is_valid_ucn(result))
	{
		result = Mch_invalid;
	}

	return {result, it};
}

Ch_loc_t * After_rest_of_id(Ch_loc_t * it)
{
	while (true)
	{
		if (Extends_id(it->ch))
		{
			++it;
			continue;
		}

		if (it->ch == '\\')
		{
			Ch_end_t ucn = Lex_ucn(it);
			if (ucn.end && Extends_id(ucn.ch))
			{
				it = ucn.end;
				continue;
			}
		}

		break;
	}

	return it;
}

Ch_loc_t * After_rest_of_ppnum(Ch_loc_t * it)
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
		if (it->ch == '.')
		{
			++it;
			continue;
		}
		else if (it->ch == 'e' || it->ch == 'E' || it->ch == 'p' || it->ch == 'P')
		{
			++it;

			if (it->ch == '+' || it->ch == '-')
			{
				++it;
			}

			continue;
		}
		else if (it->ch == '$')
		{
			// Clang does not allow '$' in ppnums, 
			//  even though the spec would seem to suggest that 
			//  implimentation defined id chars should be included in PP nums...

			break;
		}
		else if (Extends_id(it->ch))
		{
			// Everything (else) which extends ids can extend a ppnum

			++it;
			continue;
		}
		else if (it->ch == '\\')
		{
			Ch_end_t ucn = Lex_ucn(it);
			if (ucn.end && Extends_id(ucn.ch))
			{
				it = ucn.end;
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

	return it;
}

Ch_loc_t * After_rest_of_line_comment(Ch_loc_t * it)
{
	while (it->ch != Mch_end)
	{
		if (it->ch == '\n')
			break;

		++it;
	}

	return it;
}

Tokk_end_t Lex_rest_of_block_comment(Ch_loc_t * it)
{
	Tokk_t tokk = Tokk_unknown;

	while (it->ch != Mch_end)
	{
		char32_t ch = it->ch;
		++it;

		if (ch == '*' && it->ch == '/')
		{
			tokk = Tokk_comment;
			++it;
			break;
		}
	}

	return {tokk, it};
}

Tokk_end_t Lex_rest_of_str_lit(
	Tokk_t tokk,
	Ch_loc_t * it)
{
	char32_t ch_close = U'"';
	switch (tokk)
	{
	case Tokk_utf16_char_constant:
	case Tokk_utf32_char_constant:
	case Tokk_wide_char_constant:
	case Tokk_char_constant:
		ch_close = U'\'';
		break;
	}

	int len = 0;
	while (it->ch != Mch_end)
	{
		char32_t ch = it->ch;

		// Missing closing quote

		if (ch == '\n')
			return {Tokk_unknown, it};

		// Include in string

		++it;

		// Check for close quote

		if (ch == ch_close)
		{
			if (ch_close == '\'' && len == 0)
			{
				// Zero length char lits are invalid

				return {Tokk_unknown, it};
			}
			else
			{
				return {tokk, it};
			}
		}

		// Something other than close quote, include in string body

		++len;

		// Deal with back slash

		if (ch == '\\')
		{
			if (it->ch == ch_close || it->ch == '\\')
			{
				++len;
				++it;
			}
		}
	}

	// Reached end without seeing close quote

	return {Tokk_unknown, it};
}

Ch_loc_t * After_whitespace(Ch_loc_t * it)
{
	while (it->ch != Mch_end)
	{
		if (!Is_ws(it->ch))
			break;

		++it;
	}

	return it;
}

Tokk_end_t Lex_leading_token(Ch_loc_t * it)
{
	char32_t ch_0 = it->ch;
	char32_t ch_1 = (ch_0 == Mch_end) ? Mch_end : it[1].ch;
	char32_t ch_2 = (ch_1 == Mch_end) ? Mch_end : it[2].ch;

	if (ch_0 == 'u' && ch_1 == '8' && ch_2 == '"')
	{
		it += 3;
		return Lex_rest_of_str_lit(Tokk_utf8_string_literal, it);
	}
	else if ((ch_0 == 'u' || ch_0 == 'U' || ch_0 == 'L') &&
			 (ch_1 == '"' || ch_1 == '\''))
	{
		it += 2;

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

		return Lex_rest_of_str_lit(tokk, it);
	}
	else if (ch_0 == '"' || ch_0 == '\'')
	{
		++it;

		Tokk_t tokk = (ch_0 == '"') ? Tokk_string_literal : Tokk_char_constant;

		return Lex_rest_of_str_lit(tokk, it);
	}
	else if (ch_0 == '/' && ch_1 == '*')
	{
		it += 2;
		return Lex_rest_of_block_comment(it);
	}
	else if (ch_0 == '/' && ch_1 == '/')
	{
		it += 2;
		return {Tokk_comment, After_rest_of_line_comment(it)};
	}
	else if (ch_0 == '.' && ch_1 >= '0' && ch_1 <= '9')
	{
		it += 2;
		return {Tokk_numeric_constant, After_rest_of_ppnum(it)};
	}
	else if (Starts_id(ch_0))
	{
		++it;
		return {Tokk_raw_identifier, After_rest_of_id(it)};
	}
	else if (ch_0 >= '0' && ch_0 <= '9')
	{
		++it;
		return {Tokk_numeric_constant, After_rest_of_ppnum(it)};
	}
	else if (Is_ws(ch_0) || ch_0 == '\0')
	{
		++it;
		return {Tokk_unknown, After_whitespace(it)};
	}
	else if (ch_0 =='\\')
	{
		Ch_end_t ucn = Lex_ucn(it);
		if (ucn.end)
		{
			if (Starts_id(ucn.ch))
			{
				it = ucn.end;
				return {Tokk_raw_identifier, After_rest_of_id(it)};
			}
			else
			{
				// Bogus UCN, return it as an unknown token

				return {Tokk_unknown, ucn.end};
			}
		}
		else
		{
			// Stray backslash, return as unknown token

			return {Tokk_unknown, it + 1};
		}
	}
	else
	{
		return Lex_punctuation(it);
	}
}



// printing tokens

void Print_byte_escaped(Byte_t byte)
{
	switch (byte)
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
		printf("%c", byte);
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
	    	Byte_t high_nibble = (Byte_t)((byte >> 4) & 0xF);
	    	Byte_t low_nibble = (Byte_t)(byte & 0xF);
	    	
			char high_nibble_char = high_nibble < 10 ? '0' + high_nibble : 'A' + (high_nibble - 10);
	    	char low_nibble_char = low_nibble < 10 ? '0' + low_nibble : 'A' + (low_nibble - 10);
	    	
			printf("\\x");
	    	printf("%c", high_nibble_char);
	    	printf("%c", low_nibble_char);
		}
		break;
	}
}

void Print_token(
	Tokk_t tokk,
	Byte_t * loc_begin,
	Byte_t * loc_end,
	size_t line,
	size_t col)
{
	// Token Kind

	printf("%s", Str_from_tokk(tokk));

	// Token text

	printf(" \"");

	for (; loc_begin < loc_end; ++loc_begin)
	{
		Print_byte_escaped(*loc_begin);
	}

	printf("\" ");

	// token loc

	printf(
		"%zd:%zd",
		line,
		col);

	printf("\n");
}

size_t Len_leading_line_break(
	Byte_t * loc_begin,
	Byte_t * loc_end)
{
	long long bytes_len = loc_end - loc_begin;
	if (bytes_len <= 0)
		return 0;

	Byte_t byte = loc_begin[0];
	if (byte == '\n')
		return 1;

	if (byte == '\r')
	{
		if (bytes_len >= 2 && loc_begin[1] == '\n')
			return 2;

		return 1;
	}

	return 0;
}

void Advance_line_info(
	Byte_t * loc_begin,
	Byte_t * loc_end,
	size_t * line_r,
	size_t * col_r)
{
	while (loc_begin < loc_end)
	{
		size_t len = Len_leading_line_break(loc_begin, loc_end);
		if (len)
		{
			loc_begin += len;

			*line_r += 1;
			*col_r = 1;
		}
		else
		{
			loc_begin += 1;

			*col_r += 1;
		}
	}
}

void Print_raw_tokens(Byte_span_t span)
{
	// Decode + scrub

	Ch_loc_t * ary = Decode_byte_span(span);

	Scrub_carriage_returns(ary);
	Scrub_trigraphs(ary);
	Scrub_escaped_line_breaks(ary);

	// Keep track of line info

	size_t line = 1;
	size_t col = 1;

	// Lex!

	Ch_loc_t * it = ary;
	while (it->ch != Mch_end)
	{
		Tokk_end_t token = Lex_leading_token(it);

		Byte_t * loc_begin = it->loc;
		Byte_t * loc_end = token.end->loc;

		Print_token(
			token.tokk,
			loc_begin,
			loc_end,
			line,
			col);

		Advance_line_info(
			loc_begin,
			loc_end,
			&line,
			&col);

		it = token.end;
	}
}



// main

int wmain(int argc, wchar_t *argv[])
{
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

	// Print tokens

	Byte_span_t span;
	span.begin = file_bytes;
	span.end = file_bytes + file_length;

	Print_raw_tokens(span);
}