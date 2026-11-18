#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <locale.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <X11/Xft/Xft.h>
#ifdef XINERAMA
#include <X11/extensions/Xinerama.h>
#endif

#include "buf.h"
#include "drw.h"
#include "util.h"

#include "config.def.h"

typedef struct {
	size_t start, end;
} Row;

typedef struct {
	int x, y, w, h;
} Rect;

static Display *dpy;
static int screen;
static Window root, win;
static Drw *drw;
static Clr *scheme, *curscheme;
static XIM xim;
static XIC xic;
static Atom wmdelete, wmprotocols, netwmstate, netsticky, clientinfo;
static Buf *buf;
static char dir[PATH_MAX], id[64], txtpath[PATH_MAX], pospath[PATH_MAX];
static unsigned int ww, wh, lineh, charw;
static Row *rows;
static size_t nrows, rowcap, top;
static struct timespec changed;
static volatile sig_atomic_t quitflag;
static int lockfd = -1, haspos, pinned = 1, posx, posy, posw, posh;
static long tags;

static void setpaths(void);

static long
msecsince(struct timespec *t)
{
	struct timespec now;

	clock_gettime(CLOCK_MONOTONIC, &now);
	return (now.tv_sec - t->tv_sec) * 1000 + (now.tv_nsec - t->tv_nsec) / 1000000;
}

static int
validid(const char *s)
{
	int i;

	for (i = 0; i < 15; i++)
		if (i == 8 ? s[i] != '-' : (s[i] < '0' || s[i] > '9'))
			return 0;
	if (!s[15])
		return 1;
	if (s[15] != '-' || !s[16])
		return 0;
	for (i = 16; s[i]; i++)
		if (s[i] < '0' || s[i] > '9' || i > 24)
			return 0;
	return 1;
}

static int
notename(const char *name, char *idout, size_t n)
{
	size_t len = strlen(name);

	if (len < 5 || len - 4 >= n || strcmp(name + len - 4, ".txt"))
		return 0;
	memcpy(idout, name, len - 4);
	idout[len - 4] = '\0';
	return validid(idout);
}

static int
readrect(int fd, Rect *r, int *pin, long *t)
{
	char line[128];
	ssize_t n = pread(fd, line, sizeof line - 1, 0);
	int p;
	long tg;

	if (n <= 0)
		return 0;
	line[n] = '\0';
	if (sscanf(line, "%d %d %d %d %d %ld", &r->x, &r->y, &r->w, &r->h, &p, &tg) != 6 || r->w <= 0 || r->h <= 0)
		return 0;
	if (pin)
		*pin = p != 0;
	if (t)
		*t = tg;
	return 1;
}

static void
writepos(void)
{
	char line[128];
	int n;

	if (lockfd < 0 || !haspos)
		return;
	n = snprintf(line, sizeof line, "%d %d %d %d %d %ld\n", posx, posy, posw, posh, pinned, tags);
	if (ftruncate(lockfd, 0) == 0 && pwrite(lockfd, line, n, 0) == n)
		fsync(lockfd);
}

static int
lockpos(void)
{
	Rect r;

	if ((lockfd = open(pospath, O_RDWR | O_CREAT, 0600)) < 0)
		return 0;
	if (flock(lockfd, LOCK_EX | LOCK_NB) < 0) {
		close(lockfd);
		lockfd = -1;
		return 0;
	}
	if (readrect(lockfd, &r, &pinned, &tags)) {
		haspos = 1;
		posx = r.x;
		posy = r.y;
		posw = r.w;
		posh = r.h;
	}
	return 1;
}

static int
openrects(Rect *out, int max)
{
	DIR *d;
	struct dirent *e;
	char other[64], path[PATH_MAX];
	int n = 0, fd;

	if (!(d = opendir(dir)))
		return 0;
	while ((e = readdir(d)) && n < max) {
		if (!notename(e->d_name, other, sizeof other) || !strcmp(other, id))
			continue;
		if ((size_t)snprintf(path, sizeof path, "%s/.%s.pos", dir, other) >= sizeof path)
			continue;
		if ((fd = open(path, O_RDONLY)) < 0)
			continue;
		if (flock(fd, LOCK_SH | LOCK_NB) < 0 && readrect(fd, &out[n], NULL, NULL))
			n++;
		close(fd);
	}
	closedir(d);
	return n;
}

