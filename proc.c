#include <sys/file.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <kvm.h>

#include "proc.h"
#include "util.h"
#include "main.h"

#define GETSYSCTL(name, var) getsysctl(name, &(var), sizeof(var))
#define ITER_PROCS(i, p, ps) \
	for(i = 0; i < HASH_TABLE_SIZE; i++) \
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

static void proc_update_single(struct myproc *proc, struct myproc **procs);
static void proc_handle_rename(struct myproc *p);

static kvm_t *kd = NULL; // kvm handle

// States taken from top(8)
/* these are for detailing the cpu states */
int cpu_states[CPUSTATES];
char *cpustatenames[] = {
	"user", "nice", "system", "interrupt", "idle", NULL
};

/* these are for detailing the memory statistics */

int memory_stats[7];
char *memorynames[] = {
	"K Active, ", "K Inact, ", "K Wired, ", "K Cache, ", "K Buf, ",
	"K Free", NULL
};

int swap_stats[7];
char *swapnames[] = {
	"K Total, ", "K Used, ", "K Free, ", "% Inuse, ", "K In, ", "K Out",
	NULL
};

// Process states - short form
const char *state_abbrev[] = {
  "", "START", "RUN\0\0\0", "SLEEP", "STOP", "ZOMB", "WAIT", "LOCK"
};

// Taken from top(8)
static void getsysctl(const char *name, void *ptr, size_t len)
{
	size_t nlen = len;

	if (sysctlbyname(name, ptr, &nlen, NULL, 0) == -1) {
		fprintf(stderr, "top: sysctl(%s...) failed: %s\n", name,
            strerror(errno));
    abort();
	}
	if (nlen != len) {
		fprintf(stderr, "top: sysctl(%s...) expected %lu, got %lu\n",
            name, (unsigned long)len, (unsigned long)nlen);
    abort();
	}
}

void proc_free(struct myproc *p)
{
  char **iter;
  if (p->argv)
 		for(iter = p->argv; *iter; iter++)
 			free(*iter);
  free(p->basename);
  free(p->cmd);
  free(p->state);
 	free(p->argv);
 	free(p->unam);
 	free(p->gnam);
  free(p->tty);
 	free(p);
}

static void getprocstat(struct procstat *pst)
{
	struct loadavg sysload;
  int i, mib[2];
	size_t bt_size;
  struct timeval boottime;

#ifdef CPU_PERCENTAGE
  // TODO
#endif

#define NIL(x) if(!x) x = 1
	NIL(pst->cputime_total);
	NIL(pst->cputime_period);
#undef NIL

	GETSYSCTL("vm.loadavg", sysload);

	for (i = 0; i < 3; i++)
		pst->loadavg[i] = (double)sysload.ldavg[i] / sysload.fscale;

	mib[0] = CTL_KERN;
	mib[1] = KERN_BOOTTIME;
	bt_size = sizeof(boottime);
	if (sysctl(mib, 2, &boottime, &bt_size, NULL, 0) != -1 &&
	    boottime.tv_sec != 0) {
		pst->boottime = boottime;
	} else {
		pst->boottime.tv_sec = -1;
	}
}

char *proc_state(struct kinfo_proc *pp) {
  char state;
  char *status = umalloc(16 * sizeof(char));

  if (pp) {
    state = pp->ki_stat;

    switch (state) {
      case SRUN:
        if (pp->ki_oncpu != 0xff)
          sprintf(status, "CPU%d", pp->ki_oncpu);
        else
          strcpy(status, "RUN");
        break;
      case SLOCK:
        if (pp->ki_kiflag & KI_LOCKBLOCK) {
          sprintf(status, "*%.6s", pp->ki_lockname);
          break;
        }
        /* fall through */
      case SSLEEP:
        if (pp->ki_wmesg != NULL) {
          sprintf(status, "%.6s", pp->ki_wmesg);
          break;
        }
        /* FALLTHROUGH */
      default:
        if (state >= 0 &&
            state < (signed) (sizeof(state_abbrev) / sizeof(*state_abbrev)))
          sprintf(status, "%.6s", state_abbrev[(int)state]);
        else
          sprintf(status, "?%5d", state);
        break;
    }
  } else {
    strcpy(status, " ");
  }

  return status;
}

struct myproc *proc_new(struct kinfo_proc *pp) {
  struct myproc *this = NULL;

  this = umalloc(sizeof(*this));

  this->basename = ustrdup(pp->ki_comm);
  this->uid = pp->ki_uid;
  this->gid = pp->ki_rgid;

  struct passwd *passwd;
  struct group  *group;

#define GETPW(id, var, truct, fn, member)       \
  truct = fn(id);                               \
  if(truct){                                    \
    var = ustrdup(truct->member);               \
  }else{                                        \
    char buf[8];                                \
    snprintf(buf, sizeof buf, "%d", id);        \
    var = ustrdup(buf);                         \
  }                                             \

  GETPW(this->uid, this->unam, passwd, getpwuid, pw_name);
  GETPW(this->gid, this->gnam,  group, getgrgid, gr_name);

  this->pid  = pp->ki_pid;
  this->ppid = -1;
  this->jid = pp->ki_jid;

  proc_handle_rename(this);

  return this;
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
	}else
{
		procs[p->pid % HASH_TABLE_SIZE] = p;
	}
}

// initialize hash table and kvm
struct myproc **proc_init()
{

	if((kd = kvm_open(NULL, _PATH_DEVNULL, NULL, O_RDONLY, NULL)) == NULL){
		perror("kd");
		abort();
	}

