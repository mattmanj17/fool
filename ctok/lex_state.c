
#include <stdio.h>

typedef enum
{
	CH_one_ch_tok,
	CH_bslash,
	CH_bang_or_caret,
	CH_eq,
	CH_star,
	CH_colon,
	CH_htag,
	CH_vbar,
	CH_plus,
	CH_amp,
	CH_minus,
	CH_gt,
	CH_lt,
	CH_percent,
	CH_eEpP,
	CH_squote,
	CH_dquote,
	CH_L,
	CH_U,
	CH_u,
	CH_fslash,
	CH_dot,
	CH_8,
	CH_012345679,
	CH_line_break,
	CH_space,
	CH_id_no_digit_no_eEpP,
} CH;

CH ch_map[128] =
{
	CH_one_ch_tok,			// NUL
	CH_one_ch_tok,			// SOH
	CH_one_ch_tok,			// STX
	CH_one_ch_tok,			// ETX
	CH_one_ch_tok,			// EOT
	CH_one_ch_tok,			// ENQ
	CH_one_ch_tok,			// ACK
	CH_one_ch_tok,			// BEL
	CH_one_ch_tok,			// BS
	CH_space,				// HT
	CH_line_break,			// LF
	CH_space,				// VT
	CH_space,				// FF
	CH_line_break,			// CR
	CH_one_ch_tok,			// SO
	CH_one_ch_tok,			// SI
	CH_one_ch_tok,			// DLE
	CH_one_ch_tok,			// DC1
	CH_one_ch_tok,			// DC2
	CH_one_ch_tok,			// DC3
	CH_one_ch_tok,			// DC4
	CH_one_ch_tok,			// NAK
	CH_one_ch_tok,			// SYN
	CH_one_ch_tok,			// ETB
	CH_one_ch_tok,			// CAN
	CH_one_ch_tok,			// EM
	CH_one_ch_tok,			// SUB
	CH_one_ch_tok,			// ESC
	CH_one_ch_tok,			// FS
	CH_one_ch_tok,			// GS
	CH_one_ch_tok,			// RS
	CH_one_ch_tok,			// US
	CH_space,				// SP
	CH_bang_or_caret,		// !
	CH_dquote,				// "
	CH_htag,				// #
	CH_id_no_digit_no_eEpP,	// $ (allowed in ids as extension :/)
	CH_percent,				// %
	CH_amp,					// &
	CH_squote,				// '
	CH_one_ch_tok,			// (
	CH_one_ch_tok,			// )
	CH_star,				// *
	CH_plus,				// +
	CH_one_ch_tok,			// ,
	CH_minus,				// -
	CH_dot,					// .
	CH_fslash,				// /
	CH_012345679,			// 0
	CH_012345679,			// 1
	CH_012345679,			// 2
	CH_012345679,			// 3
	CH_012345679,			// 4
	CH_012345679,			// 5
	CH_012345679,			// 6
	CH_012345679,			// 7
	CH_8,					// 8
	CH_012345679,			// 9
	CH_colon,				// :
	CH_one_ch_tok,			// ;
	CH_lt,					// <
	CH_eq,					// =
	CH_gt,					// >
	CH_one_ch_tok,			// ?
	CH_one_ch_tok,			// @
	CH_id_no_digit_no_eEpP,	// A
	CH_id_no_digit_no_eEpP,	// B
	CH_id_no_digit_no_eEpP,	// C
	CH_id_no_digit_no_eEpP,	// D
	CH_eEpP,				// E
	CH_id_no_digit_no_eEpP,	// F
	CH_id_no_digit_no_eEpP,	// G
	CH_id_no_digit_no_eEpP,	// H
	CH_id_no_digit_no_eEpP,	// I
	CH_id_no_digit_no_eEpP,	// J
	CH_id_no_digit_no_eEpP,	// K
	CH_L,					// L
	CH_id_no_digit_no_eEpP,	// M
	CH_id_no_digit_no_eEpP,	// N
	CH_id_no_digit_no_eEpP,	// O
	CH_eEpP,				// P
	CH_id_no_digit_no_eEpP,	// Q
	CH_id_no_digit_no_eEpP,	// R
	CH_id_no_digit_no_eEpP,	// S
	CH_id_no_digit_no_eEpP,	// T
	CH_U,					// U
	CH_id_no_digit_no_eEpP,	// V
	CH_id_no_digit_no_eEpP,	// W
	CH_id_no_digit_no_eEpP,	// X
	CH_id_no_digit_no_eEpP,	// Y
	CH_id_no_digit_no_eEpP,	// Z
	CH_one_ch_tok,			// [
	CH_bslash,				// \ (back slash)
	CH_one_ch_tok,			// ]
	CH_bang_or_caret,		// ^
	CH_id_no_digit_no_eEpP,	// _
	CH_one_ch_tok,			// `
	CH_id_no_digit_no_eEpP,	// a
	CH_id_no_digit_no_eEpP,	// b
	CH_id_no_digit_no_eEpP,	// c
	CH_id_no_digit_no_eEpP,	// d
	CH_eEpP,				// e
	CH_id_no_digit_no_eEpP,	// f
	CH_id_no_digit_no_eEpP,	// g
	CH_id_no_digit_no_eEpP,	// h
	CH_id_no_digit_no_eEpP,	// i
	CH_id_no_digit_no_eEpP,	// j
	CH_id_no_digit_no_eEpP,	// k
	CH_id_no_digit_no_eEpP,	// l
	CH_id_no_digit_no_eEpP,	// m
	CH_id_no_digit_no_eEpP,	// n
	CH_id_no_digit_no_eEpP,	// o
	CH_eEpP,				// p
	CH_id_no_digit_no_eEpP,	// q
	CH_id_no_digit_no_eEpP,	// r
	CH_id_no_digit_no_eEpP,	// s
	CH_id_no_digit_no_eEpP,	// t
	CH_u,					// u
	CH_id_no_digit_no_eEpP,	// v
	CH_id_no_digit_no_eEpP,	// w
	CH_id_no_digit_no_eEpP,	// x
	CH_id_no_digit_no_eEpP,	// y
	CH_id_no_digit_no_eEpP,	// z
	CH_one_ch_tok,			// {
	CH_vbar,				// |
	CH_one_ch_tok,			// }
	CH_one_ch_tok,			// ~
	CH_one_ch_tok,			// DEL
};

