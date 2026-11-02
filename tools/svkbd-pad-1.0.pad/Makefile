# svkbd - simple virtual keyboard
# See LICENSE file for copyright and license details.

include config.mk

SRC = svkbd.c
LAYOUTS = layout.en.h layout.en.fnx.h layout.en.full.h layout.ru.h\
	layout.ru.fnx.h layout.ru.full.h layout.cursor.v.side.h

all: options svkbd

options:
	@echo svkbd build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

config.h:
	@echo creating $@ from config.def.h
	@cp config.def.h $@

svkbd: layouts.h themes.h ${LAYOUTS} config.h ${SRC}
	@echo CC -o $@
	@${CC} -o $@ ${SRC} ${LDFLAGS} ${CFLAGS}

clean:
	@echo cleaning
	@rm -f svkbd svkbd-${VERSION}.tar.gz

dist: clean
	@echo creating dist tarball
	@mkdir -p svkbd-${VERSION}
	@cp LICENSE Makefile README.md config.def.h config.mk\
		layouts.h themes.h ${SRC} svkbd-${VERSION}
	@for i in layout.*.h; \
	do \
		cp $$i svkbd-${VERSION}; \
	done
	@tar -cf svkbd-${VERSION}.tar svkbd-${VERSION}
	@gzip svkbd-${VERSION}.tar
	@rm -rf svkbd-${VERSION}

install: all
	@echo installing executable files to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f svkbd ${DESTDIR}${PREFIX}/bin/svkbd
	@chmod 755 ${DESTDIR}${PREFIX}/bin/svkbd

uninstall:
	@echo removing executable files from ${DESTDIR}${PREFIX}/bin
	@rm -f ${DESTDIR}${PREFIX}/bin/svkbd

.PHONY: all options clean dist install uninstall
