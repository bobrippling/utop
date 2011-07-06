CFLAGS  = -g -Wall -Wextra -pedantic
LDFLAGS = -g -lncurses

.PHONY: clean

utop: main.o proc.o gui.o util.o
	${CC} ${LDFLAGS} -o $@ $^

clean:
	@rm -f *.o utop

gui.o: gui.c proc.h gui.h util.h
main.o: main.c proc.h gui.h
proc.o: proc.c proc.h
util.o: util.c
