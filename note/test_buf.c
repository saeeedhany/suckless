#include <stdio.h>
#include <string.h>

#include "buf.h"

static int failed;

static int
utf8len(unsigned char c)
{
	if (c < 0x80)
		return 1;
	if ((c & 0xe0) == 0xc0)
		return 2;
	if ((c & 0xf0) == 0xe0)
		return 3;
	return 4;
}

static void
feed(Buf *b, const char *keys)
{
	const char *p = keys;
	int n;

	while (*p) {
		unsigned char c = *p;
		switch (c) {
		case 0x1b: bufkey(b, KeyEsc); p++; continue;
		case '\n': bufkey(b, KeyReturn); p++; continue;
		case '\b': bufkey(b, KeyBackspace); p++; continue;
		case 0x17: bufkey(b, KeyCtrlW); p++; continue;
		case 0x15: bufkey(b, KeyCtrlU); p++; continue;
		case 0x12: bufkey(b, KeyCtrlR); p++; continue;
		}
		if (bufmode(b) == ModeInsert) {
			n = utf8len(c);
			bufinsert(b, p, n);
			p += n;
		} else {
			bufkey(b, c);
			p++;
		}
	}
}

static void
check(const char *name, const char *start, size_t cur, int mode, const char *keys,
      const char *want, size_t wantcur, int wantmode)
{
	Buf *b = bufnew();
	const char *t;
	size_t n;

	bufload(b, start, strlen(start));
	bufsetcursor(b, cur);
	bufsetmode(b, mode);
	feed(b, keys);
	t = buftext(b, &n);
	if (n != strlen(want) || memcmp(t, want, n) || bufcursor(b) != wantcur || bufmode(b) != wantmode) {
		printf("FAIL %s: got \"%.*s\" cursor %zu mode %d, want \"%s\" cursor %zu mode %d\n",
		       name, (int)n, t, bufcursor(b), bufmode(b), want, wantcur, wantmode);
		failed++;
	}
	buffree(b);
}

static void
checkdirty(void)
{
	Buf *b = bufnew();

	bufload(b, "ab", 2);
	if (bufdirty(b)) {
		printf("FAIL dirty after load\n");
		failed++;
	}
	bufkey(b, 'l');
	if (bufdirty(b)) {
		printf("FAIL motion set dirty\n");
		failed++;
	}
	bufkey(b, 'x');
	if (!bufdirty(b)) {
		printf("FAIL edit left clean\n");
		failed++;
	}
	bufclean(b);
	if (bufdirty(b)) {
		printf("FAIL dirty after clean\n");
		failed++;
	}
	buffree(b);
}

static void
checkcap(void)
{
	Buf *b = bufnew();
	size_t n;

	bufload(b, "a", 1);
	bufkey(b, 'y');
	bufkey(b, 'y');
	feed(b, "99999p");
	buftext(b, &n);
	if (n != 1 + 2 * 9999) {
		printf("FAIL count cap: %zu bytes, want %d\n", n, 1 + 2 * 9999);
		failed++;
	}
	buffree(b);
}

static void
checkyank(const char *name, const char *start, size_t cur, const char *keys, int want, const char *wanttext)
{
	Buf *b = bufnew();
	const char *s;
	size_t n;
	int got;

	bufload(b, start, strlen(start));
	bufsetcursor(b, cur);
	bufsetmode(b, ModeNormal);
	feed(b, keys);
	got = bufyanked(b, &s, &n);
	if (got != want || (want && (n != strlen(wanttext) || memcmp(s, wanttext, n)))) {
		printf("FAIL %s: yanked %d\n", name, got);
		failed++;
	} else if (want && bufyanked(b, &s, &n)) {
		printf("FAIL %s: yank reported twice\n", name);
		failed++;
	}
	buffree(b);
}

