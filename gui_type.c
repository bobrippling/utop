#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <unistd.h>

#include <ncurses.h>

#include "proc_state.h"
#include "sysinfo.h"
#include "proc.h"

#include "main.h"
#include "util.h"

#include "gui_type.h"

#define X_TYPES      \
  X_TYPE(ty_running) \
  X_TYPE(ty_zombie)  \
  X_TYPE(ty_owned)   \
  X_TYPE(ty_kern)    \
  X_TYPE(ty_other)

enum proc_type
{
#define X_TYPE(x) x,
	X_TYPES
#undef X_TYPE
};
#define N_TYPES (ty_other + 1)

const char *const type_names[] = {
#define X_TYPE(x) #x + 3, /* +3 to skip "ty_" */
	X_TYPES
#undef X_TYPE
};

struct proc_list
{
	struct myproc **procs;
	size_t n;
};

static void proc_list_add(struct proc_list *list, struct myproc *p)
{
	list->procs = urealloc(list->procs, (++list->n + 1) * sizeof *list->procs);
	list->procs[list->n - 1] = p;
	list->procs[list->n] = NULL;
}

static void proc_list_free(struct proc_list *list)
{
	free(list->procs);
}

static void add_proc_type_r(struct myproc *p, struct proc_list type_map[])
{
	int idx = -1;

	p->mark = 1;

	switch(p->state){
		case PROC_STATE_RUN:
			idx = ty_running;
			break;

		case PROC_STATE_ZOMBIE:
			idx = ty_zombie;
			break;

		default:
			if(PROC_IS_KERNEL(p))
				idx = ty_kern;
			else if(PROC_IS_OWNED(p))
				idx = ty_owned;
			else
				idx = ty_other;
	}

	proc_list_add(&type_map[idx], p);

	for(struct myproc **i = p->children; i && *i; i++)
		add_proc_type_r(*i, type_map);
}

static void move_clear_printw(
		const int y, const int top,
		const char *fmt, ...)
{
	if(y < top)
		return;

	move(y, 0);
	clrtoeol();

	va_list l; va_start(l, fmt);
	vwprintw(stdscr, fmt, l);
	va_end(l);
}


void show_proc_type(
		struct myproc **heads, struct sysinfo *info,
		int *py, int const top)
{
	move(*py, 0);
	clrtobot();

	if(!info->count)
		return;

	if(*py >= LINES)
		return;

	struct proc_list *type_map = umalloc(N_TYPES * sizeof *type_map);

	ITER_PROC_HEADS(struct myproc *, p, heads)
		add_proc_type_r(p, type_map);

	/* have the types, show */
	for(enum proc_type i = 0; i < N_TYPES; i++){

		move_clear_printw((*py)++, top, "--- %s ---", type_names[i]);

		for(struct myproc **it = type_map[i].procs; it && *it; it++){
			struct myproc *p = *it;

			if(*py == LINES - top)
				break;

			move_clear_printw((*py)++, top, "% 7d %s", p->pid, p->shell_cmd);
		}

		/* cleanup #1 */
		proc_list_free(type_map + i);
	}

	/* cleanup #2 */
	free(type_map);
}