static int
overlaps(Rect *a, Rect *b)
{
	return a->x < b->x + b->w && b->x < a->x + a->w && a->y < b->y + b->h && b->y < a->y + a->h;
}

static int
screens(Rect *out, int max)
{
	int n = 0;
#ifdef XINERAMA
	XineramaScreenInfo *info;
	int i, count;

	if (XineramaIsActive(dpy) && (info = XineramaQueryScreens(dpy, &count))) {
		for (i = 0; i < count && n < max; i++, n++) {
			out[n].x = info[i].x_org;
			out[n].y = info[i].y_org;
			out[n].w = info[i].width;
			out[n].h = info[i].height;
		}
		XFree(info);
	}
#endif
	if (!n) {
		out[0].x = out[0].y = 0;
		out[0].w = DisplayWidth(dpy, screen);
		out[0].h = DisplayHeight(dpy, screen);
		n = 1;
	}
	return n;
}

static int
onscreen(Rect *r)
{
	Rect sc[16];
	int i, n = screens(sc, LENGTH(sc));

	for (i = 0; i < n; i++)
		if (overlaps(r, &sc[i]))
			return 1;
	return 0;
}

static void
pullonscreen(void)
{
	Rect sc[16], r = { posx, posy, posw, posh };
	int i, n = screens(sc, LENGTH(sc)), best = 0;
	long d, bestd = -1, dx, dy;

	if (onscreen(&r))
		return;
	for (i = 0; i < n; i++) {
		dx = (posx + posw / 2) - (sc[i].x + sc[i].w / 2);
		dy = (posy + posh / 2) - (sc[i].y + sc[i].h / 2);
		d = dx * dx + dy * dy;
		if (bestd < 0 || d < bestd) {
			bestd = d;
			best = i;
		}
	}
	posx = MAX(sc[best].x, MIN(posx, sc[best].x + sc[best].w - posw));
	posy = MAX(sc[best].y, MIN(posy, sc[best].y + sc[best].h - posh));
}

static int
place(void)
{
	Rect open[64], sc[16], c;
	int n = openrects(open, LENGTH(open)), ns, i, s = 0, top, right, moved;
	Window rw, cw;
	int px = 0, py = 0, wx, wy;
	unsigned int mask;

	if (!n)
		return 0;
	ns = screens(sc, LENGTH(sc));
	XQueryPointer(dpy, root, &rw, &cw, &px, &py, &wx, &wy, &mask);
	for (i = 0; i < ns; i++)
		if (px >= sc[i].x && px < sc[i].x + sc[i].w && py >= sc[i].y && py < sc[i].y + sc[i].h)
			s = i;
	top = sc[s].y + sc[s].h;
	right = sc[s].x;
	for (i = 0; i < n; i++) {
		c = open[i];
		if (!overlaps(&c, &sc[s]))
			continue;
		top = MIN(top, c.y);
		right = MAX(right, c.x + c.w);
	}
	if (right == sc[s].x)
		return 0;
	c.w = ww;
	c.h = wh;
	c.x = right - ww;
	c.y = top;
	do {
		moved = 0;
		for (i = 0; i < n; i++) {
			if (overlaps(&c, &open[i])) {
				c.y = open[i].y + open[i].h + 8;
				moved = 1;
			}
		}
		if (c.y + c.h > sc[s].y + sc[s].h) {
			c.y = top;
			c.x -= ww + 8;
			moved = c.x >= sc[s].x;
		}
	} while (moved);
	posx = c.x;
	posy = c.y;
	return 1;
}

static void
restoreall(const char *self)
{
	DIR *d;
	struct dirent *e;
	char other[64];
	pid_t pid;

	if (!(d = opendir(dir)))
		return;
	while ((e = readdir(d))) {
		if (!notename(e->d_name, other, sizeof other))
			continue;
		snprintf(id, sizeof id, "%s", other);
		setpaths();
		if (!lockpos())
			continue;
		close(lockfd);
		lockfd = -1;
		if ((pid = fork()) == 0) {
			setsid();
			execlp(self, self, "-o", other, (char *)NULL);
			_exit(1);
		}
	}
	closedir(d);
}

