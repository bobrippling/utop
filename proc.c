#include <sys/param.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/user.h>
#include <sys/vmmeter.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
  char **iter;
  if (p->argv)
    for(iter = p->argv; *iter; iter++)
      free(*iter);
  free(p->basename);
  free(p->cmd);
  free(p->state_str);
  free(p->argv);
  free(p->unam);
  free(p->gnam);
  free(p->tty);
  free(p);
}

static void getprocstat(struct procstat *pst)
{
  // Implementation dependend code from machine.c:

  // Populate load averge
  get_load_average(pst);

  /* // get the memory usage */
  get_mem_usage(pst);

  // TODO: get CPU utilization
  get_cpu_stats(pst);
}

struct myproc *proc_get(struct myproc **procs, pid_t pid)
{
  struct myproc *p;

  /* if(pid >= 0) */
  for(p = procs[pid % HASH_TABLE_SIZE]; p; p = p->hash_next)
    if(p->pid == pid)
      return p;

  return NULL;
}

int proc_listcontains(struct myproc **procs, pid_t pid)
{
  return !!proc_get(procs, pid);
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
  }else{
    procs[p->pid % HASH_TABLE_SIZE] = p;
  }
}

// initialize hash table
struct myproc **proc_init()
{
  struct myproc **procs;
  struct myproc *root=NULL;

  procs = umalloc(HASH_TABLE_SIZE * sizeof *proc_init());

  // Add a dummy process with pid 0 and ppid -1 to the list:
  root = umalloc(sizeof(*root));

  root->pid = 0;
  root->ppid = -1;
  // Probably not needed anymore
  /* root->basename = ustrdup("{root}"); */
  /* root->argv = umalloc(2*sizeof(char*)); */
  /* root->argv[0] = ustrdup(root->basename); */
  /* root->argv[1] = NULL; */
  /* root->state = 0; */
  /* root->cmd = ustrdup(root->basename); */

  proc_addto(procs, root);

  return procs;
}

void proc_cleanup(struct myproc **procs)
{
  struct myproc *p;
  int i;

  if(procs) {
    proc_free(procs[0]); // root node
    ITER_PROCS(i, p, procs)
        proc_free(p);
  }
  machine_cleanup();
}

void proc_handle_rename(struct myproc *this)
{
  if(machine_proc_exists(this->pid) != NULL){

    char **iter, **argv;

    /* dup argv */
    int i, argc, slen;
    char *cmd, *pos;

    argv = machine_get_argv(this->pid);
    argc = slen = 0;

    if(argv) {
      /* free old argv */
      for(i = 0; i < this->argc; i++)
        free(this->argv[i]);
      free(this->argv);


      for(iter = argv; *iter; iter++)
        argc++;

      this->argv = umalloc((argc+1) * sizeof *this->argv); // + NULL terminator

      for(i = 0; i < argc; i++){
        slen += strlen(argv[i]) + 1 /* space */;
        this->argv[i] = ustrdup(argv[i]);
      }

      if((pos = strchr(this->argv[0], ':'))){
        /* sshd: ... */
        *pos = '\0';
        free(this->basename);
        this->basename = ustrdup(this->argv[0]);
        *pos = ':';
        this->basename_offset = 0;
      }else{
        pos = strrchr(this->argv[0], '/'); // locate the last '/' in argv[0], which is the full command path

        // malloc in just a second..
        if(!pos++)
          pos = this->argv[0];
        this->basename_offset = pos - this->argv[0];
        free(this->basename);
        this->basename = ustrdup(pos);

        if(*this->argv[0] == '-')
          this->basename_offset++; /* login shell, basename offset needs increasing */
      }

      this->argv[argc] = NULL;
      this->argc = argc;
    } else { // argv is empty like for init for example
      this->basename_offset = 0;
      this->argv = umalloc((argc+2)*sizeof(char*));
      this->argv[0] = ustrdup(this->basename);
      this->argv[1] = NULL;

      slen = strlen(this->basename);
      argc = 1;
    }

    /* recreate this->cmd from argv */
    free(this->cmd);

    if(this->flag & P_SYSTEM){ // it's a system process, render it nicely like top does
      cmd = this->cmd = umalloc(strlen(this->basename)+3); // [this->basename], [] + \0
      sprintf(this->cmd, "[%s]", this->basename);
    } else {
      cmd = this->cmd = umalloc(slen + 1);
      for(int i = 0; i < argc; i++)
        cmd += sprintf(cmd, "%s ", this->argv[i]);
    }

    if(global_debug){
      fprintf(stderr, "recreated argv for %s\n", this->basename);
      for(i = 0; this->argv[i]; i++)
        fprintf(stderr, "argv[%d] = %s\n", i, this->argv[i]);
    }
  }
}

static void proc_update_single(struct myproc *proc, struct myproc **procs, struct procstat *pst)
{
  pid_t oldppid = proc->ppid;

  // Get the kinfo_proc pointer to the current PID
  if ( machine_update_proc(proc, pst) == 0) {
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
  } else // If KVM reports that the process isn't existing anymore, drop it.
    proc_free(proc);
}

const char *proc_str(struct myproc *p)
{
  static char buf[256];

  snprintf(buf, sizeof buf,
           "{ pid=%d, ppid=%d, state=%d, cmd=\"%s\"}",
           p->pid, p->ppid, (int)p->state, p->cmd);

  return buf;
}

void proc_update(struct myproc **procs, struct procstat *pst)
{
  int i;
  int count, running, owned, zombies;

  getprocstat(pst);

  count = running = owned = zombies = 0;

  for(i = 1; i < HASH_TABLE_SIZE; i++){ // i=0 is the dummy node
    struct myproc **change_me;
    struct myproc *p;

    change_me = &procs[i];

    for(p = procs[i]; p; ) {

      if(machine_proc_exists(p->pid) == NULL) { // process doesn't exist anymore
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
        pst->count--;
        p = next;
      }else{
        if(p){
          proc_update_single(p, procs, pst);
          count++;

          if(p->state == SRUN)
            running++;
          if(p->uid == global_uid)
            owned++;
        }

        change_me = &p->hash_next;
        p = p->hash_next;
      }
    }
  }

  pst->count   = count;
  pst->running = running;
  pst->owned   = owned;
  pst->zombies = zombies;

  machine_proc_listall(procs, pst);
}

void proc_handle_renames(struct myproc **ps)
{
  struct myproc *p;
  int i;

  ITER_PROCS(i, p, ps)
      proc_handle_rename(p);
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
        if(p->cmd && strstr(p->cmd, str) && n-- <= 0)
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

struct myproc *procs_find_root(struct myproc **procs)
{
  struct myproc *p;
  p = procs[0];
  if(p)
    return p;
  return NULL;
}