CH Classify_ch(char ch)
{
	if ((unsigned char)ch > 127)
		return CH_one_ch_tok;

	return ch_map[ch];
}

typedef enum 
{
	LEX_STATE_START,					// first state

	LEX_STATE_WS,						// ' ', \t, etc
	LEX_STATE_LINES,					// \r\n
	LEX_STATE_PPNUN,					// 0.0
	LEX_STATE_PPNUM_SIGN,				// 0e+
	LEX_STATE_DOT,						// .
	LEX_STATE_FSLASH,					// /
	LEX_STATE_IN_BLOCK_COMMENT,			// /*
	LEX_STATE_STAR_IN_BLOCK_COMMENT,	// /* *
	LEX_STATE_LINE_COMMENT,				// //
	LEX_STATE_LOWER_U,					// u8""
	LEX_STATE_UPPER_U,					// U""
	LEX_STATE_UPPER_L,					// L"" or L''
	LEX_STATE_IN_STR_LIT,				// "foo"
	LEX_STATE_ESC_STR_LIT,				// \t, etc
	LEX_STATE_IN_CHAR_LIT,				// 'a'
	LEX_STATE_ESC_CHAR_LIT,				// \t, etc
	LEX_STATE_ID,						// [0-9a-zA-Z_$]
	LEX_STATE_PERCENT,					// %
	LEX_STATE_LT,						// <
	LEX_STATE_GT,						// >
	LEX_STATE_MINUS,					// -
	LEX_STATE_AMP,						// &
	LEX_STATE_PLUS,						// +
	LEX_STATE_VBAR,						// |
	LEX_STATE_HTAG,						// #
	LEX_STATE_COLON,					// :
	LEX_STATE_EQ_OR_EXIT,				// == ^=, etc

	LEX_STATE_DELAY_EXIT,				// include char in tok, done with tok
	LEX_STATE_EXIT,						// no inc char in tok, done with tok
} LEX_STATE;