	return umalloc(HASH_TABLE_SIZE * sizeof *proc_init());
}

void proc_cleanup(struct myproc **procs)
{
	struct myproc *p;
	int i;

  if(procs) {
    ITER_PROCS(i, p, procs)
        proc_free(p);
  }
  if(kd)
    kvm_close(kd);
}

void proc_listall(struct myproc **procs, struct procstat *stat)
{
  // This is the number of processes that kvm_getprocs returns
  int num_procs = 0;

  // get all processes
  struct kinfo_proc *pbase; // defined in /usr/include/sys/user.h

  if ((pbase = kvm_getprocs(kd, KERN_PROC_PROC, 0, &num_procs) )) {
    struct kinfo_proc *pp;
    int i;

    // We iterate over each kinfo_struct pointer and check if it
    // exists already in our hash table. If it is not present yet, add
    // it to the table and increase the global process counter
    for(pp = pbase, i = 0; i < num_procs; pp++, i++) {
      if(!proc_listcontains(procs, pp->ki_pid)) {
        struct myproc *p = proc_new(pp);

        // Skip zombies here
        if(p) {
          proc_addto(procs, p);
          stat->count++;
          if(pp->ki_stat == SZOMB)
            stat->zombies++;
        }
      }
    }
  }
}

static void proc_handle_rename(struct myproc *this)
{
	struct kinfo_proc *proc = NULL;
	int num_procs = 0;

	if((proc = kvm_getprocs(kd, KERN_PROC_PID, this->pid, &num_procs)) != NULL){
		char **iter, **argv;

		argv = kvm_getargv(kd, proc, 0);

		/* dup argv */
		if(argv){
			int i, argc, slen;
			char *cmd;

			/* free old argv */
      for(i = 0; i < this->argc; i++)
        free(this->argv[i]);
      free(this->argv);

			argc = slen = 0;

			for(iter = argv; *iter; iter++)
				argc++;

			this->argv = umalloc((argc+1) * sizeof *this->argv);

			for(i = 0; i < argc; i++){
				slen += strlen(argv[i]) + 1 /* space */;
				this->argv[i] = ustrdup(argv[i]);
			}

			this->argv[argc] = NULL;
      this->argc = argc;

			/* recreate this->cmd from argv */
			free(this->cmd);
			cmd = this->cmd = umalloc(slen + 1);
			for(int i = 0; i < argc; i++)
				cmd += sprintf(cmd, "%s ", argv[i]);

      if(global_debug){
        fprintf(stderr, "recreated argv for %s\n", this->basename);
        for(i = 0; this->argv[i]; i++)
          fprintf(stderr, "argv[%d] = %s\n", i, argv[i]);
        fprintf(stderr, "argv[%d] = %s\n", i, argv[i]);
      }
		}
	}
}

static void proc_update_single(struct myproc *proc, struct myproc **procs)
{
  struct kinfo_proc *pp = NULL; // defined in /usr/include/sys/user.h
  int num_procs = 0; // This is the number of processes that kvm_getprocs returns
  pid_t oldppid = proc->ppid;

  // Get the kinfo_proc pointer to the current PID
  if ( (pp = kvm_getprocs(kd, KERN_PROC_PID, proc->pid, &num_procs)) != NULL ) {
    proc->basename = ustrdup(pp->ki_comm);
    proc->pid = pp->ki_pid;
    proc->ppid = pp->ki_ppid;
    proc->state = proc_state(pp);
    proc->uid = pp->ki_ruid;
    proc->gid = pp->ki_rgid;

    // Set tty
    char buf[8];
    devname_r(pp->ki_tdev, S_IFCHR, buf, 8);
    proc->tty = ustrdup(buf);

    if(proc->ppid && oldppid != proc->ppid){
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
			"{ pid=%d, ppid=%d, state=%s, cmd=\"%s\" }",
			p->pid, p->ppid, p->state, p->basename);

	return buf;
}

void proc_update(struct myproc **procs, struct procstat *pst)
{
	int i;
	int count, running, owned, zombies;
  int num_procs = 0;

	getprocstat(pst);

	count = running = owned = zombies = 0;

	for(i = 0; i < HASH_TABLE_SIZE; i++){
		struct myproc **change_me;
		struct myproc *p;

		change_me = &procs[i];
		for(p = procs[i]; p; ) {
			if(kvm_getprocs(kd, KERN_PROC_PID, p->pid, &num_procs) == NULL) {
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
        if (p)
          proc_free(p);
				p = next;
			}else{
				if(p){
					proc_update_single(p, procs);
					if(p->ppid != 2)
						count++; /* else it's kthreadd or init */

					/* if(p->state) // TODO */
					/* 	running++; */
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

  proc_listall(procs, pst);
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

	ITER_PROCS(i, p, ps)
		if(strstr(p->cmd, str) && n-- <= 0)
			return p;

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

struct myproc *proc_any(struct myproc **procs)
{
	struct myproc *p;
	int i;
	ITER_PROCS(i, p, procs)
		return p;

	fputs("no procs\n", stderr);
	abort();
}

void procs_mark_undisplayed(struct myproc **procs)
{
	struct myproc *p;
	int i;
	ITER_PROCS(i, p, procs)
		p->displayed = 0;
}

struct myproc *proc_undisplayed(struct myproc **procs)
{
	struct myproc *p;
	int i;
	ITER_PROCS(i, p, procs)
		if(!p->displayed)
			return p;
	return NULL;
}
