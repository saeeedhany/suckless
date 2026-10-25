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

@test "each tool builds from its .def.h alone, with no config.h" {
  make -C "$WORK" >/dev/null
  for f in dwm/config.h st/config.h dmenu/config.h dwmblocks/blocks.h; do
    [ ! -e "$WORK/$f" ] || { echo "generated: $f"; return 1; }
  done
}

@test "a changed .def.h is rebuilt without make clean" {
  make -C "$WORK" >/dev/null
  sleep 1
  for f in dwm/config.def.h st/config.def.h dmenu/config.def.h dwmblocks/blocks.def.h; do
    touch "$WORK/$f"
    run make -C "$(dirname "$WORK/$f")" -q
    [ "$status" -ne 0 ] || { echo "not rebuilt after $f changed"; return 1; }
  done
}

@test "install needs no root and lands in ~/.local" {
  make -C "$WORK" install HOME="$BATS_TEST_TMPDIR/home" >/dev/null
  for b in dwm st dmenu dmenu_run stest dwmblocks; do
    [ -x "$BATS_TEST_TMPDIR/home/.local/bin/$b" ] || { echo "missing: $b"; return 1; }
  done
  [ -f "$BATS_TEST_TMPDIR/home/.local/share/man/man1/dwm.1" ]
}
