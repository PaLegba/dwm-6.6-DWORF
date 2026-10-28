X11INC=/usr/X11R6/include
X11LIB=/usr/X11R6/lib

FREETYPEINC=/usr/include/freetype2


gcc rotofont_example.c -o rotofont_example -I${X11INC} -I${FREETYPEINC} -L${X11LIB} -lX11 -lfontconfig -lfreetype -lXft 
