
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

static int Count_digits(const char * str)
{
	return Count_chars(Is_digit, str);
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
	tok_int_lit,
	tok_punct,
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

static token_t Try_lex_str_lit(const char * str)
{
	// todo support encoding-prefix

	if (str[0] != '"')
		return Make_error_token();

	const char * tok_start = str;
	++str;

	while (true)
	{
		char ch0 = str[0];

		if (str[0] == '\0')
			return Make_error_token();

		++str;

		if (ch0 == '"')
			break;

		//??? todo add support for escapes

		if (Find_in_str(ch0, "\\\r\n"))
			return Make_error_token();
	}
	
	token_t token;
	token.kind = tok_str_lit;
	token.len = (int)(str - tok_start);

	return token;
}

static token_t Try_lex_char_lit(const char * str)
{
	if (str[0] != '\'')
		return Make_error_token();

	if (str[1] == '\0')
		return Make_error_token();

	//??? todo add support for escapes

	if (Find_in_str(str[1], "\\\'\r\n"))
		return Make_error_token();

	if (str[2] != '\'')
		return Make_error_token();

	token_t token;
	token.kind = tok_char_lit;
	token.len = 3;

	return token;
}

static int Length_of_integer_suffix(const char * str) //??? do not love this name
{
	char lower_str[4] = {0};
	strncpy(lower_str, str, 3);
	lower_str[0] |= 0x20;

	if (lower_str[0] != 'u' && lower_str[0] != 'l')
		return 0;

	lower_str[1] |= 0x20;
	lower_str[2] |= 0x20;

	if (Starts_with(lower_str, "llu", 3))
		return 3;

	if (Starts_with(lower_str, "ull", 3))
		return 3;

	if (Starts_with(lower_str, "ll", 2))
		return 2;

	if (Starts_with(lower_str, "lu", 2))
		return 2;

	if (Starts_with(lower_str, "ul", 2))
		return 2;

	return 1;
}

static token_t Try_lex_int_lit(const char * str)
{
	if (!Is_digit(str[0]))
		return Make_error_token();

	const char * tok_start = str;

	token_t token;
	token.kind = tok_int_lit;

	if (str[0] != '0')
	{
		str += Count_digits(str);
	}
	else if (str[1] == 'x' || str[1] == 'X')
	{
		str += 2;
		str += Count_hex(str);
	}
	else
	{
		str += Count_octal(str);
	}

	str += Length_of_integer_suffix(str);

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
	//??? todo add support for floating constants

	lex_fn_t lex_fns[] =
	{
		Try_lex_str_lit,
		Try_lex_char_lit, // character constant
		Try_lex_int_lit, // decimal constant/octal constant/hexadecimal constant
		Try_lex_id, // keyword/identifier/enumeration-constant
		Try_lex_punct, // punctuator
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