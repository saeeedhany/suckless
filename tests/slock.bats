#!/usr/bin/env bats

setup() {
  REPO="$(cd "$BATS_TEST_DIRNAME/.." && pwd)"
  WORK="$BATS_TEST_TMPDIR/slock"
  rsync -a "$REPO/slock/" "$WORK/"
}

@test "slock builds from config.def.h alone" {
  make -C "$WORK" >/dev/null 2>&1
  [ -x "$WORK/slock" ]
  [ ! -e "$WORK/config.h" ]
}

@test "slock drops privileges to a group that exists here" {
  group=$(sed -n 's/^static const char \*group = "\(.*\)";/\1/p' "$REPO/slock/config.def.h")
  getent group "$group" >/dev/null
}

@test "slock takes its colors from Xresources, so theme recolors it" {
  make -C "$WORK" >/dev/null 2>&1
  for r in slock.init slock.input slock.failed; do
    grep -qa "$r" "$WORK/slock" || { echo "no $r"; return 1; }
  done
}

@test "the top Makefile leaves slock out: it installs setuid root, once, with sudo" {
  ! grep -qE '^TOOLS.*slock' "$REPO/Makefile"
  grep -q 'chmod u+s' "$REPO/slock/Makefile"
}
