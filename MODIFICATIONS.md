# dwm-6.6-DWORF — Project Modifications


Last updated: 2026-10-26

This document describes the current state of `dwm-6.6-DWORF`, which is a heavily modified fork of dwm 6.6. It is intended for humans who need to understand what changed, what was fixed, and where the current limitations are.

The project is not a clean upstream dwm tree. It is an experimental reparenting window manager with a side titlebar, shading, per-client sizing factors, gaps, and several applied patches.

---

## 1. What this project is

Base: **dwm 6.6** from suckless.

Main additions:

- Reparenting window manager: each managed client gets a frame window.
- Vertical titlebar on the left side of every frame.
- Titlebar buttons:
  - close `[X]`
  - cfact/mfact/flex control `[o]`
- Double-click shading with horizontal or vertical shaded titlebars.
- `cfact` (client size factor) support in some layouts.
- Window gaps (`gappx`).
- `attachtop` behavior for new clients.
- Always-center placement for floating windows, with monitor-bounds clamping.
- Per-tag layouts, `nmaster`, `mfact`, and bar visibility.
- Per-window keyboard layout/group memory (`pwkl`).
- Fullscreen windows cover the whole monitor without a residual titlebar.
- Optional extra status bar on the primary monitor (`dualstatus`).
- `zoomswap` behavior for `zoom`.
- Urgent border color and configurable titlebar button colors.
- Additional layouts from external snippets: `bstack`, `grid`, `gaplessgrid`, `tcl`, `fibonacci` (`spiral` and `dwindle`).
- Stack reordering helpers (`pushup`, `pushdown`) and drag-reorder helper (`tilemovemouse`).

Build target: `dwm-6.6`.

Authoritative source files:

- `dwm.c`
- `config.h`
- `themes.h`
- `lay.bstack.c`
- `lay.grid.c`

Secondary/reference files:

- `config.def.h` — complete clean default configuration; builds against the current source.
- `patch-to-add/` — reference patches, some applied, some not.
- `rotofont_*` — experiments and notes for rotated text.

---

## 2. Reparenting architecture

### 2.1 Frame/client split

The original dwm manages client windows directly. This fork manages a separate frame:

```c
struct Client {
    Window win;      /* frame window */
    Window client;   /* real application window */
    ...
};
```

Lifecycle:

1. A normal client window is discovered by `manage()`.
2. A frame window (`c->win`) is created as a child of root.
3. The real client (`c->client`) is reparented into the frame at `titlebar_width, 0`.
4. The frame is mapped and the client is mapped inside it.
5. On release, `unmanage()` reparents the client back to root, restores its old border and event mask, and destroys the frame.

`wintoclient()` matches both the frame and the real client, so events from either window can be resolved to the same `Client`.

Each frame selects `SubstructureRedirectMask` for its child. This is required for a reparenting WM: without it, a client's own `ConfigureRequest` would be applied directly to its window inside the frame, bypassing the WM. With redirect enabled:

- tiled clients cannot resize themselves out of their tile
- floating clients still get their requested geometry through `configurerequest()`
- `maprequest()` also handles an already-managed client being explicitly mapped again (unless it is shaded)

This specifically matters for video players such as mplayer/Xv that resize their own window after mapping.

### 2.2 Border ownership

While managed:

- The **frame** owns the visible border (`c->win`, border width `c->bw`).
- The **client** has border width `0`.
- The original client border width is stored in `c->oldbw` and restored on unmanage.

This is why layout code passes coordinates and sizes as frame geometry, and why `WIDTH(c)` / `HEIGHT(c)` include the frame border.

### 2.3 Geometry model

Current model:

- `c->x`, `c->y`, `c->w`, `c->h` describe the frame interior.
- The client content width is `c->w - titlebar_width`.
- The client is placed inside the frame at `x = titlebar_width`, `y = 0`.
- `resizeclient()` moves/resizes the frame, then moves/resizes the client.
- `configure()` sends the real client a `ConfigureNotify` with client geometry, not frame geometry.

