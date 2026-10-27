#!/usr/bin/env bats

load helpers/x

setup() {
  REPO="$(cd "$BATS_TEST_DIRNAME/.." && pwd)"
  WORK="$BATS_TEST_TMPDIR/dmenu"
  rsync -a "$REPO/dmenu/" "$WORK/"
  make -C "$WORK" >/dev/null 2>&1
}

teardown() {
  [[ -n "${DMENU_PID:-}" ]] && kill "$DMENU_PID" 2>/dev/null
  stop_xvfb
}

@test "compiled defaults are the grey theme" {
  grep -q '\[SchemeNorm\] = { "#93877c", "#282828" }' "$REPO/dmenu/config.def.h"
  grep -q '\[SchemeSel\] = { "#ccc3b8", "#282828" }' "$REPO/dmenu/config.def.h"
  grep -q '\[SchemeOut\] = { "#282828", "#c8a46b" }' "$REPO/dmenu/config.def.h"
}

@test "dmenu takes its background from Xresources" {
  start_xvfb
  printf 'dmenu.background: #161717\ndmenu.selbackground: #161717\n' | xrdb -load
  echo x | "$WORK/dmenu" & DMENU_PID=$!
  wait_for_pixel 600 44 "#161717"
}

@test "dmenu runs with no Xresources loaded at all" {
  start_xvfb
  xrdb -remove 2>/dev/null || true
  echo x | "$WORK/dmenu" & DMENU_PID=$!
  wait_for_pixel 600 44 "#282828"
  kill -0 "$DMENU_PID"
}

pick_with() {
  start_xvfb
  command -v xdotool >/dev/null || skip "xdotool not installed"
  out="$BATS_TEST_TMPDIR/out"
  printf 'one\ntwo\nthree\n' | "$WORK/dmenu" -l 3 >"$out" & DMENU_PID=$!
  wait_for_pixel 600 44 "#282828"
  sleep 0.2
  xdotool key "$@" Return
  wait "$DMENU_PID" || true
  picked=$(cat "$out")
}

@test "ctrl j moves down the list" {
  pick_with ctrl+j ctrl+j
  [ "$picked" = three ]
}

@test "ctrl k moves back up the list" {
  pick_with ctrl+j ctrl+j ctrl+k
  [ "$picked" = two ]
}

geometry() {
  local id i
  for i in $(seq 30); do
    id=$(xdotool search --class dmenu 2>/dev/null | head -1) && [[ -n "$id" ]] && break
    sleep 0.1
  done
  xwininfo -id "$id" | awk '
    /Absolute upper-left X/ { x = $NF } /Absolute upper-left Y/ { y = $NF }
    /Width:/ { w = $NF } /Height:/ { h = $NF } /Border width/ { b = $NF }
    END { print x, y, w, h, b }'
}

@test "dmenu floats in the top right corner, under the bar, with a thin border" {
  start_xvfb
  command -v xdotool >/dev/null || skip "xdotool not installed"
  printf 'one\ntwo\n' | "$WORK/dmenu" -l 2 & DMENU_PID=$!
  read -r x y w h b < <(geometry)
  [ "$b" -eq 1 ]
  [ $((x + w + 2 * b)) -eq $((800 - 12)) ]
  [ "$y" -gt 12 ] && [ "$y" -lt 60 ]
}

@test "dmenu draws its border in the color from Xresources" {
  start_xvfb
  command -v xdotool >/dev/null || skip "xdotool not installed"
  printf 'dmenu.background: #161717\ndmenu.bordercolor: #484b4b\n' | xrdb -load
  printf 'one\ntwo\n' | "$WORK/dmenu" -l 2 & DMENU_PID=$!
  read -r x y w h b < <(geometry)
  wait_for_pixel $((x + w / 2)) "$y" "#484b4b"
  wait_for_pixel $((x + w / 2)) $((y + b + 2)) "#161717"
}

@test "compiled border default is the grey line2" {
  grep -q 'bordercolor = "#5a5a5a"' "$REPO/dmenu/config.def.h"
}
