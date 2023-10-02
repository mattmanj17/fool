
#include <assert.h>
#include <stdbool.h>

#include "peek.h"

#include "unicode.h"



// trigraphs

cp_len_t Peek_trigraph(lcp_span_t span)
{
	const lcp_t * cursor = span.lcp_mic;

	uint32_t cp0 = cursor[0].cp;
	if (cp0 != '?')
		return {UINT32_MAX, 0};

	uint32_t cp1 = cursor[1].cp;
	if (cp1 != '?')
		return {UINT32_MAX, 0};

	uint32_t cp2 = cursor[2].cp;
	switch (cp2)
	{
	case '<': return {'{', 3};
	case '>': return {'}', 3};
	case '(': return {'[', 3};
	case ')': return {']', 3};
	case '=': return {'#', 3};
	case '/': return {'\\', 3};
	case '\'': return {'^', 3};
	case '!': return {'|', 3};
	case '-': return {'~', 3};
	}

	return {UINT32_MAX, 0};
}



// Escaped line breaks

int Len_escaped_end_of_line(const lcp_t * cursor)
{
	if (cursor[0].cp != '\\')
		return 0;

	int len = 1;
	while (Is_cp_ascii_horizontal_white_space(cursor[len].cp))
	{
		++len;
	}

	// BUG clang does a horrifying thing where it slurps
	//  up a \n\r as a single line break when measuring
	//  a line continue, EVEN THOUGH it only defines
	//  "physical line breaks" as \n, \r, and \r\n.
	//  It has been like that since the very first
	//  version of the tokenizer, go figure.

	if (cursor[len].cp == '\n')
	{
		if (cursor[len + 1].cp == '\r')
			return len + 2; // :(

		return len + 1;
	}

	if (cursor[len].cp == '\r')
	{
		if (cursor[len + 1].cp == '\n')
			return len + 2;

		return len + 1;
	}

	return 0;
}

int Len_escaped_end_of_lines(const lcp_t * mic)
{
	int len = 0;

	while (true)
	{
		int len_esc_eol = Len_escaped_end_of_line(mic + len);
		if (len_esc_eol == 0)
			break;

		len += len_esc_eol;
	}

	return len;
}

static cp_len_t Peek_escaped_line_breaks(lcp_span_t span)
{
	const lcp_t * cursor = span.lcp_mic;

	int len_esc_eol = Len_escaped_end_of_lines(cursor);
	if (!len_esc_eol)
		return {UINT32_MAX, 0};

	if ((cursor + len_esc_eol) == span.lcp_mac)
	{
		// Treat trailing escaped eol as a '\0'

		return {'\0', len_esc_eol};
	}
	else
	{
		return {cursor[len_esc_eol].cp, len_esc_eol + 1};
	}
}



// Convert '\r' and "\r\n" to '\n'

static cp_len_t Peek_line_break(lcp_span_t span)
{
	const lcp_t * cursor = span.lcp_mic;

	if (cursor[0].cp == '\r')
	{
		// Need to check .num_ch because we do this after
		//  Collapse(Peek_escaped_line_breaks) :/

		if ((cursor[1].cp == '\n') && (cursor[1].num_ch == 1))
		{
			return {'\n', 2};
		}
		else
		{
			return {'\n', 1};
		}
	}
	
	return {UINT32_MAX, 0};
}



// Generic driver function to run a collapse function over a span

typedef cp_len_t (*collapse_fn_t)(lcp_span_t span);

void Collapse(collapse_fn_t collapse_fn, lcp_span_t * span)
{
	lcp_t * cursor_from = span->lcp_mic;
	lcp_t * cursor_to = span->lcp_mic;

	while (cursor_from < span->lcp_mac)
	{
		// BUG the 'len' in cp_len here is not characters, 
		//  but lcps ('logical' characters). This distinction
		//  has confused/bit me before, so it is probably bad.

		cp_len_t cp_len = collapse_fn({cursor_from, span->lcp_mac});

		if (cp_len.len)
		{
			cursor_to->cp = cp_len.cp;
			cursor_to->str = cursor_from->str;

			int num_ch = 0;
			for (int i_lcp = 0; i_lcp < cp_len.len; ++i_lcp)
			{
				num_ch += cursor_from[i_lcp].num_ch;
			}
			cursor_to->num_ch = num_ch;

			cursor_from += cp_len.len;

			assert(cursor_to->str + cursor_to->num_ch == cursor_from->str);
			++cursor_to;
		}
		else
		{
			*cursor_to = *cursor_from;
			++cursor_to;
			++cursor_from;
		}
	}
	assert(cursor_from == span->lcp_mac);

	// Copy trailing '\0'

	*cursor_to = *cursor_from;
	span->lcp_mac = cursor_to;
}



// Exposed function

void Collapse_lcp_span(lcp_span_t * lcp_span)
{
	// BUG ostensibly the standard says Collapse(Peek_line_break)
	//  should happen first, but it has to happen
	//  after Collapse(Peek_escaped_line_breaks) because of a hack
	//  we do there to match clang
	
	Collapse(Peek_trigraph, lcp_span);
	Collapse(Peek_escaped_line_breaks, lcp_span);
	Collapse(Peek_line_break, lcp_span);
}