`applysizehints()` treats size hints as client hints and temporarily subtracts `titlebar_width` from the requested frame width. This keeps min/max/increment calculations meaningful after the titlebar was introduced.

`resizetile()` is an additional helper used by supported tiled layouts. It calls the normal resize path, and if client size hints made the window smaller than the allocated tile, it forces the full tile size. This prevents cumulative gaps at the bottom/right of stack areas. Floating windows still use normal size hints.

### 2.4 WM properties

All client-related X properties are consistently read/written on `c->client`, not on the frame:

- `WM_STATE`
- `WM_HINTS`
- `WM_NORMAL_HINTS`
- `WM_TRANSIENT_FOR`
- `_NET_WM_STATE`
- `_NET_WM_WINDOW_TYPE`
- `_NET_CLIENT_LIST` entries

The frame is used only for drawing, borders, and event handling.

### 2.5 Save set

On manage:

- `XAddToSaveSet(dpy, c->client)`

On normal release:

- `XRemoveFromSaveSet(dpy, c->client)`

This protects client windows if the window manager terminates unexpectedly.

### 2.6 Startup and stale frames

Frames are marked with `_DWM_FRAME`.
Bars and the WM check window are marked with `_DWM_HELPER`.

At startup, `scan()` first calls `cleanupstale()`:

- Finds old frame windows left by a previous dwm instance.
- Finds their child client windows.
- Reparents those clients back to root at their current screen coordinates.
- Maps them again.
- Destroys stale frames.
- Destroys stale helper windows (old bars / WM check windows).

After cleanup, `scan()` runs an upstream-like scan:

- viewable/iconic clients
- skip override-redirect windows
- skip already-managed windows
- manage normal clients first, then transient clients

This makes restart after a crash (`kill -9`) recover the already-running clients.

### 2.7 Exit and signal handling

The original immediate signal cleanup approach was unsafe because it called Xlib from a signal handler and could hang. Current behavior:

- `SIGTERM` / `SIGINT` handler only sets `running = 0` and writes a byte to a self-pipe.
- The main loop uses `select()` on the X connection and the self-pipe.
- Cleanup happens in normal process context after the loop exits.

On cleanup, every client is unmanaged, reparented to root, mapped, and its save-set entry removed. Bars, frames, helper windows, cursors, colors, GCs, and the display connection are cleaned up.

`UnmapNotify` handling releases the frame for both real and synthetic unmap events once the real client is actually unmapped. This matters for modal/dialog windows that hide or withdraw themselves instead of being destroyed. On release, the client is mapped back to root only if it was viewable before release or is still shaded; withdrawn/hidden clients stay unmapped at root. This avoids orphan grey frames left behind after modal windows close.

---

## 3. Titlebar

### 3.1 Normal vertical titlebar

Each normal managed frame has a vertical titlebar on the left:

- width: at least `titlebar_width` from `config.h`
- during `setup()` the width is expanded to `MAX(titlebar_width, bh)` so it matches the configured font/bar height; this avoids shrinking the rotated font on high-DPI displays
- currently only left side is supported (`titlebar_side = 0`; right side is not implemented)
- title is drawn vertically with a rotated Xft font
- close button at the top
- `-` and `+` buttons at the bottom when the frame is tall enough

The rotated text uses Fontconfig/Xft matrix rotation. Important detail: for a rotated font, `XGlyphInfo.ext.height` is the text length along the titlebar, while `ext.width` is its thickness. The drawing code uses the rotated metrics and clipping rectangle to keep text inside the titlebar and away from buttons.

The runtime titlebar width is expanded to at least `bh`, so the configured font fits without shrinking. `rotatedfont()` additionally measures the actual rotated text with `XftTextExtentsUtf8()` and, if its ink thickness is still wider than the titlebar, reduces the pixel size only as much as needed (with a small margin). This prevents uppercase letters, punctuation, and Cyrillic glyphs from being clipped on high-DPI setups while keeping the font as large as possible. Both normal vertical titlebars and vertical shaded titlebars use this measured font.

