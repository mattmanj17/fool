
#include <stdlib.h>
#include <string.h>

#include "windows_helpers.h"
#include "windows_crap.h"

void Display_error_box_and_exit(const char * msg, uint32_t exit_code)
{
	// Retrieve the system error message for the last-error code

	void * lpMsgBuf;
	void * lpDisplayBuf;
	uint32_t dw = WC_GetLastError(); 

	WC_FormatMessageA(
		WC_FORMAT_MESSAGE_ALLOCATE_BUFFER | 
		WC_FORMAT_MESSAGE_FROM_SYSTEM |
		WC_FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		dw,
		WC_MAKELANGID(WC_LANG_NEUTRAL, WC_SUBLANG_DEFAULT),
		(char *) &lpMsgBuf,
		0, 
		NULL);

	// Display the error message and clean up

	uint64_t size = 0;
	size += WC_lstrlenA((const char *)lpMsgBuf);
	size += WC_lstrlenA(msg);
	size += 40;

	lpDisplayBuf = (void *)WC_LocalAlloc(
							WC_LMEM_ZEROINIT, 
							size); 

	WC_StringCchPrintfA(
		(char *)lpDisplayBuf, 
		WC_LocalSize(lpDisplayBuf),
		"%s : GetLastError = %d: %s", 
		msg, 
		dw, 
		lpMsgBuf);

	WC_MessageBoxA(
		NULL, 
		(const char *)lpDisplayBuf, 
		"Error", 
		WC_MB_OK); 

	WC_LocalFree(lpMsgBuf);
	WC_LocalFree(lpDisplayBuf);

	exit((int)exit_code);
}

typedef bool (*walk_dir_callback)(
	const char * root_dir, 
	const char * this_dir, 
	const char * full_path);

typedef void (*walk_file_callback)(
	const char * root_dir, 
	const char * this_file, 
	const char * full_path);

void Walk_dir(
	const char * dir, 
	walk_dir_callback dir_callback,
	walk_file_callback file_callback)
{
	WC_WIN32_FIND_DATAA ffd;
	char szDir[WC_MAX_PATH];
	size_t length_of_arg;
	void * hFind = WC_INVALID_HANDLE_VALUE;
	uint32_t dwError=0;

	WC_StringCchLengthA(dir, WC_MAX_PATH, &length_of_arg);

	if (length_of_arg > (WC_MAX_PATH - 3))
	{
		Display_error_box_and_exit("directory path is too long", dwError);
		return;
	}

	WC_StringCchCopyA(szDir, WC_MAX_PATH, dir);
	WC_StringCchCopyA(szDir, WC_MAX_PATH, "\\*");

	hFind = WC_FindFirstFileA(szDir, &ffd);

	if (WC_INVALID_HANDLE_VALUE == hFind) 
	{
		Display_error_box_and_exit("error from FindFirstFileA", dwError);
		return;
	}

	do
	{
		if (ffd.dwFileAttributes & WC_FILE_ATTRIBUTE_DIRECTORY)
		{
			if (strcmp(ffd.cFileName, ".") == 0)
				continue;

			if (strcmp(ffd.cFileName, "..") == 0)
				continue;

			char buffer[WC_MAX_PATH];

			(void) WC_PathCombineA(buffer, dir, ffd.cFileName);

			if (!dir_callback(dir, ffd.cFileName, buffer))
				continue;

			Walk_dir(buffer, dir_callback, file_callback);
		}
		else
		{
			char buffer[WC_MAX_PATH];

			(void) WC_PathCombineA(buffer, dir, ffd.cFileName);

			file_callback(dir, ffd.cFileName, buffer);
		}
	}
	while (WC_FindNextFileA(hFind, &ffd) != 0);

	dwError = WC_GetLastError();
	if (dwError != WC_ERROR_NO_MORE_FILES)
	{
		Display_error_box_and_exit("error from FindNextFileA", dwError);
	}

	WC_FindClose(hFind);
}