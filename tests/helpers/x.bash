#!/usr/bin/env bash
# Headless X for pixel tests. Needs Xvfb, xrdb and ImageMagick.

start_xvfb() {
  command -v Xvfb >/dev/null || skip "Xvfb not installed (pacman -S xorg-server-xvfb)"
  export DISPLAY=:99
  Xvfb :99 -screen 0 800x600x24 -nolisten tcp -noreset >/dev/null 2>&1 &
  XVFB_PID=$!
  local i
  for i in $(seq 50); do xdpyinfo >/dev/null 2>&1 && return 0; sleep 0.1; done
  echo "Xvfb did not start" >&2
  return 1
}

stop_xvfb() {
  [[ -n "${XVFB_PID:-}" ]] && kill "$XVFB_PID" 2>/dev/null
  wait "$XVFB_PID" 2>/dev/null || true
}

# pixel_at <x> <y>: color of the root window pixel as #rrggbb
pixel_at() {
  # xwd, not import: import misreads Xvfb pixels (#1b1f27 -> #1b1b1f)
  xwd -root -silent | magick xwd:- -crop "1x1+$1+$2" txt:- 2>/dev/null |
    grep -oE '#[0-9A-Fa-f]{6}' | head -1 | tr 'A-F' 'a-f'
}

# wait_for_pixel <x> <y> <#rrggbb>: up to 3s for the pixel to become a color
wait_for_pixel() {
  local i
  for i in $(seq 30); do
    [[ "$(pixel_at "$1" "$2")" == "$3" ]] && return 0
    sleep 0.1
  done
  echo "pixel $1,$2 is $(pixel_at "$1" "$2"), wanted $3" >&2
  return 1
}
