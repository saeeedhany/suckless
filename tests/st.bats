#!/usr/bin/env bats

load helpers/x

setup() {
  REPO="$(cd "$BATS_TEST_DIRNAME/.." && pwd)"
  WORK="$BATS_TEST_TMPDIR/st"
  rsync -a "$REPO/st/" "$WORK/"
  make -C "$WORK" >/dev/null 2>&1
}

teardown() {
  [[ -n "${ST_PID:-}" ]] && kill "$ST_PID" 2>/dev/null
  stop_xvfb
}

@test "the binary carries the marker theme looks for" {
  grep -qa cursorColor "$WORK/st"
}

@test "compiled palette is the grey theme" {
  grep -q '"#282828", /\* background \*/\|"#282828",.*background' "$REPO/st/config.def.h"
  grep -q '"#c77a7a"' "$REPO/st/config.def.h"
}

@test "st reloads its background on USR1" {
  start_xvfb
  printf 'st.background: #282828\n' | xrdb -load
  "$WORK/st" -g 80x24+0+0 & ST_PID=$!
  wait_for_pixel 300 200 '#282828'
  printf 'st.background: #e8e6e1\n' | xrdb -merge
  kill -USR1 "$ST_PID"
  wait_for_pixel 300 200 '#e8e6e1'
  kill -0 "$ST_PID"
}

@test "repeated USR1 reloads do not leak X connections" {
  start_xvfb
  printf 'st.background: #282828\n' | xrdb -load
  "$WORK/st" -g 80x24+0+0 & ST_PID=$!
  wait_for_pixel 300 200 '#282828'
  before=$(ls -l /proc/$ST_PID/fd | grep -c socket)
  for i in 1 2 3 4; do kill -USR1 "$ST_PID"; sleep 0.3; done
  xsetroot -name x
  sleep 0.3
  after=$(ls -l /proc/$ST_PID/fd | grep -c socket)
  [ "$after" -le "$before" ] || { echo "sockets $before -> $after"; return 1; }
  kill -0 "$ST_PID"
}

bidi() {
  cc -I"$WORK" "$REPO/tests/bidi_test.c" "$WORK/bidi.c" $(pkg-config --cflags --libs fribidi) -o "$BATS_TEST_TMPDIR/bidi" &&
    "$BATS_TEST_TMPDIR/bidi"
}

@test "bidi: a line with no right to left text is drawn as it is" {
  run bidi
  [ "$status" -eq 0 ]
  [ "${lines[0]}" = "same 006c:0 0073:1 0020:2 002d:3 006c:4" ]
}

@test "bidi: arabic is reordered and joined, one letter per cell, colors moving with letters" {
  run bidi
  [ "$status" -eq 0 ]
  [ "${lines[1]}" = "copy 0061:0 0062:1 0020:2 fee1:6 fe8e:5 fee0:4 feb3:3" ]
}

@test "st links fribidi" {
  ldd "$WORK/st" | grep -q libfribidi
}

@test "st survives drawing a line of mixed arabic and latin" {
  start_xvfb
  xrdb -load /dev/null
  "$WORK/st" -e sh -c 'printf "ls سلام 123 (مرحبا)\n"; sleep 3' & ST_PID=$!
  sleep 1.5
  kill -0 "$ST_PID"
  kill "$ST_PID"
}