int
main(void)
{
	check("insert types", "", 0, ModeInsert, "hi", "hi", 2, ModeInsert);
	check("esc steps back", "hi", 2, ModeInsert, "\x1b", "hi", 1, ModeNormal);
	check("backspace", "abc", 3, ModeInsert, "\b", "ab", 2, ModeInsert);
	check("ctrl w", "one two", 7, ModeInsert, "\x17", "one ", 4, ModeInsert);
	check("ctrl u", "ab\ncd", 5, ModeInsert, "\x15", "ab\n", 3, ModeInsert);
	check("hjkl", "ab\ncd", 0, ModeNormal, "ljh", "ab\ncd", 3, ModeNormal);
	check("k keeps column", "abc\nd\nefg", 2, ModeNormal, "jj", "abc\nd\nefg", 8, ModeNormal);
	check("w b e", "one two three", 0, ModeNormal, "wwbe", "one two three", 6, ModeNormal);
	check("0 ^ $", "  ab", 3, ModeNormal, "0", "  ab", 0, ModeNormal);
	check("caret", "  ab", 0, ModeNormal, "^", "  ab", 2, ModeNormal);
	check("dollar", "ab\ncd", 0, ModeNormal, "$", "ab\ncd", 1, ModeNormal);
	check("gg G", "a\nb\nc", 2, ModeNormal, "Ggg", "a\nb\nc", 0, ModeNormal);
	check("count G", "a\nb\nc", 0, ModeNormal, "2G", "a\nb\nc", 2, ModeNormal);
	check("1G", "a\nb\nc", 4, ModeNormal, "1G", "a\nb\nc", 0, ModeNormal);
	check("x", "abc", 1, ModeNormal, "x", "ac", 1, ModeNormal);
	check("count x", "abcd", 0, ModeNormal, "2x", "cd", 0, ModeNormal);
	check("dd", "a\nb\nc", 2, ModeNormal, "dd", "a\nc", 2, ModeNormal);
	check("2dd", "a\nb\nc", 0, ModeNormal, "2dd", "c", 0, ModeNormal);
	check("dw", "one two", 0, ModeNormal, "dw", "two", 0, ModeNormal);
	check("D", "abcd", 1, ModeNormal, "D", "a", 0, ModeNormal);
	check("cw", "one two", 0, ModeNormal, "cwXY\x1b", "XY two", 1, ModeNormal);
	check("cw one letter word", "a b", 0, ModeNormal, "cwX\x1b", "X b", 0, ModeNormal);
	check("cw on last letter", "foo bar", 2, ModeNormal, "cwX\x1b", "foX bar", 2, ModeNormal);
	check("cc", "ab\ncd", 0, ModeNormal, "ccz\x1b", "z\ncd", 0, ModeNormal);
	check("C", "abcd", 1, ModeNormal, "Cz\x1b", "az", 1, ModeNormal);
	check("r", "abc", 1, ModeNormal, "rZ", "aZc", 1, ModeNormal);
	check("J", "ab\ncd", 0, ModeNormal, "J", "ab cd", 2, ModeNormal);
	check("o O", "a", 0, ModeNormal, "ob\x1bOc\x1b", "a\nc\nb", 2, ModeNormal);
	check("i a I A", "bc", 1, ModeNormal, "Ix\x1b" "Ay\x1b", "xbcy", 3, ModeNormal);
	check("yy p", "a\nb", 0, ModeNormal, "yyjp", "a\nb\na", 4, ModeNormal);
	check("dd P", "a\nb", 2, ModeNormal, "ddP", "b\na", 0, ModeNormal);
	check("x p", "ab", 0, ModeNormal, "xp", "ba", 1, ModeNormal);
	check("count r", "abcde", 0, ModeNormal, "3rxlx", "xxxe", 3, ModeNormal);
	check("count r too long", "ab", 0, ModeNormal, "3rx", "ab", 0, ModeNormal);
	check("counts multiply", "a b c d e f g h", 0, ModeNormal, "2d3w", "g h", 0, ModeNormal);
	check("count p", "ab", 0, ModeNormal, "x3p", "baaa", 3, ModeNormal);
	check("count linewise p", "a\nb", 0, ModeNormal, "yy2p", "a\na\na\nb", 2, ModeNormal);
	check("J on an empty line", "a\n\nb", 2, ModeNormal, "J", "a\nb", 2, ModeNormal);
	check("huge count is capped", "ab", 0, ModeNormal, "999999999999l", "ab", 1, ModeNormal);
	check("d$", "one two", 4, ModeNormal, "d$", "one ", 3, ModeNormal);
	check("d0", "one two", 4, ModeNormal, "d0", "two", 0, ModeNormal);
	check("d^", "  ab", 3, ModeNormal, "d^", "  b", 2, ModeNormal);
	check("de", "one two", 0, ModeNormal, "de", " two", 0, ModeNormal);
	check("db", "one two", 4, ModeNormal, "db", "two", 0, ModeNormal);
	check("dh", "abc", 2, ModeNormal, "dh", "ac", 1, ModeNormal);
	check("2dl", "abcd", 1, ModeNormal, "2dl", "ad", 1, ModeNormal);
	check("dj", "a\nb\nc", 0, ModeNormal, "dj", "c", 0, ModeNormal);
	check("dk", "a\nb\nc", 2, ModeNormal, "dk", "c", 0, ModeNormal);
	check("dj on the last line", "a\nb", 2, ModeNormal, "dj", "a\nb", 2, ModeNormal);
	check("dG", "a\nb\nc", 2, ModeNormal, "dG", "a", 0, ModeNormal);
	check("dgg", "a\nb\nc", 2, ModeNormal, "dgg", "c", 0, ModeNormal);
	check("d2G", "a\nb\nc\nd", 6, ModeNormal, "d2G", "a", 0, ModeNormal);
	check("cj", "a\nb\nc", 0, ModeNormal, "cjz\x1b", "z\nc", 0, ModeNormal);
	check("c$", "one two", 4, ModeNormal, "c$x\x1b", "one x", 4, ModeNormal);
	check("c0 at the line start", "ab", 0, ModeNormal, "c0x\x1b", "xab", 0, ModeNormal);
	check("db at a line start reaches the line above", "one\ntwo", 4, ModeNormal, "db", "two", 0, ModeNormal);
	check("de on the last letter reaches the next line", "one\ntwo three", 2, ModeNormal, "de", "on three", 2, ModeNormal);
	check("dw on the last word stops at the line end", "one\ntwo", 0, ModeNormal, "dw", "\ntwo", 0, ModeNormal);
	check("y$ keeps the cursor", "one two", 4, ModeNormal, "y$", "one two", 4, ModeNormal);
	check("yb moves to the start", "one two", 4, ModeNormal, "yb", "one two", 0, ModeNormal);
	check("ygg moves to the top", "a\nb\nc", 4, ModeNormal, "ygg", "a\nb\nc", 0, ModeNormal);
	check("y$ p", "ab cd", 3, ModeNormal, "y$0P", "cdab cd", 1, ModeNormal);
	check("yj p", "a\nb\nc", 0, ModeNormal, "yjGp", "a\nb\nc\na\nb", 6, ModeNormal);
	check("undo insert session", "", 0, ModeInsert, "hello world\x1bu", "", 0, ModeNormal);
	check("redo", "", 0, ModeInsert, "hello\x1bu\x12", "hello", 4, ModeNormal);
	check("undo dd", "a\nb", 0, ModeNormal, "ddu", "a\nb", 0, ModeNormal);
	check("utf8 l and x", "\xc3\xa9" "b", 0, ModeNormal, "x", "b", 0, ModeNormal);
	check("utf8 backspace", "a\xd8\xb3", 3, ModeInsert, "\b", "a", 1, ModeInsert);
	check("utf8 h", "\xd8\xb3\xd9\x84", 2, ModeNormal, "h", "\xd8\xb3\xd9\x84", 0, ModeNormal);
	checkdirty();
	checkcap();
	checkyank("yy yanks the line", "one\ntwo", 0, "yy", 1, "one");
	checkyank("2yy yanks two lines", "one\ntwo\nthree", 0, "2yy", 1, "one\ntwo");
	checkyank("yw yanks a word", "hello there", 0, "yw", 1, "hello ");
	checkyank("y$", "one two", 4, "y$", 1, "two");
	checkyank("y0", "one two", 4, "y0", 1, "one ");
	checkyank("ye", "one two", 0, "ye", 1, "one");
	checkyank("yb", "one two", 4, "yb", 1, "one ");
	checkyank("yG", "a\nb\nc", 2, "yG", 1, "b\nc");
	checkyank("ygg", "a\nb\nc", 4, "ygg", 1, "a\nb\nc");
	checkyank("yk", "a\nb", 2, "yk", 1, "a\nb");
	checkyank("y2j", "a\nb\nc\nd", 0, "y2j", 1, "a\nb\nc");
	checkyank("yj on the last line", "a\nb", 2, "yj", 0, "");
	checkyank("yb across a line", "one\ntwo", 4, "yb", 1, "one\n");
	checkyank("ye across a line", "one\ntwo", 2, "ye", 1, "e\ntwo");
	checkyank("dd is not a yank", "one\ntwo", 0, "dd", 0, "");
	checkyank("x is not a yank", "one", 0, "x", 0, "");
	checkyank("moving is not a yank", "one", 0, "l", 0, "");
	if (failed)
		printf("%d failed\n", failed);
	else
		printf("all passed\n");
	return failed != 0;
}
