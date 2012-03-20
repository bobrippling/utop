CFLAGS  = -g -Wall -Wextra -pedantic -std=c99 -D_POSIX_SOURCE
LDFLAGS = -g -lncurses
PREFIX  = /usr/local
OBJ     = main.o proc.o gui.o util.o

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

gui.o: gui.c proc.h gui.h util.h
main.o: main.c proc.h gui.h
proc.o: proc.c proc.h util.h
util.o: util.c util.h
