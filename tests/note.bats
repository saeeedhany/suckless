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

@test "a new note is pinned and sits top right" {
  start_dwm_note
  note & win=$(note_win)
  xprop -id "$win" _NET_WM_STATE | grep -q STICKY
  x=$(geometry "$win" X); w=$(geometry "$win" WIDTH)
  [ $((800 - x - w)) -le 20 ]
}

@test "a second note goes below the first" {
  start_dwm_note
  note & a=$(note_win)
  note & b=$(note_win "$a")
  [ "$(geometry "$b" Y)" -gt "$(geometry "$a" Y)" ]
  [ "$(geometry "$b" X)" -eq "$(geometry "$a" X)" ]
}

@test "a new note takes focus from the terminal on the tag" {
  start_dwm_note
  rsync -a "$REPO/st/" "$BATS_TEST_TMPDIR/stsrc/"
  make -C "$BATS_TEST_TMPDIR/stsrc" >/dev/null 2>&1
  "$BATS_TEST_TMPDIR/stsrc/st" & term=$(new_window ""); sleep 0.5
  note & win=$(note_win)
  [ "$(xdotool getactivewindow)" = "$win" ]
  pkill -f "$BATS_TEST_TMPDIR/stsrc/st" || true
}

@test "note -r brings back text, position and pinned state" {
  start_dwm_note
  note & win=$(note_win)
  xdotool type "back"
  xdotool windowmove "$win" 200 300; sleep 0.6
  x=$(geometry "$win" X); y=$(geometry "$win" Y)
  xdotool key super+shift+c; sleep 0.5
  note -r; win=$(note_win)
  [ "$(geometry "$win" X)" -eq "$x" ]
  [ "$(geometry "$win" Y)" -eq "$y" ]
  xprop -id "$win" _NET_WM_STATE | grep -q STICKY
  [ "$(cat "$NOTES_DIR"/*.txt)" = "back" ]
}

@test "an unpinned note comes back on its own tag" {
  start_dwm_note
  note & win=$(note_win)
  xdotool type "tag three"
  xdotool key super+shift+p; sleep 0.3
  xdotool key super+shift+3; sleep 0.6
  xdotool key super+3; sleep 0.3
  xdotool key super+shift+c; sleep 0.5
  xdotool key super+1; sleep 0.3
  note -r; win=$(note_win)
  [ "$(geometry "$win" X)" -lt 0 ]
  xdotool key super+3; sleep 0.3
  [ "$(geometry "$win" X)" -ge 0 ]
}

@test "note -r twice opens each note once" {
  start_dwm_note
  note & win=$(note_win)
  xdotool type "once"
  xdotool key super+shift+c; sleep 0.5
  note -r; note -r; sleep 1
  [ "$(xdotool search --class '^note$' | wc -l)" -eq 1 ]
}

@test "two open notes keep their own text" {
  start_dwm_note
  note & a=$(note_win)
  xdotool type "first"
  note & b=$(note_win "$a")
  xdotool type "second"; sleep 0.6
  [ "$(for f in "$NOTES_DIR"/*.txt; do cat "$f"; echo; done | sort | tr '\n' ' ')" = "first second " ]
}

@test "a missing or garbled position file still opens the note" {
  mkdir -p "$BATS_TEST_TMPDIR/notes"
  printf hi > "$BATS_TEST_TMPDIR/notes/20260101-000000.txt"
  echo garbage > "$BATS_TEST_TMPDIR/notes/.20260101-000000.pos"
  printf yo > "$BATS_TEST_TMPDIR/notes/20260101-000001.txt"
  start_dwm_note
  note -r; sleep 1.5
  [ "$(xdotool search --class '^note$' | wc -l)" -eq 2 ]
}

@test "other files in the notes folder are left alone" {
  mkdir -p "$BATS_TEST_TMPDIR/notes"
  : > "$BATS_TEST_TMPDIR/notes/todo.md"
  start_dwm_note
  note -r; sleep 1
  [ -z "$(xdotool search --class '^note$')" ]
  [ -e "$BATS_TEST_TMPDIR/notes/todo.md" ]
}

@test "a position off every screen is pulled back on" {
  mkdir -p "$BATS_TEST_TMPDIR/notes"
  printf far > "$BATS_TEST_TMPDIR/notes/20260101-000000.txt"
  echo "3000 200 240 180 1 1" > "$BATS_TEST_TMPDIR/notes/.20260101-000000.pos"
  start_dwm_note
  note -r; win=$(note_win)
  [ "$(geometry "$win" X)" -ge 0 ]
  [ $(( $(geometry "$win" X) + $(geometry "$win" WIDTH) )) -le 800 ]
}

