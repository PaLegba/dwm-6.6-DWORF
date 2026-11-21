# Build the bundled classic wmapps.
CC ?= cc
CFLAGS ?= -O2 -Wall
CFLAGS += -std=gnu89 -fcommon -I/usr/X11R6/include -I/usr/include
LDFLAGS ?= -L/usr/X11R6/lib
XLIBS = -lXpm -lXext -lX11
MLIBS = $(XLIBS) -lm
BIN = bin

APPS = wmfire wmfirew wmitime wmtime wmtimer \
	wmmatrix wmmoonclock wmsun wmglobe
HELPERS = fireload_cpu fireload_file

all: $(BIN) $(addprefix $(BIN)/,$(APPS)) $(addprefix $(BIN)/,$(HELPERS))

$(BIN):
	@mkdir -p $(BIN)

$(BIN)/wmfire: wmfire/wmfire.c wmfire/wmgeneral.c
	$(CC) $(CFLAGS) -Iwmfire $^ -o $@ $(LDFLAGS) $(XLIBS)

$(BIN)/fireload_cpu: wmfire/fireload_cpu.c
	$(CC) $(CFLAGS) -Iwmfire $^ -o $@

$(BIN)/fireload_file: wmfire/fireload_file.c
	$(CC) $(CFLAGS) -Iwmfire $^ -o $@

$(BIN)/wmfirew: wmfirew/wmfirew/wmfirew.c wmfirew/wmgeneral/wmgeneral.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(XLIBS)

$(BIN)/wmitime: wmitime/wmitime/wmitime.c wmitime/wmgeneral/wmgeneral.c \
		wmitime/wmgeneral/misc.c wmitime/wmgeneral/list.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(MLIBS)

$(BIN)/wmtime: wmtime/wmtime/wmtime.c wmtime/wmgeneral/wmgeneral.c \
		wmtime/wmgeneral/misc.c wmtime/wmgeneral/list.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(MLIBS)

$(BIN)/wmtimer: wmtimer/wmtimer/wmtimer.c wmtimer/wmgeneral/wmgeneral.c \
		wmtimer/wmgeneral/misc.c wmtimer/wmgeneral/list.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(MLIBS)

$(BIN)/wmmatrix: wmmatrix/wmMatrix.c wmmatrix/matrix.c wmmatrix/xutils.c \
		wmmatrix/yarandom.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(XLIBS)

$(BIN)/wmmoonclock: wmmoonclock/Src/wmMoonClock.c \
		wmmoonclock/Src/CalcEphem.c wmmoonclock/Src/Moon.c \
		wmmoonclock/Src/MoonRise.c wmmoonclock/Src/xutils.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(MLIBS)

$(BIN)/wmsun: wmsun/wmSun/wmSun.c wmsun/wmSun/SunRise.c \
		wmsun/wmgeneral/wmgeneral.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) $(MLIBS)

$(BIN)/wmglobe: wmglobe/src/wmglobe.c wmglobe/src/rend.c \
		wmglobe/src/wmgutil.c wmglobe/src/sunpos.c \
		wmglobe/src/myconvert.c wmglobe/src/mycontext.c \
		wmglobe/src/wraster_shim.c
	$(CC) $(CFLAGS) -Iwmglobe/src $^ -o $@ $(LDFLAGS) $(MLIBS)

clean:
	rm -rf $(BIN)

.PHONY: all clean
