#include <ncurses.h>
#include <sys/types.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <errno.h>

#include "proc.h"
#include "gui.h"
#include "util.h"

#define HALF_DELAY_TIME 5
#define WAIT_TIME (HALF_DELAY_TIME * 100)
#define CTRL_AND(c) ((c) & 037)
#define INDENT "    "
#define STATUS(y, x, ...) do{ mvprintw(y, x, __VA_ARGS__); clrtoeol(); }while(0)

static int pos_top = 0, pos_y = 0;


static int  search = 0;
static int  search_idx = 0;
static char search_str[32] = { 0 };
static struct proc *search_proc = NULL;
static int uid;

void getch_delay(int on)
{
	if(on)
		halfdelay(HALF_DELAY_TIME); /* x/10s of a second wait */
	else
		cbreak();
}

void gui_init()
{
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

void gui_term()
{
	endwin();
}

int search_proc_offset(int *found, struct proc **procs)
{
	return proc_offset(search_proc, proc_get(procs, 1), found);
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
		const int owned = proc->uid == uid;
		char buf[256];
		int len = LINES;

		move(y, 0);

		len -= snprintf(buf, sizeof buf,
				"% 7d %c %-8s %-8s",
				proc->pid, proc->state,
				proc->unam, proc->gnam);
		addstr(buf);

		i = indent;
		while(i -->= 0){
			addstr(INDENT);
			len -= 2;
		}

		i = getcurx(stdscr) + proc->basename_offset;

#define ATTR_NOT_OWNED A_BOLD | COLOR_PAIR(1 + COLOR_BLACK)
#define ATTR_SEARCH    A_BOLD | COLOR_PAIR(1 + COLOR_RED)
#define ATTR_BASENAME  A_BOLD | COLOR_PAIR(1 + COLOR_CYAN)

		if(proc == search_proc)
			attron(ATTR_SEARCH);
		else if(!owned)
			attron(ATTR_NOT_OWNED);

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

	if(search){
		mvprintw(0, 0, "/%s", search_str);

		if(search_proc){
			int ty = 1;
			if(search_proc_offset(&ty, procs)){
				pos_top = ty - LINES / 2;
				if(pos_top < 0)
					pos_top = 0;
			}
		}

	}else{
		STATUS(0, 0, "%d processes, %d running", pst->count, pst->running);
	}

	showproc(proc_get(procs, 1), &y, 0);

	clrtobot();

	if(search){
		move(0, strlen(search_str) + 1);
	}else{
		move(1 + pos_y - pos_top, 0);
		mvchgat(1 + pos_y - pos_top, 0, -1, A_UNDERLINE, /* color pair index */ 0, NULL);
	}
}

void gui_search(int ch, struct proc **procs)
{
	switch(ch){
		case '\r':
		{
			int y = 1;
			if(search_proc_offset(&y, procs))
				position(y, 0);
		} /* fall */
		case CTRL_AND('['):
			search = 0;
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
		search_proc = proc_find(search_str, procs);
	else
		search_proc = NULL;
}

struct proc *curproc(struct proc **procs)
{
	int i = pos_y;
	return proc_from_idx(proc_get(procs, 1), &i);
}

void gui_run(struct proc **procs)
{
	struct procstat pst;
	long last_update = 0;
	int fin = 0;

	uid = getuid();
	proc_update(procs, &pst);

	do{
		const long now = mstime();
		int ch;

		if(last_update + WAIT_TIME < now){
			last_update = now;
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

				case 'L':
					position(pos_top + LINES - 2, 0);
					break;
				case 'H':
					position(pos_top, 0);
					break;
				case 'M':
					position(pos_top + LINES / 2, 0);
					break;

				case 'd':
				{
					struct proc *p = curproc(procs);
					if(p){
						char sig[8];
						int i, wait = 0;

						STATUS(0, 0, "kill %d (%s) with: ", p->pid, p->basename);
						echo();
						getnstr(sig, sizeof sig);
						noecho();

						if(!*sig)
							break;

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

						if(wait){
							getch_delay(0);
							attron( COLOR_PAIR(1 + COLOR_RED));
							STATUS(1, 0, "any key to continue");
							attroff(COLOR_PAIR(1 + COLOR_RED));
							getch();
							getch_delay(1);
						}
					}
					break;
				}

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
