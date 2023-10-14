#pragma once

#include <stdbool.h>

#include "unicode.h"
#include "lcp.h"



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
	tokk_comment,
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
	tokk_whitespace,
	tokk_unterminated_quote,
	tokk_zero_length_char_lit,
	tokk_unterminated_block_comment,
	tokk_unknown_byte,

	tokk_max
} token_kind_t;

const char * str_from_tokk(token_kind_t tokk);



typedef struct//!!!FIXME_typedef_audit
{
	lcp_t * lcp_max;
	token_kind_t tokk;
	int _padding;
} lex_t;

lex_t Lex_leading_token(lcp_t * cursor, lcp_t * terminator);
