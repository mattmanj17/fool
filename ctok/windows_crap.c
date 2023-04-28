
#include <windows.h>
#include <strsafe.h>
#include <shlwapi.h>
#include <assert.h>

#include "windows_crap.h"

#pragma comment(lib, "Shlwapi.lib")

#define CASSERT(f) static_assert((f), #f)

uint32_t WC_GetLastError()
{
    return GetLastError();
}

uint32_t WC_FormatMessageA(
    uint32_t dwFlags,
    const void * lpSource,
    uint32_t dwMessageId,
    uint32_t dwLanguageId,
    char * lpBuffer,
    uint32_t nSize,
    va_list * Arguments)
{
    return FormatMessageA(
            dwFlags, 
            lpSource, 
            dwMessageId, 
            dwLanguageId, 
            lpBuffer, 
            nSize, 
            Arguments);
}

void * WC_LocalAlloc(
    uint32_t uFlags,
    uint64_t uBytes)
{
    return LocalAlloc(uFlags, uBytes);
}

int WC_lstrlenA(
    const char * lpString)
{
    return lstrlenA(lpString);
}

long WC_StringCchPrintfA(
    char* pszDest,
    size_t cchDest,
    const char* pszFormat,
    ...)
{
    HRESULT hr;

    hr = StringValidateDestA(pszDest, cchDest, STRSAFE_MAX_CCH);

    if (SUCCEEDED(hr))
    {
        va_list argList;

        va_start(argList, pszFormat);

        hr = StringVPrintfWorkerA(pszDest,
                cchDest,
                NULL,
                pszFormat,
                argList);

        va_end(argList);
    }
    else if (cchDest > 0)
    {
        *pszDest = '\0';
    }

    return hr;
}

uint64_t WC_LocalSize(
    void * hMem)
{
    return LocalSize(hMem);
}

int WC_MessageBoxA(
    void * hWnd,
    const char * lpText,
    const char * lpCaption,
    uint32_t uType)
{
    return MessageBoxA((HWND)hWnd, lpText, lpCaption, uType);
}

void * WC_LocalFree(
    void * hMem)
{
    return LocalFree(hMem);
}

long WC_StringCchLengthA(
    const char* psz,
    size_t cchMax,
    size_t* pcchLength)
{
    HRESULT hr;

    if ((psz == NULL) || (cchMax > STRSAFE_MAX_CCH))
    {
        hr = STRSAFE_E_INVALID_PARAMETER;
    }
    else
    {
        hr = StringLengthWorkerA(psz, cchMax, pcchLength);
    }

    if (FAILED(hr) && pcchLength)
    {
        *pcchLength = 0;
    }

    return hr;
}

long WC_StringCchCopyA(
    char* pszDest,
    size_t cchDest,
    const char* pszSrc)
{
    HRESULT hr;

    hr = StringValidateDestA(pszDest, cchDest, STRSAFE_MAX_CCH);

    if (SUCCEEDED(hr))
    {
        hr = StringCopyWorkerA(pszDest,
                cchDest,
                NULL,
                pszSrc,
                STRSAFE_MAX_LENGTH);
    }
    else if (cchDest > 0)
    {
        *pszDest = '\0';
    }

    return hr;
}

char * WC_PathCombineA(
    char * pszDest, 
    const char * pszDir, 
    const char * pszFile)
{
    return PathCombineA(pszDest, pszDir, pszFile);
}

int WC_FindNextFileA(
    void * hFindFile,
    WC_WIN32_FIND_DATAA * lpFindFileData)
{
    CASSERT(sizeof(WC_WIN32_FIND_DATAA) == sizeof(_WIN32_FIND_DATAA));
    return FindNextFileA(hFindFile, (LPWIN32_FIND_DATAA)lpFindFileData);
}

void * WC_FindFirstFileA(
    const char * lpFileName,
    WC_WIN32_FIND_DATAA * lpFindFileData)
{
    CASSERT(sizeof(WC_WIN32_FIND_DATAA) == sizeof(_WIN32_FIND_DATAA));
    return FindFirstFileA(lpFileName, (LPWIN32_FIND_DATAA)lpFindFileData);
}

int WC_FindClose(
    void * hFindFile)
{
    return FindClose(hFindFile);
}