
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
	tok_error,   // BUG (matthewd) create a 'tok_skip' kind, and have tok_error always stop lexing?
	tok_str_lit,
	tok_char_lit,
	tok_pp_num,
	tok_id,
	tok_punct,

	tok_max,
} token_kind_t;

typedef struct 
{
	token_kind_t kind;
	int len;
} token_t;

static token_t Make_error_token(void)
{
	token_t token;
	token.kind = tok_error;
	token.len = 0;
	return token;
}

static bool Is_valid_token(token_t token)
{
	if (token.kind == tok_error)
		return false;

	if (token.kind < 0)
		return false;

	if (token.kind >= tok_max)
		return false;

	if (token.len <= 0)
		return false;

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

static token_t Try_lex_str_lit(const char * str) //??? this fucntion is a little long
{
	const char * tok_start = str;

	str += Length_of_str_encoding_prefix(str);

	if (str[0] != '"')
		return Make_error_token();

	++str;

	while (true)
	{
		char ch0 = str[0];

		if (str[0] == '\0')
			return Make_error_token();

		++str;

		if (ch0 == '"')
			break;

		if (ch0 == '\\')
		{
			int len_esc = Length_of_escape(str);
			if (len_esc == 0)
				return Make_error_token();

			str += len_esc;
		}

		if (Find_in_str(ch0, "\r\n"))
			return Make_error_token();
	}
	
	token_t token;
	token.kind = tok_str_lit;
	token.len = (int)(str - tok_start);

	return token;
}



static token_t Try_lex_char_lit(const char * str) //??? this fucntion is a little long
{
	const char * tok_start = str;

	if (str[0] == 'L')
		++str;

	if (str[0] != '\'')
		return Make_error_token();

	++str;

	// NOTE (matthewd) deciding NOT to handle multi char literals...

	char ch_body_0 = str[0];
	if (ch_body_0 == '\0')
		return Make_error_token();

	if (Find_in_str(ch_body_0, "\'\r\n"))
		return Make_error_token();

	++str;

	if (ch_body_0 == '\\')
	{
		int len_esc = Length_of_escape(str);
		if (len_esc == 0)
			return Make_error_token();

		str += len_esc;
	}

	if (str[0] != '\'')
		return Make_error_token();

	++str;

	token_t token;
	token.kind = tok_char_lit;
	token.len = (int)(str - tok_start);

	return token;
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

static token_t Try_lex_pp_num(const char * str)
{
	const char * tok_start = str;

	int len_start = Len_pp_num_start(str);
	if (!len_start)
		return Make_error_token();

	str += len_start;

	while (true)
	{
		int len_continue = Len_pp_num_continue(str);
		if (!len_continue)
			break;

		str += len_continue;
	}

	token_t token;
	token.kind = tok_pp_num;
	token.len = (int)(str - tok_start);

	return token;
}



static token_t Try_lex_id(const char * str)
{
	//??? todo add support for universal-character-names

	if (!Can_start_id(str[0]))
		return Make_error_token();

	token_t token;
	token.kind = tok_id;
	token.len = Count_chars(Extends_id, str);
	return token;
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

static token_t Try_lex_punct(const char * str)
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
				token_t token;
				token.kind = tok_punct;
				token.len = (int)len;
				return token;
			}

			++punct_of_len;
		}
	}

	return Make_error_token();
}



typedef token_t (*lex_fn_t)(const char *);

static token_t Try_lex_token(const char * str)
{
	lex_fn_t lex_fns[] =
	{
		Try_lex_str_lit,
		Try_lex_char_lit,
		Try_lex_pp_num,
		Try_lex_id,
		Try_lex_punct,
	};

	for (int i = 0; i < len_ary(lex_fns); ++i)
	{
		token_t token = lex_fns[i](str);
		if (!Is_valid_token(token))
			continue;

		return token;
	}

	return Make_error_token();
}


static void Clean_and_print_ch(char ch)
{
	const char * good_ch = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!#$%&'()*+,-./:;<=>?@[]^_`{|}~";
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
	const char * line_start, 
	int i_line,
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
		i_line + 1,
		(int)(tok_start - line_start + 1));
}

