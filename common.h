#pragma once

#define COUNT_OF(x) ((sizeof(x)/sizeof(0[(x)])) / ((size_t)(!(sizeof(x) % sizeof(0[(x)])))))

#define CASSERT(x) static_assert((x), #x)