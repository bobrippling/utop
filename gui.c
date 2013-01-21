// enable ncurses printw() type checks
#define GCC_PRINTF 1
#define GCC_SCANF  1

#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/types.h>

#include "structs.h"

#include "proc.h"
#include "gui.h"
#include "util.h"
#include "config.h"
#include "main.h"
#include "machine.h"
#include "util.h"

#define TOP_OFFSET 3
#define DRAW_SPACE (LINES - TOP_OFFSET - 1)

#define STATUS(y, x, ...) do{ mvprintw(y, x, __VA_ARGS__); clrtoeol(); }while(0)
#define WAIT_STATUS(...) do{ STATUS(0, 0, __VA_ARGS__); ungetch(getch()); }while(0)

#define SEARCH_ON(on) do{ gui_text_entry(on); search = on; *search_str = '\0'; }while(0)

static int pos_top = 0, pos_y = 0;

static int  search = 0;
static int  search_idx = 0, search_offset = 0, search_pid = 0;
static char search_str[32] = { 0 };
static struct myproc *search_proc = NULL;

static int frozen = 0;

static pid_t lock_proc_pid = -1;

void getch_delay(int on)
{
	if(on)
		halfdelay(HALF_DELAY_TIME); /* x/10s of a second wait */
	else
		cbreak();
}

