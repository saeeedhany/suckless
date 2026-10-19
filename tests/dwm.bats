#!/usr/bin/env bats

load helpers/x

setup() {
  REPO="$(cd "$BATS_TEST_DIRNAME/.." && pwd)"
  WORK="$BATS_TEST_TMPDIR/dwm"
  rsync -a "$REPO/dwm/" "$WORK/"
  make -C "$WORK" >/dev/null 2>&1
}

# window_x <name>: x position of the window with that title (negative = hidden
# on another tag)
window_x() {
  local id
  id=$(xdotool search --name "$1" | head -1)
  xwininfo -id "$id" | awk '/Absolute upper-left X/ { print $4 }'
}

teardown() {
  [[ -n "${DWM_PID:-}" ]] && kill "$DWM_PID" 2>/dev/null
  stop_xvfb
}

@test "dmenucmd passes no color flags" {
  line=$(grep 'dmenucmd\[\]' "$REPO/dwm/config.def.h")
  [[ "$line" != *"-nb"* && "$line" != *"-nf"* && "$line" != *"-sb"* && "$line" != *"-sf"* ]]
}

@test "the binary carries the xresources marker theme looks for" {
  grep -qa normbgcolor "$WORK/dwm"
}

@test "bar uses the compiled default bg #282828" {
  start_xvfb
  xrdb -load /dev/null
  "$WORK/dwm" & DWM_PID=$!
  wait_for_pixel 400 5 '#282828'
}

@test "bar follows Xresources after SIGHUP and dwm survives" {
  start_xvfb
  xrdb -load /dev/null
  "$WORK/dwm" & DWM_PID=$!
  wait_for_pixel 400 5 '#282828'
  printf 'dwm.normbgcolor: #161717\ndwm.selbgcolor: #161717\n' | xrdb -merge
  kill -HUP "$DWM_PID"
  # dwm acts on the signal at its next X event; theme sends one the same way
  xsetroot -name "dots"
  wait_for_pixel 400 5 '#161717'
  kill -0 "$DWM_PID"
}

@test "SIGHUP recolors in place: dwm keeps its pid and windows keep their tags" {
  command -v xdotool >/dev/null || skip "xdotool not installed"
  rsync -a "$REPO/st/" "$BATS_TEST_TMPDIR/st/"
  make -C "$BATS_TEST_TMPDIR/st" >/dev/null 2>&1
  start_xvfb
  xrdb -load /dev/null
  "$WORK/dwm" & DWM_PID=$!
  wait_for_pixel 400 5 '#282828'
  "$BATS_TEST_TMPDIR/st/st" -t probe & ST_PID=$!
  for i in $(seq 30); do xdotool search --name probe >/dev/null 2>&1 && break; sleep 0.1; done
  sleep 0.3
  xdotool key super+shift+2
  sleep 0.3
  [ "$(window_x probe)" -lt 0 ]
  printf 'dwm.normbgcolor: #161717\ndwm.selbgcolor: #161717\n' | xrdb -merge
  kill -HUP "$DWM_PID"
  xsetroot -name "dots"
  wait_for_pixel 400 5 '#161717'
  kill -0 "$DWM_PID"
  [ "$(window_x probe)" -lt 0 ]
  kill "$ST_PID"
}
