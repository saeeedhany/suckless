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
	check("undo insert session", "", 0, ModeInsert, "hello world\x1bu", "", 0, ModeNormal);
	check("redo", "", 0, ModeInsert, "hello\x1bu\x12", "hello", 4, ModeNormal);
	check("undo dd", "a\nb", 0, ModeNormal, "ddu", "a\nb", 0, ModeNormal);
	check("utf8 l and x", "\xc3\xa9" "b", 0, ModeNormal, "x", "b", 0, ModeNormal);
	check("utf8 backspace", "a\xd8\xb3", 3, ModeInsert, "\b", "a", 1, ModeInsert);
	check("utf8 h", "\xd8\xb3\xd9\x84", 2, ModeNormal, "h", "\xd8\xb3\xd9\x84", 0, ModeNormal);
	checkdirty();
	if (failed)
		printf("%d failed\n", failed);
	else
		printf("all passed\n");
	return failed != 0;
}
