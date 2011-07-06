#include <ncurses.h>
#include <sys/types.h>

#include "proc.h"
#include "gui.h"
#include "util.h"

int pos_y = 0;

void gui_init()
{
	initscr();
	noecho();
	cbreak();
	raw();

	nonl();
	intrflush(stdscr, FALSE);
	keypad(stdscr, TRUE);

	halfdelay(2); /* x/10s of a second wait */

	if(has_colors()){
		start_color();
		use_default_colors();
		init_pair(COLOR_BLACK,   -1,            -1);
		init_pair(COLOR_GREEN,   COLOR_GREEN,   -1);
		init_pair(COLOR_WHITE,   COLOR_WHITE,   -1);
		init_pair(COLOR_RED,     COLOR_RED,     -1);
		init_pair(COLOR_CYAN,    COLOR_CYAN,    -1);
		init_pair(COLOR_MAGENTA, COLOR_MAGENTA, -1);
		init_pair(COLOR_BLUE,    COLOR_BLUE,    -1);
		init_pair(COLOR_YELLOW,  COLOR_YELLOW,  -1);
	}
}

void gui_term()
{
	endwin();
}

void showprocs(struct proc **procs)
{
	struct proc *p = proc_to_list(procs);
	char buf[256];
	int y;

	y = pos_y;

	while(p->next && y --> 0)
		p = p->next;

	for(y = 0; y < LINES && p; y++, p = p->next){
		snprintf(buf, sizeof buf, "%s - %d", p->cmd, p->pc_cpu);
		mvaddnstr(y, 0, buf, COLS - 1);
	}

	move(0, 0);
}

void gui_run(struct proc **procs)
{
	long last_update = 0;
	int fin = 0;

	do{
		const long now = mstime();
		int ch, nprocs;

		if(last_update + 500 < now){
			last_update = now;
			proc_update(procs, &nprocs);
			fprintf(stderr, "proc_update: %d, pos_y: %d\n", nprocs, pos_y);
		}

		clear();
		showprocs(procs);

		ch = getch();
		switch(ch){
			case 'q': fin = 1; break;

			case 'k': if(pos_y >        0) pos_y--; break;
			case 'j': if(pos_y < nprocs-1) pos_y++; break;
		}
	}while(!fin);
}