@test "note -o refuses ids that are not note names" {
  start_dwm_note
  run note -o ../etc/passwd
  [ "$status" -ne 0 ]
  [ -z "$(xdotool search --class '^note$')" ]
}

@test "colors come from X resources and reload on SIGUSR1" {
  start_dwm_note
  printf 'note.background: #102030\n' | xrdb -merge
  note & pid=$!; win=$(note_win)
  x=$(( $(geometry "$win" X) + 3 )); y=$(( $(geometry "$win" Y) + 3 ))
  wait_for_pixel "$x" "$y" '#102030'
  printf 'note.background: #405060\n' | xrdb -merge
  kill -USR1 "$pid"
  wait_for_pixel "$x" "$y" '#405060'
}

@test "the default background is the black theme's" {
  start_dwm_note
  note & win=$(note_win)
  wait_for_pixel $(( $(geometry "$win" X) + 3 )) $(( $(geometry "$win" Y) + 3 )) '#222424'
}

@test "ctrl shift d throws the note into the trash" {
  start_dwm_note
  note & win=$(note_win)
  xdotool type "bye"; sleep 0.6
  xdotool key ctrl+shift+d; sleep 0.5
  [ -z "$(xdotool search --class '^note$')" ]
  [ "$(cat "$NOTES_DIR"/.trash/*.txt)" = "bye" ]
  ls "$NOTES_DIR"/.trash/.*.pos >/dev/null
  [ -z "$(ls "$NOTES_DIR"/*.txt 2>/dev/null)" ]
}

@test "ctrl shift v pastes the clipboard" {
  command -v xclip >/dev/null || skip "xclip not installed"
  start_dwm_note
  printf 'from clip' | xclip -selection clipboard
  note & win=$(note_win)
  xdotool key ctrl+shift+v; sleep 0.8
  [ "$(cat "$NOTES_DIR"/*.txt)" = "from clip" ]
}

@test "the binary carries the marker theme looks for" {
  make -C "$WORK" >/dev/null
  grep -qa note.background "$WORK/note"
}

@test "steady typing with no pause is still saved" {
  start_dwm_note
  note & pid=$!; win=$(note_win)
  xdotool type --delay 150 "abcdefghijklmnopqrstuvwxyz"
  kill -9 "$pid"
  [ "$(wc -c < "$NOTES_DIR"/*.txt)" -ge 10 ]
}

@test "a dwm restart leaves focus where it was, not on a note" {
  start_dwm_note
  rsync -a "$REPO/st/" "$BATS_TEST_TMPDIR/stsrc/"
  make -C "$BATS_TEST_TMPDIR/stsrc" >/dev/null 2>&1
  note & win=$(note_win)
  xdotool type "keep"; xdotool key Escape; sleep 0.6
  "$BATS_TEST_TMPDIR/stsrc/st" & term=$(new_window ""); sleep 0.5
  [ "$(xdotool getactivewindow)" = "$term" ]
  kill -USR1 "$DWM_PID"; xsetroot -name dots; sleep 1
  [ "$(xdotool getactivewindow)" = "$term" ]
  xdotool type "x"; sleep 0.6
  [ "$(cat "$NOTES_DIR"/*.txt)" = "keep" ]
  pkill -f "$BATS_TEST_TMPDIR/stsrc/st" || true
}

@test "a note file with broken utf8 opens without reading past its text" {
  mkdir -p "$BATS_TEST_TMPDIR/notes"
  printf 'abc\342' > "$BATS_TEST_TMPDIR/notes/20260101-000000.txt"
  rsync -a "$REPO/note/" "$BATS_TEST_TMPDIR/asan/"
  make -C "$BATS_TEST_TMPDIR/asan" clean >/dev/null
  make -C "$BATS_TEST_TMPDIR/asan" CC="cc -fsanitize=address -g" note >/dev/null 2>&1
  start_dwm_note
  ASAN_OPTIONS=detect_leaks=0 "$BATS_TEST_TMPDIR/asan/note" -o 20260101-000000 2>"$BATS_TEST_TMPDIR/asan.log" & pid=$!
  win=$(note_win); sleep 0.5
  kill -0 "$pid"
  ! grep -q AddressSanitizer "$BATS_TEST_TMPDIR/asan.log"
}

