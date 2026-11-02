#!/bin/sh

# Isolated test session for dwm-dworf + svkbd.
# Only processes started here are terminated on exit.

set -u

# Locale: the terminal must be UTF-8 for Cyrillic input.
export LANG=en_US.UTF-8
export LC_ALL=en_US.UTF-8

# Session configuration.
THEME=${THEME:-default}
SVKBD_LAYOUT=${SVKBD_LAYOUT:-en.full}
XKB_LAYOUT=${XKB_LAYOUT:-us,ru}
SCREEN=${SCREEN:-1280x1024}
DISPLAY_NUM=${DISPLAY_NUM:-:1}
DNUM=${DISPLAY_NUM#:}

# Project paths.
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SVKDIR="$ROOT/tools/svkbd-pad-1.0.pad"
DWM_BIN="$ROOT/dwm-6.6"
SVKBD_BIN="$SVKDIR/svkbd"

# Core X font directories and background image.
FONT_PATHS="/usr/share/fonts/TTF
/usr/share/dwm/fonts/iconic
/usr/share/dwm/fonts/mono
/usr/share/dwm/fonts/terminus
/usr/share/fonts/terminus
/usr/share/fonts/truetype/droid
/usr/share/fonts/truetype/ttf-dejavu"
BG_IMAGE=/usr/share/dwm/bg/bge.jpg

# Refuse to reuse a display owned by another session.
if [ -e "/tmp/.X${DNUM}-lock" ]; then
	echo "display $DISPLAY_NUM is busy; stop it first" >&2
	exit 1
fi

# Build the dock-enabled WM and the standalone svkbd.
echo "building dwm and svkbd..."
make -C "$ROOT" >/dev/null || { echo "dwm build failed" >&2; exit 1; }
make -C "$SVKDIR" >/dev/null || { echo "svkbd build failed" >&2; exit 1; }

ST=$(command -v st) || { echo "st not found" >&2; exit 1; }

# PIDs are filled as children are started; cleanup kills only these.
XEPHYR_PID=
DWM_PID=
SVKBD_PID=
ST_PID=
FEH_PID=
cleanup() {
	[ -n "$ST_PID" ] && kill "$ST_PID" 2>/dev/null
	[ -n "$SVKBD_PID" ] && kill "$SVKBD_PID" 2>/dev/null
	[ -n "$DWM_PID" ] && kill "$DWM_PID" 2>/dev/null
	[ -n "$FEH_PID" ] && kill "$FEH_PID" 2>/dev/null
	[ -n "$XEPHYR_PID" ] && kill "$XEPHYR_PID" 2>/dev/null
}
trap cleanup EXIT INT TERM

# Start the nested X server.
Xephyr "$DISPLAY_NUM" -screen "$SCREEN" -ac -noreset &
XEPHYR_PID=$!
sleep 1

# Session settings and keyboard layouts.
DISPLAY=$DISPLAY_NUM xset s 120 120 +dpms
DISPLAY=$DISPLAY_NUM setxkbmap -option grp:switch,grp:alt_shift_toggle,grp_led:scroll "$XKB_LAYOUT"

# Add core X font paths and rehash them for svkbd's XFontSet.
printf '%s\n' "$FONT_PATHS" | while IFS= read -r p
do
	[ -n "$p" ] || continue
	DISPLAY=$DISPLAY_NUM xset +fp "$p" 2>/dev/null || true
done
DISPLAY=$DISPLAY_NUM xset fp rehash

# Background (optional).
DISPLAY=$DISPLAY_NUM feh --bg-tile "$BG_IMAGE" >/dev/null 2>&1 &
FEH_PID=$!

# Window manager with the selected theme.
DISPLAY=$DISPLAY_NUM "$DWM_BIN" -S "$THEME" &
DWM_PID=$!
sleep 1

# Dock keyboard. The PID is printed so it can be closed independently.
DISPLAY=$DISPLAY_NUM "$SVKBD_BIN" -d -S "$THEME" -l "$SVKBD_LAYOUT" &
SVKBD_PID=$!
echo "svkbd PID: $SVKBD_PID"
echo "Click RU on the keyboard, then type in st."
echo "Stop svkbd with: kill $SVKBD_PID"
echo "Exit the st shell to stop the whole test."

# Terminal under the same UTF-8 locale. Exit it to stop the session.
DISPLAY=$DISPLAY_NUM LANG="$LANG" LC_ALL="$LC_ALL" "$ST" -S "$THEME" -R on &
ST_PID=$!
echo "st PID: $ST_PID"
wait "$ST_PID"
