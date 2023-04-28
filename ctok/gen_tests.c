
#include <stdio.h>
#include <string.h>

#include "windows_helpers.h"



bool Should_walk_dir(
	const char * root_dir, 
	const char * this_dir, 
	const char * full_path)
{
	(void) root_dir;
	(void) this_dir;
	(void) full_path;
	return true;
}

int bink = 0;

void On_walk_file(
	const char * root_dir, 
	const char * this_file,
	const char * full_path)
{
	(void) full_path;

	const char * last_dot = strrchr(this_file, '.');
	if (!last_dot)
		return;

	if (strcmp(last_dot, ".c") != 0)
		return;

	printf("file '%s' in '%s' \n", this_file, root_dir);
}

void Gen_tests(const char * dir_path)
{
	// just printing c files, for now

	Walk_dir(dir_path, Should_walk_dir, On_walk_file);
}