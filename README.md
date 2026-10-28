# dwm-6.6-DWORF

**Dynamic Windows Of Reparenting Frames**

`dwm-6.6-DWORF` is a reparenting fork of [dwm 6.6](https://dwm.suckless.org/). It keeps dwm's minimal, keyboard-driven workflow, but gives tiled windows first-class frame management: every client gets a frame with a side titlebar, mouse dragging, per-window shading, configurable sizing factors, themes, and a companion dmenu implementation.

The goal is to bridge two worlds: the classic tiling window manager workflow and the familiar mouse-driven desktop, without turning dwm into a heavy desktop environment.

DWORF is designed to be used as a primary desktop environment, not only as a demo or a patch playground.

---

## Highlights

- **Reparenting frame architecture** — each client is reparented into its own frame. The frame owns the border, titlebar, geometry, and lifecycle.
- **Left vertical titlebars** — thin side titlebars with vertically rendered text.
- **Mouse-aware tiling** — drag the titlebar to reorder tiled windows; drag a floating window freely.
- **Double-click shading** — collapse a window to a titlebar and restore it later.
- **`cfact` / `mfact` control** — fine-grained per-window and master-area sizing.
- **Runtime themes** — all st color schemes from the bundled reference are available with `-S`.
- **Per-tag layouts** — layout, master count, `mfact`, and bar visibility are stored per tag.
- **Per-window keyboard layout** — each window remembers its XKB group.
- **Fullscreen without artifacts** — fullscreen clients cover the whole monitor and bypass the side titlebar.
- **Dual status bars** — optional second status bar on the primary monitor.
- **Gaps, urgently colored borders, monitor-bound floating windows**, and other usability improvements.
- **Companion dmenu** in `tools/dmenu-5.4-mod` with themes, highlighting, mouse support, scrolling, separators, and non-blocking stdin.

---

## Why DWORF?

Traditional tiling window managers are fast and predictable, but they often feel rigid to users who are used to dragging and resizing windows. Floating window managers are intuitive with a mouse, but lose the efficiency of tiled layouts.

DWORF keeps tiling and adds a thin, consistent frame/titlebar layer:

- Windows can be reordered by dragging their titlebars.
- Floating windows behave like normal floating windows.
- Tiled windows still obey their layout.
- Shading gives a quick way to get windows out of the way without minimizing them.
- Everything remains keyboard-accessible.

The result is still recognizably dwm, with more direct manipulation and better visual feedback.

---

## Requirements

Core dependencies:

- Xlib
- Xft
- Fontconfig
- FreeType
- Xinerama (optional, enabled by default in `config.mk`)
- XKB support through libX11

On Debian/Ubuntu-like systems, the development packages are typically:

```sh
sudo apt install build-essential libx11-dev libxft-dev \
    libfontconfig1-dev libfreetype6-dev libxinerama-dev
```

For the bundled dmenu, the same X11/Xft stack is sufficient.

---

## Build

```sh
make
```

The resulting binary is `./dwm-6.6`.

To install it manually:

```sh
sudo install -m 755 dwm-6.6 /usr/local/bin/dwm
```

Change `PREFIX` in `config.mk` if you use a different install prefix.

To rebuild after editing configuration or source:

```sh
make clean
make
```

---

## Running

Add dwm to your `.xinitrc` and start it with `startx`:

```sh
exec dwm -S ork
```

If you run the binary from the source directory:

```sh
exec /path/to/dwm-6.6 -S ork
```

The `-S` option selects a theme. Without it, dwm uses the colors defined directly in `config.h`.

A theme name is also passed automatically to the configured terminal (`st`) and dmenu command when they are spawned from dwm.

---

## Themes

Themes live in `themes.h` and come from the st color schemes in the bundled reference:

`default`, `dracula`, `solarized`, `nord`, `gruvbox`, `plan9`, `cyberpunk`, `light`, `ork`, `ledg`, `ledb`, `ledr`, `ledw`, `ledy`, `leda`, `ledc`, `ledmg`.

```sh
dwm -S ork
```

If a requested theme does not exist, dwm prints a warning and falls back to the `default` theme.

The selected theme also colors:

- normal, selected, and urgent borders;
- the status bar;
- vertical titlebars;
- titlebar close and `[o]` buttons;
- shaded titlebars;
- floating border colors.

The companion dmenu supports the same option:

```sh
dmenu -S ork
```

---

## Titlebar and mouse workflow

Each managed frame has a vertical titlebar on the left.

### Titlebar controls

- **`[X]`** — close the window.
- **`[o]`** — a single circular control for sizing:
  - LMB drag changes `mfact` when the window is the only master, otherwise `cfact`; for a floating window it resizes the window.
  - Mouse wheel over `[o]` changes `cfact`, or `mfact` for a single-master window.
  - Drag axes are layout-aware: `mfact` uses horizontal movement in `tile` and vertical movement in `bstack`; `cfact` uses the opposite axis.

### Dragging

- Drag the titlebar of a tiled window to reorder it on the tiling grid.
- Drag the titlebar of a floating window to move it freely.
- Dragging only re-evaluates a new target tile when the pointer actually enters it; it does not redraw the whole layout on every motion event.

### Shading

- Double-click a titlebar to shade/unshade a tiled window.
- In `tile`, a shaded window becomes a horizontal titlebar.
- In `bstack` and `grid`, it becomes a vertical titlebar.
- Shaded windows use the normal/inactive titlebar colors.
- Shading is supported in `tile`, `bstack`, and `grid`; switching to an unsupported layout unshades them cleanly.

### Fullscreen

Fullscreen clients cover the entire monitor. The side titlebar is not drawn and no titlebar strip is reserved.

---

## Common key bindings

The complete list is in `config.h`; these are the most important bindings.

| Binding | Action |
|---|---|
| `Mod+p` | dmenu |
| `Mod+Shift+Return` | terminal |
| `Mod+b` | toggle main bar |
| `Mod+Shift+b` | toggle extra bar |
| `Mod+j` / `Mod+k` | focus next / previous window |
| `Mod+Shift+j` / `Mod+Shift+k` | move window down / up the stack |
| `Mod+Return` | zoom / swap with previous master |
| `Mod+Shift+z` | zoom |
| `Mod+Space` | next layout |
| `Mod+t` / `Mod+f` / `Mod+m` | tile / floating / monocle |
| `Mod+h` / `Mod+l` | decrease / increase `mfact` |
| `Mod+i` / `Mod+d` | increase / decrease master count |
| `Mod+Shift+h` / `Mod+Shift+l` | increase / decrease `cfact` |
| `Mod+Shift+o` | reset `cfact` |
| `Mod+Shift+Space` | toggle floating |
| `Mod+Shift+c` | close focused window |
| `Mod+0` | view all tags |
| `Mod+1` … `Mod+9` | view tag |
| `Mod+Shift+1` … `Mod+Shift+9` | move window to tag |
| `Mod+Control+1` … `Mod+Control+9` | toggle tag in view |
| `Mod+Control+Shift+1` … `Mod+Control+Shift+9` | toggle window tag |

The default modifier is `Mod1` (Alt) unless changed in `config.h`.

---

## Layouts

Available layouts:

- `tile` — default master/stack layout
- floating
- `monocle`
- `bstack` — bottom stack
- `grid`
- `gaplessgrid`
- `tcl`
- `spiral` / `dwindle` — fibonacci variants

`cfact` geometry currently applies to `tile` and `bstack`.
Gaps (`gappx`) currently apply to `tile`, `bstack`, and `grid`.
Shading currently applies to `tile`, `bstack`, and `grid`.

---

## Status bars

dwm has a normal status bar and an optional extra status bar on the primary monitor (`extrabar` in `config.h`).

The root window name is split at the first semicolon:

```sh
xsetroot -name "main status;extra status text"
```

- Before `;` goes to the normal bar.
- After `;` goes to the extra bar.

With `topbar = 1`, the extra bar is placed at the bottom; with `topbar = 0`, it is placed at the top. The main and extra bars can be toggled independently.

---

## Per-tag state and per-window keyboard layout

Each tag remembers its own:

- layout and layout slot;
- master count;
- `mfact`;
- bar visibility.

Each window also remembers its XKB keyboard group. Switching focus restores the group that was active for that window, so different windows can use different keyboard layouts.

---

## Companion dmenu

`tools/dmenu-5.4-mod` is a modified dmenu build with:

- border width option `-bw`;
- matched-text highlighting;
- mouse click, hover, and wheel support;
- non-blocking stdin streaming;
- separator options `-d` and `-D`;
- horizontal input scrolling;
- the same `-S theme` support as dwm.

Build it with:

```sh
cd tools/dmenu-5.4-mod
make
```

Run examples:

```sh
dmenu -S ork -l 10
dmenu -S dracula -bw 2
```

---

## Configuration

Configuration is compile-time, in dwm tradition.

- `config.h` is the active configuration.
- `config.def.h` is a complete default configuration for DWORF; copy it to `config.h` for a fresh setup.
- `themes.h` contains the runtime theme palettes.
- `config.mk` controls build flags, install prefix, and Xinerama.
- After editing configuration, rebuild with `make clean && make`.

The authoritative documentation for behavior and internals is in `MODIFICATIONS.md`. The man page is `dwm.1`.

---

## Testing in Xephyr

The repository includes `xephyr.sh` as a development helper. A minimal manual setup is:

```sh
Xephyr :1 -screen 1280x1024 -ac &
DISPLAY=:1 ./dwm-6.6 -S ork &
DISPLAY=:1 xterm &
```

Useful checks:

```sh
DISPLAY=:1 xwininfo -root -tree
DISPLAY=:1 scrot /tmp/dwm.png
```

The companion dmenu and the reference st build can also be started on the same display with the same `-S` theme.

---

## Project files

- `dwm.c` — window manager implementation
- `config.h` — active build configuration
- `themes.h` — runtime color themes
- `lay.*.c` — additional layouts
- `push.c` — stack movement helpers
- `tilemovemouse.c` — tiled drag/reorder logic
- `nextprevlayout.c` — layout cycling
- `tools/dmenu-5.4-mod/` — companion dmenu
- `MODIFICATIONS.md` — detailed technical change log and architecture notes

---

## Credits

DWORF is built on **dwm 6.6** and is inspired by the suckless philosophy and the wider dwm patch ecosystem. Thanks to the authors of the patches and snippets that shaped its behavior, including work related to cfact, zoomswap, attachabove/attachtop, pertag, per-window keyboard layout, always-center, gaps, urgent borders, dual status bars, and mouse-assisted tiling.

DWORF adds its own reparenting frame layer, side titlebars, shading, geometry fixes, theming, and integration work on top of that foundation.

---

## License

MIT/X Consortium License. See `LICENSE`.

Upstream dwm is © its respective authors and the suckless community.
