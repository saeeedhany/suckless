#include <stdio.h>
#include <stdlib.h>

#include "st.h"

void *
xrealloc(void *p, size_t len)
{
	return realloc(p, len);
}

static void
show(Rune *in, int n)
{
	Glyph line[64];
	Line out;
	int i;

	for (i = 0; i < n; i++) {
		line[i].u = in[i];
		line[i].mode = 0;
		line[i].fg = i;
		line[i].bg = 0;
	}
	out = bidiline(line, n);
	printf("%s", out == line ? "same" : "copy");
	for (i = 0; i < n; i++)
		printf(" %04x:%u", (unsigned)out[i].u, out[i].fg);
	printf("\n");
}

int
main(void)
{
	Rune ltr[] = { 'l', 's', ' ', '-', 'l' };
	Rune mixed[] = { 'a', 'b', ' ', 0x0633, 0x0644, 0x0627, 0x0645 };

	show(ltr, LEN(ltr));
	show(mixed, LEN(mixed));
	return 0;
}
