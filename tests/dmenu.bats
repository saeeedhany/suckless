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
  # this dmenu centers horizontally at a quarter of the screen height
  wait_for_pixel 400 155 "#161717"
}

@test "dmenu runs with no Xresources loaded at all" {
  start_xvfb
  xrdb -remove 2>/dev/null || true
  echo x | "$WORK/dmenu" & DMENU_PID=$!
  wait_for_pixel 400 155 "#282828"
  kill -0 "$DMENU_PID"
}