unsigned char tran_map[CH_id_no_digit_no_eEpP + 1][LEX_STATE_EQ_OR_EXIT + 1] =
{
	{28,29,29,29,29,29,29,7,7,9,29,29,29,13,13,15,15,29,29,29,29,29,29,29,29,29,29,29},
	{28,29,29,29,29,29,29,7,7,9,29,29,29,14,13,16,15,29,29,29,29,29,29,29,29,29,29,29},
	{27,29,29,29,29,29,29,7,7,9,29,29,29,13,13,15,15,29,29,29,29,29,29,29,29,29,29,29},
	{27,29,29,29,29,29,28,7,7,9,29,29,29,13,13,15,15,29,28,28,28,28,28,28,28,29,29,28},
	{27,29,29,29,29,29,7,8,8,9,29,29,29,13,13,15,15,29,29,29,29,29,29,29,29,29,29,29},
	{26,29,29,29,29,29,29,7,7,9,29,29,29,13,13,15,15,29,28,28,29,29,29,29,29,29,29,29},
	{25,29,29,29,29,29,29,7,7,9,29,29,29,13,13,15,15,29,29,29,29,29,29,29,29,28,29,29},
	{24,29,29,29,29,29,29,7,7,9,29,29,29,13,13,15,15,29,29,29,29,29,29,29,28,29,29,29},
	{23,29,29,29,3,29,29,7,7,9,29,29,29,13,13,15,15,29,29,29,29,29,29,28,29,29,29,29},
	{22,29,29,29,29,29,29,7,7,9,29,29,29,13,13,15,15,29,29,29,29,29,28,29,29,29,29,29},
	{21,29,29,29,3,29,29,7,7,9,29,29,29,13,13,15,15,29,29,29,29,28,29,29,29,29,29,29},
	{20,29,29,29,29,29,29,7,7,9,29,29,29,13,13,15,15,29,28,29,27,28,29,29,29,29,28,29},
	{19,29,29,29,29,29,29,7,7,9,29,29,29,13,13,15,15,29,29,27,29,29,29,29,29,29,29,29},
	{18,29,29,29,29,29,29,7,7,9,29,29,29,13,13,15,15,29,29,28,29,29,29,29,29,29,29,29},
	{17,29,29,4,4,29,29,7,7,9,17,17,17,13,13,15,15,17,29,29,29,29,29,29,29,29,29,29},
	{15,29,29,29,29,29,29,7,7,9,29,29,15,13,13,28,15,29,29,29,29,29,29,29,29,29,29,29},
	{13,29,29,29,29,29,29,7,7,9,29,13,13,28,13,15,15,29,29,29,29,29,29,29,29,29,29,29},
	{12,29,29,3,3,29,29,7,7,9,17,17,17,13,13,15,15,17,29,29,29,29,29,29,29,29,29,29},
	{11,29,29,3,3,29,29,7,7,9,17,17,17,13,13,15,15,17,29,29,29,29,29,29,29,29,29,29},
	{10,29,29,3,3,29,29,7,7,9,17,17,17,13,13,15,15,17,29,29,29,29,29,29,29,29,29,29},
	{6,29,29,29,29,29,9,7,28,9,29,29,29,13,13,15,15,29,29,29,29,29,29,29,29,29,29,29},
	{5,29,29,3,3,29,29,7,7,9,29,29,29,13,13,15,15,29,29,29,29,29,29,29,29,29,29,29},
	{3,29,29,3,3,3,29,7,7,9,11,17,17,13,13,15,15,17,29,29,29,29,29,29,29,29,29,29},
	{3,29,29,3,3,3,29,7,7,9,17,17,17,13,13,15,15,17,29,29,29,29,29,29,29,29,29,29},
	{2,29,2,29,29,29,29,7,7,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29},
	{1,1,2,29,29,29,29,7,7,9,29,29,29,13,13,15,15,29,29,29,29,29,29,29,29,29,29,29},
	{17,29,29,3,3,29,29,7,7,9,17,17,17,13,13,15,15,17,29,29,29,29,29,29,29,29,29,29}
};