static void
onquit(int sig)
{
	quitflag = 1;
}

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

static unsigned int
textw(const char *s, size_t n)
{
	unsigned int w = 0, h;

	if (n)
		drw_font_getexts(drw->fonts, s, n, &w, &h);
	return w;
}

static void
addrow(size_t start, size_t end)
{
	if (nrows == rowcap) {
		rowcap = rowcap ? rowcap * 2 : 64;
		if (!(rows = realloc(rows, rowcap * sizeof *rows)))
			die("realloc:");
	}
	rows[nrows].start = start;
	rows[nrows].end = end;
	nrows++;
}

static void
layout(void)
{
	const char *s;
	size_t n, i, start, space, len;
	unsigned int avail = ww > 2 * padding ? ww - 2 * padding : charw, w;

	s = buftext(buf, &n);
	nrows = 0;
	start = 0;
	while (1) {
		w = 0;
		space = 0;
		for (i = start; i < n && s[i] != '\n'; i += len) {
			len = utf8len(s[i]);
			w += textw(s + i, len);
			if (w > avail && i > start)
				break;
			if (s[i] == ' ')
				space = i + 1;
		}
		if (i < n && s[i] != '\n') {
			if (space > start)
				i = space;
			addrow(start, i);
			start = i;
			continue;
		}
		addrow(start, i);
		if (i >= n)
			break;
		start = i + 1;
	}
}

static size_t
cursorrow(void)
{
	size_t cur = bufcursor(buf), r;

	for (r = 0; r < nrows; r++)
		if (cur < rows[r].end || (cur == rows[r].end && (r + 1 == nrows || rows[r + 1].start > cur)))
			return r;
	return nrows ? nrows - 1 : 0;
}

static void
draw(void)
{
	const char *s;
	char line[4096];
	size_t n, r, cr, visible, len, cur;
	unsigned int y, x, cw;

	layout();
	s = buftext(buf, &n);
	cr = cursorrow();
	visible = (wh - 2 * padding) / lineh;
	if (!visible)
		visible = 1;
	if (cr < top)
		top = cr;
	if (cr >= top + visible)
		top = cr - visible + 1;
	drw_setscheme(drw, scheme);
	drw_rect(drw, 0, 0, ww, wh, 1, 1);
	for (r = top, y = padding; r < nrows && r < top + visible; r++, y += lineh) {
		len = MIN(rows[r].end - rows[r].start, sizeof line - 1);
		memcpy(line, s + rows[r].start, len);
		line[len] = '\0';
		if (len)
			drw_text(drw, padding, y, ww - 2 * padding, lineh, 0, line, 0);
	}
	cur = bufcursor(buf);
	x = padding + textw(s + rows[cr].start, cur - rows[cr].start);
	y = padding + (cr - top) * lineh;
	drw_setscheme(drw, curscheme);
	if (bufmode(buf) == ModeInsert) {
		drw_rect(drw, x, y, 2, lineh, 1, 1);
	} else {
		len = cur < n && s[cur] != '\n' ? utf8len(s[cur]) : 0;
		cw = len ? textw(s + cur, len) : charw;
		memcpy(line, len ? s + cur : " ", len ? len : 1);
		line[len ? len : 1] = '\0';
		drw_text(drw, x, y, cw, lineh, 0, line, 0);
	}
	drw_map(drw, win, 0, 0, ww, wh);
}

static int
blank(void)
{
	const char *s;
	size_t n, i;

	s = buftext(buf, &n);
	for (i = 0; i < n; i++)
		if (!isspace((unsigned char)s[i]))
			return 0;
	return 1;
}

static void
writefile(const char *path, const char *s, size_t n)
{
	char tmp[PATH_MAX];
	int fd;

	if ((size_t)snprintf(tmp, sizeof tmp, "%s/.%s.tmp", dir, id) >= sizeof tmp)
		return;
	if ((fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0600)) < 0)
		return;
	if (write(fd, s, n) != (ssize_t)n || fsync(fd) < 0) {
		close(fd);
		unlink(tmp);
		return;
	}
	close(fd);
	rename(tmp, path);
}

