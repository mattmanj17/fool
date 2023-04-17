
#include <stdbool.h>

#include <stdio.h>
#include <string.h> //??? get rid of this later
#include <stdlib.h>



#define len_ary(array) (sizeof(array)/sizeof(0[array]))
#define for_i_in_ary(i, ary) for (int i = 0; i < len_ary(ary); ++i)



static const char * Find_in_str(char ch, const char * str)
{
	while (*str)
	{
		if (*str == ch)
			return str;

		++str;
	}

	return NULL;
}

static const char * Find_any_in_str(const char * chs, const char * str)
{
	while (*str)
	{
		const char * cursor = chs;
		while (*cursor)
		{
			if (*str == *cursor)
				return str;

			++cursor;
		}

		++str;
	}

	return NULL;
}

static bool Starts_with(const char * str, const char * prefix, int len_prefix)
{
	return strncmp(str, prefix, (size_t)len_prefix) == 0;
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

static token_t Try_lex_str_lit(const char * str)
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

static token_t Try_lex_char_lit(const char * str)
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

bool Starts_pp_num_sign(char ch)
{
	return ch == 'e' || ch == 'E' || ch == 'p' || ch == 'P';
}

bool Is_sign(char ch)
{
	return ch == '-' || ch == '+';
}

static token_t Try_lex_pp_num(const char * str)
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

	const char * tok_start = str;

	// pp_num_start

	if (Is_digit(str[0]))
	{
		++str;
	}
	else if (str[0] == '.')
	{
		++str;
		
		if (!Is_digit(str[0]))
			return Make_error_token();

		++str;
	}
	else
	{
		return Make_error_token();
	}
	
	// pp_num_continue

	while (true)
	{
		// '.'

		if (str[0] == '.')
		{
			++str;
			continue;
		}

		// [eEpP][+-]

		if (Starts_pp_num_sign(str[0]) && Is_sign(str[1]))
		{
			str += 2;
			continue;
		}

		// [0-9a-zA-Z_]

		if (Extends_id(str[0]))
		{
			++str;
			continue;
		}

		break;
	}

	token_t token;
	token.kind = tok_pp_num;
	token.len = (int)(str - tok_start);

	return token;
}

static const char * punctuation[] =
{
	"...", "<<=", ">>=",

	"do", "if", "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=", 
	"&&", "||", "*=", "/=", "%=", "+=", "-=", "&=", "^=", "|=",

	"[", "]", "(", ")", "{", "}", ".", "&", "*", "+", "-", "~", "!", 
	"/", "%", "<", ">", "^", "|", "?", ":", ";", "=", ",",
};

static token_t Try_lex_punct(const char * str)
{
	for_i_in_ary(i, punctuation)
	{
		const char * punct = punctuation[i];
		int len = (int)strlen(punct);

		if (Starts_with(str, punct, len))
		{
			token_t token;
			token.kind = tok_punct;
			token.len = (int)len;
			return token;
		}
	}

	return Make_error_token();
}

static token_t Try_lex_id(const char * str)
{
	//?? todo add support for universal-character-names

	if (!Can_start_id(str[0]))
		return Make_error_token();

	token_t token;
	token.kind = tok_id;
	token.len = Count_chars(Extends_id, str);
	return token;
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

	for_i_in_ary(i, lex_fns)
	{
		token_t token = lex_fns[i](str);
		if (!Is_valid_token(token))
			continue;

		return token;
	}

	return Make_error_token();
}

static void Print_token(
	const char * line_start, 
	int i_line,
	const char * tok_start, 
	int tok_len)
{
	// NOTE (matthewd) "%.*s" is printf magic.
	//  printf("%.*s", len, str) 
	//  will only print the first 'len' 
	//  characters from the beginning of 'str'

	printf(
		"'%.*s' %d:%d\n",
		tok_len,
		tok_start,
		i_line + 1,
		(int)(tok_start - line_start + 1));
}

static int Column_number_for_eof(
	const char * line_start,
	const char * line_end)
{
	// magic handling of \r\n

	int len_line = (int)(line_end - line_start);
	if (len_line < 2)
		return len_line;

	if (line_start[len_line - 2] == '\r' && line_start[len_line - 1] == '\n')
		return len_line - 1;

	return len_line;
}

static void Print_eof(
	int i_line,
	const char * line_start,
	const char * tok_start)
{
	printf(
		"'' %d:%d\n",
		i_line + 1,
		Column_number_for_eof(line_start, tok_start));
}

static void Print_toks_in_str(const char * str) //??? I wish this function was shorter...
{
	const char * line_start = str;
	int i_line = 0;

	bool in_block_comment = false;

	while (str[0])
	{
		if (str[0] == '\n' || str[0] == '\r')
		{
			if (str[0] == '\r' && str[1] == '\n')
			{
				++str;
			}

			// To match clang's output for eof line+col,
			//  we leave the cursor on the last newline
			//  (if the string ends with new line)

			if (!str[1])
				break;

			++i_line;
			++str;
			line_start = str;
		}
		else if (in_block_comment)
		{
			const char * star_or_newline = Find_any_in_str("*\r\n", str);
			if (!star_or_newline)
			{
				str += strlen(str);
				continue;
			}

			str = star_or_newline;

			if (str[0] == '*')
			{
				if (str[1] == '/')
				{
					str += 2;
					in_block_comment = false;
				}
				else
				{
					++str;
				}
			}
		}
		else if (str[0] == '/' && str[1] == '*')
		{
			str += 2;
			in_block_comment = true;
		}
		else if (str[0] == '/' && str[1] == '/')
		{
			const char * newline = Find_any_in_str("\r\n", str);
			if (!newline)
			{
				str += strlen(str);
			}
			else
			{
				str = newline;
			}
		}
		else if (str[0] == ' ' || str[0] == '\t')
		{
			++str;
		}
		else
		{
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

	Print_eof(i_line, line_start, str);
}



static bool Try_read_file_to_buffer(FILE * file, char * buf, int len_buf)
{
	int err = fseek(file, 0, SEEK_END);
	if (err)
		return false;

	long len_file = ftell(file);
	if (len_file < 0 || len_file >= len_buf)
		return false;

	err = fseek(file, 0, SEEK_SET);
	if (err)
		return false;

	size_t bytes_read = fread(buf, 1, (size_t)len_file, file);
	if (bytes_read != (size_t)len_file)
		return false;

	return true;
}

static bool Try_read_file_at_path_to_buffer(const char * fpath, char * buf, int len_buf)
{
	FILE * file = fopen(fpath, "rb");
	if (!file)
		return false;

	bool read_file = Try_read_file_to_buffer(file, buf, len_buf);

	fclose(file); // BUG (matthewd) ignoring return value?

	return read_file;
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

	bool read_file = Try_read_file_at_path_to_buffer(argv[1], file_buf, len_file_buf);
	if (!read_file)
	{
		printf("Failed to read file '%s'.\n", argv[1]);
		return 1;
	}

	Print_toks_in_str(file_buf);

	return 0;
}