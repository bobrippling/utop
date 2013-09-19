enum
{
	ty_running,
	ty_zombie,
	ty_owned,
	ty_kern,
	ty_other
};
#define N_TYPES (ty_other + 1)

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

static void show_proc_type(struct myproc **heads, struct sysinfo *info, int *py)
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
	for(int i = 0; i < N_TYPES; i++){
		for(struct myproc **it = type_map[i].procs; it && *it; it++){
			struct myproc *p = *it;

			move((*py)++, 0);
			clrtoeol();
			printw("% 7d %s", p->pid, p->argv0_basename);
		}

		/* cleanup #1 */
		proc_list_free(type_map + i);
	}

	/* cleanup #2 */
	free(type_map);
}
