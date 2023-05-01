
static bool Is_digit(char ch)
{
	ch -= '0';
	return ch >= 0 && ch < 10;
}

static bool Can_start_id(char ch)
{
	if (ch == '$') // allowed in ids as an extention :/
		return true;

	if (ch == '_')
		return true;

	// make lower case

	ch |= 0x20;

	// Check if letter

	ch -= 'a';
	return ch >= 0 && ch < 26;
}

static bool Extends_id(char ch)
{
	return Is_digit(ch) || Can_start_id(ch);
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

static LEX_STATE Next_state(LEX_STATE cur_state, char ch)
{
	switch (cur_state)
	{
	case LEX_STATE_START:
		{
			switch (ch)
			{
			case ' ': case '\t': 
			case '\f': case '\v':
				return LEX_STATE_WS;

			case '\r': case '\n':
				return LEX_STATE_LINES;

			case '=': case '^': case '*': case '!':
				return LEX_STATE_EQ_OR_EXIT;

			case '.':
				return LEX_STATE_DOT;
			case '/':
				return LEX_STATE_FSLASH;
			case 'u':
				return LEX_STATE_LOWER_U;
			case 'U':
				return LEX_STATE_UPPER_U;
			case 'L':
				return LEX_STATE_UPPER_L;
			case '"':
				return LEX_STATE_IN_STR_LIT;
			case '\'':
				return LEX_STATE_IN_CHAR_LIT;
			case '%':
				return LEX_STATE_PERCENT;
			case '<':
				return LEX_STATE_LT;
			case '>':
				return LEX_STATE_GT;
			case '#':
				return LEX_STATE_HTAG;
			case '&':
				return LEX_STATE_AMP;
			case '+':
				return LEX_STATE_PLUS;
			case '-':
				return LEX_STATE_MINUS;
			case ':':
				return LEX_STATE_COLON;
			case '|':
				return LEX_STATE_VBAR;
			}

			if (Is_digit(ch))
				return LEX_STATE_PPNUN;

			if (Can_start_id(ch))
				return LEX_STATE_ID;

			return LEX_STATE_DELAY_EXIT;
		}
		break;

	case LEX_STATE_PPNUM_SIGN:	
		switch (ch)
		{
		case '-': case '+':
			return LEX_STATE_PPNUN;
		}

		// intentional fallthrough

	case LEX_STATE_PPNUN:
		{
			switch (ch)
			{
			case '.':
				return LEX_STATE_PPNUN;
			case 'e': case 'E': case 'p': case 'P':
				return LEX_STATE_PPNUM_SIGN;
			}

			if (Extends_id(ch))
				return LEX_STATE_PPNUN;

			return LEX_STATE_EXIT;
		}

	case LEX_STATE_LOWER_U:	
		{
			// reusing LEX_STATE_UPPER_U because it does what we want

			if (ch == '8')
				return LEX_STATE_UPPER_U; 

			if (Extends_id(ch))
				return LEX_STATE_ID;

			return LEX_STATE_EXIT;
		}
		break;

	case LEX_STATE_UPPER_U:	
		{
			if (ch == '"')
				return LEX_STATE_IN_STR_LIT; 

			if (Extends_id(ch))
				return LEX_STATE_ID;

			return LEX_STATE_EXIT;
		}
		break;		

	case LEX_STATE_UPPER_L:	
		{
			if (ch == '\'')
				return LEX_STATE_IN_CHAR_LIT; 

			if (ch == '"')
				return LEX_STATE_IN_STR_LIT; 

			if (Extends_id(ch))
				return LEX_STATE_ID;

			return LEX_STATE_EXIT;
		}
		break;	

	case LEX_STATE_ID:		
		{
			if (Extends_id(ch))
				return LEX_STATE_ID;

			return LEX_STATE_EXIT;
		}
		break;

	case LEX_STATE_WS:
		switch (ch)
		{
		case ' ': case '\t': 
		case '\f': case '\v': 
			return LEX_STATE_WS;
		default:
			return LEX_STATE_EXIT;
		}

	case LEX_STATE_LINES:
		switch (ch)
		{
		case ' ': case '\t': 
		case '\f': case '\v':
		case '\r': case '\n':
			return LEX_STATE_LINES;
		default: 
			return LEX_STATE_EXIT;
		}	

	case LEX_STATE_DOT:
		switch (ch)
		{
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			return LEX_STATE_PPNUN;
		default: 
			return LEX_STATE_EXIT;
		}	

	case LEX_STATE_FSLASH:	
		switch (ch)
		{
		case '*':
			return LEX_STATE_IN_BLOCK_COMMENT;
		case '/':
			return LEX_STATE_LINE_COMMENT;
		case '=':
			return LEX_STATE_DELAY_EXIT;
		default: 
			return LEX_STATE_EXIT;
		}

	case LEX_STATE_STAR_IN_BLOCK_COMMENT:	
		switch (ch)
		{
		case '/':
			return LEX_STATE_DELAY_EXIT;
		}

		// intentional fall through

	case LEX_STATE_IN_BLOCK_COMMENT:	
		switch (ch)
		{
		case '*':
			return LEX_STATE_STAR_IN_BLOCK_COMMENT;
		default: 
			return LEX_STATE_IN_BLOCK_COMMENT;
		}	

	case LEX_STATE_LINE_COMMENT:	
		switch (ch)
		{
		case '\r': case '\n':
			return LEX_STATE_EXIT;
		default: 
			return LEX_STATE_LINE_COMMENT;
		}

	case LEX_STATE_IN_STR_LIT:
		switch (ch)
		{
		case '\r': case '\n':
			return LEX_STATE_EXIT;
		case '"':
			return LEX_STATE_DELAY_EXIT;
		case '\\':
			return LEX_STATE_ESC_STR_LIT;
		default: 
			return LEX_STATE_IN_STR_LIT;
		}	

	case LEX_STATE_ESC_STR_LIT:	
		switch (ch)
		{
		case '\r': case '\n':
			return LEX_STATE_EXIT;
		default: 
			return LEX_STATE_IN_STR_LIT;
		}

	case LEX_STATE_IN_CHAR_LIT:	
		switch (ch)
		{
		case '\r': case '\n':
			return LEX_STATE_EXIT;
		case '\'':
			return LEX_STATE_DELAY_EXIT;
		case '\\':
			return LEX_STATE_ESC_CHAR_LIT;
		default: 
			return LEX_STATE_IN_CHAR_LIT;
		}	

	case LEX_STATE_ESC_CHAR_LIT:	
		switch (ch)
		{
		case '\r': case '\n':
			return LEX_STATE_EXIT;
		default: 
			return LEX_STATE_IN_CHAR_LIT;
		}

	case LEX_STATE_PERCENT:	 // need to munge if %:
		switch (ch)
		{
		case ':': case '=': case '>':
			return LEX_STATE_DELAY_EXIT;
		default: 
			return LEX_STATE_EXIT;
		}

	case LEX_STATE_LT:	
		switch (ch)
		{
		case '<':
			return LEX_STATE_EQ_OR_EXIT;
		case '%': case ':': case '=':
			return LEX_STATE_DELAY_EXIT;
		default: 
			return LEX_STATE_EXIT;
		}

	case LEX_STATE_GT:	
		switch (ch)
		{
		case '>':
			return LEX_STATE_EQ_OR_EXIT;
		case '=':
			return LEX_STATE_DELAY_EXIT;
		default: 
			return LEX_STATE_EXIT;
		}

	case LEX_STATE_MINUS:
		switch (ch)
		{
		case '>': case '-': case '=':
			return LEX_STATE_DELAY_EXIT;
		default: 
			return LEX_STATE_EXIT;
		}

	case LEX_STATE_AMP:	
		switch (ch)
		{
		case '&': case '=':
			return LEX_STATE_DELAY_EXIT;
		default: 
			return LEX_STATE_EXIT;
		}

	case LEX_STATE_PLUS:
		switch (ch)
		{
		case '+': case '=':
			return LEX_STATE_DELAY_EXIT;
		default: 
			return LEX_STATE_EXIT;
		}	

	case LEX_STATE_VBAR:
		switch (ch)
		{
		case '|': case '=':
			return LEX_STATE_DELAY_EXIT;
		default: 
			return LEX_STATE_EXIT;
		}

	case LEX_STATE_HTAG:
		switch (ch)
		{
		case '#':
			return LEX_STATE_DELAY_EXIT;
		default: 
			return LEX_STATE_EXIT;
		}

	case LEX_STATE_COLON:	
		switch (ch)
		{
		case '>':
			return LEX_STATE_DELAY_EXIT;
		default: 
			return LEX_STATE_EXIT;
		}	

	case LEX_STATE_EQ_OR_EXIT:
		switch (ch)
		{
		case '=':
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

		state = next_state;

		++str;
		ch = str[0];
	}

	if (state == LEX_STATE_DOT && ch == '.')
	{
		// Handle '...' token

		//??? needs to handle \\\n

		if (str[1] == '.')
			return 3;
	}

	return (int)(str - str_begin);
}