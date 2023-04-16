
#include <stdint.h>
#include <stdbool.h>

#include <stdio.h>
#include <string.h>
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

static bool Is_space(char ch)
{
	return ch == ' ' || ch == '\t' || ch == '\n';
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

static int Count_digits(const char * str)
{
	return Count_chars(Is_digit, str);
}

static int Count_hex(const char * str)
{
	return Count_chars(Is_hex, str);
}

static int Count_whitespace(const char * str)
{
	return Count_chars(Is_space, str);
}

static int Count_chars_in_block_comment(const char * str)
{
	if (str[0] != '/' || str[1] != '*')
		return 0;

	const char * begin = str;
	str += 2;

	while (true)
	{
		const char * star = Find_in_str('*', str);
		if (!star)
		{
			str += (int)strlen(str);
			break;
		}

		str = star;
		++str;

		if (*str == '/')
		{
			++str;
			break;
		}
	}

	return (int)(str - begin);
}

static int Count_chars_in_comment(const char * str)
{
	if (str[0] != '/')
		return 0;
	
	if (str[1] == '*')
		return Count_chars_in_block_comment(str);

	if (str[1] == '/')
	{
		const char * newline = Find_in_str('\n', str);
		if (!newline)
			return (int)strlen(str);

		return (int)(newline - str);
	}
	
	// '/' by itself, not a comment

	return 0;
}

static int Count_chars_to_skip(const char * str)
{
	const char * begin = str;

	while (true)
	{
		if (Is_space(str[0]))
		{
			str += Count_whitespace(str);
			continue;
		}

		int len_comment = Count_chars_in_comment(str);
		if (len_comment == 0)
			break;

		str += len_comment;
	}

	return (int)(str - begin);
}



typedef enum 
{
	tok_error,   // BUG (matthewd) create a 'tok_skip' kind, and have tok_error always stop lexing?
	tok_char_lit,
	tok_int_lit,
	tok_keyword,
	tok_id,

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

static token_t Try_lex_char_lit(const char * str)
{
	if (str[0] != '\'')
		return Make_error_token();

	if (str[1] == '\0')
		return Make_error_token();

	if (Find_in_str(str[1], "\\\'\n"))
		return Make_error_token();

	if (str[2] != '\'')
		return Make_error_token();

	token_t token;
	token.kind = tok_char_lit;
	token.len = 3;

	return token;
}

static token_t Try_lex_int_lit(const char * str)
{
	if (!Is_digit(str[0]))
		return Make_error_token();

	token_t token;
	token.kind = tok_int_lit;

	if (str[0] != '0')
	{
		token.len = Count_digits(str);
		return token;
	}

	if (str[1] == 'x' || str[1] == 'X')
	{
		if (Count_hex(str + 2) != 2)
			return Make_error_token();

		token.len = 4;
		return token;
	}

	// literal 0

	token.len = 1;
	return token;
}

static const char * keywords[] =
{
	"_Static_assert",
	
	"_Thread_local",
	
	"_Imaginary",
	
	"_Noreturn",
	
	"continue", "register", "restrict", "unsigned", "volatile", "_Alignas",
	"_Alignof", "_Complex", "_Generic",

	"default", "typedef", "_Atomic",

	"double", "extern", "inline", "return", "signed", "sizeof", "static",
	"struct", "switch",

	"break", "const", "float", "short", "union", "while", "_Bool",

	"auto", "case", "char", "else", "enum", "goto", "long", "void",

	"for", "int", "...", "<<=", ">>=",

	"do", "if", "->", "++", "--", "<<", ">>", "<=", ">=", "==", "!=", 
	"&&", "||", "*=", "/=", "%=", "+=", "-=", "&=", "^=", "|=",

	"[", "]", "(", ")", "{", "}", ".", "&", "*", "+", "-", "~", "!", 
	"/", "%", "<", ">", "^", "|", "?", ":", ";", "=", ",",
};

static token_t Try_lex_keyword(const char * str)
{
	for_i_in_ary(i, keywords)
	{
		const char * keyword = keywords[i];
		size_t len = strlen(keyword);

		if (strncmp(str, keyword, len) == 0)
		{
			token_t token;
			token.kind = tok_keyword;
			token.len = (int)len;
			return token;
		}
	}

	return Make_error_token();
}

static token_t Try_lex_id(const char * str)
{
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
		Try_lex_char_lit,
		Try_lex_int_lit,
		Try_lex_keyword,
		Try_lex_id,
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

static const char * Find_start_of_line(const char * begin, const char * end)
{
	while (true)
	{
		if (begin == end)
			break;
		
		if (!begin[0])
			break;

		if (begin[0] == '\n')
		{
			return begin + 1;
		}

		++begin;
	}

	return NULL;
}

static void Skip_whitespace_and_comments(
	const char ** p_str,
	const char ** p_line_start,
	int * p_i_line)
{
	const char * str = *p_str;
	const char * line_start = *p_line_start;
	int i_line = *p_i_line;

	int len_skip = Count_chars_to_skip(*p_str);

	const char * cursor = str;
	str += len_skip;

	while (true)
	{
		const char * line_start_next = Find_start_of_line(cursor, str);
		if (!line_start_next)
			break;

		if (!line_start_next[0])
			break;

		++i_line;
		line_start = line_start_next;
		cursor = line_start_next;
	}

	*p_str = str;
	*p_line_start = line_start;
	*p_i_line = i_line;
}

static void Print_token(
	const char * str, 
	int len, 
	const char * line_start, 
	int i_line)
{
	// NOTE (matthewd) "%.*s" is printf magic.
	//  printf("%.*s", len, str) 
	//  will only print the first 'len' 
	//  characters from the beginning of 'str'

	printf(
		"'%.*s' %d:%d\n",
		len,
		str,
		i_line + 1,
		(int)(str - line_start + 1));
}

static void Print_toks_in_str(const char * str)
{
	const char * line_start = str;
	int i_line = 0;

	while (str[0])
	{
		Skip_whitespace_and_comments(
			&str, 
			&line_start, 
			&i_line);

		if (!str[0])
			break;

		token_t token = Try_lex_token(str);
		if (!Is_valid_token(token))
		{
			printf("Lex error\n");
			return;
		}

		Print_token(str, token.len, line_start, i_line);

		str += token.len;
	}

	Print_token(str - 1, 0, line_start, i_line);
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



#define len_file_buf 2048

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		printf("wrong number of arguments, expect a single file name\n");
		return 1;
	}

	char file_buf[len_file_buf];
	memset(file_buf, 0, len_file_buf);

	bool read_file = Try_read_file_at_path_to_buffer(argv[1], file_buf, len_file_buf);
	if (!read_file)
	{
		printf("Failed to read file '%s'.\n", argv[1]);
		return 1;
	}

	Print_toks_in_str(file_buf);

	return 0;
}