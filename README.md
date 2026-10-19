# suckless

My dwm, st, dmenu and dwmblocks, as they run on my machine.

```
make && sudo make install
```

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
  instead of restarting, and resource values are copied with a bound.
- **st** 0.9.2 with xresources-signal-reloading, plus a fix: a key missing
  from Xresources no longer inherits the previous key's value.
- **dmenu** the BreadOnPenguins fork (center + xresources), plus a fix for a
  crash when no Xresources are loaded.
- **dwmblocks** torrinfail's, with `blocks.def.h` rebuilt from my installed
  binary (its source was lost). Scripts live in dots: `config/dwmblocks/scripts/`.

## Tests

`bats tests/` builds everything and checks real pixels in a headless X
server (needs `xorg-server-xvfb`).
