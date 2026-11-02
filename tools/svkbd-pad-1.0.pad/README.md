SVKBD
=====
This is a simple virtual keyboard, intended to be used in environments,
where no keyboard is available.

Installation
------------

	% make
	% make install

This builds a single `svkbd` binary with all configured layouts.

Usage
-----

	% svkbd -L
	% svkbd -d -l en.full
	% svkbd -d -f -l cursor.v.side -g 60x780+0+0
	% svkbd -S default -l en.full

`-L` lists the available layouts. `-l NAME` selects the initial layout.
`-S NAME` selects a theme from `themes.h`. `-d` announces the window as a
dock window; with dwm-dworf it is managed without a frame and reserves a
screen edge. `-f` marks the instance as fixed: its visual layout does not
follow XKB focus changes, it does not call `XkbLockGroup()` on startup or on
key presses, and no automatic layout key is inserted. Use `-f` for secondary
keyboards (for example, a cursor pad in a side dock) so switching the focused
window's layout does not replace their layout.

Themes
------

`themes.h` uses the same theme names and 16-color palettes as dwm and dmenu.
The `default` theme is the original svkbd-pad palette; the old ANSI palette is
available as `ansi`. Other themes use a fixed mapping from the 16-color palette
to the keyboard roles. Without `-S`, the colors in `config.h` are used.

Layouts and XKB groups
----------------------

`layouts.h` contains the layout table. Each entry has a name, a short label,
an XKB group (0 = en, 1 = ru, and so on), and a variant (`""`, `fnx`, `full`,
...). The included layouts are:

	en, en.fnx, en.full, ru, ru.fnx, ru.full, cursor.v.side

The short label is the filled circle `●` (U+25CF), which is available in the
default DejaVu Sans Mono `monospace` font on the OpenBSD target. It is used on
the layout key instead of a language name so the same key will work when more
groups (de, fr, cz, ...) are added.

When XKB has at least two groups, svkbd inserts an on-screen layout key after
the space key. Pressing it advances to the next XKB group (not only 0 <-> 1),
selects the matching layout for that group/variant, and calls
`XkbLockGroup()` for the focused window. Future layouts only need a `Layout`
entry with the right group and variant; `switch_locale()` cycles through all
`xkb_num_groups`.

svkbd also selects `XkbStateNotify` and follows the server's effective XKB
group. On focus changes dwm restores each client's remembered group, so a
normal svkbd instance automatically shows the layout for the newly focused
window. Per-window group storage remains the window manager's responsibility.

With `-f`, an instance ignores `XkbStateNotify` for layout selection, skips
`XkbLockGroup()` on startup and on key presses, and does not insert the
automatic layout key. A layout may still contain an explicit key with
`XK_ISO_Next_Group`; on a fixed instance that key is sent to the focused
window, so it changes the focused window's group while the fixed keyboard
stays in its own layout. This is the intended mode for secondary side-dock
keyboards.

With only one XKB group, or if XKB is unavailable, no automatic layout key is
inserted and the selected layout stays static. Setting `layoutbutton = False`
in `config.h` disables the automatic key for all instances.

`layout.h` is a legacy generated single-layout file and is not used by the
current build.

Fonts and locale
----------------

Labels are UTF-8 strings drawn with Xft (`XftDrawStringUtf8`); the default
`font[]` is the Fontconfig family `monospace:size=14`. There is no XFontSet or
core-font path, so the old `missing fontset` warnings no longer apply.
`monospace` on the OpenBSD target resolves to DejaVu Sans Mono, which covers
`⌫` BackSpace (U+232B), the arrow glyphs `←↑→↓` (U+2190-2193), `⌧` close
(U+2327, X in a rectangle box), `⇄` (U+21C4), and the filled circle `●`
(U+25CF). It does not cover the earth-globe `🌐` (U+1F310); use `●`/`⇄` or
install a font containing U+1F310.

svkbd still forces a UTF-8 locale (`C.UTF-8`/`en_US.UTF-8` fallback) for key
handling. The receiving terminal must also run under a UTF-8 locale. Under
`LANG=C`, `XLookupString` returns 0 bytes for Cyrillic keys and nothing is
inserted even though svkbd sends the correct `Cyrillic_o` keysym. Launch the
terminal (or the whole X session) with `LANG`/`LC_CTYPE` set to a UTF-8
locale.

Unmapped keysyms are ignored instead of sending keycode 0. Before sending a
key, svkbd also makes sure the server's XKB group matches the displayed
layout.

Geometry
--------

	% svkbd -g 400x200+1+1

Starts svkbd with a size of 400x200 at the upper left corner.

A window taller than it is wide is treated by dwm-dworf as a vertical side
dock. Use `+0` for the left edge and `-0` for the right edge:

	% svkbd -d -f -l cursor.v.side -g 60x780+0+0   # left side
	% svkbd -d -f -l cursor.v.side -g 60x780-0+0   # right side

dwm-dworf reserves the corresponding side of the work area; the keyboard can
be combined with the normal bottom keyboard. The `-f` option keeps the side
keyboard's layout fixed while focus/layout changes affect the main keyboard.

Repository
----------

	git clone http://git.suckless.org/svkbd