text_gap() {
  xwd -id "$1" -silent | magick xwd:- txt:- 2>/dev/null |
    sed -nE 's/^ *([0-9]+), *([0-9]+):.*(#[0-9A-Fa-f]{6}).*/\1 \2 \3/p' | awk '
    $2 >= 9 && $2 < 26 && $1 > 0 {
      c = tolower($3)
      if (c == "#c2a46a") { if (bar == "" || $1 < bar) bar = $1 }
      else if (c != "#222424" && c != "#5a5a5a" && c != "#4e4e4e") { if ($1 > txt) txt = $1 }
    }
    END { if (bar == "" || txt == "") print -999; else print bar - txt }'
}

text_extent() {
  xwd -id "$1" -silent | magick xwd:- txt:- 2>/dev/null |
    sed -nE 's/^ *([0-9]+), *([0-9]+):.*(#[0-9A-Fa-f]{6}).*/\1 \2 \3/p' | awk -v y0="${2:-9}" -v y1="${3:-26}" '
    $2 >= y0 && $2 < y1 && $1 > 0 {
      c = tolower($3)
      if (c == "#c2a46a") { if (bar == "" || $1 < bar) bar = $1 }
      else if (c != "#222424" && c != "#5a5a5a" && c != "#4e4e4e") {
        if (left == "" || $1 < left) left = $1
        if ($1 > right) right = $1
      }
    }
    END { print bar + 0, left + 0, right + 0 }'
}

@test "latin text is measured with the font that draws it, so the cursor sits right after it" {
  start_dwm_note
  note & win=$(note_win)
  xdotool type "hello there"; sleep 0.4
  gap=$(text_gap "$win")
  [ "$gap" -ge 0 ]
  [ "$gap" -le 4 ]
}

@test "an arabic line reads right to left: it hugs the right edge and the cursor sits at its left" {
  start_dwm_note
  setxkbmap -layout ara
  note & win=$(note_win)
  xdotool type "سلامسلامسلام"; sleep 0.4
  read -r bar left right < <(text_extent "$win")
  w=$(geometry "$win" WIDTH)
  [ "$right" -ge $((w - 16)) ]
  [ $((left - bar)) -ge 0 ]
  [ $((left - bar)) -le 4 ]
}

@test "on two monitors an unpinned note comes back on its monitor and tag" {
  command -v xdotool >/dev/null || skip "xdotool not installed"
  mkdir -p "$BATS_TEST_TMPDIR/notes"
  printf 'right' > "$BATS_TEST_TMPDIR/notes/20260101-000000.txt"
  echo "900 100 240 180 0 4" > "$BATS_TEST_TMPDIR/notes/.20260101-000000.pos"
  rsync -a "$REPO/dwm/" "$BATS_TEST_TMPDIR/dwm/"
  make -C "$BATS_TEST_TMPDIR/dwm" >/dev/null 2>&1
  make -C "$WORK" >/dev/null
  export PATH="$WORK:$PATH" NOTES_DIR="$BATS_TEST_TMPDIR/notes" DISPLAY=:99
  start_two_monitors
  note -r; win=$(note_win)
  [ "$(geometry "$win" X)" -lt 0 ]
  xdotool mousemove 1200 300; sleep 0.2; xdotool key super+3; sleep 0.4
  [ "$(geometry "$win" X)" -ge 800 ]
}

@test "logging out saves what was typed" {
  start_dwm_note
  note & pid=$!; win=$(note_win)
  xdotool type "bye"
  kill "$XVFB_PID"; sleep 0.8
  [ "$(cat "$NOTES_DIR"/*.txt)" = "bye" ]
}

@test "logging out with an empty note leaves nothing behind" {
  start_dwm_note
  note & note_win >/dev/null
  kill "$XVFB_PID"; sleep 0.8
  [ -z "$(ls -A "$NOTES_DIR")" ]
}

@test "a bad color in X resources is ignored" {
  start_dwm_note
  printf 'note.background: #zzzzzz\n' | xrdb -merge
  note & pid=$!; win=$(note_win)
  sleep 0.3
  kill -0 "$pid"
}

