#include <stdio.h>
#include <stdlib.h>

#include "file.h"

static char * Try_read_file_to_buffer(FILE * file)
{
	int err = fseek(file, 0, SEEK_END);
	if (err)
		return NULL;

	long len_file = ftell(file);
	if (len_file < 0)
		return NULL;

	err = fseek(file, 0, SEEK_SET);
	if (err)
		return NULL;

	char * buf = (char *)calloc((size_t)(len_file + 1), 1);
	if (!buf)
		return NULL;

	size_t bytes_read = fread(buf, 1, (size_t)len_file, file);
	if (bytes_read != (size_t)len_file)
		return NULL;

	return buf;
}

char * Try_read_file_at_path_to_buffer(const char * fpath)
{
	FILE * file = fopen(fpath, "rb");
	if (!file)
		return NULL;

	char * buf = Try_read_file_to_buffer(file);

	fclose(file); // BUG (matthewd) ignoring return value?

	return buf;
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