static const char * Find_first_in_str_or_end( //??? this function is doing too much again... at least needs a better name
	const char * chars_to_find, 
	const char * str)
{
	while (true)
	{
		if (!str[0])
			return str;

		const char * cursor = chars_to_find;
		while (cursor[0])
		{
			if (str[0] == cursor[0])
				return str;

			++cursor;
		}

		++str;
	}
}

static void Print_toks_in_str(const char * str) //??? I wish this function was shorter...
{
	//??? gah, need to rewrite this: keeping comments + whitespace needs to be handled lower
	//  ... this means I need to rethink line+col number tracking

	const char * line_start = str;
	int i_line = 0;

	bool in_block_comment = false;
	const char * block_comment_begin = NULL;
	const char * line_start_block_comment = NULL;
	int i_line_block_comment = 0;

	bool in_white = false;
	const char * ws_begin = NULL;
	const char * line_start_ws = NULL;
	int i_line_ws = 0;

	char ch0;
	char ch1;

	while (true)
	{
		ch0 = str[0];

		if (!ch0)
			break;

		ch1 = str[1];
	
		if (ch0 == '\r' || ch0 == '\n')
		{
			if (!in_white)
			{
				line_start_ws = line_start;
				i_line_ws = i_line;
				ws_begin = str;

				in_white = true;
			}

			if (ch0 == '\r' && ch1 == '\n')
			{
				++str;
			}

			++i_line;
			++str;
			line_start = str;
		}
		else if (in_block_comment)
		{
			const char * star_or_newline_or_end = Find_first_in_str_or_end("*\r\n", str);
			if (!star_or_newline_or_end[0]) // check for end of string
			{
				str = star_or_newline_or_end;

				// I wonder what clang does here...

				int len = (int)(str - block_comment_begin);
				Print_token(line_start_block_comment, i_line_block_comment, block_comment_begin, len);

				continue;
			}

			str = star_or_newline_or_end;

			if (str[0] == '*')
			{
				if (str[1] == '/')
				{
					str += 2;
					in_block_comment = false;

					int len = (int)(str - block_comment_begin);
					Print_token(line_start_block_comment, i_line_block_comment, block_comment_begin, len);
				}
				else
				{
					++str;
				}
			}
		}
		else if (ch0 == '/' && ch1 == '*')
		{
			if (in_white)
			{
				in_white = false;
				int len = (int)(str - ws_begin);
				Print_token(line_start_ws, i_line_ws, ws_begin, len);
			}

			line_start_block_comment = line_start;
			i_line_block_comment = i_line;
			block_comment_begin = str;
			str += 2;
			in_block_comment = true;
		}
		else if (ch0 == '/' && ch1 == '/')
		{
			if (in_white)
			{
				in_white = false;
				int len = (int)(str - ws_begin);
				Print_token(line_start_ws, i_line_ws, ws_begin, len);
			}

			const char * old_begin = str;

			// BUG check what clang does on line commnet without newline at end

			str = Find_first_in_str_or_end("\r\n", str);

			int len = (int)(str - old_begin);
			Print_token(line_start, i_line, old_begin, len);
		}
		else if (ch0 == ' ' || ch0 == '\t')
		{
			if (!in_white)
			{
				line_start_ws = line_start;
				i_line_ws = i_line;
				ws_begin = str;

				in_white = true;
			}

			++str;
		}
		else
		{
			if (in_white)
			{
				in_white = false;
				int len = (int)(str - ws_begin);
				Print_token(line_start_ws, i_line_ws, ws_begin, len);
			}

			token_t token = Try_lex_token(str);
			if (!Is_valid_token(token))
			{
				printf("Lex error\n");
				return;
			}

			Print_token(line_start, i_line, str, token.len);

			str += token.len;
		}
	}

	if (in_white)
	{
		in_white = false;
		int len = (int)(str - ws_begin);
		Print_token(line_start_ws, i_line_ws, ws_begin, len);
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