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

- **dwm** 6.5 with alwayscenter, attachbelow, bidi-restricted, uselessgap,
  xresources and restartsig (`dwm/patches/`). SIGHUP reloads colors in place
  instead of restarting, and resource values are copied with a bound. SIGUSR1
  restarts dwm in place, and every window keeps its tag and monitor: dwm keeps
  them in a `_NET_CLIENT_INFO` property on each window and reads it back.
- **st** 0.9.2 with xresources-signal-reloading, plus fixes: a key missing
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

## Tests

`bats tests/` builds everything and checks real pixels in a headless X
server (needs `xorg-server-xvfb`).
