# suckless

My dwm, st, dmenu and dwmblocks, as they run on my machine.

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
  `floatmax` of the screen and centered.
- **st** 0.9.3 with xresources-signal-reloading, plus fixes: a key missing
  from Xresources no longer inherits the previous key's value, the reload
  runs in the main loop instead of the signal handler, and it no longer
  leaks an X connection per reload.
  Arabic, Hebrew and Farsi read correctly: `st/bidi.c` uses FriBidi to put
  right to left runs in visual order and join Arabic letters into their
  presentation forms, one letter per cell, so the grid never shifts. Lines stay
  left aligned. The cursor and selection still follow the logical order.
- **dmenu** the BreadOnPenguins fork (center + xresources), plus a fix for a
  crash when no Xresources are loaded.
- **dwmblocks** torrinfail's, with `blocks.def.h` rebuilt from my installed
  binary (its source was lost). Scripts live in dots: `config/dwmblocks/scripts/`.

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
