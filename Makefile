# dwm - dynamic window manager
# See LICENSE file for copyright and license details.

include config.mk

SRC = drw.c dwm.c util.c
OBJ = ${SRC:.c=.o}
DISTNAME = dwm-${VERSION}-DWORF
DMENU_DIR = tools/dmenu-5.4-mod

all: dwm-6.6

.c.o:
	${CC} -c ${CFLAGS} $<

${OBJ}: config.h config.mk
dwm.o: themes.h push.c tilemovemouse.c lay.bstack.c lay.grid.c lay.gaplessgrid.c\
	lay.tcl.c lay.fibonacci.c nextprevlayout.c

config.h:
	cp config.def.h $@

dwm-6.6: ${OBJ}
	${CC} -o $@ ${OBJ} ${LDFLAGS}

dmenu:
	${MAKE} -C ${DMENU_DIR}\
		PREFIX=${PREFIX} MANPREFIX=${MANPREFIX} DESTDIR=${DESTDIR}\
		FREETYPEINC=${FREETYPEINC}

clean:
	rm -f dwm-6.6 ${OBJ} dwm-${VERSION}.tar.gz ${DISTNAME}.tar.gz
	${MAKE} -C ${DMENU_DIR} clean

dist:
	rm -rf ${DISTNAME} ${DISTNAME}.tar.gz
	mkdir -p ${DISTNAME}
	cp -R LICENSE Makefile README README.md MODIFICATIONS.md\
		config.def.h config.h config.mk dwm.1 dwm.c drw.c drw.h util.c util.h\
		dwm.png transient.c themes.h push.c tilemovemouse.c focusmaster.c\
		lay.bstack.c lay.grid.c lay.gaplessgrid.c lay.tcl.c lay.fibonacci.c\
		nextprevlayout.c rotofont_example.c rotofont_example.c.sh xephyr.sh\
		tools ${DISTNAME}
	rm -f ${DISTNAME}/dwm-6.6 ${DISTNAME}/*.o\
		${DISTNAME}/tools/dmenu-5.4-mod/dmenu\
		${DISTNAME}/tools/dmenu-5.4-mod/stest\
		${DISTNAME}/tools/dmenu-5.4-mod/*.o
	tar -cf ${DISTNAME}.tar ${DISTNAME}
	gzip ${DISTNAME}.tar
	rm -rf ${DISTNAME}

install: all
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f dwm-6.6 ${DESTDIR}${PREFIX}/bin/dwm
	chmod 755 ${DESTDIR}${PREFIX}/bin/dwm
	ln -sf dwm ${DESTDIR}${PREFIX}/bin/dwm-6.6
	mkdir -p ${DESTDIR}${MANPREFIX}/man1
	sed "s/VERSION/${VERSION}/g" < dwm.1 > ${DESTDIR}${MANPREFIX}/man1/dwm.1
	chmod 644 ${DESTDIR}${MANPREFIX}/man1/dwm.1
	@if [ "${WITH_DMENU}" = "yes" ]; then\
		${MAKE} install-dmenu;\
	fi

install-dmenu: dmenu
	${MAKE} -C ${DMENU_DIR} install\
		PREFIX=${PREFIX} MANPREFIX=${MANPREFIX} DESTDIR=${DESTDIR}\
		FREETYPEINC=${FREETYPEINC}

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/dwm ${DESTDIR}${PREFIX}/bin/dwm-6.6\
		${DESTDIR}${MANPREFIX}/man1/dwm.1
	@if [ "${WITH_DMENU}" = "yes" ]; then\
		${MAKE} uninstall-dmenu;\
	fi

uninstall-dmenu:
	${MAKE} -C ${DMENU_DIR} uninstall\
		PREFIX=${PREFIX} MANPREFIX=${MANPREFIX} DESTDIR=${DESTDIR}

.PHONY: all clean dist dmenu install install-dmenu uninstall uninstall-dmenu
