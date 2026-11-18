include config.mk

all: note

.c.o:
	$(CC) -c $(CFLAGS) $<

buf.o: buf.h
note.o: buf.h drw.h util.h config.def.h config.mk
drw.o: drw.h util.h
util.o: util.h

note: note.o buf.o drw.o util.o
	$(CC) -o $@ note.o buf.o drw.o util.o $(LDFLAGS)

test_buf: test_buf.c buf.c buf.h
	$(CC) -std=c99 -pedantic -Wall -g -o $@ test_buf.c buf.c

test: test_buf
	./test_buf

clean:
	rm -f note test_buf *.o

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f note $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/note
	mkdir -p $(DESTDIR)$(MANPREFIX)/man1
	sed "s/VERSION/$(VERSION)/g" < note.1 > $(DESTDIR)$(MANPREFIX)/man1/note.1
	chmod 644 $(DESTDIR)$(MANPREFIX)/man1/note.1

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/note $(DESTDIR)$(MANPREFIX)/man1/note.1

.PHONY: all test clean install uninstall
