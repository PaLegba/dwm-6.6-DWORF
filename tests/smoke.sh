#!/bin/sh
# DWORF layout/modal smoke test. Needs Xephyr, xterm, cc, libXtst.
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
DWM=${DWM:-"$ROOT/dwm-6.6"}
TMP=$(mktemp -d /tmp/dwm-test.XXXXXX)
PIDS=
DWM_PID=

cleanup() {
	for p in $PIDS; do
		kill "$p" 2>/dev/null || true
	done
	wait 2>/dev/null || true
	rm -rf "$TMP"
}
trap cleanup EXIT INT TERM

fail() {
	echo "FAIL: $*"
	[ -f "$TMP/dwm.log" ] && tail -20 "$TMP/dwm.log"
	exit 1
}

for c in Xephyr xterm cc xwininfo; do
	command -v "$c" >/dev/null 2>&1 || {
		echo "skip: $c not installed"
		exit 0
	}
done
pkg-config --exists xtst || {
	echo "skip: libXtst not installed"
	exit 0
}

[ -x "$DWM" ] || fail "$DWM not built"

cc -O0 -Wall -o "$TMP/xdo" "$ROOT/tests/xdo.c" -lX11 -lXtst || fail "xdo build"
cc -O0 -Wall -o "$TMP/dialog" "$ROOT/tests/dialog.c" -lX11 || fail "dialog build"

N=31
while [ -e "/tmp/.X11-unix/X$N" ]; do
	N=$((N + 1))
done
DISP=":$N"
Xephyr "$DISP" -screen 1024x768 -ac >"$TMP/xephyr.log" 2>&1 &
PIDS="$PIDS $!"
i=0
until DISPLAY=$DISP xwininfo -root >/dev/null 2>&1; do
	i=$((i + 1))
	[ "$i" -gt 100 ] && fail "Xephyr did not start"
	sleep 0.1
done

DISPLAY=$DISP "$DWM" -S default >"$TMP/dwm.log" 2>&1 &
DWM_PID=$!
PIDS="$PIDS $DWM_PID"
sleep 0.5

DISPLAY=$DISP xterm -T A >/dev/null 2>&1 &
PIDS="$PIDS $!"
sleep 0.4
DISPLAY=$DISP xterm -T B >/dev/null 2>&1 &
PIDS="$PIDS $!"
sleep 0.7

export DISPLAY=$DISP
X="$TMP/xdo"
GX= GY= GW= GH=
geo() {
	set -- $("$X" geom "$1" | cut -d' ' -f2-5)
	GX=$1 GY=$2 GW=$3 GH=$4
}

# dock branch only: work area reservation and reclaim
if [ -x "$ROOT/tools/dwtoolset/dwtoolset" ]; then
	"$ROOT/tools/dwtoolset/dwtoolset" -g 24x300+0+0 \
		>"$TMP/dock.log" 2>&1 &
	DOCK_PID=$!
	PIDS="$PIDS $DOCK_PID"
	sleep 0.5
	geo A
	[ "$GX" -ge 20 ] || fail "dock did not reserve the left work-area edge"
	kill "$DOCK_PID" 2>/dev/null || true
	sleep 0.5
	geo A
	[ "$GX" -lt 20 ] || fail "dock work area was not reclaimed"
fi

sleepkey() { "$X" key "$1" "$2"; sleep 0.2; }

sleepkey alt t

# tile: single master shaded docks left, stack takes the rest
geo A
AW=$GW
"$X" shade A; sleep 0.2
geo A
[ "$GW" -le 24 ] || fail "tile master shade is not a side bar"
[ "$GX" -le 4 ] || fail "tile master bar is not on the left"
geo B
[ "$GW" -gt 800 ] || fail "tile stack did not fill the freed master width"
"$X" shade A; sleep 0.2
geo A
[ "$GW" -gt 400 ] || fail "tile master unshade failed"