Horizontal shaded titlebars use `scaledfont()` with the same idea. The shaded bar interior is kept at `bh`, so the normal titlebar font and horizontal shaded font use the same base size; the close button also scales to the bar height. the normal font is capped to the shaded bar height, and the baseline is computed from the actual `XftTextExtentsUtf8()` ink box rather than font ascent/descent. This keeps letters such as `J` and other ascenders/descenders fully visible even when the base font is large for the monitor DPI.

Title text is redrawn when:

- the window is focused/unfocused
- the frame is exposed
- the frame is resized
- the title changes

For fullscreen clients:

- the titlebar is not drawn at all
- the real client is placed at `(0, 0)` inside the frame and uses the full frame width/height
- `configure()` reports full-frame geometry instead of subtracting `titlebar_width`
- entering/exiting fullscreen restores the previous windowed geometry normally

`_NET_WM_STATE` property changes are handled, not only EWMH `ClientMessage` notifications. This is required for clients such as mpv that set the fullscreen state directly on their window.

### 3.2 Titlebar buttons

Top button:

- close `[X]`
- hit area: top-left square of the titlebar
- action: kill the real client (`XKillClient`) and unmanage it

Bottom button (only when not shaded and the frame is tall enough; its square uses the runtime titlebar width):

- `[o]`: a circle icon.
- LMB drag: changes `mfact` when the window is the only master; otherwise changes `cfact`; for a floating window (or floating layout) it resizes the window.
- Mouse wheel over `[o]`: changes `cfact` for that window. If the window is the only master, it changes `mfact` instead. With multiple masters, scrolling over an active master changes that master's `cfact`.
- Drag axes are layout-aware: `mfact` uses horizontal movement in `tile` and vertical movement in `bstack`; `cfact` uses the opposite axis.
- Sensitivity is configurable: `cfact_drag_step`, `mfact_drag_step`, `cfact_scroll_step`, `mfact_scroll_step`.

Button colors are configurable in `config.h` / `config.def.h`:

- `titlebarclosecolor`
- `titlebarpluscolor` (the `[o]` control)
- `titlebarbtnfgcolor`

### 3.3 Double-click shading

Double-clicking the titlebar toggles shading:

- detection is manual: same button, same frame, within 250 ms, within 6 px
- double-clicks on the close and `[o]` controls are excluded
- only tiled windows in supported layouts can be shaded

When shaded:

- the real client is unmapped
- the frame remains visible and becomes a grey titlebar
- the pre-shade frame geometry is saved in `shx/shy/shw/shh`
- `unmapnotify()` must ignore this internal unmap, otherwise the window would be unmanaged immediately
- focus handling skips shaded clients; if a selected window is shaded, focus moves to another non-shaded window or to root

Double-clicking a shaded titlebar unshades it, maps the client again, restores the saved pre-shade geometry, and then lets the current layout re-arrange it.

If a shaded window is moved to an unsupported layout or toggled into floating state, `unshade()` now restores the saved geometry instead of leaving the window at shaded-titlebar size. Shading itself remains a tiling-layout feature; floating windows are unshaded rather than kept as floating shaded bars.

### 3.4 Shaded geometry

Shading direction depends on layout:

- `tile`: horizontal grey titlebar (`shadevertical = 0`)
- `bstack`: vertical grey titlebar (`shadevertical = 1`)
- `grid`: vertical grey titlebar (`shadevertical = 1`)

Horizontal shaded bar:

- fixed height `bh` (bar height)
- spans the assigned column width
- its layout cell reserves the frame border plus `2 * gappx` extra vertical space, so the bar interior is `bh` and normal `gappx` inset can be applied while keeping the bar height stable
- the close button scales to the full bar interior height and its square is used as the hit box

Vertical shaded bar:

- fixed width `titlebar_width`
- spans the assigned row/area height
- its layout cell reserves `2 * gappx` extra horizontal space, so normal `gappx` inset can be applied on all sides while the bar itself keeps width `titlebar_width`

All other windows in the same stack/row/column reflow around shaded bars.

Master promotion:

- If a master window is shaded in `tile` or `bstack`, the first non-shaded stack window is promoted into the freed master area.
- In `grid` there is no master concept, so promotion does not apply.

