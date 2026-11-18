#!/usr/bin/env bats

load helpers/x

setup() {
  REPO="$(cd "$BATS_TEST_DIRNAME/.." && pwd)"
  WORK="$BATS_TEST_TMPDIR/note"
  rsync -a "$REPO/note/" "$WORK/"
}

@test "the text engine passes its unit tests" {
  make -C "$WORK" test
}

teardown() {
  pkill -f "$WORK/note" 2>/dev/null || true
  [[ -n "${DWM_PID:-}" ]] && kill "$DWM_PID" 2>/dev/null
  stop_xvfb
}

start_dwm_note() {
  command -v xdotool >/dev/null || skip "xdotool not installed"
  rsync -a "$REPO/dwm/" "$BATS_TEST_TMPDIR/dwm/"
  make -C "$BATS_TEST_TMPDIR/dwm" >/dev/null 2>&1
  make -C "$WORK" >/dev/null
  export PATH="$WORK:$PATH"
  export NOTES_DIR="$BATS_TEST_TMPDIR/notes"
  start_xvfb
  xrdb -load /dev/null
  "$BATS_TEST_TMPDIR/dwm/dwm" & DWM_PID=$!
  wait_for_pixel 400 5 '#282828'
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

note_win() {
  local before="${1:-}" id i
  for i in $(seq 50); do
    for id in $(xdotool search --class '^note$' 2>/dev/null); do
      [[ " $before " == *" $id "* ]] || { sleep 0.3; echo "$id"; return 0; }
    done
    sleep 0.1
  done
  return 1
}

geometry() { xdotool getwindowgeometry --shell "$1" | grep -E "^$2=" | cut -d= -f2; }

@test "a new note opens and typed text is saved to its file" {
  start_dwm_note
  note & win=$(note_win)
  xdotool type "buy milk"; sleep 0.6
  [ "$(cat "$NOTES_DIR"/*.txt)" = "buy milk" ]
}

@test "note files are named by creation time" {
  start_dwm_note
  note & note_win >/dev/null
  ls "$NOTES_DIR" | grep -Eq '^[0-9]{8}-[0-9]{6}\.txt$'
}

@test "closing a note keeps its text" {
  start_dwm_note
  note & win=$(note_win)
  xdotool type "keep me"
  xdotool key super+shift+c; sleep 0.5
  [ -z "$(xdotool search --class '^note$')" ]
  [ "$(cat "$NOTES_DIR"/*.txt)" = "keep me" ]
}

@test "closing an empty note leaves nothing behind" {
  start_dwm_note
  note & note_win >/dev/null
  xdotool key super+shift+c; sleep 0.5
  [ -z "$(ls -A "$NOTES_DIR")" ]
}

@test "a hard kill after a pause keeps the text" {
  start_dwm_note
  note & pid=$!; win=$(note_win)
  xdotool type "safe"; sleep 0.6
  kill -9 "$pid"
  [ "$(cat "$NOTES_DIR"/*.txt)" = "safe" ]
}

@test "escape then dd clears a line" {
  start_dwm_note
  note & win=$(note_win)
  xdotool type "one"; xdotool key Return; xdotool type "two"
  xdotool key Escape; xdotool type "dd"; sleep 0.6
  [ "$(cat "$NOTES_DIR"/*.txt)" = "one" ]
}

@test "vim keys work by physical key in the arabic layout" {
  start_dwm_note
  setxkbmap -layout us,ara
  note & win=$(note_win)
  xdotool type "one"; xdotool key Return; xdotool type "two"
  xdotool key Escape
  setxkbmap -layout ara,us
  xdotool key Arabic_yeh Arabic_yeh; sleep 0.6
  [ "$(cat "$NOTES_DIR"/*.txt)" = "one" ]
}

@test "the window is a plain note: class note, about 30 by 9 cells" {
  start_dwm_note
  note & win=$(note_win)
  xprop -id "$win" WM_CLASS | grep -q '"note", "note"'
  [ "$(geometry "$win" WIDTH)" -lt 400 ]
  [ "$(geometry "$win" HEIGHT)" -lt 300 ]
}
