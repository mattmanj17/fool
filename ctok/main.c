
#include <stdint.h>
#include <stdbool.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>



#define len_ary(array) (sizeof(array)/sizeof(0[array]))
#define for_i_in_ary(i, ary) for (int i = 0; i < len_ary(ary); ++i)



const char * Find_in_str(char ch, const char * str)
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

static token_t Make_error_token()
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



int Count_newlines(const char * begin, const char * end)
{
	int newlines = 0;

	while (true)
	{
		if (begin == end)
			break;
		
		if (!begin[0])
			break;

		if (begin[0] == '\n')
		{
			++newlines;
		}

		++begin;
	}

	return newlines;
}

const char * Find_start_of_last_line(const char * begin, const char * end)
{
	const char * start_of_last_line = begin;

	while (true)
	{
		if (begin == end)
			break;
		
		if (!begin[0])
			break;

		if (begin[0] == '\n')
		{
			start_of_last_line = begin + 1;
		}

		++begin;
	}

	return start_of_last_line;
}

void Skip_whitespace_and_comments(
	const char ** p_str,
	const char ** p_line_start,
	int * p_i_line)
{
	// NOTE (matthewd) this function may be a bit opaque.
	//  We want to keep track of what line we are on
	//  as we are lexing, so we need to deal with '\n's
	//  carfeully. This does that, factored into
	//  its own function to avoid bloating Print_toks_in_str

	// BUG (matthewd) it would better perf to pass 
	//  p_line_start/p_i_line down into Count_chars_to_skip
	//  and just do all this in one pass, but I choose
	//  not to for clarity

	int len_skip = Count_chars_to_skip(*p_str);

	const char * begin_skip = *p_str;
	*p_str += len_skip;

	int newlines_count = Count_newlines(begin_skip, *p_str);
	if (newlines_count)
	{
		*p_line_start = Find_start_of_last_line(begin_skip, *p_str);
		*p_i_line += newlines_count;
	}
}

void Print_token(
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

void Print_eof(
	const char * str, 
	const char * line_start, 
	int i_line,
	const char * line_start_prev)
{
	// We want to match the line/col numbers output by clang,
	//  so we have to do some munging. Specifically,
	//  we need to do a handwave when the last char
	//  in a file is '\n'

	if (str == line_start)
	{
		printf(
			"eof '' %d:%lld\n",
			i_line,
			line_start - line_start_prev);
	}
	else
	{
		printf(
			"eof '' %d:%lld\n",
			i_line + 1,
			str - line_start + 1);
	}
}

void Print_toks_in_str(const char * str)
{
	const char * line_start_prev = NULL;
	const char * line_start = str;
	int i_line = 0;

	while (str[0])
	{
		line_start_prev = line_start;
		Skip_whitespace_and_comments(&str, &line_start, &i_line);

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

	Print_eof(str, line_start, i_line, line_start_prev);
}



bool Try_read_file_to_buffer(FILE * file, char * buf, int len_buf)
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

bool Try_read_file_at_path_to_buffer(const char * fpath, char * buf, int len_buf)
{
	FILE * file = fopen(fpath, "rb");
	if (!file)
		return false;

	bool read_file = Try_read_file_to_buffer(file, buf, len_buf);

	fclose(file); // BUG (matthewd) ignoring return value?

	return read_file;
}



#define len_path_buf 64
#define len_file_buf 2048

static const char * fnames[] =
{
	"00001.c", "00002.c", "00003.c", "00004.c", "00005.c", "00006.c", 
	"00007.c", "00008.c", "00009.c", "00010.c", "00011.c", "00012.c", 
	"00013.c", "00014.c", "00015.c", "00016.c", "00017.c", "00018.c", 
	"00019.c", "00020.c", "00021.c", "00022.c", "00023.c", "00024.c", 
	"00026.c", "00027.c", "00028.c", "00029.c", "00030.c", "00031.c", 
	"00032.c", "00033.c", "00034.c", "00035.c", "00036.c", "00037.c", 
	"00038.c", "00039.c", "00041.c", "00042.c", "00043.c", "00044.c", 
	"00045.c", "00046.c", "00047.c", "00048.c", "00049.c", "00050.c", 
	"00051.c", "00052.c", "00053.c", "00054.c", "00055.c", "00057.c", 
	"00058.c", "00059.c", "00072.c", "00073.c", "00076.c", "00077.c", 
	"00078.c", "00080.c", "00081.c", "00082.c", "00086.c", "00087.c", 
	"00088.c", "00089.c", "00090.c", "00091.c", "00092.c", "00093.c", 
	"00094.c", "00095.c", "00096.c", "00099.c", "00100.c", "00101.c", 
	"00102.c", "00103.c", "00105.c", "00106.c", "00107.c", "00109.c", 
	"00110.c", "00111.c", "00114.c", "00116.c", "00117.c", "00118.c", 
	"00120.c", "00121.c", "00124.c", "00126.c", "00127.c", "00128.c", 
	"00130.c", "00133.c", "00134.c", "00135.c", "00140.c", "00144.c", 
	"00146.c", "00147.c", "00148.c", "00149.c", "00150.c","00151.c",
	"00155.c", "00209.c",
};

void Init_path_buf(
	char * path_buf, 
	char ** p_fname_buf,
	size_t * p_len_fname_buf)
{
	memset(path_buf, 0, len_path_buf);
	const char * root = "C:\\Users\\drape\\Desktop\\good_c\\";
	size_t len_root = strlen(root);

	if (len_root >= len_path_buf)
	{
		printf("len_root >= len_path_buf!!!\n");
		exit(1);
	}

	strcpy(path_buf, root);

	*p_fname_buf = path_buf + len_root;
	*p_len_fname_buf = len_path_buf - len_root;
}

void Init_fname_buf(
	char * fname_buf, 
	size_t len_fname_buf,
	int i_fname)
{
	memset(fname_buf, 0, len_fname_buf);

	const char * fname = fnames[i_fname];
	size_t len_fname = strlen(fname);

	if (len_fname >= len_fname_buf)
	{
		printf("full path to %s is too long.\n", fname);
		exit(1);
	}

	strcpy(fname_buf, fname);
}

int main(void)
{
	char path_buf[len_path_buf];
	char * fname_buf;
	size_t len_fname_buf;
	Init_path_buf(path_buf, &fname_buf, &len_fname_buf);

	for_i_in_ary(i_fname, fnames)
	{
		Init_fname_buf(fname_buf, len_fname_buf, i_fname);

		char file_buf[len_file_buf];
		memset(file_buf, 0, len_file_buf);

		bool read_file = Try_read_file_at_path_to_buffer(path_buf, file_buf, len_file_buf);
		if (!read_file)
		{
			printf("Failed to read file '%s'.\n", path_buf);
			return 1;
		}

		Print_toks_in_str(file_buf);
	}

	return 0;
}