static void
save(void)
{
	const char *s;
	size_t n;

	s = buftext(buf, &n);
	writefile(txtpath, s, n);
	bufclean(buf);
}

static void
setpaths(void)
{
	if ((size_t)snprintf(txtpath, sizeof txtpath, "%s/%s.txt", dir, id) >= sizeof txtpath ||
	    (size_t)snprintf(pospath, sizeof pospath, "%s/.%s.pos", dir, id) >= sizeof pospath)
		die("note: path too long");
}

static void
newid(void)
{
	char base[32];
	time_t t = time(NULL);
	int fd, i;

	strftime(base, sizeof base, "%Y%m%d-%H%M%S", localtime(&t));
	for (i = 1;; i++) {
		if (i == 1)
			snprintf(id, sizeof id, "%s", base);
		else
			snprintf(id, sizeof id, "%s-%d", base, i);
		setpaths();
		if ((fd = open(txtpath, O_WRONLY | O_CREAT | O_EXCL, 0600)) >= 0) {
			close(fd);
			return;
		}
		if (errno != EEXIST)
			die("note: cannot create %s:", txtpath);
	}
}

static void
load(void)
{
	char *s = NULL;
	size_t n = 0, cap = 0;
	ssize_t r;
	int fd;

	if ((fd = open(txtpath, O_RDONLY)) < 0)
		die("note: cannot open %s:", txtpath);
	do {
		if (n == cap) {
			cap = cap ? cap * 2 : 4096;
			if (!(s = realloc(s, cap)))
				die("realloc:");
		}
		r = read(fd, s + n, cap - n);
		if (r > 0)
			n += r;
	} while (r > 0);
	close(fd);
	bufload(buf, s ? s : "", n);
	free(s);
}

static KeySym
physical(XKeyEvent *ev)
{
	KeySym k, first = NoSymbol;
	int g, level = ev->state & ShiftMask ? 1 : 0;

	for (g = 0; g < 4; g++) {
		k = XkbKeycodeToKeysym(dpy, ev->keycode, g, level);
		if (k == NoSymbol)
			continue;
		if (first == NoSymbol)
			first = k;
		if (k < 0x100 || (k >= 0xff00 && k <= 0xffff))
			return k;
	}
	return first;
}

static int
special(KeySym k)
{
	switch (k) {
	case XK_Escape: return KeyEsc;
	case XK_Return: case XK_KP_Enter: return KeyReturn;
	case XK_Tab: return KeyTab;
	case XK_BackSpace: return KeyBackspace;
	case XK_Delete: case XK_KP_Delete: return KeyDelete;
	case XK_Left: case XK_KP_Left: return KeyLeft;
	case XK_Right: case XK_KP_Right: return KeyRight;
	case XK_Up: case XK_KP_Up: return KeyUp;
	case XK_Down: case XK_KP_Down: return KeyDown;
	case XK_Home: case XK_KP_Home: return KeyHome;
	case XK_End: case XK_KP_End: return KeyEnd;
	}
	return 0;
}

static void
keypress(XKeyEvent *ev)
{
	char text[64];
	KeySym k = physical(ev), ksym;
	Status status;
	int len, sp;

	if (ev->state & ControlMask) {
		switch (k) {
		case XK_w: bufkey(buf, KeyCtrlW); break;
		case XK_u: bufkey(buf, KeyCtrlU); break;
		case XK_r: bufkey(buf, KeyCtrlR); break;
		}
		return;
	}
	if ((sp = special(k))) {
		bufkey(buf, sp);
		return;
	}
	if (bufmode(buf) == ModeNormal) {
		if (k >= 0x20 && k < 0x7f)
			bufkey(buf, (int)k);
		return;
	}
	len = Xutf8LookupString(xic, ev, text, sizeof text, &ksym, &status);
	if ((status == XLookupChars || status == XLookupBoth) && len > 0 && (unsigned char)text[0] >= 0x20 && text[0] != 0x7f)
		bufinsert(buf, text, len);
}

