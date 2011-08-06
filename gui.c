#include <ncurses.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>

#include "proc.h"
#include "gui.h"
#include "util.h"

#define HALF_DELAY_TIME 5

/* 500ms */
#define WAIT_TIME (HALF_DELAY_TIME * 100)
/* 30s */
#define FULL_WAIT_TIME (WAIT_TIME * 60)

#define CTRL_AND(c) ((c) & 037)
#define INDENT "    "
#define STATUS(y, x, ...) do{ mvprintw(y, x, __VA_ARGS__); clrtoeol(); }while(0)

static int pos_top = 0, pos_y = 0;


static int  search = 0;
static int  search_idx = 0, search_offset = 0;
static char search_str[32] = { 0 };
static struct proc *search_proc = NULL;

void getch_delay(int on)
{
	if(on)
		halfdelay(HALF_DELAY_TIME); /* x/10s of a second wait */
	else
		cbreak();
}

void gui_init()
{
	static int init = 0;

	if(init){
		refresh();
	}else{
		init = 1;

		initscr();
		noecho();
		cbreak();
		raw();

		nonl();
		intrflush(stdscr, FALSE);
		keypad(stdscr, TRUE);

		getch_delay(1);

		if(has_colors()){
			start_color();
			use_default_colors();

			init_pair(1 + COLOR_BLACK  , COLOR_BLACK  , -1);
			init_pair(1 + COLOR_GREEN  , COLOR_GREEN  , -1);
			init_pair(1 + COLOR_WHITE  , COLOR_WHITE  , -1);
			init_pair(1 + COLOR_RED    , COLOR_RED    , -1);
			init_pair(1 + COLOR_CYAN   , COLOR_CYAN   , -1);
			init_pair(1 + COLOR_MAGENTA, COLOR_MAGENTA, -1);
			init_pair(1 + COLOR_BLUE   , COLOR_BLUE   , -1);
			init_pair(1 + COLOR_YELLOW , COLOR_YELLOW , -1);
		}
	}
}

void gui_term()
{
	endwin();
}

int search_proc_to_idx(int *y, struct proc **procs)
{
	struct proc *init = proc_get(procs, 1);
	if(search_proc == init)
		return 0;
	*y = 1;
	return proc_to_idx(search_proc, init, y);
}

void position(int newy, int newx)
{
	(void)newx;

	pos_y = newy; /* checked above */

	if(pos_y < 0)
		pos_y = 0;

	if(pos_y - LINES + 2 > pos_top)
		pos_top = pos_y - LINES + 2;
	if(pos_y < pos_top)
		pos_top = pos_y;
}

void showproc(struct proc *proc, int *py, int indent)
{
	struct proc *p;
	int y = *py;
	int i;

	if(y >= LINES)
		return;

	if(y > 0){
		extern int global_uid;
		const int owned = proc->uid == global_uid;
		char buf[256];
		int len = LINES;

#define ATTR_NOT_OWNED A_BOLD | COLOR_PAIR(1 + COLOR_BLACK)
#define ATTR_SEARCH    A_BOLD | COLOR_PAIR(1 + COLOR_RED)
#define ATTR_BASENAME  A_BOLD | COLOR_PAIR(1 + COLOR_CYAN)

		move(y, 0);

		if(proc == search_proc)
			attron(ATTR_SEARCH);
		else if(!owned)
			attron(ATTR_NOT_OWNED);

		len -= snprintf(buf, sizeof buf,
				"% 7d %c %-8s %-8s",
				proc->pid, proc->state,
				proc->unam, proc->gnam);
		addstr(buf);

		if(proc->state == 'R'){
			int y, x;
			getyx(stdscr, y, x);
			mvchgat(y, x - 19, 1, 0, COLOR_GREEN + 1, NULL);
			move(y, x);
		}

		i = indent;
		while(i -->= 0){
			addstr(INDENT);
			len -= 2;
		}

		i = getcurx(stdscr) + proc->basename_offset;

		addnstr(proc->cmd, COLS - indent - len - 1);
		clrtoeol();

		if(owned)
			attron(ATTR_BASENAME);
		mvaddnstr(y, i, proc->basename, COLS - indent - len - 1);

		if(proc == search_proc)
			attroff(ATTR_SEARCH);
		else if(owned)
			attroff(ATTR_BASENAME);
		else
			attroff(ATTR_NOT_OWNED);

	}

	for(p = proc->child_first; p; p = p->child_next){
		y++;
		showproc(p, &y, indent + 1);
	}

	*py = y;
}