@test "a note that cannot be read is not opened, so it cannot be emptied" {
  mkdir -p "$BATS_TEST_TMPDIR/notes/20260101-000000.txt"
  start_dwm_note
  run timeout 5 note -o 20260101-000000
  [ "$status" -ne 0 ]
  [ -d "$BATS_TEST_TMPDIR/notes/20260101-000000.txt" ]
}

@test "a new note is 36 by 12 cells, a little bigger than before" {
  start_dwm_note
  note & win=$(note_win)
  [ "$(geometry "$win" WIDTH)" -ge 330 ]
  [ "$(geometry "$win" HEIGHT)" -ge 225 ]
}

@test "pasting drops control bytes, keeps newlines and turns tabs into spaces" {
  command -v xclip >/dev/null || skip "xclip not installed"
  start_dwm_note
  printf 'a\001b\r\nc\td' | xclip -selection clipboard
  note & win=$(note_win)
  xdotool key ctrl+shift+v; sleep 0.8
  [ "$(cat "$NOTES_DIR"/*.txt)" = "$(printf 'ab\nc  d')" ]
}

@test "a saved size bigger than the screen is shrunk to fit" {
  mkdir -p "$BATS_TEST_TMPDIR/notes"
  printf big > "$BATS_TEST_TMPDIR/notes/20260101-000000.txt"
  echo "100 100 99999 99999 1 1" > "$BATS_TEST_TMPDIR/notes/.20260101-000000.pos"
  start_dwm_note
  note -r; win=$(note_win)
  [ "$(geometry "$win" WIDTH)" -le 800 ]
  [ "$(geometry "$win" HEIGHT)" -le 600 ]
}

@test "a save that fails is tried again" {
  start_dwm_note
  note & win=$(note_win)
  chmod 500 "$NOTES_DIR"
  xdotool type "later"; sleep 0.8
  chmod 700 "$NOTES_DIR"; sleep 1
  [ "$(cat "$NOTES_DIR"/*.txt)" = "later" ]
}

@test "note -r reopens notes even when note is not on PATH" {
  mkdir -p "$BATS_TEST_TMPDIR/notes"
  printf here > "$BATS_TEST_TMPDIR/notes/20260101-000000.txt"
  start_dwm_note
  (PATH=/usr/bin:/bin; exec -a note "$WORK/note" -r)
  note_win >/dev/null
}

@test "ctrl shift c cycles the note color and the color is remembered" {
  start_dwm_note
  note & win=$(note_win)
  xdotool type "tinted"
  x=$(( $(geometry "$win" X) + 3 )); y=$(( $(geometry "$win" Y) + 3 ))
  wait_for_pixel "$x" "$y" '#222424'
  xdotool key ctrl+shift+c
  wait_for_pixel "$x" "$y" '#4a4435'
  xdotool key ctrl+shift+c
  wait_for_pixel "$x" "$y" '#3d4136'
  sleep 0.5
  [ "$(cut -d' ' -f7 "$NOTES_DIR"/.*.pos)" = 2 ]
  xdotool key super+shift+c; sleep 0.5
  note -r; win=$(note_win)
  wait_for_pixel $(( $(geometry "$win" X) + 3 )) $(( $(geometry "$win" Y) + 3 )) '#3d4136'
}

@test "the five note colors come from X resources" {
  start_dwm_note
  printf 'note.color1: #112233\n' | xrdb -merge
  note & win=$(note_win)
  xdotool key ctrl+shift+c
  wait_for_pixel $(( $(geometry "$win" X) + 3 )) $(( $(geometry "$win" Y) + 3 )) '#112233'
}

@test "an old six field position file still opens with the plain color" {
  mkdir -p "$BATS_TEST_TMPDIR/notes"
  printf old > "$BATS_TEST_TMPDIR/notes/20260101-000000.txt"
  echo "100 100 300 200 1 1" > "$BATS_TEST_TMPDIR/notes/.20260101-000000.pos"
  start_dwm_note
  note -r; win=$(note_win)
  [ "$(geometry "$win" X)" -eq 100 ]
  wait_for_pixel 103 103 '#222424'
}

start_two_monitors() {
  Xvfb :99 -screen 0 1600x600x24 -nolisten tcp -noreset >/dev/null 2>&1 & XVFB_PID=$!
  for i in $(seq 50); do xdpyinfo >/dev/null 2>&1 && break; sleep 0.1; done
  xrandr --setmonitor left 800/200x600/150+0+0 screen >/dev/null
  xrandr --setmonitor right 800/200x600/150+800+0 none >/dev/null
  xrdb -load /dev/null
  "$BATS_TEST_TMPDIR/dwm/dwm" & DWM_PID=$!
  wait_for_pixel 400 5 '#282828'
}

