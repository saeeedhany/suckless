#include <stdlib.h>
#include <string.h>

#include "buf.h"

#define NOWANT ((size_t)-1)
#define ENDWANT ((size_t)-2)

typedef struct Snap Snap;
struct Snap {
	char *s;
	size_t n, cur;
	Snap *next;
};

struct Buf {
	char *s;
	size_t n, cap, cur, want;
	int mode, dirty, insession;
	int count, op;
	char *reg;
	size_t regn;
	int reglines;
	Snap *undo, *redo;
};

static void *
xrealloc(void *p, size_t n)
{
	if (!(p = realloc(p, n ? n : 1)))
		abort();
	return p;
}

static int
iscont(unsigned char c)
{
	return (c & 0xc0) == 0x80;
}

static size_t
next(Buf *b, size_t i)
{
	if (i >= b->n)
		return b->n;
	for (i++; i < b->n && iscont(b->s[i]); i++)
		;
	return i;
}

static size_t
prev(Buf *b, size_t i)
{
	if (i == 0)
		return 0;
	for (i--; i > 0 && iscont(b->s[i]); i--)
		;
	return i;
}

static size_t
linestart(Buf *b, size_t i)
{
	while (i > 0 && b->s[i - 1] != '\n')
		i--;
	return i;
}

static size_t
lineend(Buf *b, size_t i)
{
	while (i < b->n && b->s[i] != '\n')
		i++;
	return i;
}

static size_t
firstnonblank(Buf *b, size_t i)
{
	size_t e = lineend(b, i);

	for (i = linestart(b, i); i < e && (b->s[i] == ' ' || b->s[i] == '\t'); i++)
		;
	return i;
}

static size_t
column(Buf *b, size_t i)
{
	size_t c = 0, p = linestart(b, i);

	for (; p < i; p = next(b, p))
		c++;
	return c;
}

static size_t
atcolumn(Buf *b, size_t ls, size_t c)
{
	size_t e = lineend(b, ls);

	while (c-- > 0 && ls < e)
		ls = next(b, ls);
	return ls;
}

static int
class(Buf *b, size_t i)
{
	unsigned char c;

	if (i >= b->n)
		return 0;
	c = b->s[i];
	if (c == ' ' || c == '\t' || c == '\n')
		return 0;
	if (c >= 0x80 || c == '_' || (c >= '0' && c <= '9') || ((c | 32) >= 'a' && (c | 32) <= 'z'))
		return 1;
	return 2;
}

static void
clamp(Buf *b)
{
	size_t ls, le;

	if (b->cur > b->n)
		b->cur = b->n;
	if (b->mode != ModeNormal)
		return;
	ls = linestart(b, b->cur);
	le = lineend(b, b->cur);
	if (b->cur >= le && le > ls)
		b->cur = prev(b, le);
}

static Snap *
snap(Buf *b, Snap *next)
{
	Snap *s = xrealloc(NULL, sizeof *s);

	s->s = xrealloc(NULL, b->n);
	memcpy(s->s, b->s, b->n);
	s->n = b->n;
	s->cur = b->cur;
	s->next = next;
	return s;
}

static void
freesnaps(Snap *s)
{
	Snap *t;

	for (; s; s = t) {
		t = s->next;
		free(s->s);
		free(s);
	}
}

static void
pushundo(Buf *b)
{
	b->undo = snap(b, b->undo);
	freesnaps(b->redo);
	b->redo = NULL;
}

static void
restore(Buf *b, Snap **from, Snap **to)
{
	Snap *s = *from;

	if (!s)
		return;
	*from = s->next;
	*to = snap(b, *to);
	free(b->s);
	b->s = s->s;
	b->n = b->cap = s->n;
	b->cur = s->cur;
	free(s);
	b->dirty = 1;
	b->want = NOWANT;
	clamp(b);
}

static void
insertat(Buf *b, size_t at, const char *s, size_t n)
{
	if (b->n + n > b->cap) {
		b->cap = (b->n + n) * 2;
		b->s = xrealloc(b->s, b->cap);
	}
	memmove(b->s + at + n, b->s + at, b->n - at);
	memcpy(b->s + at, s, n);
	b->n += n;
	b->dirty = 1;
}

static void
setreg(Buf *b, size_t from, size_t to, int lines)
{
	b->regn = to - from;
	b->reg = xrealloc(b->reg, b->regn);
	memcpy(b->reg, b->s + from, b->regn);
	b->reglines = lines;
}

