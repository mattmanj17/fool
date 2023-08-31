

#include <stdio.h>
#include <string.h>

#include "print_toks_in_file.h"



int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		printf("wrong number of arguments, only expected a file path\n");
		return 1;
	}
	
	Print_tokens_in_file(argv[1]);
	return 0;
}