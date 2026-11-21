# dwslit - classic *box-style dockapp slit for dwm-dworf
include config.mk

SRC = dwslit.c

all: options dwslit

options:
	@echo dwslit build options:
	@echo "CFLAGS   = ${CFLAGS}"
	@echo "LDFLAGS  = ${LDFLAGS}"
	@echo "CC       = ${CC}"

config.h:
	@cp config.def.h $@

dwslit: ${SRC} config.h
	@echo CC -o $@
	@${CC} -o $@ ${SRC} ${CFLAGS} ${LDFLAGS}

wmapps:
	@${MAKE} -C wmapps

clean:
	@rm -f dwslit
	@${MAKE} -C wmapps clean

install: all
	@echo installing executable to ${DESTDIR}${PREFIX}/bin
	@mkdir -p ${DESTDIR}${PREFIX}/bin
	@cp -f dwslit ${DESTDIR}${PREFIX}/bin
	@chmod 755 ${DESTDIR}${PREFIX}/bin/dwslit

uninstall:
	@rm -f ${DESTDIR}${PREFIX}/bin/dwslit

dist: clean
	@mkdir -p dwslit-${VERSION}
	@cp LICENSE Makefile config.def.h config.mk dwslit.c README.md dwslit-${VERSION}
	@cp -R wmapps dwslit-${VERSION}
	@tar -cf dwslit-${VERSION}.tar dwslit-${VERSION}
	@gzip dwslit-${VERSION}.tar
	@rm -rf dwslit-${VERSION}

.PHONY: all options wmapps clean install uninstall dist
