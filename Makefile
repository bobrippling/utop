CFLAGS  = -g -Wall -Wextra -pedantic -std=c99
LDFLAGS = -g -lncurses -lkvm
PREFIX  = /usr
SRC     = main.c machine.c gui.c proc.c util.c
OBJ     = ${SRC:.c=.o}

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

gui.o: gui.c proc.h gui.h util.h config.h machine.c machine.h
main.o: main.c proc.h gui.h config.h
proc.o: proc.c proc.h util.h machine.c machine.h
util.o: util.c util.h
machine.o: machine.c machine.h
