#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#include "structs.h"
#include "proc.h"
#include "util.h"
#include "main.h"
#include "machine.h"

#define PROC_IS_KERNEL(p) ((p)->ppid == 0 || (p)->ppid == 2)

#define ITER_PROCS(i, p, ps)                    \
	for(i = 0; i < HASH_TABLE_SIZE; i++)          \
		for(p = ps[i]; p; p = p->hash_next)

/* Processes are saved into a hash table with size HASH_TABLE_SIZE and
 * key pid % HASH_TABLE_SIZE. If the key already exists, the new
 * element is appended to the last one throught hash_next:

 [     0] = { NULL },
 [     1] = { { "vim", 4321 }, NULL },
 [     2] = { { "sh",  9821 }, { "xterm", 9801 }, NULL }
 ...
 [NPROCS] = { NULL },

*/

static void proc_add_child(struct myproc *parent, struct myproc *child)
{
	size_t n = 0;
	struct myproc **i;

	for(i = parent->children; i && *i; i++, n++);

	n++; /* new */

	parent->children = urealloc(parent->children, (n + 1) * sizeof *parent->children);
	parent->children[n - 1] = child;
	parent->children[n]     = NULL;
}

static void proc_rm_child(struct myproc *parent, struct myproc *p)
{
	size_t n, idx, found;
	struct myproc **pi;

	n = idx = found = 0;

	for(pi = parent->children; pi && *pi; pi++, n++)
		if(*pi == p)
			found = 1;
		else if(!found)
			idx++;

	if(!found)
		return;

	if(n <= 1){
		free(parent->children);
		parent->children = NULL;
	}else{
		n--; /* gone */

		/*memmove(parent->children + idx,
		 parent->children + idx + 1,
		 n * sizeof *parent->children);*/
		size_t i;
		for(i = idx; i < n; i++)
			parent->children[i] = parent->children[i + 1];

		parent->children = urealloc(parent->children, (n + 1) * sizeof *parent->children);
		parent->children[n] = NULL;
	}
}

static void proc_free(struct myproc *p, struct myproc **procs)
{
	struct myproc *i = proc_get(procs, p->ppid);

	/* remove parent references */
	if(i)
		proc_rm_child(i, p);

	/* remove hash-table references */

	i = procs[p->pid % HASH_TABLE_SIZE];
	if(i == p){
		procs[p->pid % HASH_TABLE_SIZE] = p->hash_next;
	}else{
		while(i && i->hash_next != p)
			i = i->hash_next;
		if(i)
			/* i->hash_next is p, reroute */
			i->hash_next = p->hash_next;
	}

	free(p->unam);
	free(p->gnam);

	free(p->shell_cmd);
	/*free(p->argv0_basename); - do not free*/
	argv_free(p->argc, p->argv);

	free(p->tty);

	free(p);
}

struct myproc *proc_get(struct myproc **procs, pid_t pid)
{
	struct myproc *p;

	if(pid >= 0)
		for(p = procs[pid % HASH_TABLE_SIZE]; p; p = p->hash_next)
			if(p->pid == pid)
				return p;

	return NULL;
}

// Add a struct myproc pointer to the hash table
void proc_addto(struct myproc **procs, struct myproc *p)
{
	struct myproc *last = procs[p->pid % HASH_TABLE_SIZE];

	if(last){
		while(last->hash_next)
			last = last->hash_next;
		last->hash_next = p;
	}else{
		procs[p->pid % HASH_TABLE_SIZE] = p;
	}
	p->hash_next = NULL;

	struct myproc *parent = proc_get(procs, p->ppid);
	if(parent)
		proc_add_child(parent, p);
}

// initialize hash table
struct myproc **proc_init()
{
	return umalloc(HASH_TABLE_SIZE * sizeof *proc_init());
}

const char *proc_state_str(struct myproc *p)
{
	return (const char *[]){
		"R",
			"S",
			"D",
			"T",
			"Z",
			"X",
			"t",
			"?",
	}[p->state];
}

void proc_create_shell_cmd(struct myproc *this)
{
	char *cmd;
	size_t cmd_len;

	/* recreate this->shell_cmd from argv */
	for(size_t i = cmd_len = 0; i < this->argc; i++)
		cmd_len += strlen(this->argv[i]) + 1;

	free(this->shell_cmd);
	this->shell_cmd = umalloc(cmd_len + 1);

	cmd = this->shell_cmd;
	for(size_t i = 0; i < this->argc; i++)
		cmd += sprintf(cmd, "%s ", this->argv[i]);
}

static void proc_update_single(
		struct myproc *proc,
		struct myproc **procs,
		struct sysinfo *info)
{
	if(machine_proc_exists(proc)){
		const pid_t oldppid = proc->ppid;

		machine_update_proc(proc);

		info->count++;

		if(PROC_IS_KERNEL(proc))
			info->count_kernel++;

		if(proc->uid == globals.uid)
			info->owned++;
		info->procs_in_state[proc->state]++;

		if(oldppid != proc->ppid){
			struct myproc *parent = proc_get(procs, proc->ppid);

			if(parent){
				proc_add_child(parent, proc);
			}else{
				/* TODO: reparent to init? */
			}
		}

		proc_create_shell_cmd(proc);
	}else{
		proc_free(proc, procs);
	}
}

