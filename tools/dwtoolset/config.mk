VERSION = 0.1

PREFIX ?= /usr/local
MANPREFIX = ${PREFIX}/share/man
ICONPATH = ${PREFIX}/share/dwm-dworf/icons

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

INCS = -I. -I${X11INC}
LIBS = -L${X11LIB} -lX11 -lXtst

CPPFLAGS = -DVERSION=\"${VERSION}\" -DICONPATH=\"${ICONPATH}\"
CFLAGS = -std=c99 -pedantic -Wall -Os ${INCS} ${CPPFLAGS}
LDFLAGS = ${LIBS}
CC = cc