static int
hasatom(Atom prop, Atom want)
{
	Atom type, *a;
	int format, found = 0;
	unsigned long n, after, i;
	unsigned char *data = NULL;

	if (XGetWindowProperty(dpy, win, prop, 0L, 32L, False, XA_ATOM, &type, &format, &n, &after, &data) == Success && data) {
		a = (Atom *)data;
		for (i = 0; i < n; i++)
			if (a[i] == want)
				found = 1;
	}
	if (data)
		XFree(data);
	return found;
}

static void
readtags(void)
{
	Atom type;
	int format;
	unsigned long n, after;
	unsigned char *data = NULL;

	if (XGetWindowProperty(dpy, win, clientinfo, 0L, 3L, False, XA_CARDINAL, &type, &format, &n, &after, &data) == Success && data && n >= 1)
		tags = ((long *)data)[0];
	if (data)
		XFree(data);
}

static void
moved(XConfigureEvent *ev)
{
	Rect r = { ev->x, ev->y, ev->width, ev->height };

	if (!onscreen(&r))
		return;
	posx = r.x;
	posy = r.y;
	posw = r.w;
	posh = r.h;
	haspos = 1;
	writepos();
}

static void
setup(int fresh)
{
	XSetWindowAttributes swa;
	XClassHint ch = { "note", "note" };
	XWMHints wmh = { .flags = InputHint, .input = True };
	XSizeHints *sh;
	const char *cols[2];
	long info[3];
	int usepos = 0;

	if (!setlocale(LC_CTYPE, "") || !XSupportsLocale())
		fputs("note: no locale support\n", stderr);
	if (!(dpy = XOpenDisplay(NULL)))
		die("note: cannot open display");
	screen = DefaultScreen(dpy);
	root = RootWindow(dpy, screen);
	drw = drw_create(dpy, screen, root, 1, 1);
	if (!drw_fontset_create(drw, fonts, LENGTH(fonts)))
		die("note: no fonts could be loaded");
	lineh = drw->fonts->h;
	charw = textw("0", 1);
	cols[ColFg] = foreground;
	cols[ColBg] = background;
	scheme = drw_scm_create(drw, cols, 2);
	cols[ColFg] = background;
	cols[ColBg] = cursorcolor;
	curscheme = drw_scm_create(drw, cols, 2);
	ww = 2 * padding + columns * charw;
	wh = 2 * padding + lines * lineh;
	if (haspos) {
		ww = posw;
		wh = posh;
		pullonscreen();
		usepos = 1;
	} else if (fresh) {
		usepos = place();
	}
	drw_resize(drw, ww, wh);
	netwmstate = XInternAtom(dpy, "_NET_WM_STATE", False);
	netsticky = XInternAtom(dpy, "_NET_WM_STATE_STICKY", False);
	clientinfo = XInternAtom(dpy, "_NET_CLIENT_INFO", False);

	swa.background_pixel = scheme[ColBg].pixel;
	swa.event_mask = ExposureMask | KeyPressMask | StructureNotifyMask | PropertyChangeMask;
	win = XCreateWindow(dpy, root, usepos ? posx : 0, usepos ? posy : 0, ww, wh, 0, CopyFromParent, CopyFromParent,
		CopyFromParent, CWBackPixel | CWEventMask, &swa);
	XSetClassHint(dpy, win, &ch);
	XStoreName(dpy, win, "note");
	XSetWMHints(dpy, win, &wmh);
	sh = XAllocSizeHints();
	sh->flags = PSize | (usepos ? USPosition : 0);
	sh->x = posx;
	sh->y = posy;
	sh->width = ww;
	sh->height = wh;
	XSetWMNormalHints(dpy, win, sh);
	XFree(sh);
	wmprotocols = XInternAtom(dpy, "WM_PROTOCOLS", False);
	wmdelete = XInternAtom(dpy, "WM_DELETE_WINDOW", False);
	XSetWMProtocols(dpy, win, &wmdelete, 1);

	if (!(xim = XOpenIM(dpy, NULL, NULL, NULL))) {
		XSetLocaleModifiers("@im=none");
		xim = XOpenIM(dpy, NULL, NULL, NULL);
	}
	if (xim)
		xic = XCreateIC(xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
			XNClientWindow, win, XNFocusWindow, win, NULL);
	if (!xic)
		die("note: no input method");
	if (pinned)
		XChangeProperty(dpy, win, netwmstate, XA_ATOM, 32, PropModeReplace, (unsigned char *)&netsticky, 1);
	if (tags) {
		info[0] = tags;
		info[1] = 0;
		info[2] = 1;
		XChangeProperty(dpy, win, clientinfo, XA_CARDINAL, 32, PropModeReplace, (unsigned char *)info, 3);
	}
	if (fresh)
		bufsetmode(buf, ModeInsert);
	XMapWindow(dpy, win);
	XSync(dpy, False);
}