Layouts that do not support shading (`monocle`, floating, `gaplessgrid`, `tcl`, `fibonacci`) automatically unshade windows when switched to.

---

## 4. Layouts and layout features

Included layouts:

- `tile` — default
- floating (`NULL` arrange function)
- `monocle`
- `bstack` — bottom stack
- `grid`
- `gaplessgrid`
- `tcl`
- `spiral` / `dwindle` (fibonacci variants)

Feature support matrix:

| Feature | tile | bstack | grid | other layouts |
|---|---|---|---|---|
| vertical titlebar | yes | yes | yes | yes |
| close/[o] controls | yes | yes | yes | yes |
| double-click shading | yes (horizontal) | yes (vertical) | yes (vertical) | no, auto-unshade |
| `cfact` geometry | yes | yes | no visual effect | no |
| gaps (`gappx`) | yes | yes | yes | no |
| master promotion when shaded | yes | yes | n/a | n/a |

### 4.1 `cfact`

`cfact` is a per-client size factor (default `1.0`, range `0.25`–`4.0`).

Supported in:

- `tile`: master/stack heights are distributed proportionally to `cfact`
- `bstack`: master/stack widths are distributed proportionally to `cfact`

Key bindings in `config.h`:

- `Mod+Shift+h` — increase `cfact` by `0.25`
- `Mod+Shift+l` — decrease `cfact` by `0.25`
- `Mod+Shift+o` — reset `cfact` to `1.0`

The `[o]` titlebar control also changes `cfact` by dragging or by scrolling the mouse wheel over it. See section 3.2.

### 4.2 `attachtop`

New clients are attached on top of the stack, after the last master window, instead of becoming the new master or following the selected window.

- The insertion point is after the last non-floating client on the same tag that belongs to the master area.
- If no such client exists, the client is attached at the head.
- Floating clients and clients on unrelated tags are skipped while counting masters.
- Used by `manage()`, `sendmon()`, and monitor-removal relocation in `updategeom()`.
- This replaces the earlier `attachabove` behavior.

### 4.3 `zoomswap`

`zoom` swaps the selected window with the previous master instead of only pushing the old master down.

- `prevzoom` stores the window that was previously swapped out.
- Pressing zoom again on the new master swaps back.
- `prevzoom` is cleared if the client is unmanaged or no longer on the current monitor.

### 4.4 Gaps (`gappx`)

Configured by `gappx` in `config.h`.

Implemented in `resizetile()`, so it applies to layouts that use this helper:

- `tile`
- `bstack`
- `grid`

Gap behavior:

- all tiled windows, including shaded titlebars, get `gappx` on all sides
- shaded bar cells reserve extra space along the collapsed axis (`2 * gappx`) so the bar keeps its configured height/width while still receiving top/bottom or left/right gaps
- layout position increments use allocated cell sizes, so gaps do not accumulate into missing bottom/right space

Other layouts and floating windows are unchanged.

### 4.5 Floating placement and bounds

- New floating windows are centered in the monitor's usable window area.
- Floating windows are clamped to the monitor/window-area bounds both at manage time and on later resize requests.
- This prevents clients such as GIMP from creating initial windows larger than the assigned screen.
- Centering is re-applied after the layout/size-hint pass so that size-hint rounding does not leave the window off-center.

### 4.6 Stack reordering / drag

- `pushup` / `pushdown` move clients in the stack order.
- `tilemovemouse` is invoked from the titlebar drag area through `titlebarclick()`.
- In a tiling layout, with a tiled (non-floating) client, dragging the titlebar reorders clients by dragging over another window.
- Reordering now happens only when the pointer enters a different target tile; the stack is not recalculated or redrawn on every motion event.
- The dragged client is moved directly to the target position instead of being pushed one slot per motion event.
- After a reorder, the target is recomputed from the new layout. This absorbs size changes caused by different `cfact` values and prevents immediate back-and-forth reorders.
- If the client is floating (`c->isfloating`) or the current layout is floating (`arrange == NULL`), the same titlebar drag is routed to `movemouse()` and moves the window freely like normal dwm floating movement.
- This means titlebar LMB drag works in both floating layout and floating state without requiring the Mod key.

