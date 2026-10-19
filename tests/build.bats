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

@test "config.def.h is byte-identical to the live tree" {
  [ -d "$HOME/.config/suckless/dwm" ] || skip "no live tree"
  for t in dwm st dmenu; do
    cmp "$REPO/$t/config.def.h" "$HOME/.config/suckless/$t/config.def.h"
  done
}
