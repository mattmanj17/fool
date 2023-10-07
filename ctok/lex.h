#pragma once

#include <stdbool.h>

#include "unicode.h"



typedef enum
{
	tok_unknown,
	tok_raw_identifier,
	tok_l_paren,
	tok_r_paren,
	tok_l_brace,
	tok_r_brace,
	tok_l_square,
	tok_r_square,
	tok_semi,
	tok_star,
	tok_equal,
	tok_amp,
	tok_plusplus,
	tok_exclaimequal,
	tok_numeric_constant,
	tok_colon,
	tok_minus,
	tok_period,
	tok_slash,
	tok_comma,
	tok_arrow,
	tok_plus,
	tok_string_literal,
	tok_minusminus,
	tok_percent,
	tok_pipe,
	tok_caret,
	tok_greater,
	tok_greaterequal,
	tok_equalequal,
	tok_less,
	tok_lessequal,
	tok_ampamp,
	tok_pipepipe,
	tok_exclaim,
	tok_plusequal,
	tok_minusequal,
	tok_starequal,
	tok_ellipsis,
	tok_char_constant,
	tok_lessless,
	tok_question,
	tok_wide_char_constant,
	tok_tilde,
	tok_greatergreater,
	tok_slashequal,
	tok_wide_string_literal,
	tok_hash,
	tok_comment,
	tok_hashhash,
	tok_pipeequal,
	tok_lesslessequal,
	tok_ampequal,
	tok_greatergreaterequal,
	tok_percentequal,
	tok_caretequal,
	tok_coloncolon,

	tok_max
} tok_t;

const char * str_from_tok(tok_t tok);



typedef struct
{
	lcp_t * lcp_max;
	tok_t tok;
	int _padding;
} lex_t;

lex_t Lex_leading_token(lcp_t * cursor, lcp_t * terminator);