### 4.7 Per-tag monitor state

Each monitor keeps a `Pertag` structure with per-tag copies of:

- `nmaster`
- `mfact`
- selected layout slot (`sellt`) and both layout pointers
- bar visibility (`showbar`)

`view()` and `toggleview()` select the active tag index and call `applytag()` to restore that tag's state before arranging. `setlayout()`, `incnmaster()`, `setmfact()`, `togglebar()`, `cfactmouse()`, and `cfactscroll()` write changes back to the active tag entry. Viewing all tags uses index `0`; regular tags use `1..LENGTH(tags)`.

Client-specific state such as `cfact` remains attached to the client, not to the tag.

### 4.8 Per-window keyboard layout

Each client remembers its XKB keyboard group in `kbdgrp`.

- `manage()` stores the currently active group for a new client.
- `unfocus()` stores the group that was active while the client was focused.
- `focus()` restores the client's stored group with `XkbLockGroup()`.

This gives per-window keyboard layout switching without additional client cooperation.

The `pwkl` mouse/tag part is already covered by the existing `CLEANMASK`: it keeps only real modifier bits, so buttons and modifier state do not block tag-bar mouse actions while alternate layouts are active.

---

## 5. Colors and visual state

Color schemes:

- `SchemeNorm` — normal/unfocused
- `SchemeSel` — selected/focused
- `SchemeUrg` — urgent

`drawborder()` chooses the frame border color with this priority:

1. urgent (`SchemeUrg`)
2. focused/selected (`SchemeSel`)
3. normal (`SchemeNorm`)

Urgent handling:

- `WM_HINTS` urgency is handled on the real client.
- `seturgent()` updates both WM hints and the frame border.
- `updatewmhints()` updates the border after reading urgency.
- Focusing an urgent window clears urgency and restores the selected border.
- An urgent titlebar uses the `SchemeUrg` background/foreground as well as the urgent border.

Shaded titlebars use the normal (inactive) titlebar colors:

- background: `SchemeNorm` background
- text: `SchemeNorm` foreground
- selected shaded windows keep their selected border behavior; only the bar background/text follow the normal titlebar scheme.

For windows on inactive monitors:

- the titlebar is drawn as selected only when the client is on the globally selected monitor (`c == selmon->sel`);
- title changes on a background monitor no longer repaint that titlebar as if it were active;
- normal background title updates stay normal-colored. Apps that want attention should set `XUrgencyHint`; that is the conventional signal and is already honored by the urgent scheme.

Urgent border color is configured by `urgbordercolor`.

Titlebar button colors are configured by the four `titlebar*color` variables listed above.

Known unused color variables:

- `floatnormbordercolor`
- `floatselbordercolor`

They exist in `config.h` but are not currently used by `drawborder()`.

Shaded titlebars use the normal titlebar colors from `SchemeNorm`; the selected border behavior is unchanged.

### 5.1 Runtime themes (`-S`)

`themes.h` contains color themes derived from the st color schemes.

Launch syntax:

```sh
dwm -S ork
```

If `-S` is not given, the normal colors from `config.h` are used unchanged.
If the requested theme name is unknown, dwm prints a warning, switches to the
`default` theme, and continues starting normally.

Available theme names:

- `default`
- `dracula`
- `solarized`
- `nord`
- `gruvbox`
- `plan9`
- `cyberpunk`
- `light`
- `ork`
- `ledg`, `ledb`, `ledr`, `ledw`, `ledy`, `leda`, `ledc`, `ledmg`

Theme mapping used by `applytheme()`:

- normal fg/bg: theme `defaultfg` / `defaultbg`
- normal border: theme bright black (`color[8]`)
- selected fg/bg/border: theme background / cursor color / cursor color
- urgent border: theme red (`color[1]`)
- close button: theme red (`color[1]`)
- `[o]` button: theme green (`color[2]`)
- button glyphs: theme bright white (`color[15]`)
- floating border colors: theme yellow / bright yellow (`color[3]` / `color[11]`)

