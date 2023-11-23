
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

/*
	This blob of code exists to work around weird whitespace handling in clang.

	I think no one really uses raw lexing mode that much (at least for non-debug reasons),
	and so it does not have to have the 'tighest' output ever.

	However, while trying to replicate the clang raw lexer exactly, these weirnesses are a drag.
	So, I have decided to just scrub our test input to remove cases where my lexer and clang
	-dump-raw-tokens would disagree. They are all cases involving whitespace, which does not
	even matter in later lexing stages, so I can live with this.

	Three examples of weird clang cases:

	- The string '\n\t' becomes one token, but the string '\t\n' becomes
	  two tokens ('\t' and '\n'). My lexer returns one token in both cases.

	- The string '\\\n\n\n' becomes one token, but the string '\n\\\\n\n' becomes
	  two tokens ('\n' and '\\\n\n'). My lexer returns one token in both cases.

	- Unterminated string literals (and line comments) which end in '\\\n\n' behave
	  strangly. Namely, clang will turn '"foo\\\n\n' into two tokens,
	  '"foo\\\n' and '\n'. My lexer still returns two tokens, but it splits it as
	  '"foo' and '\\\n\n'

	To work around this, any time there is a block of 'whitespace' (that is,
	escaped line breaks, normal line breaks, or horizontal whitespace),
	we sort it so escaped line breaks come first, normal line breaks come second, and
	horizontal whitespace comes last. we also ensure a single space character between
	the escaped line breaks and the normal line breaks, to prevent the 
	"unterminated string literal" case described above.
*/

size_t Len_leading_bslash(char * input, char * end)
{
	long long signed_input_length = end - input;
	if (signed_input_length <= 0)
		return 0;

	if (input[0] == '\\')
		return 1;

	// Handle ??/ trigraph

	if (signed_input_length > 2 && 
		input[0] == '?' &&
		input[1] == '?' &&
		input[2] == '/')
		return 3;

	return 0;
}

bool Is_hz_ws(char ch)
{
	switch (ch)
	{
	case ' ':
	case '\t':
	case '\f':
	case '\v':
		return true;

	default:
		return false;
	}
}

size_t Len_leading_eol(char * input, char * end)
{
	if (input >= end)
		return 0;

	if (input[0] == '\n')
		return 1;

	if (input[0] == '\r')
	{
		if (end - input > 1 && input[1] == '\n')
			return 2;

		return 1;
	}

	return 0;
}

size_t Len_leading_esc_eol(char * input, char * end)
{
	size_t length = Len_leading_bslash(input, end);
	if (length == 0)
		return 0;

	while (input < end)
	{
		if (!Is_hz_ws(input[length]))
			break;

		++length;
	}

	size_t len_leading_eol = Len_leading_eol(input + length, end);
	if (len_leading_eol == 0)
		return 0;

	return length + len_leading_eol;
}

typedef struct 
{
	char * esc_eols;
	char * eols;
	char * hz_ws;
} ws_buffers_t;

typedef struct 
{
	size_t len_esc_eol;
	size_t len_eol;
	size_t len_hz_ws;
} ws_lengths_t;

void Sort_leading_ws(
	char * input, 
	char * end, 
	const ws_buffers_t * ws_buffers_ref,
	ws_lengths_t * ws_lengths_out)
{
	assert(ws_buffers_ref);
	assert(ws_buffers_ref->esc_eols);
	assert(ws_buffers_ref->eols);
	assert(ws_buffers_ref->hz_ws);

	char * esc_eols = ws_buffers_ref->esc_eols;
	char * eols = ws_buffers_ref->eols;
	char * hz_ws = ws_buffers_ref->hz_ws;

	size_t len_esc_eol = 0;
	size_t len_eol = 0;
	size_t len_hz_ws = 0;

	while (input < end)
	{
		size_t len_leading_esc_eol = Len_leading_esc_eol(input, end);
		size_t len_leading_eol = Len_leading_eol(input, end);

		if (len_leading_esc_eol != 0)
		{
			memcpy(
				esc_eols, 
				input, 
				len_leading_esc_eol);

			len_esc_eol += len_leading_esc_eol;
			esc_eols += len_leading_esc_eol;

			input += len_leading_esc_eol;
		}
		else if (len_leading_eol != 0)
		{
			memcpy(
				eols, 
				input, 
				len_leading_eol);

			len_eol += len_leading_eol;
			eols += len_leading_eol;

			input += len_leading_eol;
		}
		else if (Is_hz_ws(input[0]))
		{
			hz_ws[0] = input[0];

			++len_hz_ws;
			++hz_ws;

			++input;
		}
		else
		{
			break;
		}
	}

	assert(ws_lengths_out);
	ws_lengths_out->len_esc_eol = len_esc_eol;
	ws_lengths_out->len_eol = len_eol;
	ws_lengths_out->len_hz_ws = len_hz_ws;
}

