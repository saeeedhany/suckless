PREFIX  := $(HOME)/.local
CC      := cc
CFLAGS  := -std=gnu99 -pedantic -Wall -Wno-deprecated-declarations -Os
LDFLAGS := -lX11

# FreeBSD (uncomment)
#LDFLAGS += -L/usr/local/lib -I/usr/local/include
# # OpenBSD (uncomment)
#LDFLAGS += -L/usr/X11R6/lib -I/usr/X11R6/include

all: options dwmblocks kbsignal

options:
	@echo dwmblocks build options:
	@echo "CFLAGS  = ${CFLAGS}"
	@echo "LDFLAGS = ${LDFLAGS}"
	@echo "CC      = ${CC}"

dwmblocks: dwmblocks.c blocks.def.h
	${CC} -o dwmblocks dwmblocks.c ${CFLAGS} ${LDFLAGS}

kbsignal: kbsignal.c blocks.def.h
	${CC} -o kbsignal kbsignal.c ${CFLAGS} ${LDFLAGS}

clean:
	rm -f *.o *.gch dwmblocks kbsignal

install: dwmblocks kbsignal
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f dwmblocks kbsignal ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/dwmblocks ${DESTDIR}${PREFIX}/bin/kbsignal

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/dwmblocks ${DESTDIR}${PREFIX}/bin/kbsignal

.PHONY: all options clean install uninstall