void gui_text_entry(int on)
{
	if(on)
		echo();
	else
		noecho();
	curs_set(on);
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

#ifndef OLD_CURSOR
		gui_text_entry(0);
#endif

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

int search_proc_to_idx(int *y, struct myproc **procs)
{
	struct myproc *init = proc_first(procs);
	if(search_proc == init){
		*y = 0;
		return 1;
	}
	*y = 1;
	return proc_to_idx(search_proc, init, y);
}

struct myproc *curproc(struct myproc **procs)
{
	int i = pos_y;
	return proc_from_idx(proc_first(procs), &i);
}

void position(int newy)
{
	pos_y = newy;

	if(pos_y < 0)
		pos_y = 0;

	if(pos_y > pos_top + DRAW_SPACE)
		pos_top = pos_y - DRAW_SPACE;

	if(pos_y < pos_top)
		pos_top = pos_y;
}

void goto_proc(struct myproc **procs, struct myproc *p)
{
	int y = TOP_OFFSET;
	proc_to_idx(p, proc_first(procs), &y);
	position(y);
}

void goto_me(struct myproc **procs)
{
	goto_proc(procs, proc_get(procs, getpid()));
}

void goto_lock(struct myproc **procs)
{
	if(lock_proc_pid == -1){
		attron( COLOR_PAIR(1 + COLOR_RED));
		WAIT_STATUS("no process locked on");
		attroff(COLOR_PAIR(1 + COLOR_RED));
	}else{
		goto_proc(procs, proc_get(procs, lock_proc_pid));
	}
}

void showproc(struct myproc *proc, int *py, int indent)
{
	int y = *py;

	proc->mark = 1;

	if(y >= LINES)
		return;

	if(y >= TOP_OFFSET){ // otherwise we're iterating over a process that's above pos_top
		const int is_owned         = proc->uid == global_uid;
		const int is_locked        = proc->pid == lock_proc_pid;
		const int is_searched      = proc      == search_proc;
		const int is_searched_alt  = *search_str && strstr(proc->shell_cmd, search_str);
		int x;

		move(y, 0);

		if(is_searched)
			attron(ATTR_SEARCH);
		else if(is_locked)
			attron(ATTR_LOCK);
		else if(is_searched_alt)
			attron(ATTR_SEARCH_ALT);
		else if(!is_owned)
			attron(ATTR_NOT_OWNED);

		addstr(machine_proc_display_line(proc));

		getyx(stdscr, y, x);
#if 0
		if(proc->state == PROC_STATE_RUN){
			mvchgat(y, 14, 7, 0, CFG_COLOR_RUNNING + 1, NULL);
			move(y, x);
		}
#endif
		clrtoeol();

		/* position for process name */
		x += INDENT + 1;
		for(int indent_copy = indent; indent_copy > 0; indent_copy--)
			x += INDENT;

		mvprintw(y, x, "%s", proc->shell_cmd);

		/* basename shading */
		if(is_owned && !is_locked && !is_searched && !is_searched_alt){
      if(proc->argv){
        x += proc->argv0_basename - proc->argv[0];
        attron(ATTR_BASENAME);
        mvaddstr(y, x, proc->argv0_basename);
        attroff(ATTR_BASENAME);
      }
    }

		if(is_searched)
			attroff(ATTR_SEARCH);
		else if(is_locked)
			attroff(ATTR_LOCK);
		else if(is_searched_alt)
			attroff(ATTR_SEARCH_ALT);
		else if(is_owned)
			attroff(ATTR_BASENAME);
		else
			attroff(ATTR_NOT_OWNED);
	}

	// done with our proc, increment y
	y++;

	/*
	 * need to iterate over all children,
	 * since we may currently be on a process above the top
	 */
	for(struct myproc **iter = proc->children; iter && *iter; iter++)
		showproc(*iter, &y, indent + 1);

	*py = y;
}

void showprocs(struct myproc **procs, struct sysinfo *info)
{
	int y = TOP_OFFSET - pos_top;

	proc_unmark(procs);

	if(!global_kernel)
		proc_mark_kernel(procs);

	for(struct myproc *p = proc_first(procs); p; p = proc_first_next(procs))
		showproc(p, &y, 0);

	move(y, 0);
	clrtobot();

	if(search){
		const int red = !search_proc && *search_str;;

		move(1, 0); // TODO: search_proc info here
		clrtoeol();
		move(2, 0);
		clrtoeol();

		if(red)
			attron(COLOR_PAIR(1 + COLOR_RED));
		mvprintw(0, 0, "%d %c%s", search_offset, "/?"[search_pid], search_str);
		if(red)
			attroff(COLOR_PAIR(1 + COLOR_RED));
		clrtoeol();

	}else{
		int y, width;

		STATUS(0, 0, "%d processes, %d running, "
				"%d owned, %d kernel, %d zombies, "
				"load averages: %.2f, %.2f, %.2f, "
				"uptime: %s",
				info->count, info->procs_in_state[PROC_STATE_RUN],
				info->owned, info->count_kernel, info->procs_in_state[PROC_STATE_ZOMBIE],
				info->loadavg[0], info->loadavg[1], info->loadavg[2],
				uptime_from_boottime(info->boottime.tv_sec));

		STATUS(1, 0, "Mem: %s", machine_format_memory(info));
		STATUS(2, 0, "CPU: %s%s", machine_format_cpu_pct(info), frozen ? " [FROZEN]" : "");

		y = TOP_OFFSET + pos_y - pos_top;

		width = machine_proc_display_width();

#ifdef OLD_CURSOR
		mvchgat(y, 0, width, A_UNDERLINE, 0, NULL);
#else
		mvchgat(y, 0, COLS, A_UNDERLINE, 0, NULL);
#endif
		move(y, width);
	}
}

int waitgetch()
{
	int ret;
	gui_text_entry(1);
	getch_delay(0);
	ret = getch();
	getch_delay(1);
	gui_text_entry(0);
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

void delete(struct myproc *p, struct myproc **ps)
{
	char sig[8];
	int i, wait = 0;

	(void)ps;

	STATUS(0, 0, "kill %d (%s) with: ", p->pid, p->argv0_basename);
	gui_text_entry(1);
	getnstr(sig, sizeof sig);
	gui_text_entry(0);

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

void renice(struct myproc *p, struct myproc **ps)
{
	char increment[3]; // -20 to 20
	int i, wait = 0;

	(void)ps;

	STATUS(0, 0, "renice %d (%s) with [-20:20]: ", p->pid, p->argv0_basename);
	gui_text_entry(1);
	getnstr(increment, sizeof increment);
	gui_text_entry(0);

	if(!*increment)
		return;

	if(sscanf(increment, "%d", &i) != 1){
		STATUS(0, 0, "not a number");
		wait = 1;
	}else{
		if( (i < -20) || (i > 20) ){
			i = -1;
			STATUS(0, 0, "not a valid nice increment");
			wait = 1;
		}
	}

	if(!wait && i != -1)
		if((setpriority(PRIO_PROCESS, p->pid, i)) != 0){
			STATUS(0, 0, "renice: %s", strerror(errno));
			wait = 1;
		}

	if(wait)
		waitch(1, 0);
	getch_delay(1);
}

void external(const char *cmd)
{
	gui_term();

	system(cmd);
	fputs("return to continue...", stdout);
	fflush(stdout);
	getchar();

	gui_init();
}

void external2(const char *cmd, struct myproc *p)
{
	int len = strlen(cmd) + 16;
	char *buf = malloc(len);

	if(!buf)
		return;

	snprintf(buf, len, "%s -p %d", cmd, p->pid);

	external(buf);

	free(buf);
}

void strace(struct myproc *p, struct myproc **ps)
{
	(void)ps;
	external2(TRACE_TOOL, p);
}

void lsof(struct myproc *p, struct myproc **ps)
{
	(void)ps;
	external2("lsof", p);
}

void shell(struct myproc *p, struct myproc **ps)
{
	char buf[16];
	char *shell = getenv("SHELL");

	(void)ps;

	if(!shell)
		shell = "/bin/sh";

	snprintf(buf, sizeof buf, "%d", p->pid);

	setenv("UTOP_PID", buf, 1);

	external(shell);
}

void gdb(struct myproc *p, struct myproc **ps)
{
	// Attach to a running process: gdb <name> <pid>
	char *cmd;
	int len;

	(void)ps;

	len = strlen(p->argv0_basename) + 16;
	cmd = malloc(len);

	if(!cmd)
		return;

	snprintf(cmd, len, "gdb -- '%s' %d", p->argv0_basename, p->pid);

	external(cmd);

	free(cmd);
}

int try_external(int ch, struct myproc **procs)
{
	struct myproc *const cp = curproc(procs);
	int r = 0;

	if(cp)
		for(int i = 0; externals[i].handler; i++)
			if(externals[i].ch == ch)
				externals[i].handler(cp, procs), r = 1;

	return r;
}

void show_info(struct myproc *p, struct myproc **procs)
{
	int i;

	clear();
	mvprintw(2, 0,
					 "pid: %d, ppid: %d\n"
					 "uid: %d (%s), gid: %d (%s)\n"
#ifdef __FreeBSD__
					 "jid: %d\n"
#endif
					 "state: %s, nice: %d\n"
					 "CPU time: %s, MEM size: %s\n"
					 "tty: %s\n"
					 ,
					 p->pid, p->ppid,
					 p->uid, p->unam, p->gid, p->gnam,
#ifdef __FreeBSD__
					 p->jid,
#endif
					 proc_state_str(p), p->nice,
					 format_seconds(p->cputime), format_kbytes(p->memsize),
					 p->tty);

  if(p->argv)
    for(i = 0; p->argv[i]; i++)
      printw("argv[%d] = \"%s\"\n", i, p->argv[i]);

	for(;;){
		int ch;

		move(0, 0);
		printw("command (q to return): ");
		ch = waitgetch();

		if(ch == 'q')
			break;

		if(!try_external(ch, procs))
			mvprintw(1, 0, "unrecognised command '%c'", ch);
		else
			move(1, 0), clrtoeol();
	}
}

void on_curproc(const char *fstr, void (*f)(struct myproc *, struct myproc **), int ask, struct myproc **procs)
{
	extern int global_force;

	struct myproc *p;

	if(lock_proc_pid == -1 || !(p = proc_get(procs, lock_proc_pid))){
		p = search_proc ? search_proc : curproc(procs);
	}else{
		STATUS(0, 0, "using locked process %d, \"%s\", any key to continue", p->pid, p->argv0_basename);
		getch_delay(0);
		getch();
		getch_delay(1);
	}

	if(p){
		if(ask && !global_force && !confirm("%s: %d (%s)? (y/n) ", fstr, p->pid, p->argv0_basename))
			return;
		f(p, procs);
	}
}

void lock_to(struct myproc *p)
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

void gui_search(int ch, struct myproc **procs)
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

		search_offset = 0;
		SEARCH_ON(0);
	}else{
		switch(ch){
			default:
ins_char:
				if(isprint(ch) && search_idx < (signed)sizeof search_str - 2){
					search_str[search_idx++] = ch;
					search_str[search_idx	] = '\0';
				}
				break;

			case '\r':
			{
				int y;
				if(search_proc_to_idx(&y, procs))
					position(y);

				/* fall */

			case CTRL_AND('['):
				SEARCH_ON(0);
				search_offset = 0;
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
					SEARCH_ON(0);
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

	if(search_proc){
		int ty;
		if(search_proc_to_idx(&ty, procs)){
			// could use position(), but give a bit of context
			pos_top = ty - LINES / 2;
			if(pos_top < 0)
				pos_top = 0;
		}
	}

	if(do_lock)
		lock_to(search_proc);
}

void gui_run(struct myproc **procs)
{
	struct sysinfo info;
	long last_update = 0;
	int fin = 0;

	memset(&info, 0, sizeof info);
	machine_init(&info);
	proc_update(procs, &info);

	do{
		const long now = mstime();
		int ch;

		if(!frozen && last_update + WAIT_TIME < now){
			last_update = now;
			proc_update(procs, &info);
		}

		showprocs(procs, &info);

		machine_update(&info);

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
					position(info.count - (global_kernel ? 0 : info.count_kernel));
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
					if(pos_top < info.count - 1){
						pos_top++;
						if(pos_y < pos_top)
							pos_y = pos_top;
					}
					break;
				case EXPOSE_ONE_MORE_LINE_TOP_CHAR:
					if(pos_top > 0){
						pos_top--;
						if(pos_y > pos_top + DRAW_SPACE)
							pos_y = pos_top + DRAW_SPACE;
					}
					break;

				case SCROLL_TO_LAST_CHAR:
					position(pos_top + DRAW_SPACE);
					break;
				case SCROLL_TO_FIRST_CHAR:
					position(pos_top);
					break;
				case SCROLL_TO_MIDDLE_CHAR:
					position(pos_top + DRAW_SPACE / 2);
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
							pos_top = pos_y - DRAW_SPACE;
							break;

						case 'z':
							pos_top = pos_y - DRAW_SPACE / 2;
							break;

					}
					if(pos_top < 0)
						pos_top = 0;
					break;


				case REDRAW_CHAR:
					/* redraw */
					last_update = 0; /* force refresh */
					clear();
					break;

				case LOCK_CHAR:
					lock_to(curproc(procs));
					break;

				case FREEZE_CHAR:
					frozen ^= 1;
					break;

				case SEARCH_BACKWARD:
				case SEARCH_FORWARD:
					*search_str = '\0';
					search_idx = 0;
					search_pid = ch == '?';
					SEARCH_ON(1);
					move(0, 0);
					clrtoeol();
					break;

				case INFO_CHAR:
					on_curproc("info", show_info, 0, procs);
					break;

				default:
					try_external(ch, procs);
					break;
			}
		}
	}while(!fin);
}
