CFLAGS  = -g -Wall -Wextra -pedantic -std=c99
LDFLAGS = -g -lncurses
PREFIX  = /usr/local
OBJ     = main.o proc.o gui.o util.o machine.o

include config.mk

.PHONY: clean install uninstall

utop: ${OBJ}
	${CC} ${LDFLAGS} -o $@ ${OBJ}

clean:
	@rm -f *.o utop

install: utop
	cp utop ${PREFIX}/bin
	mkdir -p ${PREFIX}/share/man/man1/
	cp utop.1 ${PREFIX}/share/man/man1/

uninstall:
	rm -f ${PREFIX}/bin/utop ${PREFIX}/share/man/man1/

gui.o: gui.c proc.h gui.h util.h config.h main.h machine.h sys.h
machine.o: machine.c machine_darwin.c machine_freebsd.c util.h machine.h \
  proc.h sys.h main.h
machine_darwin.o: machine_darwin.c machine_freebsd.c util.h machine.h \
  proc.h sys.h main.h
machine_freebsd.o: machine_freebsd.c util.h machine.h proc.h sys.h main.h
machine_linux.o: machine_linux.c util.h proc.h sys.h machine.h main.h
main.o: main.c proc.h gui.h util.h machine.h
proc.o: proc.c proc.h util.h main.h machine.h
util.o: util.c util.h