fake_dmenu() {
  mkdir -p "$BATS_TEST_TMPDIR/fake"
  printf '#!/bin/sh\ncat > "%s/menu"\n%s\n' "$BATS_TEST_TMPDIR" "$1" > "$BATS_TEST_TMPDIR/fake/dmenu"
  chmod +x "$BATS_TEST_TMPDIR/fake/dmenu"
  export PATH="$BATS_TEST_TMPDIR/fake:$PATH"
}

two_notes() {
  mkdir -p "$BATS_TEST_TMPDIR/notes"
  printf '  first line\nmore' > "$BATS_TEST_TMPDIR/notes/20260101-000000.txt"
  touch -d '1 hour ago' "$BATS_TEST_TMPDIR/notes/20260101-000000.txt"
  printf 'second' > "$BATS_TEST_TMPDIR/notes/20260101-000001.txt"
}

@test "note -p lists saved notes newest first by their first line, and opens the pick" {
  two_notes
  start_dwm_note
  fake_dmenu 'sed -n 2p "'"$BATS_TEST_TMPDIR"'/menu"'
  note -p; note_win >/dev/null
  [ "$(cat "$BATS_TEST_TMPDIR/menu")" = "$(printf '1  second\n2  first line')" ]
  ! flock -n "$NOTES_DIR/.20260101-000000.pos" true
  [ "$(xdotool search --class '^note$' | wc -l)" -eq 1 ]
}

@test "an open note is marked in the picker, and picking it flags it instead of opening a second one" {
  two_notes
  start_dwm_note
  note -o 20260101-000001 & win=$(note_win)
  xdotool key super+shift+p super+shift+2; sleep 0.5
  fake_dmenu 'sed -n 1p "'"$BATS_TEST_TMPDIR"'/menu"'
  note -p; sleep 1
  [ "$(head -1 "$BATS_TEST_TMPDIR/menu")" = "1  second  (open)" ]
  [ "$(xdotool search --class '^note$' | wc -l)" -eq 1 ]
  xprop -id "$win" WM_HINTS | grep -qi urgen
}

@test "escape in the picker opens nothing" {
  two_notes
  start_dwm_note
  fake_dmenu 'exit 1'
  run note -p
  [ "$status" -eq 0 ]
  sleep 0.5
  [ -z "$(xdotool search --class '^note$')" ]
}

@test "the picker shows arabic first lines whole" {
  mkdir -p "$BATS_TEST_TMPDIR/notes"
  printf 'سلام' > "$BATS_TEST_TMPDIR/notes/20260101-000000.txt"
  start_dwm_note
  fake_dmenu 'exit 1'
  note -p
  [ "$(cat "$BATS_TEST_TMPDIR/menu")" = "1  سلام" ]
}

@test "a wrapped arabic line keeps reading right to left on a row that starts with english" {
  mkdir -p "$BATS_TEST_TMPDIR/notes"
  printf 'سلام سلام سلام سلام سلام سلام سلام سلام سلام helloworldlong' > "$BATS_TEST_TMPDIR/notes/20260101-000000.txt"
  start_dwm_note
  note -r; win=$(note_win); sleep 0.3
  read -r bar left right < <(text_extent "$win" 30 44)
  w=$(geometry "$win" WIDTH)
  [ "$right" -ge $((w - 16)) ]
}

@test "typing english at the end of an arabic line puts the cursor right after the english" {
  start_dwm_note
  setxkbmap -layout ara
  note & win=$(note_win)
  xdotool type "سلام"
  setxkbmap -layout us
  xdotool type " abc"; sleep 0.4
  read -r bar left right < <(text_extent "$win")
  [ "$bar" -gt $((left + 20)) ]
}

