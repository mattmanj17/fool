
#include <stdlib.h>
#include <stdio.h>

#include "alloc.h"

void * allocations[256];
int num_allocations = 0;

void * Allocate(int size)
{
	if (num_allocations >= 256)
	{
		printf("TOO MANY ALLOCATIONS");
		exit(1);
	}

	void * allocation = calloc((size_t)size, 1);
	if (!allocation)
	{
		printf("ALLOCATION FAILED");
		exit(1);
	}

	allocations[num_allocations] = allocation;
	++num_allocations;

	return allocation;
}