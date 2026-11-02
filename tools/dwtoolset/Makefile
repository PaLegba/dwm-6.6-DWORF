# dwtoolset - vertical dock toolset and systray host
include config.mk

SRC = dwtoolset.c
ICONS = icons/dwm.term.new.xbm icons/dwm.win.zoom.xbm icons/dwm.main.dec.xbm \
	icons/dwm.main.inc.xbm icons/dwm.stack.up.xbm icons/dwm.stack.down.xbm \
	icons/dwm.svkbd.xbm

all: options dwtoolset

options:
	@echo dwtoolset build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

config.h:
	@cp config.def.h $@

dwtoolset: ${SRC} config.h ${ICONS}
	@echo CC -o $@
	@${CC} -o $@ ${SRC} ${CFLAGS} ${LDFLAGS}

clean:
	@rm -f dwtoolset

install: all
	@echo installing executable to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f dwtoolset ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/dwtoolset
	@echo installing icons to ${DESTDIR}${ICONPATH}
	@mkdir -p ${DESTDIR}${ICONPATH}
	@cp -f ${ICONS} ${DESTDIR}${ICONPATH}

uninstall:
	@rm -f ${DESTDIR}${PREFIX}/bin/dwtoolset
	@for f in ${ICONS}; do rm -f ${DESTDIR}${ICONPATH}/$$(basename $$f); done

dist: clean
	@mkdir -p dwtoolset-${VERSION}
	@cp LICENSE Makefile config.def.h config.mk dwtoolset.c README.md dwtoolset-${VERSION}
	@cp -R icons dwtoolset-${VERSION}
	@tar -cf dwtoolset-${VERSION}.tar dwtoolset-${VERSION}
	@gzip dwtoolset-${VERSION}.tar
	@rm -rf dwtoolset-${VERSION}

.PHONY: all options clean install uninstall dist