@test "on two monitors a new note opens focused even when the other monitor was picked by keyboard" {
  command -v xdotool >/dev/null || skip "xdotool not installed"
  rsync -a "$REPO/dwm/" "$BATS_TEST_TMPDIR/dwm/"
  make -C "$BATS_TEST_TMPDIR/dwm" >/dev/null 2>&1
  make -C "$WORK" >/dev/null
  export PATH="$WORK:$PATH" NOTES_DIR="$BATS_TEST_TMPDIR/notes" DISPLAY=:99
  start_two_monitors
  xdotool mousemove 1200 300; sleep 0.2
  note & first=$(note_win)
  xdotool key super+comma; sleep 0.3
  note & win=$(note_win "$first")
  xdotool type "hello"; sleep 0.6
  [ "$(xdotool getactivewindow)" = "$win" ]
  grep -qx hello "$NOTES_DIR"/*.txt
}

@test "a theme switch while the picker is open does not cancel the pick" {
  two_notes
  start_dwm_note
  fake_dmenu 'sleep 1; sed -n 2p "'"$BATS_TEST_TMPDIR"'/menu"'
  note -p & pid=$!
  sleep 0.4; kill -USR1 "$pid"
  wait "$pid"
  note_win >/dev/null
}

@test "every note ignores broken pipes, however it was started" {
  start_dwm_note
  note & pid=$!; note_win >/dev/null
  (( 0x$(awk '/^SigIgn/ { print $2 }' "/proc/$pid/status") & 0x1000 ))
}

@test "a note shows a mark while its saves fail, and drops it once a save works" {
  [ "$(id -u)" -ne 0 ] || skip "root can write to any folder"
  start_dwm_note
  note & win=$(note_win)
  x=$(( $(geometry "$win" X) + $(geometry "$win" WIDTH) - 4 )); y=$(( $(geometry "$win" Y) + 3 ))
  chmod 500 "$NOTES_DIR"
  xdotool type "stuck"
  wait_for_pixel "$x" "$y" '#c2a46a'
  chmod 700 "$NOTES_DIR"
  wait_for_pixel "$x" "$y" '#222424'
}

@test "an empty line after an arabic line keeps the cursor on the right" {
  start_dwm_note
  setxkbmap -layout ara
  note & win=$(note_win)
  xdotool type "سلام"; xdotool key Return; sleep 0.4
  read -r bar left right < <(text_extent "$win" 30 44)
  [ "$bar" -gt $(( $(geometry "$win" WIDTH) / 2 )) ]
}

@test "the picker cuts a long arabic first line on a whole letter" {
  mkdir -p "$BATS_TEST_TMPDIR/notes"
  for i in $(seq 60); do printf 'س'; done > "$BATS_TEST_TMPDIR/notes/20260101-000000.txt"
  start_dwm_note
  fake_dmenu 'exit 1'
  note -p
  iconv -f UTF-8 -t UTF-8 "$BATS_TEST_TMPDIR/menu" >/dev/null
  [ "$(wc -c < "$BATS_TEST_TMPDIR/menu")" -le 100 ]
  [ "$(wc -c < "$BATS_TEST_TMPDIR/menu")" -ge 80 ]
}

@test "a dwm restart keeps a hidden unpinned note on its own monitor and tag" {
  command -v xdotool >/dev/null || skip "xdotool not installed"
  mkdir -p "$BATS_TEST_TMPDIR/notes"
  printf 'right' > "$BATS_TEST_TMPDIR/notes/20260101-000000.txt"
  echo "900 100 240 180 0 4" > "$BATS_TEST_TMPDIR/notes/.20260101-000000.pos"
  rsync -a "$REPO/dwm/" "$BATS_TEST_TMPDIR/dwm/"
  make -C "$BATS_TEST_TMPDIR/dwm" >/dev/null 2>&1
  make -C "$WORK" >/dev/null
  export PATH="$WORK:$PATH" NOTES_DIR="$BATS_TEST_TMPDIR/notes" DISPLAY=:99
  start_two_monitors
  note -r; win=$(note_win)
  [ "$(geometry "$win" X)" -lt 0 ]
  xdotool mousemove 100 300; sleep 0.2
  kill -USR1 "$DWM_PID"; xsetroot -name dots; sleep 1
  xdotool key super+period super+3; sleep 0.4
  [ "$(geometry "$win" X)" -ge 800 ]
}

@test "a broken byte before a line break keeps the next line on its own row" {
  mkdir -p "$BATS_TEST_TMPDIR/notes"
  printf 'x\xd8\nab' > "$BATS_TEST_TMPDIR/notes/20260101-000000.txt"
  start_dwm_note
  note -o 20260101-000000 & win=$(note_win)
  read -r bar left right < <(text_extent "$win" 30 44)
  [ "$right" -gt 0 ]
}

@test "the picker marks no note as open when the notes folder is read only" {
  [ "$(id -u)" -ne 0 ] || skip "root can write to any folder"
  two_notes
  start_dwm_note
  chmod 500 "$NOTES_DIR"
  fake_dmenu 'exit 1'
  note -p
  chmod 700 "$NOTES_DIR"
  [ "$(wc -l < "$BATS_TEST_TMPDIR/menu")" -eq 2 ]
  ! grep -q "(open)" "$BATS_TEST_TMPDIR/menu"
}

@test "yy copies the line to the clipboard" {
  command -v xclip >/dev/null || skip "xclip not installed"
  start_dwm_note
  note & win=$(note_win)
  xdotool type "first"; xdotool key Return; xdotool type "second"
  xdotool key Escape; xdotool type "kyy"; sleep 0.3
  [ "$(timeout 2 xclip -o -selection clipboard)" = "first" ]
}

@test "yw copies a word to the clipboard" {
  command -v xclip >/dev/null || skip "xclip not installed"
  start_dwm_note
  note & win=$(note_win)
  xdotool type "buy milk"
  xdotool key Escape; xdotool type "0yw"; sleep 0.3
  [ "$(timeout 2 xclip -o -selection clipboard)" = "buy " ]
}

@test "ctrl shift y copies the whole note to the clipboard" {
  command -v xclip >/dev/null || skip "xclip not installed"
  start_dwm_note
  note & win=$(note_win)
  xdotool type "one"; xdotool key Return; xdotool type "سلام"
  xdotool key ctrl+shift+y; sleep 0.3
  [ "$(timeout 2 xclip -o -selection clipboard)" = "$(printf 'one\nسلام')" ]
}

@test "deleting text leaves the clipboard alone" {
  command -v xclip >/dev/null || skip "xclip not installed"
  start_dwm_note
  printf 'keep' | xclip -selection clipboard
  note & win=$(note_win)
  xdotool type "gone"; xdotool key Escape; xdotool type "dd"; sleep 0.3
  [ "$(timeout 2 xclip -o -selection clipboard)" = "keep" ]
}

@test "a note pastes what it copied itself" {
  command -v xclip >/dev/null || skip "xclip not installed"
  start_dwm_note
  note & win=$(note_win)
  xdotool type "twice"; xdotool key Escape; xdotool type "yyA"; xdotool key ctrl+shift+v; sleep 0.8
  [ "$(cat "$NOTES_DIR"/*.txt)" = "twicetwice" ]
}

@test "copied text stays on the clipboard after the note closes" {
  command -v xclip >/dev/null || skip "xclip not installed"
  start_dwm_note
  note & pid=$!; win=$(note_win)
  xdotool type "keep me"; xdotool key Escape; xdotool type "yy"; sleep 0.3
  kill "$pid"; wait "$pid" || true
  [ "$(timeout 2 xclip -o -selection clipboard)" = "keep me" ]
}

@test "the process that keeps the clipboard does not hold the note open" {
  command -v xclip >/dev/null || skip "xclip not installed"
  start_dwm_note
  note & pid=$!; win=$(note_win)
  xdotool type "held"; xdotool key Escape; xdotool type "yy"; sleep 0.3
  kill "$pid"; wait "$pid" || true
  fake_dmenu 'exit 1'
  note -p
  ! grep -q "(open)" "$BATS_TEST_TMPDIR/menu"
}

@test "the process that keeps the clipboard leaves once something else is copied" {
  command -v xclip >/dev/null || skip "xclip not installed"
  start_dwm_note
  "$WORK/note" & pid=$!; win=$(note_win)
  xdotool type "old"; xdotool key Escape; xdotool type "yy"; sleep 0.3
  kill "$pid"; wait "$pid" || true
  sleep 0.3
  [ "$(pgrep -fc "$WORK/note")" -eq 1 ]
  printf 'new' | xclip -selection clipboard; sleep 0.5
  [ "$(pgrep -fc "$WORK/note")" -eq 0 ]
  [ "$(timeout 2 xclip -o -selection clipboard)" = "new" ]
}

@test "y with a motion copies to the clipboard" {
  command -v xclip >/dev/null || skip "xclip not installed"
  start_dwm_note
  note & win=$(note_win)
  xdotool type "buy oat milk"; xdotool key Escape; xdotool type "0wy\$"; sleep 0.3
  [ "$(timeout 2 xclip -o -selection clipboard)" = "oat milk" ]
}
