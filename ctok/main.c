
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <limits.h>



// char classes

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



// "after" functions

static const char * after_char_lit(const char * pChz)
{
	assert(pChz);

	assert(pChz[0] == '\'');
	assert(pChz[1]);
	assert(pChz[1] != '\\');
	assert(pChz[1] != '\'');
	assert(pChz[1] != '\n');
	assert(pChz[2] == '\'');

	return pChz + 3;
}

static const char * after_hex_lit(const char * pChz)
{
	assert(pChz);

	assert(pChz[0] == '0');
	assert(pChz[1] == 'x' || pChz[1] == 'X');
	assert(is_hex_digit(pChz[2]));
	assert(is_hex_digit(pChz[3]));

	return pChz + 4;
}

static const char * after_decimal_lit(const char * pChz)
{
	assert(pChz);

	while (is_digit(pChz[0]))
		++pChz;

	return pChz;
}

static const char * after_int_lit(const char * pChz)
{
	assert(pChz);

	assert(is_digit(pChz[0]));

	if (pChz[0] == '0')
	{
		if (pChz[1] == 'x' || pChz[1] == 'X')
			return after_hex_lit(pChz);

		return pChz + 1;
	}

	return after_decimal_lit(pChz);
}

static const char * after_id(const char * pChz)
{
	assert(pChz);

	while (is_id_char(pChz[0]))
		++pChz;

	return pChz;
}



// keywords

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

static int len_next_kw(const char * pChz)
{
	assert(pChz);

	const char ** ppChzKw = g_apChzKw;
	while (**ppChzKw)
	{
		const char * pChzKw = *ppChzKw;
		size_t len = strlen(pChzKw);

		assert(len < INT_MAX);

		++ppChzKw;

		if (strncmp(pChz, pChzKw, len) == 0)
			return (int)len;
	}

	return 0;
}



// main 'get next tok' func

static const char * after_next_tok(const char * pChz)
{
	assert(pChz);

	char ch = pChz[0];

	if (ch == '\'')
		return after_char_lit(pChz);

	if (is_digit(ch))
		return after_int_lit(pChz);

	int kw_len = len_next_kw(pChz);
	if (kw_len)
		return pChz + kw_len;

	if (is_letter_or_underscore(ch))
		return after_id(pChz);

	assert(0);
	return NULL;
}



// deal with whitespace + comments

static const char * after_line_comment(const char * pChz)
{
	assert(pChz);
	assert(pChz[0] == '/');
	assert(pChz[1] == '/');
	
	pChz += 2;

	while (1)
	{
		char ch = pChz[0];
		if (!ch)
			break;

		if (ch == '\n')
			break;

		++pChz;
	}

	return pChz;
}

// Block comments are special : they can span multiple lines
//  So, we have a little struct we return from them to encode 
//  the two values we care about. It fits in 64 bits, so it
//  can still be passed in a register.

typedef struct {
	int m_dpChz_;
	short m_dLine_;
	short m_iChInLine;
} SAfterLines;

static SAfterLines after_block_comment(const char * pChz)
{
	assert(pChz);
	assert(pChz[0] == '/');
	assert(pChz[1] == '*');

	const char * pChzOrig = pChz;

	pChz += 2;

	SAfterLines afterlines = {0};

	while (1)
	{
		char ch = pChz[0];
		if (!ch)
			break;

		if (ch == '*' && pChz[1] == '/')
		{
			pChz += 2;
			afterlines.m_iChInLine += 2;
			break;
		}

		if (ch == '\n')
		{
			++afterlines.m_dLine_;
			afterlines.m_iChInLine = 0;
		}

		++pChz;
	}

	afterlines.m_dpChz_ = (int)(pChz - pChzOrig);
	return afterlines;
}

static SAfterLines after_comments_and_whitespace(const char * pChz)
{
	assert(pChz);

	const char * pChzOrig = pChz;

	SAfterLines afterlines0 = {0};

	while (1)
	{
		char ch0 = pChz[0];
		if (is_white(ch0))
		{
			if (ch0 == '\n')
			{
				++afterlines0.m_dLine_;
				afterlines0.m_iChInLine = 0;
			}
			else
			{
				++afterlines0.m_iChInLine;
			}

			++pChz;
			continue;
		}

		if (ch0 == '/')
		{
			char ch1 = pChz[1];
			if (ch1 == '/')
			{
				pChz = after_line_comment(pChz);
				++afterlines0.m_dLine_; //??? wrong if line comment was eof
				afterlines0.m_iChInLine = 0;
				continue;
			}
			else if (ch1 == '*')
			{
				SAfterLines afterlines1 = after_block_comment(pChz);
				pChz += afterlines1.m_dpChz_;
				afterlines0.m_dLine_ += afterlines1.m_dLine_;
				afterlines0.m_iChInLine = afterlines1.m_iChInLine;
				continue;
			}
		}

		break;
	}

	afterlines0.m_dpChz_ = (int)(pChz - pChzOrig);
	return afterlines0;
}



// print all of the tokens in a string,
//  in "clang -dump-tokens" format (roughly)

void print_toks_in_pchz(const char * pChz)
{
	int line = 1;
	const char * pChzLineMic = pChz;

	while (1)
	{
		SAfterLines afterlines = after_comments_and_whitespace(pChz);
		pChz += afterlines.m_dpChz_;

		if (afterlines.m_dLine_)
		{
			line += afterlines.m_dLine_;
			pChzLineMic = pChz - afterlines.m_iChInLine;
		}

		if (!pChz[0])
			break;

		const char * pChzTokMac = after_next_tok(pChz);
		assert(pChzTokMac);
		if (!pChzTokMac)
			break;

		printf(
			"'%.*s' %d:%lld\n", 
			(int)(pChzTokMac - pChz), 
			pChz,
			line,
			pChz - pChzLineMic + 1);

		pChz = pChzTokMac;
	}

	//??? FIXME line:col is wrong...

	printf(
		"eof '' %d:%lld\n",
		line,
		pChz - pChzLineMic);
}



// test file names...

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



// main

int main (void)
{
	char aChPathBuffer[64];
	memset(aChPathBuffer, 0, 64);

	const char * pChzRootDir = "C:\\Users\\drape\\Desktop\\good_c\\";
	strcpy(aChPathBuffer, pChzRootDir);

	char * pChPathBufferEnd = aChPathBuffer + strlen(pChzRootDir);

	char aChFileBufer[2048];

	const char ** ppChzFileName = g_apChzFileName;
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
			assert(size < 2048);
			err = fseek(f, 0, SEEK_SET);
			assert(!err);
			
			size_t read = fread(aChFileBufer, 1, (size_t)size, f);
			assert(read == (size_t)size);
			
			err = fclose(f);
			assert(!err);
		}
		
		print_toks_in_pchz(aChFileBufer);

		++ppChzFileName;
	}

	return 0;
}