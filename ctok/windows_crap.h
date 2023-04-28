#pragma once

#include <stdint.h>

#define WC_FORMAT_MESSAGE_ALLOCATE_BUFFER   0x00000100
#define WC_FORMAT_MESSAGE_FROM_SYSTEM       0x00001000
#define WC_FORMAT_MESSAGE_IGNORE_INSERTS    0x00000200
#define WC_MAKELANGID(p, s)                 ((((unsigned short)(s)) << 10) | (unsigned short)(p))
#define WC_LANG_NEUTRAL                     0x00
#define WC_SUBLANG_DEFAULT                  0x01
#define WC_LMEM_ZEROINIT                    0x0040
#define WC_MB_OK                            0x00000000L
#define WC_MAX_PATH                         260
#define WC_INVALID_HANDLE_VALUE             ((void *)(uint64_t)-1)
#define WC_ERROR_NO_MORE_FILES              18L
#define WC_FILE_ATTRIBUTE_DIRECTORY         0x00000010  

uint32_t WC_GetLastError();

uint32_t WC_FormatMessageA(
    uint32_t dwFlags,
    const void * lpSource,
    uint32_t dwMessageId,
    uint32_t dwLanguageId,
    char * lpBuffer,
    uint32_t nSize,
    va_list * Arguments);

void * WC_LocalAlloc(
    uint32_t uFlags,
    uint64_t uBytes);

int WC_lstrlenA(
    const char * lpString);

long WC_StringCchPrintfA(
    char* pszDest,
    size_t cchDest,
    const char* pszFormat,
    ...);

uint64_t WC_LocalSize(
    void * hMem);

int WC_MessageBoxA(
    void * hWnd,
    const char * lpText,
    const char * lpCaption,
    uint32_t uType);

void * WC_LocalFree(
    void * hMem);

typedef struct {
    uint32_t dwLowDateTime;
    uint32_t dwHighDateTime;
} WC_FILETIME;

typedef struct  {
    uint32_t dwFileAttributes;
    WC_FILETIME ftCreationTime;
    WC_FILETIME ftLastAccessTime;
    WC_FILETIME ftLastWriteTime;
    uint32_t nFileSizeHigh;
    uint32_t nFileSizeLow;
    uint32_t dwReserved0;
    uint32_t dwReserved1;
    char cFileName[ WC_MAX_PATH ];
    char cAlternateFileName[ 14 ];

    char _padding[2];
} WC_WIN32_FIND_DATAA;

void * WC_FindFirstFileA(
    const char * lpFileName,
    WC_WIN32_FIND_DATAA * lpFindFileData);

int WC_FindNextFileA(
    void * hFindFile,
    WC_WIN32_FIND_DATAA * lpFindFileData);

long WC_StringCchLengthA(
    const char* psz,
    size_t cchMax,
    size_t* pcchLength);

long WC_StringCchCopyA(
    char* pszDest,
    size_t cchDest,
    const char* pszSrc);

char * WC_PathCombineA(
    char * pszDest, 
    const char * pszDir, 
    const char * pszFile);

int WC_FindClose(
    void * hFindFile);