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

// start with 1, 0 is the dummy node
#define ITER_PROCS(i, p, ps)                    \
  for(i = 1; i < HASH_TABLE_SIZE; i++)          \
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
	int n = 0;
	struct myproc **i;

	for(i = parent->children; i && *i; i++, n++);

	n++; /* new */

	parent->children = urealloc(parent->children, (n + 1) * sizeof *parent->children);
	parent->children[n - 1] = child;
	parent->children[n]     = NULL;
}

static void proc_rm_child(struct myproc *parent, struct myproc *p)
{
	int n, idx, found;
	struct myproc **i;

	n = idx = found = 0;

	for(i = parent->children; i && *i; i++, n++)
		if(*i == p)
			found = 1;
		else if(!found)
			idx++;

	if(!found)
		return;

	n--; /* gone */
	if(n <= 0){
		free(parent->children);
		parent->children = NULL;
	}else{
		/*memmove(parent->children + idx,
		 parent->children + idx + 1,
		 n * sizeof *parent->children);*/
		int i;
		for(i = idx; i < n; i++)
			parent->children[i] = parent->children[i + 1];

		parent->children = urealloc(parent->children, (n + 1) * sizeof *parent->children);
		parent->children[n] = NULL;
	}
}

void proc_free(struct myproc *p, struct myproc **procs)
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
	for(char **iter = p->argv; iter && *iter; iter++)
		free(*iter);
	free(p->argv);

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
  struct myproc *last;
#define parent last

  last = procs[p->pid % HASH_TABLE_SIZE];
  if(last){
    while(last->hash_next)
      last = last->hash_next;
    last->hash_next = p;
		p->hash_next = NULL;
  }else{
    procs[p->pid % HASH_TABLE_SIZE] = p;
  }

	parent = proc_get(procs, p->ppid);
	if(parent)
		proc_add_child(parent, p);

#undef parent
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
		"?",
	}[p->state];
}

void proc_create_shell_cmd(struct myproc *this)
{
	char *cmd;
	int cmd_len;

	/* recreate this->shell_cmd from argv */
	for(int i = cmd_len = 0; i < this->argc; i++)
		cmd_len += strlen(this->argv[i]) + 1;

	free(this->shell_cmd);
	this->shell_cmd = umalloc(cmd_len + 1);

	cmd = this->shell_cmd;
	for(int i = 0; i < this->argc; i++)
		cmd += sprintf(cmd, "%s ", this->argv[i]);
}

void proc_handle_rename(struct myproc *this, struct myproc **procs)
{
	if(machine_proc_exists(this)){
		machine_update_proc(this, procs);

		proc_create_shell_cmd(this);
	}
}

static void proc_update_single(
		struct myproc *proc,
		struct myproc **procs,
		struct sysinfo *info)
{
  if(machine_proc_exists(proc)){
		const pid_t oldppid = proc->ppid;

		machine_update_proc(proc, procs);

		info->count++;
		if(proc->uid == global_uid)
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
  }else{
    proc_free(proc, procs);
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

	info->count = info->owned = 0;
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

void proc_handle_renames(struct myproc **ps)
{
  struct myproc *p;
  int i;

  ITER_PROCS(i, p, ps)
      proc_handle_rename(p, ps);
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

struct myproc *proc_find_n_child(const char *str, struct myproc *proc, int *n)
{
	struct myproc **i;

	if(proc->shell_cmd && strstr(proc->shell_cmd, str) && --*n < 0)
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
			if(p->shell_cmd && strstr(p->shell_cmd, str) && n-- <= 0)
				return p;

	return NULL;
#else
	/* search in the same order the procs are displayed */
	return proc_find_n_child(str, proc_first(ps), &n);
#endif
}

int proc_to_idx(struct myproc *p, struct myproc *parent, int *py)
{
  struct myproc **iter;
  int ret = 0;
  int y;

  if(p == parent)
    return 1;

  y = *py;
  for(iter = parent->children; iter && *iter; iter++)
    if(p == *iter || (++y, proc_to_idx(p, *iter, &y))){
      ret = 1;
      break;
    }

  *py = y;
  return ret;
}

struct myproc *proc_from_idx(struct myproc *parent, int *idx)
{
  struct myproc **iter, *ret = NULL;
  int i = *idx;
#define RET(x) do{ ret = x; goto fin; }while(0)

  if(i <= 0)
    return parent;

  for(iter = parent->children; iter && *iter; iter++){
    if(--i <= 0){
      RET(*iter);
    }else{
      struct myproc *p = proc_from_idx(*iter, &i);
      if(p)
        RET(p);
    }
  }

fin:
  *idx = i;
  return ret;

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
		p->mark = 0;
}
