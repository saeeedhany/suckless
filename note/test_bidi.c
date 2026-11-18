#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>

#include "bidi.h"

static int failed;

static void
expect(const char *name, int ok)
{
	if (!ok) {
		printf("FAIL %s\n", name);
		failed++;
	}
}

static unsigned int
cp(const char *s)
{
	const unsigned char *u = (const unsigned char *)s;

	if (u[0] < 0x80)
		return u[0];
	if (u[0] < 0xe0)
		return ((u[0] & 0x1f) << 6) | (u[1] & 0x3f);
	return ((u[0] & 0x0f) << 12) | ((u[1] & 0x3f) << 6) | (u[2] & 0x3f);
}

int
main(void)
{
	Vis v = { 0 };
	size_t j;
	int allpres;

	bidivis(&v, "abc", 3, -1);
	expect("latin stays as is", v.n == 3 && !memcmp(v.s, "abc", 3) && !v.base && v.nc == 3 && v.log[0] == 0 && v.log[2] == 2);

	bidivis(&v, "\xd8\xb3\xd9\x84\xd8\xa7\xd9\x85", 8, -1);
	expect("arabic is a right to left row", v.base == 1 && v.nc == 4);
	expect("arabic is reversed", v.log[0] == 6 && v.log[1] == 4 && v.log[2] == 2 && v.log[3] == 0);
	for (allpres = 1, j = 0; j < v.nc; j++)
		if (!v.rtl[j] || ((j + 1 < v.nc ? v.off[j + 1] : v.n) > v.off[j] &&
		    (cp(v.s + v.off[j]) < 0xfe70 || cp(v.s + v.off[j]) > 0xfeff)))
			allpres = 0;
	expect("arabic letters are joined into presentation forms", allpres);

	bidivis(&v, "ab \xd8\xb3\xd9\x84", 7, -1);
	expect("latin first keeps a left to right row", v.base == 0 && v.nc == 5);
	expect("mixed latin then arabic", v.log[0] == 0 && v.log[1] == 1 && v.log[2] == 2 && v.log[3] == 5 && v.log[4] == 3);

	bidivis(&v, "\xd8\xb3\xd9\x84 ab", 7, -1);
	expect("arabic first makes a right to left row", v.base == 1);
	expect("mixed arabic then latin", v.log[0] == 5 && v.log[1] == 6 && v.log[2] == 4 && v.log[3] == 2 && v.log[4] == 0);

	bidivis(&v, "a\xe2", 2, -1);
	expect("broken utf8 is kept byte for byte", v.n == 2 && !memcmp(v.s, "a\xe2", 2));

	bidivis(&v, "", 0, -1);
	expect("empty row", v.n == 0 && v.nc == 0 && !v.base);

	bidivis(&v, "\xd9\x84\xd8\xa7", 4, -1);
	expect("lam alef becomes one ligature", v.nc == 2 && v.n == 3 && cp(v.s) == 0xfefb);

	bidivis(&v, "helloworld", 10, 1);
	expect("a right to left line keeps its direction on a latin row", v.base == 1 && v.n == 10 && !memcmp(v.s, "helloworld", 10));

	bidivis(&v, "123", 3, 1);
	expect("a digit row of a right to left line stays right to left", v.base == 1);

	bidivis(&v, "\xd8\xb3\xd9\x84", 4, 0);
	expect("arabic in a left to right line is still reversed", v.base == 0 && v.log[0] == 2 && v.log[1] == 0);

	expect("line direction comes from the first strong letter",
	    bidiparadir("12 \xd8\xb3 ab", 8) == 1 && bidiparadir("ab \xd8\xb3", 5) == 0 && bidiparadir("123", 3) == -1 && bidiparadir("", 0) == -1);

	bidivis(&v, "\xd8\xb3\xef\xbf\xbd", 5, -1);
	expect("a real replacement character survives an arabic row", v.n >= 3 && memmem(v.s, v.n, "\xef\xbf\xbd", 3) != NULL);

	bidivis(&v, "\xd8\xb3\xe2", 3, -1);
	expect("a broken byte survives an arabic row", memchr(v.s, 0xe2, v.n) != NULL && v.base == 1);

	bidifree(&v);
	if (failed)
		printf("%d failed\n", failed);
	else
		printf("all passed\n");
	return failed != 0;
}
