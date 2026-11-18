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

	bidivis(&v, "abc", 3);
	expect("latin stays as is", v.n == 3 && !memcmp(v.s, "abc", 3) && !v.base && v.nc == 3 && v.log[0] == 0 && v.log[2] == 2);

	bidivis(&v, "\xd8\xb3\xd9\x84\xd8\xa7\xd9\x85", 8);
	expect("arabic is a right to left row", v.base == 1 && v.nc == 4);
	expect("arabic is reversed", v.log[0] == 6 && v.log[1] == 4 && v.log[2] == 2 && v.log[3] == 0);
	for (allpres = 1, j = 0; j < v.nc; j++)
		if (cp(v.s + v.off[j]) < 0xfe70 || cp(v.s + v.off[j]) > 0xfeff || !v.rtl[j])
			allpres = 0;
	expect("arabic letters are joined into presentation forms", allpres);

	bidivis(&v, "ab \xd8\xb3\xd9\x84", 7);
	expect("latin first keeps a left to right row", v.base == 0 && v.nc == 5);
	expect("mixed latin then arabic", v.log[0] == 0 && v.log[1] == 1 && v.log[2] == 2 && v.log[3] == 5 && v.log[4] == 3);

	bidivis(&v, "\xd8\xb3\xd9\x84 ab", 7);
	expect("arabic first makes a right to left row", v.base == 1);
	expect("mixed arabic then latin", v.log[0] == 5 && v.log[1] == 6 && v.log[2] == 4 && v.log[3] == 2 && v.log[4] == 0);

	bidivis(&v, "a\xe2", 2);
	expect("broken utf8 is kept byte for byte", v.n == 2 && !memcmp(v.s, "a\xe2", 2));

	bidivis(&v, "", 0);
	expect("empty row", v.n == 0 && v.nc == 0 && !v.base);

	bidifree(&v);
	if (failed)
		printf("%d failed\n", failed);
	else
		printf("all passed\n");
	return failed != 0;
}