void showprocs(struct proc **procs, struct procstat *pst)
{
	int y = -pos_top + 1;

	showproc(proc_get(procs, 1), &y, 0);

	clrtobot();

	if(search){
		if(!search_proc)
			attron(COLOR_PAIR(1 + COLOR_RED));
		mvprintw(0, 0, "%d /%s", search_offset, search_str);
		if(!search_proc)
			attroff(COLOR_PAIR(1 + COLOR_RED));
		clrtoeol();

		if(search_proc){
			int ty;
			if(search_proc_to_idx(&ty, procs)){
				pos_top = ty - LINES / 2;
				if(pos_top < 0)
					pos_top = 0;
			}
		}

	}else{
		STATUS(0, 0, "%d processes, %d running, %d owned",
			pst->count, pst->running, pst->owned);
		clrtoeol();

		move(1 + pos_y - pos_top, 0);
		mvchgat(1 + pos_y - pos_top, 0, -1, A_UNDERLINE, /* color pair index */ 0, NULL);
	}
}

void gui_search(int ch, struct proc **procs)
{
	switch(ch){
		case '\r':
		{
			int y;
			if(search_proc_to_idx(&y, procs))
				position(y, 0);

			/* fall */

		case CTRL_AND('['):
			search = search_offset = 0;
			break;
		}

		case CTRL_AND('n'): search_offset++; break;
		case CTRL_AND('p'):
			if(search_offset > 0)
				search_offset--;
			break;

		case CTRL_AND('?'):
		case CTRL_AND('H'):
		case 263:
		case 127:
			if(search_idx > 0)
				search_str[--search_idx] = '\0';
			else
				search = 0;
			break;

		case CTRL_AND('u'):
			search_idx = 0;
			*search_str = '\0';
			break;

		default:
			if(isprint(ch) && search_idx < (signed)sizeof search_str - 2){
				search_str[search_idx++] = ch;
				search_str[search_idx  ] = '\0';
			}
			break;
	}

	if(search && *search_str && search_idx)
		search_proc = proc_find_n(search_str, procs, search_offset);
	else
		search_proc = NULL;
}

struct proc *curproc(struct proc **procs)
{
	int i = pos_y;
	return proc_from_idx(proc_get(procs, 1), &i);
}

int waitgetch()
{
	int ret;
	getch_delay(0);
	ret = getch();
	getch_delay(1);
	return ret;
}

void waitch(int y, int x)
{
	attron( COLOR_PAIR(1 + COLOR_RED));
	STATUS(y, x, "any key to continue");
	attroff(COLOR_PAIR(1 + COLOR_RED));
	waitgetch();
}

int confirm(const char *fmt, ...)
{
	va_list l;
	int ch;

	va_start(l, fmt);
	move(0, 0);
	vwprintw(stdscr, fmt, l);
	va_end(l);
	clrtoeol();

	ch = waitgetch();
	if(ch == 'y' || ch == 'Y')
		return 1;
	return 0;
}

void delete(struct proc *p)
{
	char sig[8];
	int i, wait = 0;

	STATUS(0, 0, "kill %d (%s) with: ", p->pid, p->basename);
	echo();
	getnstr(sig, sizeof sig);
	noecho();

	if(!*sig)
		return;

	for(i = 0; i < (signed) strlen(sig); i++)
		sig[i] = toupper(sig[i]);

	if('0' <= *sig && *sig <= '9'){
		if(sscanf(sig, "%d", &i) != 1){
			STATUS(0, 0, "not a number");
			wait = 1;
			i = -1;
		}
	}else{
		char *cmp = sig;

		if(!strncmp(sig, "SIG", 3))
			cmp += 3;

		i = str_to_sig(cmp);
		if(i == -1){
			STATUS(0, 0, "not a signal");
			wait = 1;
		}
	}

	if(!wait && i != -1){
		if(kill(p->pid, i)){
			STATUS(0, 0, "kill: %s", strerror(errno));
			wait = 1;
		}
	}

	if(wait)
		waitch(1, 0);
}

