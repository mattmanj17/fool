
#include <stdint.h>
#include <stdbool.h>

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <limits.h>



#define size_of_array(array) (sizeof(array)/sizeof(0[array]))
#define for_each_index_in_array(index, array) for (int index = 0; index < size_of_array(array); ++index)



// character helpers

static bool is_character_in_string(char character_to_find, const char * string)
{
	char character; 
	while ((character = *string) != '\0')
	{
		if (character == character_to_find)
			return true;

		++string;
	}

	return false;
}

static bool is_space_or_tab(char character)
{
	return character == ' ' || character == '\t';
}

static bool is_decimal_digit(char character)
{
	return is_character_in_string(character, "0123456789");
}

static bool is_hexadecimal_digit(char character)
{
	return is_character_in_string(
			character, 
			"0123456789"
			"abcdef"
			"ABCDEF");
}

static bool is_letter_or_underscore(char character)
{
	return is_character_in_string(
			character, 
			"_"
			"abcdefghijklmnopqrstuvwxyz"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ");
}

static bool is_identifier_character(char character)
{
	return is_decimal_digit(character) || is_letter_or_underscore(character);
}



// "after" functions

static const char * after_character_literal(const char * string)
{
	assert(string);

	assert(string[0] == '\'');
	assert(string[1]);
	assert(string[1] != '\\');
	assert(string[1] != '\'');
	assert(string[1] != '\n');
	assert(string[2] == '\'');

	return string + 3;
}

static const char * after_hexadecimal_literal(const char * string)
{
	assert(string);

	assert(string[0] == '0');
	assert(string[1] == 'x' || string[1] == 'X');
	assert(is_hexadecimal_digit(string[2]));
	assert(is_hexadecimal_digit(string[3]));

	return string + 4;
}

static const char * after_decimal_literal(const char * string)
{
	assert(string);

	while (is_decimal_digit(string[0]))
		++string;

	return string;
}

static const char * after_integer_literal(const char * string)
{
	assert(string);

	assert(is_decimal_digit(string[0]));

	if (string[0] == '0')
	{
		if (string[1] == 'x' || string[1] == 'X')
			return after_hexadecimal_literal(string);

		return string + 1;
	}

	return after_decimal_literal(string);
}

static const char * after_identifier(const char * string)
{
	assert(string);

	while (is_identifier_character(string[0]))
		++string;

	return string;
}



// keywords

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

static int length_of_keyword_at_start(const char * string)
{
	assert(string);

	for_each_index_in_array(keyword_index, keywords)
	{
		const char * keyword = keywords[keyword_index];
		size_t length_of_keyword = strlen(keyword);

		assert(length_of_keyword < INT_MAX);

		if (strncmp(string, keyword, length_of_keyword) == 0)
			return (int)length_of_keyword;
	}

	return 0;
}



// main 'get next tok' func

static const char * after_next_token(const char * string)
{
	assert(string);

	char character_0 = string[0];

	if (character_0 == '\'')
		return after_character_literal(string);

	if (is_decimal_digit(character_0))
		return after_integer_literal(string);

	int keyword_length = length_of_keyword_at_start(string);
	if (keyword_length > 0)
		return string + keyword_length;

	if (is_letter_or_underscore(character_0))
		return after_identifier(string);

	assert(false);
	return NULL;
}



// Helper to find the next '\n' (or '\0')

static const char * find_end_of_line(const char * string)
{
	char character;
	while ((character = string[0]) != '\0')
	{
		if (character == '\n')
			break;

		++string;
	}

	return string;
}



// helper to look for a char on a line
//  returns a pChz pointing at 1 of 3 things:
//  1. the first '\n' in pChzLine
//  2. the '\0' at the end of pChzLine
//  3. the first occurance of ch in pChzLine, before any '\n'

const char * find_character_or_end_of_line(char character_to_find, const char * string)
{
	char character;
	while ((character = string[0]) != '\0')
	{
		if (character == character_to_find)
			return string;

		if (character == '\n')
			return string;

		++string;
	}

	return string;
}



// print all of the tokens in a string,
//  in "clang -dump-tokens" format (roughly)

void print_tokens_in_string(const char * string)
{
	int current_line_number = 1;
	const char * beginning_of_current_line = string;

	bool in_block_comment = false; // :/

	char character_0;
	while ((character_0 = string[0]) != '\0')
	{
		char character_1 = string[1];

		if (in_block_comment)
		{
			if (character_0 == '\n')
			{
				++string;

				// Do not advance to the next 'line'
				//  if there is nothing after this '\n'
				//??? FIXME 

				if (character_1 != '\0')
				{
					++current_line_number;
					beginning_of_current_line = string;
				}
			}
			else if (character_0 == '*' && character_1 == '/')
			{
				in_block_comment = false;
				string += 2;
			}
			else
			{
				string = find_character_or_end_of_line('*', string);
			}
		}
		else if (character_0 == '/' && character_1 == '*')
		{
			in_block_comment = true;
			string += 2;
		}
		else if (character_0 == '/' && character_1 == '/')
		{
			string += 2;
			string = find_end_of_line(string);
		}
		else if (character_0 == '\n')
		{
			++string;

			// Do not advance to the next 'line'
			//  if there is nothing after this '\n'

			if (character_1 != '\0')
			{
				++current_line_number;
				beginning_of_current_line = string;
			}
		}
		else if (is_space_or_tab(character_0))
		{
			++string;
		}
		else
		{
			const char * after_token = after_next_token(string);
			assert(after_token);
			if (!after_token)
				break;

			long long length_of_token = after_token - string;
			assert(length_of_token > 0);
			assert(length_of_token < INT_MAX);

			// NOTE (matthewd) +1 here is because we want to display character 
			//  indicies with '1' being the left most character on the line

			long long index_of_start_of_token_on_line = string - beginning_of_current_line + 1;

			// NOTE (matthewd) "%.*s" is printf magic.
			//  printf("%.*s", number_of_characters_to_print, string) 
			//  will only print the first 'number_of_characters_to_print' 
			//  characters from the beginning of 'string'

			printf(
				"'%.*s' %d:%lld\n", 
				(int)length_of_token, 
				string,
				current_line_number,
				index_of_start_of_token_on_line);

			string = after_token;
		}
	}

	// NOTE (matthewd) not adding +1 to the character index here,
	//  to match how clang reports eofs...

	printf(
		"eof '' %d:%lld\n",
		current_line_number,
		string - beginning_of_current_line);
}



// test file names...

static const char * file_names[] =
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



// main

#define length_of_file_path_buffer 64
#define length_of_file_buffer 2048

int main (void)
{
	char file_path_buffer[length_of_file_path_buffer];
	memset(file_path_buffer, 0, length_of_file_path_buffer);

	const char * root_directory_path = "C:\\Users\\drape\\Desktop\\good_c\\";
	size_t length_of_root_directory_path = strlen(root_directory_path);

	assert(length_of_root_directory_path < length_of_file_path_buffer);
	strcpy(file_path_buffer, root_directory_path);

	char * file_name_buffer = file_path_buffer + length_of_root_directory_path;
	size_t length_of_file_name_buffer = length_of_file_path_buffer - length_of_root_directory_path;

	for_each_index_in_array(filename_index, file_names)
	{
		char file_buffer[length_of_file_buffer];
		memset(file_buffer, 0, length_of_file_buffer);

		{
			memset(file_name_buffer, 0, length_of_file_name_buffer);

			const char * file_name = file_names[filename_index];
			size_t length_of_file_name = strlen(file_name);

			assert(length_of_file_name < length_of_file_name_buffer);
			strcpy(file_name_buffer, file_name);

			FILE * file = fopen(file_path_buffer, "rb");
			assert(file);

			int seek_error = fseek(file, 0, SEEK_END);
			assert(!seek_error);

			long size_of_file = ftell(file);
			assert(size_of_file > 0);
			assert(size_of_file < length_of_file_buffer);

			seek_error = fseek(file, 0, SEEK_SET);
			assert(!seek_error);
			
			size_t bytes_read = fread(file_buffer, 1, (size_t)size_of_file, file);
			assert(bytes_read == (size_t)size_of_file);
			
			int close_error = fclose(file);
			assert(!close_error);
		}

		print_tokens_in_string(file_buffer);
	}

	return 0;
}