# Build or install every tool present: make, make install, make uninstall, make clean
TOOLS = dwm st dmenu dwmblocks

all install uninstall clean:
	@for t in $(TOOLS); do \
		[ -d $$t ] || continue; \
		$(MAKE) -C $$t $@ || exit 1; \
	done

.PHONY: all install uninstall clean