void external(const char *cmd, struct proc *p)
{
	char buf[16];

	gui_term();

	snprintf(buf, sizeof buf, "%s -p %d", cmd, p->pid);
	system(buf);
	fputs("return to continue...", stdout);
	fflush(stdout);
	getchar();

	gui_init();
}

void strace(struct proc *p)
{
	external("strace", p);
}

void lsof(struct proc *p)
{
	external("lsof", p);
}

void info(struct proc *p)
{
	int y, x;
	int i;

	clear();
	mvprintw(0, 0,
			"pid: %d, ppid: %d\n"
			"uid: %d (%s), gid: %d (%s)\n"
			"state: %c\n"
			"tty: %d, pgrp: %d\n"
			,
			p->pid, p->ppid,
			p->uid, p->unam, p->gid, p->gnam,
			p->state,
			p->tty, p->pgrp);

	for(i = 0; p->argv[i]; i++)
		printw("argv[%d] = \"%s\"\n", i, p->argv[i]);

	getyx(stdscr, y, x);
	(void)x;

	waitch(y + 1, 0);
}

void on_curproc(const char *fstr, void (*f)(struct proc *), int ask, struct proc **procs)
{
	struct proc *p = curproc(procs);
	extern int global_force;

	if(p){
		if(ask && !global_force && !confirm("%s: %d (%s)? (y/n) ", fstr, p->pid, p->basename))
			return;
		f(p);
	}
}

void gui_run(struct proc **procs)
{
	struct procstat pst;
	long last_update = 0, last_full_refresh;
	int fin = 0;

	proc_update(procs, &pst);

	last_full_refresh = mstime();
	do{
		const long now = mstime();
		int ch;

		if(last_update + WAIT_TIME < now){
			last_update = now;
			if(last_full_refresh + FULL_WAIT_TIME < now){
				last_full_refresh = now;
				proc_handle_renames(procs);
			}
			proc_update(procs, &pst);
		}

		if(pos_y > pst.count - 1)
			position(pst.count - 1, 0);

		showprocs(procs, &pst);

		ch = getch();
		if(ch == -1)
			continue;

		if(search){
			gui_search(ch, procs);
		}else{
			switch(ch){
				case 'q':
					fin = 1;
					break;

				case 'k':
					if(pos_y > 0)
						position(pos_y - 1, 0);
					break;
				case 'j':
					position(pos_y + 1, 0);
					break;

				case 'g':
					position(0, 0);
					break;
				case 'G':
					position(pst.count, 0);
					break;

				case CTRL_AND('u'):
					position(pos_y - LINES / 2, 0);
					break;
				case CTRL_AND('d'):
					position(pos_y + LINES / 2, 0);
					break;
				case CTRL_AND('b'):
					position(pos_y - LINES, 0);
					break;
				case CTRL_AND('f'):
					position(pos_y + LINES, 0);
					break;

				case CTRL_AND('e'):
					if(pos_top < pst.count - 1){
						pos_top++;
						if(pos_y < pos_top)
							pos_y = pos_top;
					}
					break;
				case CTRL_AND('y'):
					if(pos_top > 0){
						pos_top--;
						if(pos_y > pos_top + LINES - 2)
							pos_y = pos_top + LINES - 2;
					}
					break;

				case 'L':
					position(pos_top + LINES - 2, 0);
					break;
				case 'H':
					position(pos_top, 0);
					break;
				case 'M':
					position(pos_top + (pst.count > LINES ? LINES : pst.count) / 2, 0);
					break;

				case 'i':
					on_curproc("info", info, 0, procs);
					break;
				case 'd':
					on_curproc("delete", delete, 0, procs);
					break;
				case 'l':
					on_curproc("lsof", lsof, 1, procs);
					break;
				case 's':
					on_curproc("strace", strace, 1, procs);
					break;

				case '/':
					*search_str = '\0';
					search_idx = 0;
					search = 1;
					move(0, 0);
					clrtoeol();
					break;
			}
		}
	}while(!fin);
}
