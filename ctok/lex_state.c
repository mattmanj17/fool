
typedef enum // see ch_map for which chars are in which class
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

#define C_STATE (LEX_STATE_EQ_OR_EXIT + 1)

static unsigned short ch_map_pre_multiplied[128] =
{
	C_STATE * CH_one_ch_tok,			// NUL
	C_STATE * CH_one_ch_tok,			// SOH
	C_STATE * CH_one_ch_tok,			// STX
	C_STATE * CH_one_ch_tok,			// ETX
	C_STATE * CH_one_ch_tok,			// EOT
	C_STATE * CH_one_ch_tok,			// ENQ
	C_STATE * CH_one_ch_tok,			// ACK
	C_STATE * CH_one_ch_tok,			// BEL
	C_STATE * CH_one_ch_tok,			// BS
	C_STATE * CH_space,					// HT
	C_STATE * CH_line_break,			// LF
	C_STATE * CH_space,					// VT
	C_STATE * CH_space,					// FF
	C_STATE * CH_line_break,			// CR
	C_STATE * CH_one_ch_tok,			// SO
	C_STATE * CH_one_ch_tok,			// SI
	C_STATE * CH_one_ch_tok,			// DLE
	C_STATE * CH_one_ch_tok,			// DC1
	C_STATE * CH_one_ch_tok,			// DC2
	C_STATE * CH_one_ch_tok,			// DC3
	C_STATE * CH_one_ch_tok,			// DC4
	C_STATE * CH_one_ch_tok,			// NAK
	C_STATE * CH_one_ch_tok,			// SYN
	C_STATE * CH_one_ch_tok,			// ETB
	C_STATE * CH_one_ch_tok,			// CAN
	C_STATE * CH_one_ch_tok,			// EM
	C_STATE * CH_one_ch_tok,			// SUB
	C_STATE * CH_one_ch_tok,			// ESC
	C_STATE * CH_one_ch_tok,			// FS
	C_STATE * CH_one_ch_tok,			// GS
	C_STATE * CH_one_ch_tok,			// RS
	C_STATE * CH_one_ch_tok,			// US
	C_STATE * CH_space,					// SP
	C_STATE * CH_bang_or_caret,			// !
	C_STATE * CH_dquote,				// "
	C_STATE * CH_htag,					// #
	C_STATE * CH_id_no_digit_no_eEpP,	// $ (allowed in ids as extension :/)
	C_STATE * CH_percent,				// %
	C_STATE * CH_amp,					// &
	C_STATE * CH_squote,				// '
	C_STATE * CH_one_ch_tok,			// (
	C_STATE * CH_one_ch_tok,			// )
	C_STATE * CH_star,					// *
	C_STATE * CH_plus,					// +
	C_STATE * CH_one_ch_tok,			// ,
	C_STATE * CH_minus,					// -
	C_STATE * CH_dot,					// .
	C_STATE * CH_fslash,				// /
	C_STATE * CH_012345679,				// 0
	C_STATE * CH_012345679,				// 1
	C_STATE * CH_012345679,				// 2
	C_STATE * CH_012345679,				// 3
	C_STATE * CH_012345679,				// 4
	C_STATE * CH_012345679,				// 5
	C_STATE * CH_012345679,				// 6
	C_STATE * CH_012345679,				// 7
	C_STATE * CH_8,						// 8
	C_STATE * CH_012345679,				// 9
	C_STATE * CH_colon,					// :
	C_STATE * CH_one_ch_tok,			// ;
	C_STATE * CH_lt,					// <
	C_STATE * CH_eq,					// =
	C_STATE * CH_gt,					// >
	C_STATE * CH_one_ch_tok,			// ?
	C_STATE * CH_one_ch_tok,			// @
	C_STATE * CH_id_no_digit_no_eEpP,	// A
	C_STATE * CH_id_no_digit_no_eEpP,	// B
	C_STATE * CH_id_no_digit_no_eEpP,	// C
	C_STATE * CH_id_no_digit_no_eEpP,	// D
	C_STATE * CH_eEpP,					// E
	C_STATE * CH_id_no_digit_no_eEpP,	// F
	C_STATE * CH_id_no_digit_no_eEpP,	// G
	C_STATE * CH_id_no_digit_no_eEpP,	// H
	C_STATE * CH_id_no_digit_no_eEpP,	// I
	C_STATE * CH_id_no_digit_no_eEpP,	// J
	C_STATE * CH_id_no_digit_no_eEpP,	// K
	C_STATE * CH_L,						// L
	C_STATE * CH_id_no_digit_no_eEpP,	// M
	C_STATE * CH_id_no_digit_no_eEpP,	// N
	C_STATE * CH_id_no_digit_no_eEpP,	// O
	C_STATE * CH_eEpP,					// P
	C_STATE * CH_id_no_digit_no_eEpP,	// Q
	C_STATE * CH_id_no_digit_no_eEpP,	// R
	C_STATE * CH_id_no_digit_no_eEpP,	// S
	C_STATE * CH_id_no_digit_no_eEpP,	// T
	C_STATE * CH_U,						// U
	C_STATE * CH_id_no_digit_no_eEpP,	// V
	C_STATE * CH_id_no_digit_no_eEpP,	// W
	C_STATE * CH_id_no_digit_no_eEpP,	// X
	C_STATE * CH_id_no_digit_no_eEpP,	// Y
	C_STATE * CH_id_no_digit_no_eEpP,	// Z
	C_STATE * CH_one_ch_tok,			// [
	C_STATE * CH_bslash,				// \ (back slash)
	C_STATE * CH_one_ch_tok,			// ]
	C_STATE * CH_bang_or_caret,			// ^
	C_STATE * CH_id_no_digit_no_eEpP,	// _
	C_STATE * CH_one_ch_tok,			// `
	C_STATE * CH_id_no_digit_no_eEpP,	// a
	C_STATE * CH_id_no_digit_no_eEpP,	// b
	C_STATE * CH_id_no_digit_no_eEpP,	// c
	C_STATE * CH_id_no_digit_no_eEpP,	// d
	C_STATE * CH_eEpP,					// e
	C_STATE * CH_id_no_digit_no_eEpP,	// f
	C_STATE * CH_id_no_digit_no_eEpP,	// g
	C_STATE * CH_id_no_digit_no_eEpP,	// h
	C_STATE * CH_id_no_digit_no_eEpP,	// i
	C_STATE * CH_id_no_digit_no_eEpP,	// j
	C_STATE * CH_id_no_digit_no_eEpP,	// k
	C_STATE * CH_id_no_digit_no_eEpP,	// l
	C_STATE * CH_id_no_digit_no_eEpP,	// m
	C_STATE * CH_id_no_digit_no_eEpP,	// n
	C_STATE * CH_id_no_digit_no_eEpP,	// o
	C_STATE * CH_eEpP,					// p
	C_STATE * CH_id_no_digit_no_eEpP,	// q
	C_STATE * CH_id_no_digit_no_eEpP,	// r
	C_STATE * CH_id_no_digit_no_eEpP,	// s
	C_STATE * CH_id_no_digit_no_eEpP,	// t
	C_STATE * CH_u,						// u
	C_STATE * CH_id_no_digit_no_eEpP,	// v
	C_STATE * CH_id_no_digit_no_eEpP,	// w
	C_STATE * CH_id_no_digit_no_eEpP,	// x
	C_STATE * CH_id_no_digit_no_eEpP,	// y
	C_STATE * CH_id_no_digit_no_eEpP,	// z
	C_STATE * CH_one_ch_tok,			// {
	C_STATE * CH_vbar,					// |
	C_STATE * CH_one_ch_tok,			// }
	C_STATE * CH_one_ch_tok,			// ~
	C_STATE * CH_one_ch_tok,			// DEL
};