static void
deleterange(Buf *b, size_t from, size_t to)
{
	memmove(b->s + from, b->s + to, b->n - to);
	b->n -= to - from;
	b->dirty = 1;
}

static void
beginsession(Buf *b)
{
	if (!b->insession) {
		pushundo(b);
		b->insession = 1;
	}
}

static void
startinsert(Buf *b)
{
	b->mode = ModeInsert;
	b->want = NOWANT;
}

static size_t
wordfwd(Buf *b, size_t i)
{
	int c = class(b, i);

	if (c)
		while (i < b->n && class(b, i) == c)
			i = next(b, i);
	while (i < b->n && class(b, i) == 0)
		i = next(b, i);
	return i;
}

static size_t
wordback(Buf *b, size_t i)
{
	int c;

	while (i > 0 && class(b, prev(b, i)) == 0)
		i = prev(b, i);
	if (i == 0)
		return 0;
	i = prev(b, i);
	c = class(b, i);
	while (i > 0 && class(b, prev(b, i)) == c)
		i = prev(b, i);
	return i;
}

static size_t
wordend(Buf *b, size_t i)
{
	int c;

	i = next(b, i);
	while (i < b->n && class(b, i) == 0)
		i = next(b, i);
	if (i >= b->n)
		return b->n ? prev(b, b->n) : 0;
	c = class(b, i);
	while (next(b, i) < b->n && class(b, next(b, i)) == c)
		i = next(b, i);
	return i;
}

static size_t
lineafter(Buf *b, size_t ls, size_t count)
{
	size_t e = ls;

	while (count-- > 0) {
		e = lineend(b, e);
		if (e < b->n && count > 0)
			e++;
		else
			break;
	}
	return e;
}

static void
gotoline(Buf *b, size_t line)
{
	size_t i = 0;

	while (line-- > 1 && lineend(b, i) < b->n)
		i = lineend(b, i) + 1;
	b->cur = firstnonblank(b, i);
}

static void
linesop(Buf *b, int op, size_t count)
{
	size_t ls = linestart(b, b->cur), le = lineafter(b, ls, count);

	setreg(b, ls, le, 1);
	if (op == 'y')
		return;
	pushundo(b);
	if (op == 'c') {
		deleterange(b, ls, le);
		b->cur = ls;
		b->insession = 1;
		startinsert(b);
		return;
	}
	if (le < b->n)
		deleterange(b, ls, le + 1);
	else if (ls > 0)
		deleterange(b, --ls, le);
	else
		deleterange(b, ls, le);
	if (ls >= b->n && ls > 0)
		ls = linestart(b, ls);
	b->cur = firstnonblank(b, ls);
}

static void
put(Buf *b, int after)
{
	size_t at;

	if (!b->regn && !b->reglines)
		return;
	pushundo(b);
	if (b->reglines) {
		if (after) {
			at = lineend(b, b->cur);
			insertat(b, at, "\n", 1);
			insertat(b, at + 1, b->reg, b->regn);
			b->cur = at + 1;
		} else {
			at = linestart(b, b->cur);
			insertat(b, at, b->reg, b->regn);
			insertat(b, at + b->regn, "\n", 1);
			b->cur = at;
		}
		b->cur = firstnonblank(b, b->cur);
		return;
	}
	at = b->cur;
	if (after && lineend(b, at) > at)
		at = next(b, at);
	insertat(b, at, b->reg, b->regn);
	b->cur = prev(b, at + b->regn);
}

static void
join(Buf *b)
{
	size_t e = lineend(b, b->cur), s;

	if (e >= b->n)
		return;
	pushundo(b);
	for (s = e + 1; s < b->n && (b->s[s] == ' ' || b->s[s] == '\t'); s++)
		;
	deleterange(b, e, s);
	if (e > linestart(b, e) && b->s[e - 1] != ' ' && e < b->n && b->s[e] != '\n') {
		insertat(b, e, " ", 1);
		b->cur = e;
	} else {
		b->cur = e > 0 ? prev(b, e) : 0;
	}
}

static void
operate(Buf *b, int op, int key, size_t count)
{
	size_t to = b->cur, e = lineend(b, b->cur);

	if (key == op) {
		linesop(b, op, count);
		return;
	}
	if (key != 'w')
		return;
	if (op == 'c' && class(b, b->cur) != 0) {
		if (next(b, to) < b->n && class(b, next(b, to)) == class(b, to))
			to = wordend(b, to);
		while (--count > 0)
			to = wordend(b, to);
		to = next(b, to);
	} else {
		while (count-- > 0)
			to = wordfwd(b, to);
	}
	if (to > e)
		to = e;
	setreg(b, b->cur, to, 0);
	if (op == 'y')
		return;
	pushundo(b);
	deleterange(b, b->cur, to);
	if (op == 'c') {
		b->insession = 1;
		startinsert(b);
	}
}

