# Build or install every tool present: make, sudo make install, make clean
TOOLS = dwm st dmenu dwmblocks

all install:
	@for t in $(TOOLS); do \
		[ -d $$t ] || continue; \
		$(MAKE) -C $$t $@ || exit 1; \
	done

# config.h and blocks.h are copied from their .def.h only when missing, so
# clean removes them too: otherwise a changed .def.h never reaches the build
clean:
	@for t in $(TOOLS); do \
		[ -d $$t ] || continue; \
		$(MAKE) -C $$t clean || exit 1; \
	done
	rm -f dwm/config.h st/config.h dmenu/config.h dwmblocks/blocks.h

.PHONY: all install clean