# tile: single stack shaded docks right, master takes the rest
"$X" shade B; sleep 0.2
geo B
[ "$GW" -le 24 ] || fail "tile stack shade is not a side bar"
[ "$GX" -gt 800 ] || fail "tile stack bar is not on the right"
geo A
[ "$GW" -gt 800 ] || fail "tile master did not fill the freed stack width"
"$X" shade B; sleep 0.2

# bstack: collapsed rows hug the top/bottom edge
for _ in 1 2 3; do sleepkey alt space; done
geo A
AY=$GY
geo B
[ "$GY" -gt "$AY" ] || fail "bstack rows not stacked"
"$X" shade A; sleep 0.2
geo A
[ "$GH" -le 24 ] || fail "bstack master shade is not a top bar"
geo B
[ "$GH" -gt 600 ] || fail "bstack stack did not fill the freed master height"
"$X" shade A; sleep 0.2
"$X" shade B; sleep 0.2
geo B
[ "$GY" -gt 600 ] || fail "bstack stack bar is not at the bottom"
geo A
[ "$GH" -gt 600 ] || fail "bstack master did not fill the freed stack height"
"$X" shade B; sleep 0.2

# four clients for grid
DISPLAY=$DISP xterm -T C >/dev/null 2>&1 &
PIDS="$PIDS $!"
sleep 0.3
DISPLAY=$DISP xterm -T D >/dev/null 2>&1 &
PIDS="$PIDS $!"
sleep 0.7

sleepkey alt space
"$X" shade C; sleep 0.3
geo C
[ "$GW" -le 24 ] || fail "grid shaded window is not a rail bar"
[ "$GX" -gt 900 ] || fail "grid shade rail is not on the right"
geo D
[ "$GW" -gt 900 ] || fail "grid queue did not fill the freed space"
"$X" shade C; sleep 0.2

# mfact/cfact in grid
"$X" click 200 200 1
sleep 0.2
geo A
W0=$GW
sleepkey alt equal
geo A
[ "$GW" -ne "$W0" ] || fail "mfact has no effect in grid"
sleepkey alt minus
"$X" click 800 800 1
sleep 0.2
geo D
H0=$GH
sleepkey alt+shift l
geo D
[ "$GH" -ne "$H0" ] || fail "cfact has no effect in grid"
sleepkey alt+shift o

# layout cycle must not crash
for _ in 1 2 3 4 5; do sleepkey alt space; done

# modal: no frame/titlebar, closing it leaves the parent alive
"$TMP/dialog" >"$TMP/dialog.log" 2>&1 &
PIDS="$PIDS $!"
sleep 0.7
geo DIALOG
[ "$GW" -eq 240 ] && [ "$GH" -eq 140 ] ||
	fail "dialog has decorations or was resized"
"$X" click $((GX + GW / 2)) $((GY + GH / 2)) 1
sleep 0.2
sleepkey alt+shift c
sleep 0.5
grep -q "parent alive" "$TMP/dialog.log" || fail "closing the dialog killed its app"
geo PARENT || fail "parent disappeared after dialog close"

# master-count clamp: with 8 clients, 10 increments must stop at maxnmaster
for n in E F G H; do
	DISPLAY=$DISP xterm -T "$n" >/dev/null 2>&1 &
	PIDS="$PIDS $!"
	sleep 0.2
done
sleep 0.5
sleepkey alt t
for _ in $(seq 1 10); do sleepkey alt i; done
geo C
[ "$GX" -gt 400 ] || fail "nmaster exceeded maxnmaster"
for _ in $(seq 1 20); do sleepkey alt d; done
for _ in $(seq 1 10); do sleepkey alt i; done
geo C
[ "$GX" -gt 400 ] || fail "nmaster lower/upper clamp failed"

kill -TERM "$DWM_PID"
wait "$DWM_PID" 2>/dev/null || true
sleep 0.2
if grep -qi "fatal error\|segmentation fault\|assertion" "$TMP/dwm.log"; then
	fail "dwm logged a fatal error"
fi

echo "ok: layouts, shading, mfact/cfact, modal"