The selected theme name is remembered in the global `themename` and is appended as
`-S theme` when dwm spawns the configured terminal (`termcmd`) or `dmenu`
command (`dmenucmd`). This happens inside `spawn()`: the configured command
array is copied, the extra `-S theme` arguments are added, and that copy is
executed. This keeps `termcmd` editable, so extra st options can be added
normally and the theme flag is appended after them. It keeps st and dmenu
visually consistent with the running dwm theme.

The theme system only overrides colors, not fonts, gaps, titlebar width,
layouts, or keybindings.

### 5.2 Extra status bar (`dualstatus`)

`extrabar` in `config.h` enables a second status bar on the primary monitor.
The root window name is split at the first `;`:

- text before `;` is shown on the normal status bar;
- text after `;` is shown on the extra status bar.

The extra bar is positioned opposite the main bar (bottom when `topbar = 1`,
top when `topbar = 0`) and reduces the usable window area accordingly. It can
be toggled with `Mod+Shift+b`. The extra bar is only shown on `mons`; other
monitors keep their normal bar. The extra bar window is marked `_DWM_HELPER`
so stale copies are cleaned on restart.

---

## 6. Important fixes and repairs

These are the main bugs that were fixed compared with the earlier in-work state.

### Reparenting / property bugs

- `applyrules()` used the frame before it existed; it now reads `WM_CLASS` from the real client.
- `WM_STATE`, `WM_HINTS`, `WM_NORMAL_HINTS`, `WM_TRANSIENT_FOR`, `_NET_WM_STATE`, `_NET_CLIENT_LIST`, and window-type properties now use the real client.
- `killclient()` fallback killed the frame; it now kills the real client.
- `grabbuttons()` grabbed buttons on the frame; it now grabs on the real client.
- `focusin()` compared against the frame; it now compares against the client.
- Client event selection and release restore the original client event mask.

### Startup / exit

- Added save-set handling.
- Added stale-frame/bar cleanup on startup using `_DWM_FRAME` / `_DWM_HELPER`.
- Fixed signal handling hang by moving cleanup out of the signal handler into the normal event loop.
- Cleanup now unmanages clients correctly and reparents them to root.
- Fixed orphan frames after modal/dialog withdrawal: synthetic UnmapNotify now unmanages too, and hidden clients are not remapped on release.
- Added cleanup of the frame GC and titlebar button colors.

### Rendering / geometry

- Fixed client self-resize bypass: frame windows now select `SubstructureRedirectMask`, so `ConfigureRequest` from a reparented client is handled by dwm instead of being applied directly inside the frame. This fixes clients like mplayer/Xv that resize themselves after mapping.
- `maprequest()` now maps already-managed clients again when appropriate, while respecting shaded state.
- Fixed fullscreen geometry: fullscreen clients no longer reserve a titlebar strip and now cover the full monitor; titlebar drawing is skipped while fullscreen.
- Added `_NET_WM_STATE` property handling so clients that set fullscreen directly (e.g. mpv) are processed correctly.
- Fixed inactive-monitor titlebar repainting: only the globally selected client is drawn with the selected titlebar scheme.
- Fixed `expose()` so frame exposures redraw the titlebar instead of always redrawing the bar.
- Removed client-owned borders while managed; the frame owns the border.
- Fixed `configure()` and `resizeclient()` geometry for the titlebar.
- Added `resizetile()` so stack windows fill the full allocated cell even when client size increments would round them down.
- Fixed rotated titlebar text placement:
  - use `ext.height` as vertical text length
  - use rotated `ext.width` / `ext.x` for thickness and horizontal centering
  - clip text to the available titlebar area
  - avoid placing text over buttons
- Fixed an OpenBSD heap-corruption crash in `rotatedfont()` / `scaledfont()`:
  `XftFontOpenPattern()` takes ownership of the `FcPattern`, so destroying the
  pattern afterwards left the font with a dangling pointer. The later
  `XftFontClose()` / `XftDrawDestroy()` aborted with `write to free mem` as soon
  as the first client was managed. The pattern is now left to Xft.

