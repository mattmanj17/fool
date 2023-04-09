
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <limits.h>

extern const char ** g_ppChzFileName;

extern const char ** g_ppChzKw;

static const char * after_new_line(const char * pChz);

static void print_toks_on_line(int line, const char * pChzMic, const char * pChzMac);

static char peek(int i, const char * pChzMic, const char * pChzMac);

static const char * next_tok_start(const char * pChzMic, const char * pChzMac);
static const char * after_line_comment(const char * pChzMic, const char * pChzMac);
static const char * after_block_comment(const char * pChzMic, const char * pChzMac);

static const char * after_next_tok(const char * pChzMic, const char * pChzMac);
static const char * after_char_lit(const char * pChzMic, const char * pChzMac);
static const char * after_int_lit(const char * pChzMic, const char * pChzMac);
static int len_next_kw(const char * pChzMic, const char * pChzMac);
static const char * after_id(const char * pChzMic, const char * pChzMac);

static const char * after_hex_lit(const char * pChzMic, const char * pChzMac);
static const char * after_decimal_lit(const char * pChzMic, const char * pChzMac);

static int is_white(char ch);
static int is_digit(char ch);
static int is_hex_digit(char ch);
static int is_letter_or_underscore(char ch);
static int is_id_char(char ch);

int main (void)
{
	char aChPathBuffer[64];
	memset(aChPathBuffer, 0, 64);

	const char * pChzRootDir = "C:\\Users\\drape\\Desktop\\good_c\\";
	strcpy(aChPathBuffer, pChzRootDir);

	char * pChPathBufferEnd = aChPathBuffer + strlen(pChzRootDir);

	char aChFileBufer[2048];

	const char ** ppChzFileName = g_ppChzFileName;
	while (**ppChzFileName)
	{
		memset(aChFileBufer, 0, 2048);

		{
			const char * pChzFileName = *ppChzFileName;
			strcpy(pChPathBufferEnd, pChzFileName);

			FILE * f = fopen(aChPathBuffer, "rb");
			assert(f);

			int err = fseek(f, 0, SEEK_END);
			assert(!err);
			long size = ftell(f);
			assert(size > 0);
			err = fseek(f, 0, SEEK_SET);
			assert(!err);
			
			size_t read = fread(aChFileBufer, 1, (size_t)size, f);
			assert(read == (size_t)size);
			
			err = fclose(f);
			assert(!err);
		}
		
		int line = 1;
		const char * pChzLineMic = aChFileBufer;
		const char * pChzLineMac = after_new_line(aChFileBufer);

		while (1)
		{
			// FIXME fuck this does not work
			//  need to skip past block comments
			//  that is the only 'token' that
			//  can span multiple lines

			print_toks_on_line(line, pChzLineMic, pChzLineMac);

			if (!pChzLineMac[0])
				break;

			++line;
			pChzLineMic = pChzLineMac;
			pChzLineMac = after_new_line(pChzLineMac);
		}

		printf(
			"eof '' %d:%lld\n",
			line,
			pChzLineMac - pChzLineMic);

		++ppChzFileName;
	}

	return 0;
}

static const char * after_new_line(const char * pChz)
{
	while (1)
	{
		char ch = pChz[0];

		if (!pChz[0])
			break;

		++pChz;

		if (ch == '\n')
			break;
	}

	return pChz;
}

static void print_toks_on_line(
	int line, 
	const char * pChzLineMic, 
	const char * pChzLineMac)
{
	const char * pChzCursor = pChzLineMic;

	while (pChzCursor < pChzLineMac)
	{
		pChzCursor = next_tok_start(pChzCursor, pChzLineMac); // FIXME move this up a level, and include comments...
		assert(pChzCursor);
		assert(pChzCursor <= pChzLineMac);
		if (!*pChzCursor)
			break;

		if (pChzCursor == pChzLineMac)
			break;

		const char * pChzCursorNext = after_next_tok(pChzCursor, pChzLineMac);
		if (!pChzCursorNext) // only happens in error cases...
			break;

		printf(
			"'%.*s' %d:%lld\n", 
			(int)(pChzCursorNext - pChzCursor), 
			pChzCursor,
			line,
			pChzCursor - pChzLineMic + 1);

		pChzCursor = pChzCursorNext;
	}
}

static char peek(int i, const char * pChzMic, const char * pChzMac)
{
	const char * pChz = pChzMic + i;

	if (pChz >= pChzMac)
		return '\0';

	return *pChz;
}

static const char * next_tok_start(const char * pChzMic, const char * pChzMac)
{
	assert(pChzMic);
	assert(pChzMac);
	assert(pChzMic < pChzMac);

	while (pChzMic < pChzMac)
	{
		char ch0 = pChzMic[0];
		if (is_white(ch0))
		{
			++pChzMic;
			continue;
		}

		if (ch0 == '/')
		{
			char ch1 = peek(1, pChzMic, pChzMac);
			if (ch1 == '/')
			{
				pChzMic = after_line_comment(pChzMic, pChzMac);
				continue;
			}
			else if (ch1 == '*')
			{
				pChzMic = after_block_comment(pChzMic, pChzMac);
				continue;
			}
		}

		break;
	}

	return pChzMic;
}

