CFLAGS  = -g -Wall -Wextra -pedantic -std=c99 -D_POSIX_SOURCE
LDFLAGS = -g -lncurses
PREFIX  = /usr

.PHONY: clean install uninstall

utop: main.o proc.o gui.o util.o
	${CC} ${LDFLAGS} -o $@ $^

clean:
	@rm -f *.o utop

install: utop
	cp utop ${PREFIX}/bin

uninstall:
	rm -f ${PREFIX}/bin/utop

gui.o: gui.c proc.h gui.h util.h
main.o: main.c proc.h gui.h
proc.o: proc.c proc.h util.h
util.o: util.c util.h
