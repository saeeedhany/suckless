# suckless

My dwm, st, dmenu, dwmblocks and note, as they run on my machine.

```
make install
```

Everything installs to `~/.local`, so no root is needed. Each tool has one
settings file, its `config.def.h` (`blocks.def.h` for dwmblocks): edit it and
build again, no `make clean` needed.

An older copy installed with sudo into `/usr/local` can be removed once with
`sudo make uninstall PREFIX=/usr/local`.

The [dots](https://github.com/saeeedhany/dots) installer clones this repo to
`~/.config/suckless` and builds it for you.

## Colors

Each tool reads its colors from the X resource database, so a theme switch
never needs a rebuild. The compiled defaults are the `grey` theme.

| Tool | Xresources keys | Reload |
|---|---|---|
| dwm | `dwm.normbgcolor` `dwm.normfgcolor` `dwm.normbordercolor` `dwm.selbgcolor` `dwm.selfgcolor` `dwm.selbordercolor` | `kill -HUP` dwm, then any X event (colors reload in place, windows keep their tags) |
| st | `st.color0`…`st.color15` `st.foreground` `st.background` `st.cursorColor` | `kill -USR1` st (every open window recolors) |
| dmenu | `dmenu.background` `dmenu.foreground` `dmenu.selbackground` `dmenu.selforeground` | read at every launch |
| note | `note.background` `note.color1`…`note.color4` `note.foreground` `note.cursor` | `kill -USR1` note (every open note recolors) |

dwm no longer passes color flags to dmenu, so every dmenu caller looks the same.

## What's in here

- **dwm** 6.8 with alwayscenter, attachbelow, bidi-restricted, uselessgap,
  xresources and restartsig (`dwm/patches/`). SIGHUP reloads colors in place
  instead of restarting, and resource values are copied with a bound. SIGUSR1
  restarts dwm in place, and every window keeps its tag and monitor: dwm keeps
  them, and whether it floats, in a `_NET_CLIENT_INFO` property on each window
  and reads it back. `super ctrl return` opens a floating st (class `floatterm`,
  floated by a rule); `super ctrl w` floats the next window that opens, for
  Firefox, whose new windows all share one class. That window is capped at
  `floatmax` of the screen and centered. `super shift p` pins a window to
  every tag (sticky patch); a pinned floating window moves to the top right,
  never takes focus on a tag switch, and stays pinned across a restart. A
  floating window that asks for a position (`USPosition`) keeps it.
- **st** 0.9.3 with xresources-signal-reloading, plus fixes: a key missing
  from Xresources no longer inherits the previous key's value, the reload
  runs in the main loop instead of the signal handler, and it no longer
  leaks an X connection per reload.
  Arabic, Hebrew and Farsi read correctly: `st/bidi.c` uses FriBidi to put
  right to left runs in visual order and join Arabic letters into their
  presentation forms, one letter per cell, so the grid never shifts. Lines stay
  left aligned. The cursor and selection still follow the logical order.
- **dmenu** the BreadOnPenguins fork (center + xresources), plus a fix for a
  crash when no Xresources are loaded. `ctrl j` and `ctrl k` move down and up
  the list, like `ctrl n` and `ctrl p` (they used to be Return and delete right).
  It floats in the top right corner under the bar, with a thin border in
  `dmenu.bordercolor`.
- **slock** 1.7, built from `config.def.h` like the rest, dropping privileges to
  `nobody` (Arch has no `nogroup`), with its colors read from Xresources
  (`slock.init`, `slock.input`, `slock.failed`) so `theme` recolors it. It
  installs setuid root, so it stays out of `make install`: run
  `sudo make -C slock install` once, and again only when slock itself changes.
- **dwmblocks** torrinfail's, with `blocks.def.h` rebuilt from my installed
  binary (its source was lost). Scripts live in dots: `config/dwmblocks/scripts/`.

## note

A sticky note: a plain square in the theme colors that you type on.
`super n` opens a new one, pinned to every tag; `super shift n` brings back
the ones you closed; `note -r` does the same at login. Text is saved as you
type to `~/notes/<id>.txt`, and where the note sits, whether it is pinned and
its tag go to `~/notes/.<id>.pos`. Closing an empty note removes it;
`ctrl shift d` moves a note to `~/notes/.trash`. `super ctrl n` picks any
saved note in dmenu by its first line; notes already open are marked `(open)`,
and picking one flags its window instead of opening a second copy.
`ctrl shift c` cycles a note through five colors drawn from the theme
(`note.color1` to `note.color4`). A small mark in the top right corner shows
while saves are failing, and goes away once one works.

It starts in insert mode. Escape gives a small vim normal mode: `h j k l`,
`w b e`, `0 ^ $`, `gg G`, `x`, `D`, `C`, `r`, `J`, `p`, `P`, `i a I A o O`, `u`
and `ctrl r`, and the operators `d`, `c` and `y` with the motions `w e b`,
`h l`, `0 ^ $`, `j k` and `gg G` (`dd`, `cw`, `y$`, `d2j`, `ygg`), all with
counts. Commands follow the physical key, so they work in the Arabic layout too.

Every yank also goes to the system clipboard, and `ctrl shift y` copies the
whole note. X keeps no copy of the clipboard itself, so when a note closes
while its text is still the latest copy, a small `note` process stays behind
to hand it out, and quits as soon as anything else is copied. `ctrl shift v`
pastes the clipboard.

A line that starts with Arabic reads right to left and sits against the right
edge, with its letters joined (`note/bidi.c`, FriBidi like st). An empty line
under it stays on the right, so the next line starts there. `note/buf.c` holds
the text and the keys, with no X code, and `make -C note test` checks every
command.

## Upstream

Each folder is linked to the upstream it is built on (`upstream.list`), so git
knows which lines are mine and which are upstream's.

```
./upstream                  each tool, the release it is built on, and the newest one
./upstream diff dwm         only my changes on top of upstream dwm
./upstream merge dwm 6.8    move dwm to 6.8, keeping my changes
```

A merge reapplies every change here onto the new release. Where a change
clashes with upstream, it stops and names the files; fix them, then `git add`
and `git commit`, or back out with `git merge --abort`.

## Tests

`bats tests/` builds everything and checks real pixels in a headless X
server (needs `xorg-server-xvfb`).
