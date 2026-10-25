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

@test "the binary carries the restart marker the suckless command looks for" {
  grep -qa _NET_CLIENT_INFO "$WORK/dwm"
}

start_probe() {
  rsync -a "$REPO/st/" "$BATS_TEST_TMPDIR/st/"
  make -C "$BATS_TEST_TMPDIR/st" >/dev/null 2>&1
  "$BATS_TEST_TMPDIR/st/st" -t probe & ST_PID=$!
  for i in $(seq 30); do xdotool search --name probe >/dev/null 2>&1 && break; sleep 0.1; done
  sleep 0.3
}

@test "a window records its tag and monitor in _NET_CLIENT_INFO" {
  command -v xdotool >/dev/null || skip "xdotool not installed"
  start_xvfb
  xrdb -load /dev/null
  "$WORK/dwm" & DWM_PID=$!
  wait_for_pixel 400 5 '#282828'
  start_probe
  xdotool key super+shift+2
  sleep 0.3
  [ "$(xprop -id "$(xdotool search --name probe | head -1)" _NET_CLIENT_INFO | sed 's/.*= //')" = "2, 0, 0" ]
  kill "$ST_PID"
}

@test "SIGUSR1 restarts dwm in place and every window keeps its tag" {
  command -v xdotool >/dev/null || skip "xdotool not installed"
  start_xvfb
  xrdb -load /dev/null
  "$WORK/dwm" & DWM_PID=$!
  wait_for_pixel 400 5 '#282828'
  start_probe
  xdotool key super+shift+2
  sleep 0.3
  [ "$(window_x probe)" -lt 0 ]
  printf 'dwm.normbgcolor: #161717\ndwm.selbgcolor: #161717\n' | xrdb -merge
  kill -USR1 "$DWM_PID"
  xsetroot -name "dots"
  wait_for_pixel 400 5 '#161717'
  kill -0 "$DWM_PID"
  [ "$(window_x probe)" -lt 0 ]
  xdotool key super+2
  sleep 0.3
  [ "$(window_x probe)" -ge 0 ]
  kill "$ST_PID"
}

@test "a color reload frees the old colors with drw_scm_free" {
  body=$(awk '/^reloadcolors\(void\)/,/^}/' "$REPO/dwm/dwm.c")
  [[ "$body" == *"drw_scm_free(drw, scheme[i], 3)"* ]]
  ! grep -qE '^[[:space:]]+free\(scheme\[i\]\);' <<<"$body"
}

new_window() {
  local before="$1" id i
  for i in $(seq 50); do
    for id in $(xdotool search --classname st 2>/dev/null); do
      [[ " $before " == *" $id "* ]] || { echo "$id"; return 0; }
    done
    sleep 0.1
  done
  return 1
}

geometry() { xdotool getwindowgeometry --shell "$1" | grep -E "^$2=" | cut -d= -f2; }

@test "super ctrl return floats the next st only, centered; super shift return still tiles" {
  command -v xdotool >/dev/null || skip "xdotool not installed"
  rsync -a "$REPO/st/" "$BATS_TEST_TMPDIR/stbin/"
  make -C "$BATS_TEST_TMPDIR/stbin" >/dev/null 2>&1
  export PATH="$BATS_TEST_TMPDIR/stbin:$PATH"
  start_xvfb
  xrdb -load /dev/null
  "$WORK/dwm" & DWM_PID=$!
  wait_for_pixel 400 5 '#282828'
  xdotool key super+ctrl+Return
  float=$(new_window "")
  sleep 0.5
  fw=$(geometry "$float" WIDTH); fx=$(geometry "$float" X)
  [ "$fx" -gt 20 ]
  xdotool key super+shift+Return
  tiled=$(new_window "$float")
  sleep 0.5
  [ "$(geometry "$tiled" WIDTH)" -gt "$fw" ]
  [ "$(geometry "$tiled" X)" -lt 20 ]
  [ "$(geometry "$float" WIDTH)" -eq "$fw" ]
  pkill -f "$BATS_TEST_TMPDIR/stbin/st" || true
}

