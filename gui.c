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
#include "config.h"

#define STATUS(y, x, ...) do{ mvprintw(y, x, __VA_ARGS__); clrtoeol(); }while(0)
#define WAIT_STATUS(...) do{ STATUS(0, 0, __VA_ARGS__); ungetch(getch()); }while(0)

static int pos_top = 0, pos_y = 0;

static int  search = 0;
static int  search_idx = 0, search_offset = 0, search_pid = 0;
static char search_str[32] = { 0 };
static struct proc *search_proc = NULL;

static pid_t lock_proc_pid = -1;

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

			init_pair(1 + COLOR_BLACK   , COLOR_BLACK  , -1);
			init_pair(1 + COLOR_GREEN   , COLOR_GREEN  , -1);
			init_pair(1 + COLOR_WHITE   , COLOR_WHITE  , -1);
			init_pair(1 + COLOR_RED     , COLOR_RED    , -1);
			init_pair(1 + COLOR_CYAN    , COLOR_CYAN   , -1);
			init_pair(1 + COLOR_MAGENTA , COLOR_MAGENTA, -1);
			init_pair(1 + COLOR_BLUE    , COLOR_BLUE   , -1);
			init_pair(1 + COLOR_YELLOW  , COLOR_YELLOW , -1);
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

struct proc *curproc(struct proc **procs)
{
	int i = pos_y;
	return proc_from_idx(proc_get(procs, 1), &i);
}

void position(int newy)
{
	pos_y = newy; /* checked above */

	if(pos_y < 0)
		pos_y = 0;

	if(pos_y - LINES + 2 > pos_top)
		pos_top = pos_y - LINES + 2;
	if(pos_y < pos_top)
		pos_top = pos_y;
}

void goto_proc(struct proc **procs, struct proc *p)
{
	int y = 1;
	proc_to_idx(p, proc_get(procs, 1), &y);
	position(y);
}

void goto_me(struct proc **procs)
{
	goto_proc(procs, proc_get(procs, getpid()));
}

void goto_lock(struct proc **procs)
{
	if(lock_proc_pid == -1){
		attron( COLOR_PAIR(1 + COLOR_RED));
		WAIT_STATUS("no process locked on");
		attroff(COLOR_PAIR(1 + COLOR_RED));
	}else{
		goto_proc(procs, proc_get(procs, lock_proc_pid));
	}
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
		extern int max_unam_len, max_gnam_len;

		const int owned = proc->uid == global_uid;
		char buf[256];
		int len = LINES;
		int lock = proc->pid == lock_proc_pid;

		move(y, 0);

		if(lock)
			attron(ATTR_LOCK);
		else if(proc == search_proc)
			attron(ATTR_SEARCH);
		else if(!owned)
			attron(ATTR_NOT_OWNED);

		len -= snprintf(buf, sizeof buf,
				"% 7d %c "
				"%-*s %-*s "
				"% 4d"
				,
				proc->pid, proc->state,
				max_unam_len, proc->unam,
				max_gnam_len, proc->gnam,
				proc->pc_cpu
				);
		addstr(buf);

		if(proc->state == 'R'){
			int y, x;
			getyx(stdscr, y, x);
			mvchgat(y, 8, 1, 0, COLOR_GREEN + 1, NULL);
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

		if(owned && !lock)
			attron(ATTR_BASENAME);
		mvaddnstr(y, i, proc->basename, COLS - indent - len - 1);

		if(lock)
			attroff(ATTR_LOCK);
		else if(proc == search_proc)
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
		const int red = !search_proc && *search_str;;

		if(red)
			attron(COLOR_PAIR(1 + COLOR_RED));
		mvprintw(0, 0, "%d %c%s", search_offset, "/?"[search_pid], search_str);
		if(red)
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
		int y;

		STATUS(0, 0, "%d processes, %d running, %d owned",
			pst->count, pst->running, pst->owned);
		clrtoeol();

		y = 1 + pos_y - pos_top;

		mvchgat(y, 0, 36, A_UNDERLINE, 0, NULL);
		move(y, 35);
	}
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

	if(!wait && i != -1)
		if(kill(p->pid, i)){
			STATUS(0, 0, "kill: %s", strerror(errno));
			wait = 1;
		}

	if(wait)
		waitch(1, 0);
	getch_delay(1);
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
	extern int global_force;

	struct proc *p;

	if(lock_proc_pid == -1 || !(p = proc_get(procs, lock_proc_pid))){
		p = search_proc ? search_proc : curproc(procs);
	}else{
		STATUS(0, 0, "using locked process %d, \"%s\", any key to continue", p->pid, p->basename);
		getch_delay(0);
		getch();
		getch_delay(1);
	}

	if(p){
		if(ask && !global_force && !confirm("%s: %d (%s)? (y/n) ", fstr, p->pid, p->basename))
			return;
		f(p);
	}
}

void lock_to(struct proc *p)
{
	if(p){
		if(lock_proc_pid == p->pid){
			goto unlock;
		}else{
			lock_proc_pid = p->pid;
			WAIT_STATUS("locked to process %d", lock_proc_pid);
		}
	}else{
		if(lock_proc_pid == -1){
			WAIT_STATUS("no process to lock to");
		}else{
unlock:
			WAIT_STATUS("unlocked from process %d", lock_proc_pid);
			lock_proc_pid = -1;
		}
	}
}

