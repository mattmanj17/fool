
#include "ch.h"



bool Is_ch_horizontal_white_space(char ch)
{
	return ch == ' ' || ch == '\t' || ch == '\f' || ch == '\v';
}

bool Is_ch_white_space(char ch)
{
	return ch == ' ' || ch == '\t' || ch == '\f' || ch == '\v' || ch == '\n' || ch == '\r';
}

bool Is_ascii_digit(uint32_t cp)
{
	return cp >= '0' && cp <= '9';
}

bool Is_ascii_lowercase(uint32_t cp)
{
	return cp >= 'a' && cp <= 'z';
}

bool Is_ascii_uppercase(uint32_t cp)
{
	return cp >= 'A' && cp <= 'Z';
}
