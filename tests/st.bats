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