### Titlebar interactions

- Implemented close and `[o]` hit testing.
- Replaced the `+`/`-` cfact buttons with a single circular `[o]` control supporting drag and wheel input.
- Added double-click shading and restore.
- Fixed the internal shading unmap being treated as a client withdrawal.
- Shaded clients no longer receive input focus directly; focus falls back to root or another visible window.

### Layouts

- Integrated `cfact` into `tile` and `bstack`.
- Added shading-aware geometry to `tile`, `bstack`, and `grid`.
- Added master promotion when a master is shaded in `tile`/`bstack`.
- Added gaps to the supported layouts without breaking bottom/right alignment.
- Added `attachtop` (replacing `attachabove`) and `zoomswap`.
- Added always-center floating placement and monitor-bound clamping for floating clients.
- Added per-tag layouts, master count, `mfact`, and bar visibility.

### Urgent border

- Added `SchemeUrg`.
- Added `drawborder()`.
- Urgent border is applied/cleared through `seturgent()`, `updatewmhints()`, and focus changes.

---

## 7. Applied external patches

Applied/adapted:

- `dwm-6.1-cfacts.diff` — integrated as `cfact` in `tile`/`bstack`, plus keys and titlebar buttons.
- `dwm-6.0-zoomswap.diff` — integrated into `zoom()`.
- `dwm-10e232f9ace7-attachabove.diff` — superseded by `attachtop` behavior.
- `dwm-alwayscenter-20200625-f04cac6.diff` — floating windows centered; extended with monitor-bounds clamping.
- `new/dwm-attachtop-6.2.diff` — integrated as `attachtop()`.
- `dwm-pertag-20200914-61bb8b2.diff` — integrated as the `Pertag` per-tag monitor state.
- `dwm-pwkl-6.2-part1.diff` — integrated as per-client `kbdgrp` save/restore.
- `dwm-pwkl-6.2-part2-[...].diff` — its modifier-only `CLEANMASK` behavior is already present in the current macro.
- `dwm-5.9-uselessgap.diff` — adapted as `gappx` + `resizetile()` for supported layouts.
- `dwm-5.7.2-urgentborder.diff` — adapted as `SchemeUrg` + `drawborder()`.
- `dwm-6.0-dualstatus(1).diff` — integrated as the optional primary-monitor extra status bar.

---

## 8. Known limitations and TODO

1. Shading is supported only in `tile`, `bstack`, and `grid`.
2. `cfact` has no visual effect in `grid` and other layouts.
3. Gaps are applied only in `tile`, `bstack`, and `grid`.
4. `titlebar_side` is present but only left-side titlebars are implemented.
5. `titlebar_button_width` exists in config but is not currently used by the renderer.
6. Floating border colors are defined but unused.
7. `quit` is unused because its keybinding is commented out in `config.h`; this produces a compiler warning.
8. Stale-frame recovery only recognizes frames created by builds that set `_DWM_FRAME`.
9. Legacy commented-out implementations and debug helpers (`scan_TEST`, commented `resizeclient`, etc.) remain in the source.
10. `focusmaster.c` exists but is not included by the current config.

Possible next steps:

- Extend shading to `gaplessgrid`, `tcl`, `fibonacci`.
- Extend `cfact` and gaps to remaining layouts.
- Use or remove `titlebar_button_width`.
- Use floating border colors.
- Remove or intentionally re-enable the `quit` binding.
- Clean up unused debug/legacy code.
- Test with more client toolkits and multi-monitor setups.

---

## 9. Build and test

Build:

```sh
cd dwm-6.6-DWORF
make
```

On OpenBSD, set the OpenBSD FreeType and man paths in `config.mk`. Install with:

```sh
make clean
make install
startx
```

The initial clean removes host-built dmenu objects that are tracked in the
repository; otherwise `make install` could reuse the stale Linux `dmenu`
binary.

