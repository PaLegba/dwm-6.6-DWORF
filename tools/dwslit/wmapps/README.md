# Bundled wmapps

A small collection of classic Window Maker dockapps, dusted off enough to
build and run on a modern X11 system. They are intended as examples for
`dwslit`.

Build:

```sh
make
./bin/wmtime
```

All binaries are written to `bin/`. `make clean` removes them.

Requirements: libX11, libXpm, libXext and libm. Apps that needed extra or
obsolete dependencies were left out.

## Included

| App | Purpose |
|---|---|
| `wmfire` | fire display driven by CPU load |
| `wmfirew` | fire display driven by system load |
| `wmitime` | Internet time clock |
| `wmtime` | date/time clock |
| `wmtimer` | countdown timer and stopwatch |
| `wmmatrix` | Matrix-style animation |
| `wmmoonclock` | moon phase and rise/set times |
| `wmsun` | sunrise/sunset times |
| `wmglobe` | rotating day/night Earth globe |

`fireload_cpu` and `fireload_file` are helper programs for `wmfire`.

`wmglobe` normally links the full WRaster stack. This build replaces it with a
small local shim in `wmglobe/src/wraster_shim.c` that decodes image files with
the bundled `tools/stb/stb_image.h`; the built-in XPM maps still go through
Xpm. No WRaster/TIFF/PNG/JPEG/GIF libraries are needed.

Each app has a short `README.md` with its origin and license. All bundled
apps are GPL/GPLv2; see the `COPYING` file in each directory.

Only immediately working apps are bundled. Deferred sources are kept outside
this build for future repair work.

These are historical programs. They are provided as working examples, not as
maintained upstream projects.