@test "a firefox floated by super ctrl w is capped at 60% by 70% of the screen, centered" {
  command -v xdotool >/dev/null || skip "xdotool not installed"
  rsync -a "$REPO/st/" "$BATS_TEST_TMPDIR/stbin/"
  make -C "$BATS_TEST_TMPDIR/stbin" >/dev/null 2>&1
  mkdir -p "$BATS_TEST_TMPDIR/wrap"
  printf '#!/bin/sh\nexec "%s/stbin/st" -g 300x120\n' "$BATS_TEST_TMPDIR" > "$BATS_TEST_TMPDIR/wrap/firefox"
  chmod +x "$BATS_TEST_TMPDIR/wrap/firefox"
  export PATH="$BATS_TEST_TMPDIR/wrap:$PATH"
  start_xvfb
  xrdb -load /dev/null
  "$WORK/dwm" & DWM_PID=$!
  wait_for_pixel 400 5 '#282828'
  xdotool key super+ctrl+w
  win=$(new_window "")
  sleep 0.5
  w=$(geometry "$win" WIDTH); h=$(geometry "$win" HEIGHT); x=$(geometry "$win" X)
  [ "$w" -le 480 ]
  [ "$h" -le 420 ]
  [ "$x" -ge 150 ] && [ "$x" -le 170 ]
  pkill -f "$BATS_TEST_TMPDIR/stbin/st" || true
}

start_dwm_with_st() {
  command -v xdotool >/dev/null || skip "xdotool not installed"
  rsync -a "$REPO/st/" "$BATS_TEST_TMPDIR/stbin/"
  make -C "$BATS_TEST_TMPDIR/stbin" >/dev/null 2>&1
  export PATH="$BATS_TEST_TMPDIR/stbin:$PATH"
  start_xvfb
  xrdb -load /dev/null
  "$WORK/dwm" & DWM_PID=$!
  wait_for_pixel 400 5 '#282828'
}

@test "the floating terminal is st with class floatterm, floated by a rule" {
  start_dwm_with_st
  xdotool key super+ctrl+Return
  win=$(new_window "")
  sleep 0.5
  xprop -id "$win" WM_CLASS | grep -q '"floatterm"'
  [ "$(geometry "$win" X)" -gt 20 ]
  [ "$(xprop -id "$win" _NET_CLIENT_INFO | sed 's/.*, //')" = 1 ]
  pkill -f "$BATS_TEST_TMPDIR/stbin/st" || true
}

@test "floating windows stay floating, and tiled ones tiled, across a restart" {
  start_dwm_with_st
  xdotool key super+ctrl+Return
  float=$(new_window "")
  sleep 0.5
  xdotool key super+shift+Return
  tiled=$(new_window "$float")
  sleep 0.5
  fw=$(geometry "$float" WIDTH)
  kill -USR1 "$DWM_PID"
  xsetroot -name "dots"
  sleep 1
  kill -0 "$DWM_PID"
  [ "$(geometry "$float" WIDTH)" -eq "$fw" ]
  [ "$(geometry "$float" X)" -gt 20 ]
  [ "$(geometry "$tiled" X)" -lt 20 ]
  [ "$(geometry "$tiled" WIDTH)" -gt "$fw" ]
  pkill -f "$BATS_TEST_TMPDIR/stbin/st" || true
}

@test "a window floated by hand stays floating after a restart" {
  start_dwm_with_st
  xdotool key super+shift+Return
  first=$(new_window "")
  sleep 0.5
  xdotool key super+shift+space
  sleep 0.3
  kill -USR1 "$DWM_PID"
  xsetroot -name "dots"
  sleep 1
  xdotool key super+shift+Return
  second=$(new_window "$first")
  sleep 0.5
  [ "$(geometry "$second" WIDTH)" -gt 700 ]
  pkill -f "$BATS_TEST_TMPDIR/stbin/st" || true
}
