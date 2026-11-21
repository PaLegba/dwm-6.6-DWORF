# dwslit

A small classic `*box`-style Slit for `dwm-dworf-dock`.

`dwslit` is a `_NET_WM_WINDOW_TYPE_DOCK` client, so dwm reserves a screen edge
and keeps it out of tiling and focus. It hosts classic dockapps:

- windows with `WM_HINTS.initial_state == WithdrawnState`;
- or windows whose `WM_CLASS` class is `DockApp`.

Dockapps are reparented into the slit and laid out in a vertical strip or a
horizontal row, depending on the slit geometry. The icon window from
`WM_HINTS` is used when a dockapp provides one.

`dwslit` relies on a small dwm cooperation hook: dwm-dworf-dock leaves
classic dockapps unmanaged and maps them; `dwslit` adopts them from the root
window. Run `dwslit` before launching dockapps, or start it later to pick up
already existing root windows. If `dwslit` is not running, dockapps remain
unmanaged top-level windows.

## Build

```sh
make
./dwslit                    # right edge of the monitor under the pointer
./dwslit -g 600x64+0+0      # horizontal top row
```

The bundled example wmapps are in `wmapps/`:

```sh
make wmapps
./wmapps/bin/wmtime
```

`-g` uses the usual `WxH+X+Y` geometry syntax. Without `-g`, dwslit places
itself vertically on the right side of the monitor under the pointer (Xinerama
aware). The default width is `64`; dwm stretches it to the work-area height.

Build requirement: libXinerama, in addition to libX11.

## Files

- `dwslit.c` — implementation;
- `config.h` / `config.def.h` — colors, margins, default geometry;
- `config.mk` — compiler and install paths.

The design is inspired by the classic Slit from Blackbox/Openbox, but this is
an independent implementation.
