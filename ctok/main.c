
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include "print_tokens.h"

static bool FTryReadWholeFileAtPath(
	const wchar_t * fpath,
	const char ** ppChBegin,
	const char ** ppChEnd);

int wmain(int argc, wchar_t *argv[])
{
	// BUG do real argument parsing....

	wchar_t * pWChFile = NULL;
	bool fRaw = false;
	if (argc == 3)
	{
		if (wcscmp(argv[1], L"-raw") != 0)
		{
			printf("bad argumens, expected '-raw <file path>'\n");
			return 1;
		}
		else
		{
			pWChFile = argv[2];
			fRaw = true;
		}
	}
	else if (argc != 2)
	{
		printf("wrong number of arguments, only expected a file path\n");
		return 1;
	}
	else
	{
		pWChFile = argv[1];
	}

	// Read file

	const char * pChBegin;
	const char * pChEnd;
	bool success = FTryReadWholeFileAtPath(pWChFile, &pChBegin, &pChEnd);
	if (!success)
	{
		printf("Failed to read file '%ls'.\n", pWChFile);
		return 1;
	}

	// Deal with potential UTF-8 BOM

	int num_ch = (int)(pChEnd - pChBegin);
	if (num_ch >= 3 &&
		pChBegin[0] == '\xEF' &&
		pChBegin[1] == '\xBB' &&
		pChBegin[2] == '\xBF')
	{
		pChBegin += 3;
	}

	// Print tokens

	if (fRaw)
	{
		PrintRawTokens(pChBegin, pChEnd);
	}
	else
	{
		printf("PrintTokens Not yet implemented!!!\n");
		//PrintTokens(pChBegin, pChEnd);
	}
}

static bool FTryReadWholeFile(
	FILE * file,
	const char ** ppChBegin,
	const char ** ppChEnd)
{
	int err = fseek(file, 0, SEEK_END);
	if (err)
		return false;

	long len_file = ftell(file);
	if (len_file < 0)
		return false;

	err = fseek(file, 0, SEEK_SET);
	if (err)
		return false;

	// We allocated a trailing '\0' for sanity

	char * pChAlloc = (char *)calloc((size_t)(len_file + 1), 1);
	if (!pChAlloc)
		return false;

	size_t bytes_read = fread(pChAlloc, 1, (size_t)len_file, file);
	if (bytes_read != (size_t)len_file)
	{
		free(pChAlloc);
		return false;
	}

	*ppChBegin = pChAlloc;
	*ppChEnd = pChAlloc + len_file;

	return true;
}

static bool FTryReadWholeFileAtPath(
	const wchar_t * fpath,
	const char ** ppChBegin,
	const char ** ppChEnd)
{
	FILE * file = _wfopen(fpath, L"rb");
	if (!file)
		return false;

	bool fRead = FTryReadWholeFile(file, ppChBegin, ppChEnd);

	fclose(file); // BUG (matthewd) ignoring return value?

	return fRead;
}
