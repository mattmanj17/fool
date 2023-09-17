
#include "ch.h"



bool Is_ch_horizontal_white_space(char ch)
{
	return ch == ' ' || ch == '\t' || ch == '\f' || ch == '\v';
}

bool Is_ch_white_space(char ch)
{
	return Is_ch_horizontal_white_space(ch) || ch == '\n' || ch == '\r';
}