`make install` installs `/usr/local/bin/dwm` and a `dwm-6.6` symlink. It also
builds and installs the bundled dmenu (`dmenu`, `dmenu_path`, `dmenu_run`,
`stest`) only when `WITH_DMENU = yes` is set in `config.mk`; override with
`make install WITH_DMENU=no`. Use `make install-dmenu` to install only dmenu,
and `make dmenu` to build it without installing. The top-level target passes
`PREFIX`, `MANPREFIX`, `DESTDIR`, and `FREETYPEINC` to the dmenu build, so the
companion project does not need its own OpenBSD `config.mk` edits.

The build system is portable between GNU make and BSD make (bmake). It uses
GNU-style `include` (accepted by bmake for compatibility), `${SRC:.c=.o}`
(AT&T `:old=new`), the `.c.o` suffix rule, and `.PHONY`; avoid `ifeq`/`.if` and
other make-specific extensions. Both the dwm and dmenu Makefiles were verified
with GNU make and OpenBSD make.

`make dist` produces `dwm-6.6-DWORF.tar.gz` (`DISTNAME`) from the source tree,
excluding objects and binaries. It does not run `clean` first.

Run in Xephyr:

```sh
Xephyr :1 -screen 1280x1024 -ac &
DISPLAY=:1 xterm &
DISPLAY=:1 ./dwm-6.6 &
```

The included `xephyr.sh` is a rough helper for this purpose.

On the host, always use a dedicated `Xephyr` display (never the live `:0`) and
terminate only the PIDs you started, including that `Xephyr`. Do not use
`pkill`/`killall` and do not touch processes or sessions started by the
operator.

Useful checks:

```sh
DISPLAY=:1 xwininfo -root -tree
DISPLAY=:1 scrot /tmp/screenshot.png
```

Smoke-test checklist:

- start with no clients
- start two or three xterms
- verify master/stack alignment reaches the bottom/right edges
- verify gaps
- verify tile drag reorder reacts once per target tile and does not flicker/jump with different `cfact` values
- verify floating windows open centered and clamped inside the monitor
- verify new clients land at the top of the stack, after masters
- switch tags and verify layout/nmaster/mfact/showbar are restored per tag
- set two keyboard layouts, focus different windows, verify each keeps its own keyboard group
- set root name `main;extra`, verify two status bars; toggle extra bar with Mod+Shift+b
- click/verify close button
- click/verify close and `[o]` controls
- drag `[o]` to change mfact/cfact; wheel over `[o]`; verify floating drag resize
- double-click a stack titlebar to shade; double-click again to restore
- shade a master in `tile` and `bstack`; verify promotion
- switch layouts with `Mod+Space`
- verify urgent border and urgent titlebar by setting `XUrgencyHint`
- verify fullscreen client covers the whole monitor with no titlebar strip and returns to windowed geometry
- on multiple monitors, update a background window title and verify its titlebar stays normal-colored
- terminate dwm with `SIGTERM`; verify clients are released to root
- restart after `kill -9`; verify stale frames are cleaned and clients are adopted

---

## 10. Bundled dmenu (`tools/dmenu-5.4-mod`)

The repository also contains a modified dmenu 5.4 in `tools/dmenu-5.4-mod`.

Integrated patches:

- window border (`-bw`) with selected-background border color;
- matched-text highlighting with separate color schemes;
- mouse support: click items, click arrows, middle-click paste, right-click exit;
- mouse motion hover selection;
- mouse-wheel list scrolling;
- non-blocking stdin streaming with control characters:
  - `\f` clears items before the next line
  - `\a` selects the next item
  - `\b` makes the next item current
- horizontal input-field scrolling around the cursor (`drw_text_align`);
- field separators: `-d` (first occurrence) and `-D` (last occurrence), with `|` suffix reversing display/output order.

Runtime themes are also supported:

```sh
dmenu -S ork
```

The dmenu copy has its own `themes.h`, synced with the top-level dwm `themes.h`. `-S` applies before setup and before creating color schemes. Normal/selected/out/highlight schemes are all derived from the selected theme; explicit `-nb`/`-nf`/`-sb`/`-sf` options still override the theme if they appear after `-S`.

Build:

```sh
cd tools/dmenu-5.4-mod
make
```
