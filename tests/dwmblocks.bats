#!/usr/bin/env bats
# The installed binary is the only surviving record of blocks.def.h.
# Rebuilding from the reconstructed header must reproduce its .rodata.

setup() {
  REPO="$(cd "$BATS_TEST_DIRNAME/.." && pwd)"
  WORK="$BATS_TEST_TMPDIR/dwmblocks"
  rsync -a "$REPO/dwmblocks/" "$WORK/"
  make -C "$WORK" >/dev/null 2>&1
}

rodata() { objdump -s -j .rodata "$1" | grep -oE '^ [0-9a-f]{4,} .*' | cut -c11-; }

@test "rebuilt rodata is identical to the installed binary" {
  [ -x /usr/local/bin/dwmblocks ] || skip "no installed dwmblocks"
  diff <(rodata /usr/local/bin/dwmblocks) <(rodata "$WORK/dwmblocks")
}

@test "block intervals are 2 2 1 30 1 1" {
  run grep -oE ',[[:space:]]*[0-9]+,[[:space:]]*0\},' "$REPO/dwmblocks/blocks.def.h"
  [ "$status" -eq 0 ]
  intervals="$(printf '%s\n' "$output" | grep -oE '[0-9]+' | paste -sd' ' | sed 's/ 0//g')"
  [ "$intervals" = "2 2 1 30 1 1" ]
}
