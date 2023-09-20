#pragma once

#include <stdint.h>

#include "unicode.h"

cp_len_t Peek_cp(const char * mic, const char * mac);
int Len_escaped_end_of_lines(const char * mic, const char * mac);