#define exit_msg(msg) do {printf("%s line %d : %s", __FILE__, __LINE__, msg); exit(1);} while(0)

int wmain(int argc, wchar_t *argv[])
{
	if (argc != 3)
		exit_msg("wrong number of args");

	wchar_t * input_path = argv[1];
	wchar_t * output_path = argv[2];

	char * input;
	size_t input_length;
	{
		FILE * input_file = _wfopen(input_path, L"rb");
		bool fopen_failed = (input_file == NULL);
		if (fopen_failed)
			exit_msg("fopen_failed");

		// get file_length

		bool fseek_failed = (fseek(input_file, 0, SEEK_END) != 0);
		if (fseek_failed)
			exit_msg("fseek_failed");

		long signed_file_length = ftell(input_file);
		bool ftell_failed = (signed_file_length < 0);
		if (ftell_failed)
			exit_msg("ftell_failed");

		input_length = (size_t)signed_file_length;

		// return to start of file

		fseek_failed = (fseek(input_file, 0, SEEK_SET) != 0);
		if (fseek_failed)
			exit_msg("fseek_failed");

		// read file bytes and close

		input = (char *)calloc(input_length, 1);
		bool calloc_failed = (input == NULL);
		if (calloc_failed)
			exit_msg("calloc_failed");

		size_t bytes_read = fread(input, 1, input_length, input_file);
		bool fread_failed = (bytes_read != input_length);
		if (fread_failed)
			exit_msg("fread_failed");

		bool fclose_failed = (fclose(input_file) != 0);
		if (fclose_failed)
			exit_msg("fclose_failed");
	}

	// output twice the size of input, + keep track out output length

	size_t output_length = 0;

	char * output = (char *)calloc(input_length * 2, 1);
	bool calloc_failed = (output == NULL);
	if (calloc_failed)
		exit_msg("calloc_failed");

	// intermediate buffers while sorting ws

	ws_buffers_t ws_buffers = {0};

	ws_buffers.hz_ws = (char *)calloc(input_length, 1);
	calloc_failed = (ws_buffers.hz_ws == NULL);
	if (calloc_failed)
		exit_msg("calloc_failed");

	ws_buffers.eols = (char *)calloc(input_length, 1);
	calloc_failed = (ws_buffers.eols == NULL);
	if (calloc_failed)
		exit_msg("calloc_failed");

	ws_buffers.esc_eols = (char *)calloc(input_length, 1);
	calloc_failed = (ws_buffers.esc_eols == NULL);
	if (calloc_failed)
		exit_msg("calloc_failed");
	
	// copy input to output, sorting blocks of ws how we want

	char * end = input + input_length;
	while (input < end)
	{
		ws_lengths_t ws_lengths = {0};
		Sort_leading_ws(
			input, 
			end, 
			&ws_buffers, 
			&ws_lengths);

		// was the leading char not ws? just copy it over

		size_t len_ws = ws_lengths.len_esc_eol + ws_lengths.len_eol + ws_lengths.len_hz_ws;
		if (len_ws == 0)
		{
			output[output_length] = input[0];

			++output_length;
			++input;

			continue;
		}

		// append esc_eol, then eol, then hz_ws to output

		if (ws_lengths.len_esc_eol)
		{
			memcpy(
				output + output_length, 
				ws_buffers.esc_eols, 
				ws_lengths.len_esc_eol);

			output_length += ws_lengths.len_esc_eol;
		}

		if (ws_lengths.len_eol)
		{
			if (ws_lengths.len_esc_eol)
			{
				// separate esc_eol and eol, to avoid 
				//  "unterminated string literal" case described above

				output[output_length] = ' ';
				++output_length;
			}

			memcpy(
				output + output_length,
				ws_buffers.eols,
				ws_lengths.len_eol);

			output_length += ws_lengths.len_eol;
		}

		if (ws_lengths.len_hz_ws)
		{
			memcpy(
				output + output_length, 
				ws_buffers.hz_ws, 
				ws_lengths.len_hz_ws);

			output_length += ws_lengths.len_hz_ws;
		}

		input += len_ws;
	}

	// make sure we end with a clean unescaped newline

	output[output_length] = '/';
	++output_length;

	output[output_length] = '*';
	++output_length;

	output[output_length] = '*';
	++output_length;

	output[output_length] = '/';
	++output_length;

	output[output_length] = '\n';
	++output_length;

	// write output to file

	FILE * output_file = _wfopen(output_path, L"wb");
	bool fopen_failed = (output_file == NULL);
	if (fopen_failed)
		exit_msg("fopen_failed");

	size_t bytes_written = fwrite(output, 1, output_length, output_file);
	bool fwrite_failed = (bytes_written < output_length);
	if (fwrite_failed)
		exit_msg("fwrite_failed");
}