static LEX_STATE Next_state(LEX_STATE cur_state, CH ch)
{
#if 1
	return (LEX_STATE)tran_map[ch][cur_state];
#else
	switch (cur_state)
	{
	case LEX_STATE_START:
		{
			switch (ch)
			{
			case CH_space:
				return LEX_STATE_WS;
			case CH_line_break:
				return LEX_STATE_LINES;
			case CH_eq:
			case CH_star:
			case CH_bang_or_caret:
				return LEX_STATE_EQ_OR_EXIT;
			case CH_dot:
				return LEX_STATE_DOT;
			case CH_fslash:
				return LEX_STATE_FSLASH;
			case CH_u:
				return LEX_STATE_LOWER_U;
			case CH_U:
				return LEX_STATE_UPPER_U;
			case CH_L:
				return LEX_STATE_UPPER_L;
			case CH_dquote:
				return LEX_STATE_IN_STR_LIT;
			case CH_squote:
				return LEX_STATE_IN_CHAR_LIT;
			case CH_percent:
				return LEX_STATE_PERCENT;
			case CH_lt:
				return LEX_STATE_LT;
			case CH_gt:
				return LEX_STATE_GT;
			case CH_htag:
				return LEX_STATE_HTAG;
			case CH_amp:
				return LEX_STATE_AMP;
			case CH_plus:
				return LEX_STATE_PLUS;
			case CH_minus:
				return LEX_STATE_MINUS;
			case CH_colon:
				return LEX_STATE_COLON;
			case CH_vbar:
				return LEX_STATE_VBAR;
			case CH_8:
			case CH_012345679:
				return LEX_STATE_PPNUN;
			case CH_eEpP:
			case CH_id_no_digit_no_eEpP:
				return LEX_STATE_ID;
			default:
				return LEX_STATE_DELAY_EXIT;
			}
		}
		break;

	case LEX_STATE_PPNUM_SIGN:	
		switch (ch)
		{
		case CH_minus:
		case CH_plus:
			return LEX_STATE_PPNUN;
		}

		// intentional fallthrough

	case LEX_STATE_PPNUN:
		{
			switch (ch)
			{
			case CH_dot:
				return LEX_STATE_PPNUN;
			case CH_eEpP:
				return LEX_STATE_PPNUM_SIGN;
			case CH_8:
			case CH_012345679:
			case CH_u:
			case CH_U:
			case CH_L:
			case CH_id_no_digit_no_eEpP:
				return LEX_STATE_PPNUN;
			default:
				return LEX_STATE_EXIT;
			}
		}

	case LEX_STATE_LOWER_U:	
		{
			// reusing LEX_STATE_UPPER_U in the CH_8 because it does what we want

			switch (ch)
			{
			case CH_8:
				return LEX_STATE_UPPER_U; 
			case CH_012345679:
			case CH_u:
			case CH_U:
			case CH_L:
			case CH_eEpP:
			case CH_id_no_digit_no_eEpP:
				return LEX_STATE_ID;
			default:
				return LEX_STATE_EXIT;
			}
		}
		break;

	case LEX_STATE_UPPER_U:	
		{
			switch (ch)
			{
			case CH_dquote:
				return LEX_STATE_IN_STR_LIT; 
			case CH_8:
			case CH_012345679:
			case CH_u:
			case CH_U:
			case CH_L:
			case CH_eEpP:
			case CH_id_no_digit_no_eEpP:
				return LEX_STATE_ID;
			default:
				return LEX_STATE_EXIT;
			}
		}
		break;		

	case LEX_STATE_UPPER_L:	
		{
			switch (ch)
			{
			case CH_squote:
				return LEX_STATE_IN_CHAR_LIT;
			case CH_dquote:
				return LEX_STATE_IN_STR_LIT; 
			case CH_8:
			case CH_012345679:
			case CH_u:
			case CH_U:
			case CH_L:
			case CH_eEpP:
			case CH_id_no_digit_no_eEpP:
				return LEX_STATE_ID;
			default:
				return LEX_STATE_EXIT;
			}
		}
		break;	

	case LEX_STATE_ID:		
		{
			switch (ch)
			{
			case CH_8:
			case CH_012345679:
			case CH_u:
			case CH_U:
			case CH_L:
			case CH_eEpP:
			case CH_id_no_digit_no_eEpP:
				return LEX_STATE_ID;
			default:
				return LEX_STATE_EXIT;
			}
		}
		break;

	case LEX_STATE_WS:
		switch (ch)
		{
		case CH_space: 
			return LEX_STATE_WS;
		default:
			return LEX_STATE_EXIT;
		}

	case LEX_STATE_LINES:
		switch (ch)
		{
		case CH_space:
		case CH_line_break:
			return LEX_STATE_LINES;
		default: 
			return LEX_STATE_EXIT;
		}	

	case LEX_STATE_DOT:
		switch (ch)
		{
		case CH_8:
		case CH_012345679:
			return LEX_STATE_PPNUN;
		default: 
			return LEX_STATE_EXIT;
		}	

	case LEX_STATE_FSLASH:	
		switch (ch)
		{
		case CH_star:
			return LEX_STATE_IN_BLOCK_COMMENT;
		case CH_fslash:
			return LEX_STATE_LINE_COMMENT;
		case CH_eq:
			return LEX_STATE_DELAY_EXIT;
		default: 
			return LEX_STATE_EXIT;
		}

	case LEX_STATE_STAR_IN_BLOCK_COMMENT:	
		switch (ch)
		{
		case CH_fslash:
			return LEX_STATE_DELAY_EXIT;
		}

		// intentional fall through

	case LEX_STATE_IN_BLOCK_COMMENT:	
		switch (ch)
		{
		case CH_star:
			return LEX_STATE_STAR_IN_BLOCK_COMMENT;
		default: 
			return LEX_STATE_IN_BLOCK_COMMENT;
		}	

	case LEX_STATE_LINE_COMMENT:	
		switch (ch)
		{
		case CH_line_break:
			return LEX_STATE_EXIT;
		default: 
			return LEX_STATE_LINE_COMMENT;
		}

	case LEX_STATE_IN_STR_LIT:
		switch (ch)
		{
		case CH_line_break:
			return LEX_STATE_EXIT;
		case CH_dquote:
			return LEX_STATE_DELAY_EXIT;
		case CH_bslash:
			return LEX_STATE_ESC_STR_LIT;
		default: 
			return LEX_STATE_IN_STR_LIT;
		}	

	case LEX_STATE_ESC_STR_LIT:	
		switch (ch)
		{
		case CH_line_break:
			return LEX_STATE_EXIT;
		default: 
			return LEX_STATE_IN_STR_LIT;
		}

	case LEX_STATE_IN_CHAR_LIT:	
		switch (ch)
		{
		case CH_line_break:
			return LEX_STATE_EXIT;
		case CH_squote:
			return LEX_STATE_DELAY_EXIT;
		case CH_bslash:
			return LEX_STATE_ESC_CHAR_LIT;
		default: 
			return LEX_STATE_IN_CHAR_LIT;
		}	

	case LEX_STATE_ESC_CHAR_LIT:	
		switch (ch)
		{
		case CH_line_break:
			return LEX_STATE_EXIT;
		default: 
			return LEX_STATE_IN_CHAR_LIT;
		}

	case LEX_STATE_PERCENT:
		switch (ch)
		{
		case CH_colon:
		case CH_eq:
		case CH_gt:
			return LEX_STATE_DELAY_EXIT;
		default: 
			return LEX_STATE_EXIT;
		}

	case LEX_STATE_LT:	
		switch (ch)
		{
		case CH_lt:
			return LEX_STATE_EQ_OR_EXIT;
		case CH_percent:
		case CH_colon:
		case CH_eq:
			return LEX_STATE_DELAY_EXIT;
		default: 
			return LEX_STATE_EXIT;
		}

	case LEX_STATE_GT:	
		switch (ch)
		{
		case CH_gt:
			return LEX_STATE_EQ_OR_EXIT;
		case CH_eq:
			return LEX_STATE_DELAY_EXIT;
		default: 
			return LEX_STATE_EXIT;
		}

	case LEX_STATE_MINUS:
		switch (ch)
		{
		case CH_gt:
		case CH_minus:
		case CH_eq:
			return LEX_STATE_DELAY_EXIT;
		default: 
			return LEX_STATE_EXIT;
		}

	case LEX_STATE_AMP:	
		switch (ch)
		{
		case CH_amp:
		case CH_eq:
			return LEX_STATE_DELAY_EXIT;
		default: 
			return LEX_STATE_EXIT;
		}

	case LEX_STATE_PLUS:
		switch (ch)
		{
		case CH_plus:
		case CH_eq:
			return LEX_STATE_DELAY_EXIT;
		default: 
			return LEX_STATE_EXIT;
		}	

	case LEX_STATE_VBAR:
		switch (ch)
		{
		case CH_vbar:
		case CH_eq:
			return LEX_STATE_DELAY_EXIT;
		default: 
			return LEX_STATE_EXIT;
		}

	case LEX_STATE_HTAG:
		switch (ch)
		{
		case CH_htag:
			return LEX_STATE_DELAY_EXIT;
		default: 
			return LEX_STATE_EXIT;
		}

	case LEX_STATE_COLON:	
		switch (ch)
		{
		case CH_gt:
			return LEX_STATE_DELAY_EXIT;
		default: 
			return LEX_STATE_EXIT;
		}	

	case LEX_STATE_EQ_OR_EXIT:
		switch (ch)
		{
		case CH_eq:
			return LEX_STATE_DELAY_EXIT;
		default: 
			return LEX_STATE_EXIT;
		}

	case LEX_STATE_DELAY_EXIT:
	case LEX_STATE_EXIT:
	default:
		return LEX_STATE_EXIT;
	}
#endif
}

