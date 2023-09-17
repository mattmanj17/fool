#include <stdio.h>

#include "alloc.h"
#include "file.h"
#include "panic.h"

static bool Try_read_file_to_buffer(FILE * file, bounded_c_str_t * bstr)
{
	bstr->cursor = NULL;
	bstr->terminator = NULL;

	int err = fseek(file, 0, SEEK_END);
	if (err)
		Panic("fseek failed");

	long len_file = ftell(file);
	if (len_file < 0)
		Panic("ftell failed");

	err = fseek(file, 0, SEEK_SET);
	if (err)
		Panic("fseek failed");

	void * allocation = Allocate(len_file + 1);

	size_t bytes_read = fread(allocation, 1, (size_t)len_file, file);
	if (bytes_read != (size_t)len_file)
		Panic("fread failed");

	bstr->cursor = (char *)allocation;
	bstr->terminator = bstr->cursor + len_file;

	return true;
}

bool Try_read_file_at_path_to_buffer(const wchar_t * fpath, bounded_c_str_t * bstr)
{
	bstr->cursor = NULL;
	bstr->terminator = NULL;

	FILE * file = _wfopen(fpath, L"rb");
	if (!file)
		return false;

	bool success = Try_read_file_to_buffer(file, bstr);

	fclose(file); // BUG (matthewd) ignoring return value?

	return success;
}