static const char * after_line_comment(const char * pChzMic, const char * pChzMac)
{
	assert(pChzMic);
	assert(pChzMac);
	assert(pChzMic < pChzMac);
	assert(pChzMic[0] == '/');
	assert(peek(1, pChzMic, pChzMac) == '/');
	
	pChzMic += 2;

	while (pChzMic < pChzMac)
	{
		char ch = pChzMic[0];
		if (!ch)
			break;

		if (ch == '\n')
			break;

		++pChzMic;
	}

	return pChzMic;
}

static const char * after_block_comment(const char * pChzMic, const char * pChzMac)
{
	assert(pChzMic);
	assert(pChzMac);
	assert(pChzMic < pChzMac);
	assert(pChzMic[0] == '/');
	assert(peek(1, pChzMic, pChzMac) == '*');

	pChzMic += 2;

	while (pChzMic < pChzMac)
	{
		char ch = pChzMic[0];
		if (!ch)
			return pChzMic;

		if (ch == '*' && peek(1, pChzMic, pChzMac) == '/')
			return pChzMic + 2;

		++pChzMic;
	}

	return pChzMic;
}

static const char * after_next_tok(const char * pChzMic, const char * pChzMac)
{
	assert(pChzMic);
	assert(pChzMac);
	assert(pChzMic < pChzMac);

	char ch = pChzMic[0];
	assert(ch);

	if (ch == '\'')
		return after_char_lit(pChzMic, pChzMac);

	if (is_digit(ch))
		return after_int_lit(pChzMic, pChzMac);

	int kw_len = len_next_kw(pChzMic, pChzMac);
	if (kw_len)
		return pChzMic + kw_len;

	if (is_letter_or_underscore(ch))
		return after_id(pChzMic, pChzMac);

	assert(0);
	return NULL;
}

static const char * after_char_lit(const char * pChzMic, const char * pChzMac)
{
	assert(pChzMic);
	assert(pChzMac);
	assert(pChzMic < pChzMac);

	assert(pChzMic[0] == '\'');
	assert(peek(1, pChzMic, pChzMac));
	assert(peek(1, pChzMic, pChzMac) != '\\');
	assert(peek(2, pChzMic, pChzMac) == '\'');

	return pChzMic + 3;
}

static const char * after_int_lit(const char * pChzMic, const char * pChzMac)
{
	assert(pChzMic);
	assert(pChzMac);
	assert(pChzMic < pChzMac);

	assert(is_digit(pChzMic[0]));

	if (pChzMic[0] == '0')
	{
		char ch1 = peek(1, pChzMic, pChzMac);
		if (ch1 == 'x' || ch1 == 'X')
			return after_hex_lit(pChzMic, pChzMac);

		return pChzMic + 1;
	}

	return after_decimal_lit(pChzMic, pChzMac);
}

static const char * after_hex_lit(const char * pChzMic, const char * pChzMac)
{
	assert(pChzMic);
	assert(pChzMac);
	assert(pChzMic < pChzMac);

	assert(pChzMic[0] == '0');
	assert(peek(1, pChzMic, pChzMac) == 'x' || peek(1, pChzMic, pChzMac) == 'X');
	assert(is_hex_digit(peek(2, pChzMic, pChzMac)));
	assert(is_hex_digit(peek(3, pChzMic, pChzMac)));

	return pChzMic + 4;
}

static const char * after_decimal_lit(const char * pChzMic, const char * pChzMac)
{
	assert(pChzMic);
	assert(pChzMac);
	assert(pChzMic < pChzMac);

	while ((pChzMic < pChzMac) && is_digit(*pChzMic))
		++pChzMic;

	return pChzMic;
}

static int len_next_kw(const char * pChzMic, const char * pChzMac)
{
	assert(pChzMic);
	assert(pChzMac);
	assert(pChzMic < pChzMac);

	long long len_line = pChzMac - pChzMic;
	assert(len_line >= 0);

	const char ** ppChzKw = g_ppChzKw;
	while (**ppChzKw)
	{
		const char * pChzKw = *ppChzKw;
		size_t len = strlen(pChzKw);

		assert(len < INT_MAX);

		++ppChzKw;

		if (len > (size_t)len_line)
			continue;

		if (strncmp(pChzMic, pChzKw, len) == 0)
			return (int)len;
	}

	return 0;
}

static const char * after_id(const char * pChzMic, const char * pChzMac)
{
	assert(pChzMic);
	assert(pChzMac);
	assert(pChzMic < pChzMac);

	while ((pChzMic < pChzMac) && is_id_char(pChzMic[0]))
		++pChzMic;

	return pChzMic;
}

static int is_white(char ch)
{
	if (!ch)
		return 0;

	return strchr(" \t\n", ch) != NULL;
}

static int is_digit(char ch)
{
	if (!ch)
		return 0;

	return strchr("0123456789", ch) != NULL;
}

static int is_hex_digit(char ch)
{
	if (!ch)
		return 0;

	const char * pChz = 
		"0123456789"
		"abcdef"
		"ABCDEF";

	return strchr(pChz, ch) != NULL;
}

static int is_letter_or_underscore(char ch)
{
	if (!ch)
		return 0;

	const char * pChz = 
		"_"
		"abcdefghijklmnopqrstuvwxyz"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ";

	return strchr(pChz, ch) != NULL;
}

static int is_id_char(char ch)
{
	return is_digit(ch) || is_letter_or_underscore(ch);
}

static const char * g_apChzKw[] =
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

	"\0",
};

const char ** g_ppChzKw = g_apChzKw;

static const char * g_apChzFileName[] =
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

	"\0",
};

const char ** g_ppChzFileName = g_apChzFileName;