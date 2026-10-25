#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <X11/XKBlib.h>

typedef struct {
	char *icon;
	char *command;
	unsigned int interval;
	unsigned int signal;
} Block;

#include "blocks.def.h"

int
main(void)
{
	Display *dpy;
	XEvent ev;
	int xkbevent, err, major = XkbMajorVersion, minor = XkbMinorVersion;
	char sig[16];

	if (!(dpy = XkbOpenDisplay(NULL, &xkbevent, &err, &major, &minor, &err))) {
		fprintf(stderr, "kbsignal: cannot open display or xkb\n");
		return 1;
	}
	(void)blocks;
	(void)delim;
	(void)delimLen;
	snprintf(sig, sizeof(sig), "-RTMIN+%d", KBSIGNAL);
	signal(SIGCHLD, SIG_IGN);
	XkbSelectEventDetails(dpy, XkbUseCoreKbd, XkbStateNotify,
	                      XkbGroupStateMask, XkbGroupStateMask);
	for (;;) {
		XNextEvent(dpy, &ev);
		if (ev.type != xkbevent || ((XkbEvent *)&ev)->any.xkb_type != XkbStateNotify)
			continue;
		if (fork() == 0) {
			execlp("pkill", "pkill", sig, "-x", "dwmblocks", (char *)NULL);
			_exit(1);
		}
	}
}
