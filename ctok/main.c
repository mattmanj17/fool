

#include <stdio.h>
#include <string.h>

#include "print_toks_in_file.h"



int main(int argc, char *argv[])
{
	if (argc < 2)
	{
		printf("wrong number of arguments, expect at least a file name\n");
		return 1;
	}

	const char * cmd = argv[1];

	if (argc == 2)
	{
		Print_tokens_in_file(cmd);
	}
	else if (strcmp("gen_tests", cmd) == 0)
	{
		//
	}
	else
	{
		printf("Unknown command '%s'\n", cmd);
	}
}