int Len_leading_token(const char * str)
{
	const char * str_begin = str;
	LEX_STATE state = LEX_STATE_START;

	char ch = str[0];

	while (true)
	{
		if (ch == '\0')
			break;

		LEX_STATE next_state = Next_state(state, Classify_ch(ch));

		if (next_state == LEX_STATE_EXIT)
			break;

		++str;

		if (next_state == LEX_STATE_DELAY_EXIT)
			break;

		state = next_state;
		ch = str[0];
	}

	switch (state)
	{
	case LEX_STATE_PERCENT:
		{
			// Handle '%:%:' token

			//??? needs to handle \\\n

			if (ch == ':' && str[1] == '%' && str[2] == ':')
				return 4;
		}
		break;

	case LEX_STATE_DOT:
		{
			// Handle '...' token

			//??? needs to handle \\\n

			if (ch == '.' && str[1] == '.')
				return 3;
		}
		break;

	default:
		break;
	}

	return (int)(str - str_begin);
}

void Print_trans(void)
{
	for (int i = 0; i <= CH_id_no_digit_no_eEpP; ++i)
	{
		printf("{");
		for (int j = 0; j <= LEX_STATE_EQ_OR_EXIT; ++j)
		{
			printf("%d,", Next_state((LEX_STATE)j, (CH)i));
		}
		printf("},\n");
	}
}