#include <stdio.h>
#include <stdlib.h>

#include "file.h"

static bool Try_read_file_to_buffer(FILE * file, bounded_c_str_t * bstr)
{
	bstr->cursor = NULL;
	bstr->terminator = NULL;

	int err = fseek(file, 0, SEEK_END);
	if (err)
		return false;

	long len_file = ftell(file);
	if (len_file < 0)
		return false;

	err = fseek(file, 0, SEEK_SET);
	if (err)
		return false;

	char * buf = (char *)calloc((size_t)(len_file + 1), 1);
	if (!buf)
		return false;

	size_t bytes_read = fread(buf, 1, (size_t)len_file, file);
	if (bytes_read != (size_t)len_file)
		return false;

	bstr->cursor = buf;
	bstr->terminator = buf + len_file;

	return true;
}

bool Try_read_file_at_path_to_buffer(const char * fpath, bounded_c_str_t * bstr)
{
	bstr->cursor = NULL;
	bstr->terminator = NULL;

	FILE * file = fopen(fpath, "rb");
	if (!file)
		return false;

	bool success = Try_read_file_to_buffer(file, bstr);
	if (!success)
		return false;

	fclose(file); // BUG (matthewd) ignoring return value?

	return true;
}

static void Try_write_buffer_to_file(void * buffer, uint64_t size, FILE * file)
{
	size_t bytes_written = fwrite(buffer, 1, size, file);
	(void) bytes_written; //BUG ignoring return value?
}

void Try_write_buffer_to_file_at_path(void * buffer, uint64_t size, const char * fpath)
{
	FILE * file = fopen(fpath, "wb");
	if (!file)
		return;

	Try_write_buffer_to_file(buffer, size, file);

	fclose(file); // BUG (matthewd) ignoring return value?
}