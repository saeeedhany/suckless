#include <stdlib.h>
#include <string.h>
#include <fribidi.h>

#include "bidi.h"

static void *
grow(void *p, size_t n)
{
	if (!(p = realloc(p, n ? n : 1)))
		abort();
	return p;
}

static size_t
decode(const unsigned char *s, size_t n, FriBidiChar *c)
{
	size_t len, i;
	FriBidiChar v;

	if (s[0] < 0x80) {
		*c = s[0];
		return 1;
	}
	if ((s[0] & 0xe0) == 0xc0) {
		len = 2;
		v = s[0] & 0x1f;
	} else if ((s[0] & 0xf0) == 0xe0) {
		len = 3;
		v = s[0] & 0x0f;
	} else if ((s[0] & 0xf8) == 0xf0) {
		len = 4;
		v = s[0] & 0x07;
	} else {
		*c = 0xfffd;
		return 1;
	}
	if (len > n) {
		*c = 0xfffd;
		return 1;
	}
	for (i = 1; i < len; i++) {
		if ((s[i] & 0xc0) != 0x80) {
			*c = 0xfffd;
			return 1;
		}
		v = (v << 6) | (s[i] & 0x3f);
	}
	*c = v;
	return len;
}

static size_t
encode(FriBidiChar c, char *out)
{
	if (c < 0x80) {
		out[0] = c;
		return 1;
	}
	if (c < 0x800) {
		out[0] = 0xc0 | (c >> 6);
		out[1] = 0x80 | (c & 0x3f);
		return 2;
	}
	if (c < 0x10000) {
		out[0] = 0xe0 | (c >> 12);
		out[1] = 0x80 | ((c >> 6) & 0x3f);
		out[2] = 0x80 | (c & 0x3f);
		return 3;
	}
	out[0] = 0xf0 | (c >> 18);
	out[1] = 0x80 | ((c >> 12) & 0x3f);
	out[2] = 0x80 | ((c >> 6) & 0x3f);
	out[3] = 0x80 | (c & 0x3f);
	return 4;
}

static void
reserve(Vis *v, size_t bytes, size_t chars)
{
	if (bytes > v->cap) {
		v->cap = bytes;
		v->s = grow(v->s, v->cap);
	}
	if (chars > v->ccap) {
		v->ccap = chars;
		v->off = grow(v->off, v->ccap * sizeof *v->off);
		v->log = grow(v->log, v->ccap * sizeof *v->log);
		v->rtl = grow(v->rtl, v->ccap);
	}
}

static void
identity(Vis *v, const char *s, size_t n, size_t *lo, size_t nc)
{
	size_t j;

	memcpy(v->s, s, n);
	v->n = n;
	v->nc = nc;
	v->base = 0;
	for (j = 0; j < nc; j++) {
		v->off[j] = v->log[j] = lo[j];
		v->rtl[j] = 0;
	}
}

int
bidiparadir(const char *s, size_t n)
{
	FriBidiChar c;
	FriBidiCharType t;
	size_t i;

	for (i = 0; i < n;) {
		i += decode((const unsigned char *)s + i, n - i, &c);
		t = fribidi_get_bidi_type(c);
		if (t == FRIBIDI_TYPE_LTR)
			return 0;
		if (t == FRIBIDI_TYPE_RTL || t == FRIBIDI_TYPE_AL)
			return 1;
	}
	return 0;
}

int
bidivis(Vis *v, const char *s, size_t n, int dir)
{
	FriBidiChar *str;
	FriBidiCharType *types;
	FriBidiBracketType *brackets;
	FriBidiLevel *levels;
	FriBidiArabicProp *joins;
	FriBidiStrIndex *map;
	FriBidiParType base;
	size_t *lo, nc = 0, i, j, k;
	int rtl = 0, ok;

	if (dir < 0)
		dir = bidiparadir(s, n);
	base = dir ? FRIBIDI_PAR_RTL : FRIBIDI_PAR_LTR;
	reserve(v, 4 * n + 1, n + 1);
	str = grow(NULL, (n + 1) * sizeof *str);
	lo = grow(NULL, (n + 1) * sizeof *lo);
	for (i = 0; i < n; nc++) {
		lo[nc] = i;
		i += decode((const unsigned char *)s + i, n - i, &str[nc]);
		if (FRIBIDI_IS_RTL(fribidi_get_bidi_type(str[nc])))
			rtl = 1;
	}
	if (!rtl && !dir) {
		identity(v, s, n, lo, nc);
		free(str);
		free(lo);
		return 0;
	}
	types = grow(NULL, nc * sizeof *types);
	brackets = grow(NULL, nc * sizeof *brackets);
	levels = grow(NULL, nc * sizeof *levels);
	joins = grow(NULL, nc * sizeof *joins);
	map = grow(NULL, nc * sizeof *map);
	for (j = 0; j < nc; j++)
		map[j] = j;
	fribidi_get_bidi_types(str, nc, types);
	fribidi_get_bracket_types(str, nc, types, brackets);
	ok = fribidi_get_par_embedding_levels_ex(types, brackets, nc, &base, levels) != 0;
	if (ok) {
		fribidi_get_joining_types(str, nc, joins);
		fribidi_join_arabic(types, nc, levels, joins);
		fribidi_shape(FRIBIDI_FLAG_SHAPE_MIRRORING | FRIBIDI_FLAG_SHAPE_ARAB_PRES | FRIBIDI_FLAG_SHAPE_ARAB_LIGA,
		              levels, nc, joins, str);
		ok = fribidi_reorder_line(FRIBIDI_FLAGS_DEFAULT, types, nc, 0, base, levels, NULL, map) != 0;
	}
	if (!ok) {
		identity(v, s, n, lo, nc);
	} else {
		for (j = 0, k = 0; j < nc; j++) {
			v->off[j] = k;
			v->log[j] = lo[map[j]];
			v->rtl[j] = levels[map[j]] & 1;
			if (str[map[j]] == 0xfffd) {
				v->s[k++] = s[lo[map[j]]];
				continue;
			}
			if (str[map[j]] == FRIBIDI_CHAR_FILL)
				continue;
			k += encode(str[map[j]], v->s + k);
		}
		v->n = k;
		v->nc = nc;
		v->base = dir;
	}
	free(types);
	free(brackets);
	free(levels);
	free(joins);
	free(map);
	free(str);
	free(lo);
	return 1;
}

void
bidifree(Vis *v)
{
	free(v->s);
	free(v->off);
	free(v->log);
	free(v->rtl);
	memset(v, 0, sizeof *v);
}
