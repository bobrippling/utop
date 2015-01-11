CFLAGS  = -g -Wall -Wextra -pedantic -std=c99

LDFLAGS = -g -lncurses
PREFIX  = /usr/local
OBJ     = main.o proc.o gui.o util.o machine.o
VERSION = 0.9.3

.PHONY: clean install uninstall deps

utop: ${OBJ}
	${CC} ${LDFLAGS} -o $@ ${OBJ}

gui.c main.c util.c proc.c \
	machine_linux.c \
	machine_darwin.c \
	machine_freebsd.c \
	machine_ps.c: config.mk

machine.o: machine.c machine_darwin.c machine_freebsd.c \
	machine_linux.c machine_ps.c

config.mk:
	@if ! test -f config.mk; then echo utop needs configuring >&2; exit 1; fi

include config.mk

clean:
	rm -f ${OBJ} utop

install: utop
	cp utop ${PREFIX}/bin
	mkdir -p ${PREFIX}/share/man/man1/
	sed "s/UTOP_VERSION/${VERSION}/g" utop.1 > ${PREFIX}/share/man/man1/utop.1

uninstall:
	rm -f ${PREFIX}/bin/utop ${PREFIX}/share/man/man1/

deps:
	cc -MM ${OBJ:.o=.c} >Makefile.deps

-include Makefile.deps
