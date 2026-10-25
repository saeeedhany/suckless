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

@test "blocks: volume and keyboard update on a signal, the clock once a minute" {
  f="$REPO/dwmblocks/blocks.def.h"
  grep -qE '^#define KBSIGNAL[[:space:]]+[0-9]+$' "$f"
  grep -qE '"vol ",.*volume",[[:space:]]*30,[[:space:]]*10\},' "$f"
  grep -qE '"kb ",.*keyboard",[[:space:]]*30,[[:space:]]*KBSIGNAL\},' "$f"
  grep -qE 'date_time",[[:space:]]*60,[[:space:]]*0\},' "$f"
}

load helpers/x

teardown() {
  [[ -n "${BLOCKS_PID:-}" ]] && kill "$BLOCKS_PID" 2>/dev/null
  [[ -n "${KB_PID:-}" ]] && kill "$KB_PID" 2>/dev/null
  stop_xvfb
}

build_with_blocks() {
  printf '#define KBSIGNAL 11\nstatic const Block blocks[] = {\n%s\n};\nstatic char delim[] = " | ";\nstatic unsigned int delimLen = 5;\n' "$1" > "$WORK/blocks.def.h"
  make -C "$WORK" >/dev/null 2>&1
}

root_name() { xprop -root WM_NAME | sed -n 's/^WM_NAME([A-Z0-9_]*) = "\(.*\)"$/\1/p'; }

@test "timed blocks fire on wall clock multiples, not counted from start" {
  build_with_blocks '{"", "date +%s", 3, 0},'
  start_xvfb
  "$WORK/dwmblocks" & BLOCKS_PID=$!
  local seen=() v i
  for i in $(seq 40); do
    v="$(root_name)"
    [[ -n "$v" && " ${seen[*]} " != *" $v "* ]] && seen+=("$v")
    sleep 0.2
  done
  [ "${#seen[@]}" -ge 3 ]
  for v in "${seen[@]:1}"; do
    [ $((v % 3)) -eq 0 ] || { echo "fired at $v, seen ${seen[*]}"; return 1; }
  done
}

@test "a signal updates its block at once" {
  echo a > "$BATS_TEST_TMPDIR/v"
  build_with_blocks "{\"\", \"cat $BATS_TEST_TMPDIR/v\", 0, 10},"
  start_xvfb
  "$WORK/dwmblocks" & BLOCKS_PID=$!
  sleep 0.5
  [ "$(root_name)" = a ]
  echo b > "$BATS_TEST_TMPDIR/v"
  kill -RTMIN+10 "$BLOCKS_PID"
  sleep 0.3
  [ "$(root_name)" = b ]
}

@test "kbsignal tells dwmblocks when the keyboard group changes" {
  echo 0 > "$BATS_TEST_TMPDIR/n"
  printf '#!/bin/sh\nn=$(cat %s/n); echo $((n + 1)) > %s/n; echo $n\n' "$BATS_TEST_TMPDIR" "$BATS_TEST_TMPDIR" > "$BATS_TEST_TMPDIR/count"
  chmod +x "$BATS_TEST_TMPDIR/count"
  build_with_blocks "{\"\", \"$BATS_TEST_TMPDIR/count\", 0, KBSIGNAL},"
  printf '#include <X11/XKBlib.h>\nint main(int c, char **v) { Display *d = XOpenDisplay(0); XkbLockGroup(d, XkbUseCoreKbd, v[1][0] - 48); XSync(d, 0); return 0; }\n' > "$BATS_TEST_TMPDIR/lock.c"
  cc -o "$BATS_TEST_TMPDIR/lock" "$BATS_TEST_TMPDIR/lock.c" -lX11
  start_xvfb
  setxkbmap -layout us,ara
  export PATH="$WORK:$PATH"
  "$WORK/dwmblocks" & BLOCKS_PID=$!
  sleep 0.5
  [ "$(root_name)" = 0 ]
  "$WORK/kbsignal" & KB_PID=$!
  sleep 0.5
  "$BATS_TEST_TMPDIR/lock" 1
  sleep 0.5
  [ "$(root_name)" = 1 ]
  "$BATS_TEST_TMPDIR/lock" 0
  sleep 0.5
  [ "$(root_name)" = 2 ]
}
