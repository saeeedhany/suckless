# Build or install every tool present: make, sudo make install, make clean
TOOLS = dwm st dmenu dwmblocks

all install clean:
	@for t in $(TOOLS); do \
		[ -d $$t ] || continue; \
		$(MAKE) -C $$t $@ || exit 1; \
	done

.PHONY: all install clean
