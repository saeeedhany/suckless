#include <stddef.h>
#include <wchar.h>
#include <fribidi.h>

#include "st.h"

static int
hasrtl(Line line, int n)
{
	int i, rtl = 0;

	for (i = 0; i < n; i++) {
		if (line[i].mode & (ATTR_WIDE | ATTR_WDUMMY))
			return 0;
		if (FRIBIDI_IS_RTL(fribidi_get_bidi_type(line[i].u)))
			rtl = 1;
	}
	return rtl;
}

Line
bidiline(Line line, int n)
{
	static Glyph *out;
	static FriBidiChar *str;
	static FriBidiCharType *types;
	static FriBidiBracketType *brackets;
	static FriBidiLevel *levels;
	static FriBidiArabicProp *joins;
	static FriBidiStrIndex *map;
	static int cap;
	FriBidiParType base = FRIBIDI_PAR_LTR;
	int i;

	if (n <= 0 || !hasrtl(line, n))
		return line;
	if (n > cap) {
		cap = n;
		out = xrealloc(out, cap * sizeof(*out));
		str = xrealloc(str, cap * sizeof(*str));
		types = xrealloc(types, cap * sizeof(*types));
		brackets = xrealloc(brackets, cap * sizeof(*brackets));
		levels = xrealloc(levels, cap * sizeof(*levels));
		joins = xrealloc(joins, cap * sizeof(*joins));
		map = xrealloc(map, cap * sizeof(*map));
	}
	for (i = 0; i < n; i++) {
		str[i] = line[i].u;
		map[i] = i;
	}
	fribidi_get_bidi_types(str, n, types);
	fribidi_get_bracket_types(str, n, types, brackets);
	if (!fribidi_get_par_embedding_levels_ex(types, brackets, n, &base, levels))
		return line;
	fribidi_get_joining_types(str, n, joins);
	fribidi_join_arabic(types, n, levels, joins);
	fribidi_shape(FRIBIDI_FLAG_SHAPE_MIRRORING | FRIBIDI_FLAG_SHAPE_ARAB_PRES,
	              levels, n, joins, str);
	if (!fribidi_reorder_line(FRIBIDI_FLAGS_DEFAULT, types, n, 0, base, levels, NULL, map))
		return line;
	for (i = 0; i < n; i++) {
		out[i] = line[map[i]];
		out[i].u = str[map[i]];
	}
	return out;
}
