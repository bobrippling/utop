CFLAGS  = -g -Wall -Wextra -pedantic -std=c99
LDFLAGS = -g -lncurses
PREFIX  = /usr/local
OBJ     = main.o proc.o gui.o util.o machine.o
VERSION = 0.9.1

.PHONY: clean install uninstall

utop: ${OBJ}
	${CC} ${LDFLAGS} -o $@ ${OBJ}

gui.c main.c util.c proc.c \
	machine_linux.c \
	machine_darwin.c \
	machine_freebsd.c \
	machine_ps.c: config.mk

config.mk:
	@echo utop needs configuring
	@false

include config.mk

clean:
	@rm -f *.o utop

install: utop
	cp utop ${PREFIX}/bin
	mkdir -p ${PREFIX}/share/man/man1/
	sed "s/UTOP_VERSION/${VERSION}/g" utop.1 > ${PREFIX}/share/man/man1/utop.1

uninstall:
	rm -f ${PREFIX}/bin/utop ${PREFIX}/share/man/man1/

gui.o: gui.c structs.h proc.h gui.h util.h config.h main.h machine.h
machine.o: machine.c structs.h proc.h main.h machine_linux.c \
 machine_freebsd.c machine_ps.c util.h machine.h
machine_darwin.o: machine_darwin.c machine_freebsd.c util.h machine.h \
 proc.h structs.h main.h
machine_freebsd.o: machine_freebsd.c util.h machine.h proc.h structs.h \
 main.h
machine_linux.o: machine_linux.c util.h proc.h machine.h main.h structs.h
machine_ps.o: machine_ps.c util.h structs.h machine.h proc.h
main.o: main.c structs.h proc.h gui.h util.h machine.h
proc.o: proc.c structs.h proc.h util.h main.h machine.h
util.o: util.c util.h
