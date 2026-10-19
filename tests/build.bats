#!/usr/bin/env bats

setup() {
  REPO="$(cd "$BATS_TEST_DIRNAME/.." && pwd)"
  WORK="$BATS_TEST_TMPDIR/repo"
  rsync -a --exclude=.git "$REPO/" "$WORK/"
}

@test "each tool builds" {
  run make -C "$WORK"
  [ "$status" -eq 0 ]
  [ -x "$WORK/dwm/dwm" ]
  [ -x "$WORK/st/st" ]
  [ -x "$WORK/dmenu/dmenu" ]
}

@test "make clean picks up a changed config.def.h on the next build" {
  make -C "$WORK" >/dev/null
  for t in dwm st dmenu; do
    echo "/* changed */" >> "$WORK/$t/config.def.h"
  done
  echo "/* changed */" >> "$WORK/dwmblocks/blocks.def.h"
  make -C "$WORK" clean all >/dev/null
  for t in dwm st dmenu; do
    grep -q 'changed' "$WORK/$t/config.h" || { echo "$t kept a stale config.h"; return 1; }
  done
  grep -q 'changed' "$WORK/dwmblocks/blocks.h"
}
