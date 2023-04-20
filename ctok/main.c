
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h> // for calloc



#define len_ary(array) (sizeof(array)/sizeof(0[array]))



static const char * Find_in_str(
	char ch, 
	const char * str)
{
	while (true)
	{
		if (!str[0])
			break;

		if (str[0] == ch)
			return str;

		++str;
	}

	return NULL;
}

static bool Is_horizontal_whitespace(char ch)
{
	return ch == ' ' || ch == '\t';
}

static bool Is_line_break(char ch)
{
	return ch == '\r' || ch == '\n';
}

static bool Is_digit(char ch)
{
	return Find_in_str(ch, "0123456789") != NULL;
}

static bool Is_hex(char ch)
{
	return Find_in_str(
			ch, 
			"0123456789"
			"abcdef"
			"ABCDEF") != NULL;
}

static bool Is_octal(char ch)
{
	return Find_in_str(ch, "01234567") != NULL;
}

static bool Can_start_id(char ch)
{
	return Find_in_str(
			ch, 
			"_"
			"abcdefghijklmnopqrstuvwxyz"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ") != NULL;
}

static bool Extends_id(char ch)
{
	return Is_digit(ch) || Can_start_id(ch);
}



typedef bool (*predicate_t)(char);

static int Count_chars(
	predicate_t predicate, 
	const char * str)
{
	int len = 0;

	while (str[0])
	{
		if (!predicate(str[0]))
			break;

		++str;
		++len;
	}

	return len;
}

static int Count_hex(const char * str)
{
	return Count_chars(Is_hex, str);
}

static int Count_octal(const char * str)
{
	return Count_chars(Is_octal, str);
}



typedef enum 
{
	tok_space,
	tok_block_comment,
	tok_line_comment,
	tok_str_lit,
	tok_char_lit,
	tok_pp_num,
	tok_id,
	tok_punct,

	tok_max,
} token_kind_t;

typedef struct
{
	int length;
	token_kind_t kind; 
	int line_breaks;
	int last_line_break;
} token_t;

static const token_t clear_token = {0};

static void Append_line_break(token_t * p_token, int len_line_break)
{
	p_token->length += len_line_break;
	p_token->last_line_break = p_token->length - 1;
	p_token->line_breaks += 1;
}



static int Len_line_break(const char * str)
{
	char ch = str[0];

	if (ch == '\n')
		return 1;

	if (ch == '\r')
	{
		if (str[1] == '\n')
			return 2;

		return 1;
	}

	return 0;
}

static bool Try_lex_space(token_t * p_token, const char * str)
{
	*p_token = clear_token;
	p_token->kind = tok_space; // lol, we actually do not need to do this, since tok_space == 0

	while (true)
	{
		const char * cur = str + p_token->length;

		int len_line_break = Len_line_break(cur);
		bool is_horizontal_whitespace = Is_horizontal_whitespace(cur[0]);

		if (!len_line_break && !is_horizontal_whitespace)
			return p_token->length > 0;

		if (is_horizontal_whitespace)
		{
			++p_token->length;
		}
		else
		{
			Append_line_break(p_token, len_line_break);
		}
	}
}



static bool Try_lex_block_comment(token_t * p_token, const char * str)
{
	if (str[0] != '/' || str[1] != '*')
		return false;

	*p_token = clear_token;
	p_token->kind = tok_block_comment;
	p_token->length = 2;

	while (true)
	{
		const char * cur = str + p_token->length;

		if (cur[0] == '*' && cur[1] == '/')
		{
			p_token->length += 2;
			break;
		}

		int len_line_break = Len_line_break(cur);
		if (len_line_break)
		{
			Append_line_break(p_token, len_line_break);
			continue;
		}

		++p_token->length;
	}

	return true;
}



static bool Try_lex_line_comment(token_t * p_token, const char * str)
{
	if (str[0] != '/' || str[1] != '/')
		return false;

	*p_token = clear_token;
	p_token->kind = tok_line_comment;
	p_token->length = 2;

	while (true)
	{
		char ch = str[p_token->length];
		if (!ch)
			break;

		if (Is_line_break(ch))
			break;

		++p_token->length;
	}

	return true;
}



static int Length_of_escape(const char * str) //??? bug, this starts AFTER the leading \, which could be confusing... also do not love the name
{
	const char * simple_escapes = "abfnrtv'\"\\?";

	if (Find_in_str(str[0], simple_escapes))
		return 1;

	if (Is_octal(str[0]))
	{
		int len_octal = Count_octal(str);
		if (len_octal > 3)
			return 0;

		return len_octal;
	}

	if (str[0] == 'x' || str[0] == 'X')
	{
		++str;
		return Count_hex(str) + 1;
	}

	if (str[0] == 'u' || str[0] == 'U')
	{
		int len_expected = (str[0] == 'u') ? 4 : 8;

		++str;
		int len_hex = Count_hex(str);
		if (len_hex != len_expected)
			return 0;

		return len_expected + 1;
	}

	return 0;
}

static int Length_of_str_encoding_prefix(const char * str)
{
	if (str[0] == 'u')
	{
		if (str[1] == '8')
			return 2;

		return 1;
	}

	if (str[0] == 'U')
		return 1;

	if (str[0] == 'L')
		return 1;

	return 0;
}

static bool Try_lex_str_lit(token_t * p_token, const char * str) //??? this fucntion is a little long
{
	const char * tok_start = str;

	str += Length_of_str_encoding_prefix(str);

	if (str[0] != '"')
		return false;

	++str;

	while (true)
	{
		char ch0 = str[0];

		if (str[0] == '\0')
			return false;

		++str;

		if (ch0 == '"')
			break;

		if (ch0 == '\\')
		{
			int len_esc = Length_of_escape(str);
			if (len_esc == 0)
				return false;

			str += len_esc;
		}

		if (Find_in_str(ch0, "\r\n"))
			return false;
	}
	
	*p_token = clear_token;
	p_token->kind = tok_str_lit;
	p_token->length = (int)(str - tok_start);

	return true;
}



static bool Try_lex_char_lit(token_t * p_token, const char * str) //??? this fucntion is a little long
{
	const char * tok_start = str;

	if (str[0] == 'L')
		++str;

	if (str[0] != '\'')
		return false;

	++str;

	// NOTE (matthewd) deciding NOT to handle multi char literals...

	char ch_body_0 = str[0];
	if (ch_body_0 == '\0')
		return false;

	if (Find_in_str(ch_body_0, "\'\r\n"))
		return false;

	++str;

	if (ch_body_0 == '\\')
	{
		int len_esc = Length_of_escape(str);
		if (len_esc == 0)
			return false;

		str += len_esc;
	}

	if (str[0] != '\'')
		return false;

	++str;

	*p_token = clear_token;
	p_token->kind = tok_char_lit;
	p_token->length = (int)(str - tok_start);

	return true;
}



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

	note that, pp_num_continue is technically also supposed
	to include "universal character names" (\uxxxx or \Uxxxxxxxx),
	but I have not implemented that, for now.

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

	where identifier_nondigit is 
	[0-9a-zA-Z_] OR a "universal character name"
*/

static int Len_pp_num_start(const char * str)
{
	if (Is_digit(str[0]))
		return 1;

	if (str[0] == '.' && Is_digit(str[1]))
		return 2;
	
	return 0;
}

static bool Starts_pp_num_sign(char ch)
{
	return ch == 'e' || ch == 'E' || ch == 'p' || ch == 'P';
}

static bool Is_sign(char ch)
{
	return ch == '-' || ch == '+';
}

static int Len_pp_num_continue(const char * str)
{
	if (str[0] == '.')
		return 1;

	if (Starts_pp_num_sign(str[0]) && Is_sign(str[1]))
		return 2;

	if (Extends_id(str[0]))
		return 1;
	
	return 0;
}

static bool Try_lex_pp_num(token_t * p_token, const char * str)
{
	const char * tok_start = str;

	int len_start = Len_pp_num_start(str);
	if (!len_start)
		return false;

	str += len_start;

	while (true)
	{
		int len_continue = Len_pp_num_continue(str);
		if (!len_continue)
			break;

		str += len_continue;
	}

	*p_token = clear_token;
	p_token->kind = tok_pp_num;
	p_token->length = (int)(str - tok_start);

	return true;
}



static bool Try_lex_id(token_t * p_token, const char * str)
{
	//??? todo add support for universal-character-names

	if (!Can_start_id(str[0]))
		return false;

	*p_token = clear_token;
	p_token->kind = tok_id;
	p_token->length = Count_chars(Extends_id, str);
	return true;
}



static const char * punct_3[]
{
	"...", "<<=", ">>=",

	"\0",
};

static const char * punct_2[]
{
	"do", "if", "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=", 
	"&&", "||", "*=", "/=", "%=", "+=", "-=", "&=", "^=", "|=",

	"\0",
};

static const char * punct_1[]
{
	"[", "]", "(", ")", "{", "}", ".", "&", "*", "+", "-", "~", "!", 
	"/", "%", "<", ">", "^", "|", "?", ":", ";", "=", ",",

	"\0",
};

static const char ** punctuation[]
{
	punct_1,
	punct_2,
	punct_3,
};

static bool First_n_ch_equal(const char * str0, const char * str1, int len)
{
	for (int i = 0; i < len; ++i)
	{
		if (!str0[i])
			return false;

		if (!str1[i])
			return false;

		if (str0[i] != str1[i])
			return false;
	}

	return true;
}

static bool Try_lex_punct(token_t * p_token, const char * str)
{
	for (int len = 3; len > 0; --len)
	{
		int i = len - 1;
		const char ** punct_of_len = punctuation[i];

		while (true)
		{
			const char * punct = *punct_of_len;
			if (!punct[0])
				break;

			if (First_n_ch_equal(str, punct, len))
			{
				*p_token = clear_token;
				p_token->kind = tok_punct;
				p_token->length = (int)len;
				return true;
			}

			++punct_of_len;
		}
	}

	return false;
}


//??? have lex_fn_t return an enum, signifying if we have actually hit a lexer error, 
// or if the str just did not start with that kind of token
// ... although, some of them read more than one char to even figure THAT out...
//  goal is to not have any 'backtracking'

typedef bool (*lex_fn_t)(token_t *, const char *); 

static bool Try_lex_token(token_t * p_token, const char * str)
{
	lex_fn_t lex_fns[] =
	{
		Try_lex_space,
		Try_lex_block_comment,
		Try_lex_line_comment,
		Try_lex_str_lit,
		Try_lex_char_lit,
		Try_lex_pp_num,
		Try_lex_id,
		Try_lex_punct,
	};

	for (int i = 0; i < len_ary(lex_fns); ++i)
	{
		if (!lex_fns[i](p_token, str))
			continue;

		return true;
	}

	return false;
}


static void Clean_and_print_ch(char ch)
{
	const char * good_ch = 
		"0123456789"
		"abcdefghijklmnopqrstuvwxyz"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"!#$%&'()*+,-./:;<=>?@[]^_`{|}~";

	if (Find_in_str(ch, good_ch))
		putchar(ch);
	else if (ch == '\a') 
        printf("\\a");
    else if (ch == '\b') 
        printf("\\b");
    else if (ch == '\f') 
        printf("\\f");
    else if (ch == '\n') 
        printf("\\n");
    else if (ch == '\r') 
        printf("\\r");
    else if (ch == '\t') 
        printf("\\t");
    else if (ch == '\v') 
        printf("\\v");
    else if (ch == '"') 
        printf("\\\"");
    else if (ch == '\\') 
        printf("\\\\");
	else
		printf("\\x%02hhx", ch);
}

static void Print_token(
	const char * last_line_break, 
	int line_breaks_seen,
	const char * tok_start, 
	int tok_len)
{
	printf("\"");
	for (int i = 0; i < tok_len; ++i)
	{
		Clean_and_print_ch(tok_start[i]);
	}
	printf("\"");

	printf(
		" %d:%d\n",
		line_breaks_seen + 1,
		(int)(tok_start - last_line_break));
}

static void Print_toks_in_str(const char * str)
{
	const char * last_line_break = str - 1; // bug, the fact that we have to do this -1 here is confusing
	int line_breaks_seen = 0;

	while (str[0])
	{
		token_t token;
		if (!Try_lex_token(&token, str))
		{
			printf("Lex error\n");
			return;
		}

		Print_token(last_line_break, line_breaks_seen, str, token.length);

		if (token.line_breaks)
		{
			line_breaks_seen += token.line_breaks;
			last_line_break = str + token.last_line_break;
		}

		str += token.length;
	}
}



static char * Try_read_file_to_buffer(FILE * file, char * buf, int len_buf)
{
	int err = fseek(file, 0, SEEK_END);
	if (err)
		return NULL;

	long len_file = ftell(file);
	if (len_file < 0 || len_file >= len_buf)
		return NULL;

	err = fseek(file, 0, SEEK_SET);
	if (err)
		return NULL;

	size_t bytes_read = fread(buf, 1, (size_t)len_file, file);
	if (bytes_read != (size_t)len_file)
		return NULL;

	return buf + len_file;
}

static char * Try_read_file_at_path_to_buffer(const char * fpath, char * buf, int len_buf)
{
	FILE * file = fopen(fpath, "rb");
	if (!file)
		return NULL;

	char * end = Try_read_file_to_buffer(file, buf, len_buf);

	fclose(file); // BUG (matthewd) ignoring return value?

	return end;
}



#define len_file_buf 0x100000

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		printf("wrong number of arguments, expect a single file name\n");
		return 1;
	}

	char * file_buf = (char *)calloc(len_file_buf, 1);
	if (!file_buf)
	{
		printf("Failed to allocate memory \n");
		return 1;
	}

	char * end = Try_read_file_at_path_to_buffer(argv[1], file_buf, len_file_buf);
	if (!end)
	{
		printf("Failed to read file '%s'.\n", argv[1]);
		return 1;
	}

	Print_toks_in_str(file_buf);

	return 0;
}