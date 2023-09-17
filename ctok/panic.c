
#include <stdlib.h>
#include <stdio.h>

#include "panic.h"

void Panic(const char * msg)
{
	printf("%s", msg);
	exit(1);
}