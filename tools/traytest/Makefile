X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib
CFLAGS = -std=c99 -pedantic -Wall -Os -I${X11INC}
LDFLAGS = -L${X11LIB} -lX11
CC = cc

all: traytest

traytest: traytest.c
	${CC} -o traytest traytest.c ${CFLAGS} ${LDFLAGS}

clean:
	rm -f traytest

.PHONY: all clean