enum proc_state proc_state_parse(char c)
{
	switch(c){
		case 'R': return PROC_STATE_RUN;
		case 'I': /* very sleepy - "idle" */
		case 'S': return PROC_STATE_SLEEP;
		case 'U': /* uninterruptible wait */
		case 'D': return PROC_STATE_DISK;
		case 'T': return PROC_STATE_STOPPED;
		case 'Z': return PROC_STATE_ZOMBIE;
		case 'X': return PROC_STATE_DEAD;
		case 't': return PROC_STATE_TRACE;

		default:
							return PROC_STATE_OTHER;
	}
}

const char *proc_str(struct myproc *p)
{
	static char buf[256];

	snprintf(buf, sizeof buf,
			"{ pid=%d, ppid=%d, state=%d, cmd=\"%s\"}",
			p->pid, p->ppid, (int)p->state, p->shell_cmd);

	return buf;
}

void proc_update(struct myproc **procs, struct sysinfo *info)
{
	int i;

	info->count = info->count_kernel = info->owned = 0;
	memset(info->procs_in_state, 0, sizeof info->procs_in_state);

	for(i = 0; i < HASH_TABLE_SIZE; i++){
		struct myproc **change_me;
		struct myproc *p;

		change_me = &procs[i];

		for(p = procs[i]; p; ){
			if(p){
				if(!machine_proc_exists(p)){
					/* dead */
					struct myproc *next = p->hash_next;
					*change_me = next;
					proc_free(p, procs);
					p = next;
				}else{
					proc_update_single(p, procs, info);

					change_me = &p->hash_next;
					p = p->hash_next;
				}
			}
		}
	}

	machine_proc_get_more(procs);
}

void proc_dump(struct myproc **ps, FILE *f)
{
	struct myproc *p;
	int i;

	ITER_PROCS(i, p, ps)
		fprintf(f, "%s\n", proc_str(p));
}

struct myproc *proc_find(const char *str, struct myproc **ps)
{
	return proc_find_n(str, ps, 0);
}

static int proc_find_match(const char *shell_cmd, const char *search_str)
{
	/* TODO: regex */
	return !!ustrcasestr(shell_cmd, search_str);
}

static struct myproc *proc_find_n_child(const char *str, struct myproc *proc, int *n)
{
	struct myproc **i;

	if(proc->shell_cmd && proc_find_match(proc->shell_cmd, str) && --*n < 0)
		return proc;

	for(i = proc->children; i && *i; i++){
		struct myproc *p = *i;

		if((p = proc_find_n_child(str, p, n)))
			return p;
	}

	return NULL;
}

struct myproc *proc_find_n(const char *str, struct myproc **ps, int n)
{
#ifdef HASH_TABLE_ORDER
	struct myproc *p;
	int i;

	if(str)
		ITER_PROCS(i, p, ps)
			if(p->shell_cmd && proc_find_match(p->shell_cmd, str) && n-- <= 0)
				return p;

	return NULL;
#else
	/* search in the same order the procs are displayed */
	// TODO: multiple parents
	return proc_find_n_child(str, proc_first(ps), &n);
#endif
}

static int proc_to_idx_nested(
		struct myproc *head, struct myproc *const searchee,
		int *py)
{
	if(head == searchee)
		return 1;

	++*py;

	for(struct myproc **iter = head->children;
			iter && *iter;
			iter++)
	{
		if(searchee == *iter)
			return 1;

		if(proc_to_idx_nested(*iter, searchee, py))
			return 1;
	}

	return 0;
}

int proc_to_idx(struct myproc **procs, struct myproc *searchee, int *py)
{
	ITER_PROC_HEADS(struct myproc *, head, procs)
		if(proc_to_idx_nested(head, searchee, py))
			return 1;

	return 0;
}

static struct myproc *proc_from_idx_nested(struct myproc *head, int *idx)
{
	if(*idx == 0)
		return head;

	for(struct myproc **iter = head->children;
			iter && *iter;
			iter++)
	{
		if(--*idx <= 0){
			return *iter;
		}else{
			struct myproc *sub = proc_from_idx_nested(*iter, idx);
			if(sub)
				return sub;
		}
	}

	return NULL;
}

struct myproc *proc_from_idx(struct myproc **procs, int *idx)
{
	// TODO: kernel threads

	if(*idx < 0)
		return NULL;

	ITER_PROC_HEADS(struct myproc *, head, procs){
		struct myproc *test = proc_from_idx_nested(head, idx);

		if(test)
			return test;
	}

	return NULL;

#undef RET
}

struct myproc *proc_first(struct myproc **procs)
{
	struct myproc *p = proc_get(procs, 1); /* init */
	int i;

	if(p)
		return p;

	/* the most parent-y */
	ITER_PROCS(i, p, procs)
		if(p->ppid == 0)
			return p;

	ITER_PROCS(i, p, procs)
		if(p->ppid == 1)
			return p;

	ITER_PROCS(i, p, procs)
		if(!proc_get(procs, p->ppid))
			return p;

	return NULL;
}

struct myproc *proc_first_next(struct myproc **procs)
{
	struct myproc *p;
	int i;

	ITER_PROCS(i, p, procs)
		if(!p->mark && !proc_get(procs, p->ppid))
			return p;

	ITER_PROCS(i, p, procs)
		if(!p->mark)
			return p;

	return NULL;
}

void proc_unmark(struct myproc **procs)
{
	struct myproc *p;
	int i;

	ITER_PROCS(i, p, procs)
		/* unmark everything except those whose ppids we don't have yet */
		p->mark = p->ppid == -1;
}

void proc_mark_kernel(struct myproc **procs)
{
	struct myproc *p;
	int i;

	ITER_PROCS(i, p, procs)
		if(PROC_IS_KERNEL(p))
			p->mark = 1;
}
