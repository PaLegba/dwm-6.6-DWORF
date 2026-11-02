dwtoolset
=========

Vertical dock toolset and XEmbed systray host for dwm-dworf.

The panel is a normal `_NET_WM_WINDOW_TYPE_DOCK` client. Put it on a side edge
with `-g 56x900+0+0` (left) or `-g 56x900-0+0` (right); dwm-dworf reserves the
work area. Tool buttons are laid out top-to-bottom; systray icons are laid out
bottom-to-top in a separate subwindow. The systray area never grows into the
tool area; excess icons are clipped at the tool boundary.

The systray icon size is not fixed by the XEmbed standard; the tray host
chooses it and the client window is resized to that cell. `-s SIZE` sets the
systray icon size at startup (default 16; the tray subwindow is
`SIZE + 2 * icon_margin` wide). The default panel is deliberately narrow:
24 px wide, 4 px margins, and 16 px tool icons. Use `-s 24`/`-s 32` and
`-g WIDTHxHEIGHT+X+Y` only when a larger touch target is wanted.

Tool actions are either `A_SPAWN` (an argv) or `A_KEYS` (a keysym + modifier
mask sent with XTest). The latter follows the configured dwm bindings and does
not require any dwm code or private API. Defaults: terminal, zoom, stack
up/down, mfact dec/inc, svkbd. Add volume/brightness entries in `config.h` as
`A_SPAWN` tools with your commands.

Systray
-------

The panel owns `_NET_SYSTEM_TRAY_S0` and implements the legacy XEmbed protocol.
Modern applications that use StatusNotifierItem/DBus instead of XEmbed will not
appear here. `tools/traytest` is an XEmbed test client: run `dwtoolset` first,
then `traytest -n 10`; click a test icon to remove it.

Build
-----

```sh
make
./dwtoolset -i icons -g 24x900+0+0
```

Use `make ICONPATH="$PWD/icons"` for a local build, or `make install` to install
the binary and icons under `PREFIX`.