static void
normalkey(Buf *b, int key)
{
	size_t n, i, ls, e;
	int op = b->op, hadcount = b->count != 0;

	if (op == 'r') {
		b->op = 0;
		if (key < 0x100 && key >= ' ' && b->cur < lineend(b, b->cur)) {
			char c = key;
			pushundo(b);
			deleterange(b, b->cur, next(b, b->cur));
			insertat(b, b->cur, &c, 1);
		}
		return;
	}
	if ((key >= '1' && key <= '9') || (key == '0' && b->count)) {
		b->count = b->count * 10 + key - '0';
		return;
	}
	n = b->count ? b->count : 1;
	b->count = 0;
	b->op = 0;
	if (op == 'g') {
		if (key == 'g')
			gotoline(b, n);
		b->want = NOWANT;
		return;
	}
	if (op == 'd' || op == 'c' || op == 'y') {
		operate(b, op, key, n);
		clamp(b);
		return;
	}
	ls = linestart(b, b->cur);
	e = lineend(b, b->cur);
	switch (key) {
	case 'h': case KeyLeft:
		for (i = 0; i < n && b->cur > ls; i++)
			b->cur = prev(b, b->cur);
		b->want = NOWANT;
		break;
	case 'l': case KeyRight:
		for (i = 0; i < n && next(b, b->cur) < e; i++)
			b->cur = next(b, b->cur);
		b->want = NOWANT;
		break;
	case 'j': case KeyDown:
	case 'k': case KeyUp:
		if (b->want == NOWANT)
			b->want = column(b, b->cur);
		for (i = 0; i < n; i++) {
			if (key == 'j' || key == KeyDown) {
				if (lineend(b, ls) >= b->n)
					break;
				ls = lineend(b, ls) + 1;
			} else {
				if (ls == 0)
					break;
				ls = linestart(b, ls - 1);
			}
		}
		b->cur = b->want == ENDWANT ? lineend(b, ls) : atcolumn(b, ls, b->want);
		break;
	case 'w':
		for (i = 0; i < n; i++)
			b->cur = wordfwd(b, b->cur);
		b->want = NOWANT;
		break;
	case 'b':
		for (i = 0; i < n; i++)
			b->cur = wordback(b, b->cur);
		b->want = NOWANT;
		break;
	case 'e':
		for (i = 0; i < n; i++)
			b->cur = wordend(b, b->cur);
		b->want = NOWANT;
		break;
	case '0': case KeyHome:
		b->cur = ls;
		b->want = NOWANT;
		break;
	case '^':
		b->cur = firstnonblank(b, ls);
		b->want = NOWANT;
		break;
	case '$': case KeyEnd:
		b->cur = e;
		b->want = ENDWANT;
		break;
	case 'G':
		if (hadcount)
			gotoline(b, n);
		else
			gotoline(b, (size_t)-1);
		b->want = NOWANT;
		break;
	case 'g': case 'd': case 'c': case 'y': case 'r':
		b->op = key;
		b->count = n > 1 ? n : 0;
		return;
	case 'x':
		if (b->cur >= e)
			break;
		for (i = b->cur; n-- > 0 && i < e; i = next(b, i))
			;
		setreg(b, b->cur, i, 0);
		pushundo(b);
		deleterange(b, b->cur, i);
		break;
	case 'D':
	case 'C':
		setreg(b, b->cur, e, 0);
		pushundo(b);
		deleterange(b, b->cur, e);
		if (key == 'C') {
			b->insession = 1;
			startinsert(b);
		}
		break;
	case 'i':
		startinsert(b);
		break;
	case 'a':
		if (b->cur < e)
			b->cur = next(b, b->cur);
		startinsert(b);
		break;
	case 'I':
		b->cur = firstnonblank(b, ls);
		startinsert(b);
		break;
	case 'A':
		b->cur = e;
		startinsert(b);
		break;
	case 'o':
		pushundo(b);
		insertat(b, e, "\n", 1);
		b->cur = e + 1;
		b->insession = 1;
		startinsert(b);
		break;
	case 'O':
		pushundo(b);
		insertat(b, ls, "\n", 1);
		b->cur = ls;
		b->insession = 1;
		startinsert(b);
		break;
	case 'J':
		join(b);
		break;
	case 'p':
		put(b, 1);
		break;
	case 'P':
		put(b, 0);
		break;
	case 'u':
		restore(b, &b->undo, &b->redo);
		break;
	case KeyCtrlR:
		restore(b, &b->redo, &b->undo);
		break;
	}
	clamp(b);
}

