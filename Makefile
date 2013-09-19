CFLAGS  = -g -Wall -Wextra -pedantic -std=c99 -Wmissing-prototypes
LDFLAGS = -g -lncurses
PREFIX  = /usr/local
OBJ     = main.o proc.o gui.o util.o machine.o gui_tree.o gui_type.o
VERSION = 0.9.3

.PHONY: clean install uninstall deps

utop: ${OBJ}
	${CC} ${LDFLAGS} -o $@ ${OBJ}

${OBJ}: config.mk

config.mk:
	@echo utop needs configuring
	@false

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
	${CC} -MM ${OBJ:.o=.c} > Makefile.deps

include Makefile.deps
