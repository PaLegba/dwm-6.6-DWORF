# svkbd version
VERSION = 0.1

# Customize below to fit your system

# paths
PREFIX ?= /usr/local
MANPREFIX = ${PREFIX}/share/man

X11INC = /usr/X11R6/include
X11LIB = /usr/X11R6/lib

# includes and libs (Xft uses the universal fontconfig "monospace" family;
# both FreeType include locations are listed so Linux and BSD need no edits)
INCS = -I. -I./layouts -I/usr/include -I${X11INC} -I/usr/include/freetype2 -I${X11INC}/freetype2
LIBS = -L/usr/lib -lc -L${X11LIB} -lX11 -lXtst -lXft -lfontconfig -lfreetype

# flags
CPPFLAGS = -DVERSION=\"${VERSION}\" \
	   ${XINERAMAFLAGS}
CFLAGS = -g -std=c99 -pedantic -Wall -Os ${INCS} ${CPPFLAGS}
LDFLAGS = -g ${LIBS}

# Solaris
#CFLAGS = -fast ${INCS} -DVERSION=\"${VERSION}\"
#LDFLAGS = ${LIBS}

# compiler and linker
CC = cc

