#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

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

void proc_free(struct myproc *p)
{
	free(p->unam);
	free(p->gnam);

	free(p->shell_cmd);
	free(p->argv0_basename);
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

  last = procs[p->pid % HASH_TABLE_SIZE];
  if(last){
    while(last->hash_next)
      last = last->hash_next;
    last->hash_next = p;
		p->hash_next = NULL;
  }else{
    procs[p->pid % HASH_TABLE_SIZE] = p;
  }
}

// initialize hash table
struct myproc **proc_init()
{
  return umalloc(HASH_TABLE_SIZE * sizeof *proc_init());
}

const char *proc_state_str(struct myproc *p)
{
	return (const char *[]){"R", "S", "?"}[p->state];
}

void proc_handle_rename(struct myproc *this, struct myproc **procs)
{
  if(machine_proc_exists(this)){
    char *cmd;
		int cmd_len;

		machine_update_proc(this, procs);

    /* recreate this->shell_cmd from argv */
		for(int i = cmd_len = 0; i < this->argc; i++)
			cmd_len += strlen(this->argv[i]) + 1;

    free(this->shell_cmd);
		this->shell_cmd = umalloc(cmd_len + 1);

		cmd = this->shell_cmd;
		for(int i = 0; i < this->argc; i++)
			cmd += sprintf(cmd, "%s ", this->argv[i]);
	}
}

static void proc_update_single(struct myproc *proc, struct myproc **procs)
{
  const pid_t oldppid = proc->ppid;

  if(machine_proc_exists(proc)){
    if(oldppid != proc->ppid){
      struct myproc *parent = proc_get(procs, proc->ppid);
      struct myproc *iter;

      if (parent) {
        if(parent->child_first){
          iter = parent->child_first;

          while(iter->child_next)
            iter = iter->child_next;

          iter->child_next = proc;
        }else{
          parent->child_first = proc;
        }
      }
    }
  }else{
    proc_free(proc);
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

void proc_update(struct myproc **procs)
{
  int i;

  for(i = 0; i < HASH_TABLE_SIZE; i++){
    struct myproc **change_me;
    struct myproc *p;

    change_me = &procs[i];

    for(p = procs[i]; p; ){
			if(p){
				if(!machine_proc_exists(p)){
					/* dead */
					struct myproc *next = p->hash_next;
					struct myproc *parent = proc_get(procs, p->ppid);

					if(parent){
						struct myproc *iter, **prev;

						prev = &parent->child_first;
						for(iter = parent->child_first; iter; iter = iter->child_next)
							if(iter->pid == p->pid){
								*prev = iter->child_next;
								break;
							}else{
								prev = &iter->child_next;
							}
					}

					*change_me = next;
					proc_free(p);
					p = next;
				}else{
					proc_update_single(p, procs);

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

struct myproc *proc_find_n(const char *str, struct myproc **ps, int n)
{
	struct myproc *p;
	int i;

	if(str) {
		ITER_PROCS(i, p, ps)
			if(p->shell_cmd && strstr(p->shell_cmd, str) && n-- <= 0)
				return p;
	}

	return NULL;
}

int proc_to_idx(struct myproc *p, struct myproc *parent, int *py)
{
  struct myproc *iter;
  int ret = 0;
  int y;

  if(p == parent)
    return 1;

  y = *py;
  for(iter = parent->child_first; iter; iter = iter->child_next)
    if(p == iter || (++y, proc_to_idx(p, iter, &y))){
      ret = 1;
      break;
    }

  *py = y;
  return ret;
}

struct myproc *proc_from_idx(struct myproc *parent, int *idx)
{
  struct myproc *iter, *ret = NULL;
  int i = *idx;
#define RET(x) do{ ret = x; goto fin; }while(0)

  if(i <= 0)
    return parent;

  for(iter = parent->child_first; iter; iter = iter->child_next){
    if(--i <= 0){
      RET(iter);
    }else{
      struct myproc *p = proc_from_idx(iter, &i);
      if(p)
        RET(p);
    }
  }

fin:
  *idx = i;
  return ret;
}