void gui_search(int ch, struct proc **procs)
{
	int do_lock = 0;

	if(ch == CTRL_AND('?') ||
		ch == CTRL_AND('H') ||
		ch == 263 ||
		ch == 127)
		goto backspace;

	if(search_pid){
		if('0' <= ch && ch <= '9')
			goto ins_char;
		else if(ch == LOCK_CHAR)
			goto lock_proc;

		search_offset = search = 0;
	}else{
		switch(ch){
			default:
ins_char:
				if(isprint(ch) && search_idx < (signed)sizeof search_str - 2){
					search_str[search_idx++] = ch;
					search_str[search_idx  ] = '\0';
				}
				break;

			case '\r':
			{
				int y;
				if(search_proc_to_idx(&y, procs))
					position(y);

				/* fall */

			case CTRL_AND('['):
				search = search_offset = 0;
				break;
			}

			case CTRL_AND('d'):
				on_curproc("delete", delete, 0, procs);
				break;

			case LOCK_CHAR:
lock_proc:
				do_lock = 1;
				break;

			case SEARCH_NEXT_CHAR:
        search_offset++;
        break;
			case SEARCH_PREVIOUS_CHAR:
				if(search_offset > 0)
					search_offset--;
				break;

backspace:
				if(search_idx > 0)
					search_str[--search_idx] = '\0';
				else
					search = 0;
				break;

			case RESET_SEARCH_CHAR:
				search_idx = 0;
				*search_str = '\0';
				break;
		}

		if(search && *search_str)
			search_proc = proc_find_n(search_str, procs, search_offset);
		else
			search_proc = NULL;
	}

	if(search && search_pid && *search_str){
		int pid;
		sscanf(search_str, "%d", &pid);
		search_proc = proc_get(procs, pid);
	}

	if(do_lock)
		lock_to(search_proc);
}

void gui_run(struct proc **procs)
{
	struct procstat pst;
	long last_update = 0, last_full_refresh;
	int fin = 0;

	memset(&pst, 0, sizeof pst);

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
			position(pst.count - 1);

		showprocs(procs, &pst);

		ch = getch();
		if(ch == -1)
			continue;

		if(search){
			gui_search(ch, procs);
		}else{
			switch(ch){
				case QUIT_CHAR:
					fin = 1;
					break;

				case UP_CHAR:
					if(pos_y > 0)
						position(pos_y - 1);
					break;
				case DOWN_CHAR:
					position(pos_y + 1);
					break;

				case SCROLL_TO_TOP_CHAR:
					position(0);
					break;
				case SCROLL_TO_BOTTOM_CHAR:
					position(pst.count);
					break;

				case BACKWARD_HALF_WINDOW_CHAR:
					position(pos_y - LINES / 2);
					break;
				case FORWARD_HALF_WINDOW_CHAR:
					position(pos_y + LINES / 2);
					break;
				case BACKWARD_WINDOW_CHAR:
					position(pos_y - LINES);
					break;
				case FORWARD_WINDOW_CHAR:
					position(pos_y + LINES);
					break;

				case EXPOSE_ONE_MORE_LINE_BOTTOM_CHAR:
					if(pos_top < pst.count - 1){
						pos_top++;
						if(pos_y < pos_top)
							pos_y = pos_top;
					}
					break;
				case EXPOSE_ONE_MORE_LINE_TOP_CHAR:
					if(pos_top > 0){
						pos_top--;
						if(pos_y > pos_top + LINES - 2)
							pos_y = pos_top + LINES - 2;
					}
					break;

				case SCROLL_TO_LAST_CHAR:
					position(pos_top + LINES - 2);
					break;
				case SCROLL_TO_FIRST_CHAR:
					position(pos_top);
					break;
				case SCROLL_TO_MIDDLE_CHAR:
					position(pos_top + (pst.count > LINES ? LINES : pst.count) / 2);
					break;

				case INFO_CHAR:
					on_curproc("info", info, 0, procs);
					break;
				case KILL_CHAR:
					on_curproc("delete", delete, 0, procs);
					break;
				case LSOF_CHAR:
					on_curproc("lsof", lsof, 1, procs);
					break;
				case STRACE_CHAR:
					on_curproc("strace", strace, 1, procs);
					break;

				case GOTO_LOCKED_CHAR:
					goto_lock(procs);
					break;
				case GOTO_$$_CHAR:
					/* goto $$ */
					goto_me(procs);
					break;

				case 'z':
					ch = getch();
					switch(ch){
						case 't':
							pos_top = pos_y;
							break;

						case 'b':
							pos_top = pos_y - LINES + 2;
							break;

						case 'z':
							pos_top = pos_y - LINES / 2 - 1;
							break;

					}
					if(pos_top < 0)
						pos_top = 0;
					break;


				case REDRAW_CHAR:
					/* redraw */
					clear();
					break;

				case LOCK_CHAR:
					lock_to(curproc(procs));
					break;


				case SEARCH_BACKWARD:
				case SEARCH_FORWARD:
					*search_str = '\0';
					search_idx = 0;
					search_pid = ch == '?';
					search = 1;
					move(0, 0);
					clrtoeol();
					break;
			}
		}
	}while(!fin);
}