static void
run(void)
{
	XEvent ev;
	struct pollfd pfd = { .fd = ConnectionNumber(dpy), .events = POLLIN };
	long left;

	while (!quitflag) {
		while (XPending(dpy)) {
			XNextEvent(dpy, &ev);
			if (XFilterEvent(&ev, win))
				continue;
			switch (ev.type) {
			case Expose:
				if (ev.xexpose.count == 0)
					draw();
				break;
			case PropertyNotify:
				if (ev.xproperty.atom == netwmstate) {
					pinned = hasatom(netwmstate, netsticky);
					writepos();
				} else if (ev.xproperty.atom == clientinfo) {
					readtags();
					writepos();
				}
				break;
			case ConfigureNotify:
				moved(&ev.xconfigure);
				if (ev.xconfigure.width != (int)ww || ev.xconfigure.height != (int)wh) {
					ww = ev.xconfigure.width;
					wh = ev.xconfigure.height;
					drw_resize(drw, ww, wh);
					draw();
				}
				break;
			case KeyPress:
				keypress(&ev.xkey);
				if (bufdirty(buf))
					clock_gettime(CLOCK_MONOTONIC, &changed);
				draw();
				break;
			case ClientMessage:
				if (ev.xclient.message_type == wmprotocols && (Atom)ev.xclient.data.l[0] == wmdelete)
					quitflag = 1;
				break;
			case DestroyNotify:
				quitflag = 1;
				break;
			}
			if (quitflag)
				return;
		}
		left = -1;
		if (bufdirty(buf)) {
			left = savedelay - msecsince(&changed);
			if (left <= 0) {
				save();
				continue;
			}
		}
		poll(&pfd, 1, left);
	}
}

static void
cleanup(void)
{
	if (blank()) {
		unlink(txtpath);
		unlink(pospath);
	} else {
		save();
		writepos();
	}
	if (lockfd >= 0)
		close(lockfd);
	if (xic)
		XDestroyIC(xic);
	if (xim)
		XCloseIM(xim);
	drw_free(drw);
	XDestroyWindow(dpy, win);
	XCloseDisplay(dpy);
}

static void
notesdir(void)
{
	const char *d = getenv("NOTES_DIR"), *home = getenv("HOME");

	if (d && *d)
		snprintf(dir, sizeof dir, "%s", d);
	else
		snprintf(dir, sizeof dir, "%s/notes", home ? home : ".");
	if (mkdir(dir, 0700) < 0 && errno != EEXIST)
		die("note: cannot create %s:", dir);
}

static void
usage(void)
{
	die("usage: note [-r] [-o id]");
}

int
main(int argc, char *argv[])
{
	struct sigaction sa;
	int fresh = 1;

	memset(&sa, 0, sizeof sa);
	sa.sa_handler = onquit;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);

	notesdir();
	buf = bufnew();
	if (argc == 2 && !strcmp(argv[1], "-r")) {
		restoreall(argv[0]);
		return 0;
	} else if (argc == 3 && !strcmp(argv[1], "-o")) {
		if (!validid(argv[2]))
			die("note: not a note: %s", argv[2]);
		snprintf(id, sizeof id, "%s", argv[2]);
		setpaths();
		if (access(txtpath, R_OK) < 0)
			die("note: no such note: %s", argv[2]);
		if (!lockpos())
			return 0;
		load();
		fresh = 0;
	} else if (argc == 1) {
		newid();
		if (!lockpos())
			die("note: cannot lock %s", pospath);
	} else {
		usage();
	}
	setup(fresh);
	run();
	cleanup();
	buffree(buf);
	return 0;
}