static void
insertkey(Buf *b, int key)
{
	size_t ls = linestart(b, b->cur), e = lineend(b, b->cur), i;

	switch (key) {
	case KeyEsc:
		b->mode = ModeNormal;
		b->insession = 0;
		if (b->cur > ls)
			b->cur = prev(b, b->cur);
		b->want = NOWANT;
		clamp(b);
		break;
	case KeyReturn:
		bufinsert(b, "\n", 1);
		break;
	case KeyTab:
		bufinsert(b, "  ", 2);
		break;
	case KeyBackspace:
		if (b->cur == 0)
			break;
		beginsession(b);
		i = prev(b, b->cur);
		deleterange(b, i, b->cur);
		b->cur = i;
		break;
	case KeyDelete:
		if (b->cur >= b->n)
			break;
		beginsession(b);
		deleterange(b, b->cur, next(b, b->cur));
		break;
	case KeyCtrlW:
		if (b->cur == ls)
			break;
		beginsession(b);
		i = b->cur;
		while (i > ls && class(b, prev(b, i)) == 0)
			i = prev(b, i);
		if (i > ls) {
			int c = class(b, prev(b, i));
			while (i > ls && class(b, prev(b, i)) == c)
				i = prev(b, i);
		}
		deleterange(b, i, b->cur);
		b->cur = i;
		break;
	case KeyCtrlU:
		if (b->cur == ls)
			break;
		beginsession(b);
		deleterange(b, ls, b->cur);
		b->cur = ls;
		break;
	case KeyLeft:
		if (b->cur > ls)
			b->cur = prev(b, b->cur);
		break;
	case KeyRight:
		if (b->cur < e)
			b->cur = next(b, b->cur);
		break;
	case KeyUp:
	case KeyDown:
		i = column(b, b->cur);
		if (key == KeyDown && e < b->n)
			b->cur = atcolumn(b, e + 1, i);
		else if (key == KeyUp && ls > 0)
			b->cur = atcolumn(b, linestart(b, ls - 1), i);
		break;
	case KeyHome:
		b->cur = ls;
		break;
	case KeyEnd:
		b->cur = e;
		break;
	}
}

Buf *
bufnew(void)
{
	Buf *b = xrealloc(NULL, sizeof *b);

	memset(b, 0, sizeof *b);
	b->mode = ModeNormal;
	b->want = NOWANT;
	return b;
}

void
buffree(Buf *b)
{
	freesnaps(b->undo);
	freesnaps(b->redo);
	free(b->reg);
	free(b->s);
	free(b);
}

void
bufload(Buf *b, const char *s, size_t n)
{
	free(b->s);
	b->s = xrealloc(NULL, n);
	memcpy(b->s, s, n);
	b->n = b->cap = n;
	b->cur = 0;
	b->mode = ModeNormal;
	b->want = NOWANT;
	b->insession = b->count = b->op = 0;
	freesnaps(b->undo);
	freesnaps(b->redo);
	b->undo = b->redo = NULL;
	b->dirty = 0;
}

const char *
buftext(Buf *b, size_t *n)
{
	*n = b->n;
	return b->s ? b->s : "";
}

size_t
bufcursor(Buf *b)
{
	return b->cur;
}

void
bufsetcursor(Buf *b, size_t off)
{
	b->cur = off > b->n ? b->n : off;
	b->want = NOWANT;
}

int
bufmode(Buf *b)
{
	return b->mode;
}

void
bufsetmode(Buf *b, int mode)
{
	b->mode = mode;
	b->insession = 0;
	b->count = b->op = 0;
}

void
bufinsert(Buf *b, const char *s, size_t n)
{
	if (!n)
		return;
	beginsession(b);
	insertat(b, b->cur, s, n);
	b->cur += n;
}

void
bufkey(Buf *b, int key)
{
	if (key == KeyEsc && b->mode == ModeNormal) {
		b->count = b->op = 0;
		return;
	}
	if (b->mode == ModeInsert)
		insertkey(b, key);
	else
		normalkey(b, key);
}

int
bufdirty(Buf *b)
{
	return b->dirty;
}

void
bufclean(Buf *b)
{
	b->dirty = 0;
}