static unsigned short Classify_ch_pre_multiplied(char ch)
{
	if ((unsigned char)ch > 127)
		return C_STATE * CH_one_ch_tok;

	return ch_map_pre_multiplied[ch];
}

#define C_CH (CH_id_no_digit_no_eEpP + 1)

static unsigned char tran_map[C_CH * C_STATE] =
{
	28,29,29,29,29,29,29,7,7,9,29,29,29,13,13,15,15,29,29,29,29,29,29,29,29,29,29,29,
	28,29,29,29,29,29,29,7,7,9,29,29,29,14,13,16,15,29,29,29,29,29,29,29,29,29,29,29,
	27,29,29,29,29,29,29,7,7,9,29,29,29,13,13,15,15,29,29,29,29,29,29,29,29,29,29,29,
	27,29,29,29,29,29,28,7,7,9,29,29,29,13,13,15,15,29,28,28,28,28,28,28,28,29,29,28,
	27,29,29,29,29,29,7,8,8,9,29,29,29,13,13,15,15,29,29,29,29,29,29,29,29,29,29,29,
	26,29,29,29,29,29,29,7,7,9,29,29,29,13,13,15,15,29,28,28,29,29,29,29,29,29,29,29,
	25,29,29,29,29,29,29,7,7,9,29,29,29,13,13,15,15,29,29,29,29,29,29,29,29,28,29,29,
	24,29,29,29,29,29,29,7,7,9,29,29,29,13,13,15,15,29,29,29,29,29,29,29,28,29,29,29,
	23,29,29,29,3,29,29,7,7,9,29,29,29,13,13,15,15,29,29,29,29,29,29,28,29,29,29,29,
	22,29,29,29,29,29,29,7,7,9,29,29,29,13,13,15,15,29,29,29,29,29,28,29,29,29,29,29,
	21,29,29,29,3,29,29,7,7,9,29,29,29,13,13,15,15,29,29,29,29,28,29,29,29,29,29,29,
	20,29,29,29,29,29,29,7,7,9,29,29,29,13,13,15,15,29,28,29,27,28,29,29,29,29,28,29,
	19,29,29,29,29,29,29,7,7,9,29,29,29,13,13,15,15,29,29,27,29,29,29,29,29,29,29,29,
	18,29,29,29,29,29,29,7,7,9,29,29,29,13,13,15,15,29,29,28,29,29,29,29,29,29,29,29,
	17,29,29,4,4,29,29,7,7,9,17,17,17,13,13,15,15,17,29,29,29,29,29,29,29,29,29,29,
	15,29,29,29,29,29,29,7,7,9,29,29,15,13,13,28,15,29,29,29,29,29,29,29,29,29,29,29,
	13,29,29,29,29,29,29,7,7,9,29,13,13,28,13,15,15,29,29,29,29,29,29,29,29,29,29,29,
	12,29,29,3,3,29,29,7,7,9,17,17,17,13,13,15,15,17,29,29,29,29,29,29,29,29,29,29,
	11,29,29,3,3,29,29,7,7,9,17,17,17,13,13,15,15,17,29,29,29,29,29,29,29,29,29,29,
	10,29,29,3,3,29,29,7,7,9,17,17,17,13,13,15,15,17,29,29,29,29,29,29,29,29,29,29,
	6,29,29,29,29,29,9,7,28,9,29,29,29,13,13,15,15,29,29,29,29,29,29,29,29,29,29,29,
	5,29,29,3,3,29,29,7,7,9,29,29,29,13,13,15,15,29,29,29,29,29,29,29,29,29,29,29,
	3,29,29,3,3,3,29,7,7,9,11,17,17,13,13,15,15,17,29,29,29,29,29,29,29,29,29,29,
	3,29,29,3,3,3,29,7,7,9,17,17,17,13,13,15,15,17,29,29,29,29,29,29,29,29,29,29,
	2,29,2,29,29,29,29,7,7,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,
	1,1,2,29,29,29,29,7,7,9,29,29,29,13,13,15,15,29,29,29,29,29,29,29,29,29,29,29,
	17,29,29,3,3,29,29,7,7,9,17,17,17,13,13,15,15,17,29,29,29,29,29,29,29,29,29,29
};

static LEX_STATE Next_state(LEX_STATE cur_state, char ch)
{
	return (LEX_STATE)tran_map[Classify_ch_pre_multiplied(ch) + cur_state];
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

		LEX_STATE next_state = Next_state(state, ch);

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

////// slow version of Next_state, that we use to generate tran_map

static LEX_STATE Next_state_slow(LEX_STATE cur_state, CH ch)
{
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
}

#include <stdio.h>

void Print_trans(void)
{
	for (int i = 0; i < C_CH; ++i)
	{
		for (int j = 0; j < C_STATE; ++j)
		{
			printf("%d,", Next_state_slow((LEX_STATE)j, (CH)i));
		}
		printf("\n");
	}
}