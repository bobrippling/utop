#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// Take from top(8)
char *state_abbrev[] = {
  "", "START", "RUN\0\0\0", "SLEEP", "STOP", "ZOMB", "WAIT", "LOCK"
};

/* these are for detailing the memory statistics */
char *memorynames[] = {
	"K Active, ", "K Inact, ", "K Wired, ", "K Cache, ", "K Buf, ",
	"K Free", NULL
};


static void proc_update_single(struct myproc *proc, struct myproc **procs, struct procstat *ps);
static void proc_handle_rename(struct myproc *p);

static kvm_t *kd = NULL; // kvm handle


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
  free(p->state_str);
 	free(p->argv);
 	free(p->unam);
 	free(p->gnam);
  free(p->tty);
 	free(p);
}

static void getprocstat(struct procstat *pst)
{
	struct loadavg sysload;
  int i, mib[2], pagesize;
	size_t bt_size;
  struct timeval boottime;
  static int pageshift;

	/* get the page size and calculate pageshift from it */
	pagesize = getpagesize();
	pageshift = 0;
	while (pagesize > 1) {
		pageshift++;
		pagesize >>= 1;
	}

	/* we only need the amount of log(2)1024 for our conversion */
	pageshift -= LOG1024;

  // Load average
	GETSYSCTL("vm.loadavg", sysload);
	for (i = 0; i < 3; i++)
		pst->loadavg[i] = (double)sysload.ldavg[i] / sysload.fscale;

  pst->fscale = sysload.fscale;

  // TODO: do this only once, not continously
  // Get the boottime from the kernel to calculate uptime
	mib[0] = CTL_KERN;
	mib[1] = KERN_BOOTTIME;
	bt_size = sizeof(boottime);
	if (sysctl(mib, 2, &boottime, &bt_size, NULL, 0) != -1 &&
	    boottime.tv_sec != 0) {
		pst->boottime = boottime;
	} else {
		pst->boottime.tv_sec = -1;
	}

  // Memory stuff
  static long bufspace = 0;
  static int memory_stats[7];

  GETSYSCTL("vfs.bufspace", bufspace);
  GETSYSCTL("vm.stats.vm.v_active_count", memory_stats[0]);
  GETSYSCTL("vm.stats.vm.v_inactive_count", memory_stats[1]);
  GETSYSCTL("vm.stats.vm.v_wire_count", memory_stats[2]);
  GETSYSCTL("vm.stats.vm.v_cache_count", memory_stats[3]);
  GETSYSCTL("vm.stats.vm.v_free_count", memory_stats[5]);
  /* convert memory stats to Kbytes */
  pst->memory[0] = pagetok(memory_stats[0]);
  pst->memory[1] = pagetok(memory_stats[1]);
  pst->memory[2] = pagetok(memory_stats[2]);
  pst->memory[3] = pagetok(memory_stats[3]);
  pst->memory[4] = bufspace / 1024;
  pst->memory[5] = pagetok(memory_stats[5]);
  pst->memory[6] = -1;
}

const char *proc_state_str(struct kinfo_proc *pp) {
  static char status[10];

  char state = pp->ki_stat;

  if (pp) {
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
  this->state = pp->ki_stat;
  this->flag = pp->ki_flag;

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

        // TODO: (code from top)
        /* if (!show_kidle && pp->ki_tdflags & TDF_IDLETD) */
        /*   /\* skip kernel idle process *\/ */
        /*   continue; */
        /* if (pp->ki_stat == 0) */
        /*   /\* not in use *\/ */
        /*   continue; */

        /* if (!show_self && pp->ki_pid == sel->self) */
        /*   /\* skip self *\/ */
        /*   continue; */

        /* if (!show_system && (pp->ki_flag & P_SYSTEM)) */
        /*   /\* skip system process *\/ */
        /*   continue; */

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

    /* dup argv */
    int i, argc, slen;
    char *cmd, *pos;

    argv = kvm_getargv(kd, proc, 0);
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

      pos = strrchr(this->argv[0], '/');

      if(pos)
        this->basename_offset = pos - this->argv[0] + 1; /* space */
      else
        this->basename_offset = 0;

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
      this->cmd = umalloc(strlen(this->basename)+2); // [this->basename]
      sprintf(this->cmd, "[%s]", this->basename);
    } else {
      cmd = this->cmd = umalloc(slen + 1);
      for(int i = 0; i < argc; i++)
        cmd += sprintf(cmd, "%s ", this->argv[i]);
    }

    if(global_debug){
      fprintf(stderr, "recreated argv for %s\n", this->basename);
      for(i = 0; this->argv[i]; i++)
        fprintf(stderr, "argv[%d] = %s\n", i, argv[i]);
      fprintf(stderr, "argv[%d] = %s\n", i, argv[i]);
    }
  }
}

double pctdouble(fixpt_t pc_cpu, double fscale)
{
  return ((double)pc_cpu/fscale)*100; // in %
}

static void proc_update_single(struct myproc *proc, struct myproc **procs, struct procstat *pst)
{
  struct kinfo_proc *pp = NULL; // defined in /usr/include/sys/user.h
  int num_procs = 0; // This is the number of processes that kvm_getprocs returns
  pid_t oldppid = proc->ppid;

  // Get the kinfo_proc pointer to the current PID
  if ( (pp = kvm_getprocs(kd, KERN_PROC_PID, proc->pid, &num_procs)) != NULL ) {
    proc->basename = ustrdup(pp->ki_comm);
    proc->pid = pp->ki_pid;
    proc->ppid = pp->ki_ppid;
    proc->state = pp->ki_stat;
    proc->state_str = ustrdup(proc_state_str(pp));
    proc->uid = pp->ki_ruid;
    proc->gid = pp->ki_rgid;
    proc->nice = pp->ki_nice;
    proc->pc_cpu = pctdouble(pp->ki_pctcpu, pst->fscale);
    proc->flag = pp->ki_flag;

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
			"{ pid=%d, ppid=%d, state=%d, cmd=\"%s\" }",
           p->pid, p->ppid, (int)p->state, p->basename);

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
			if(kvm_getprocs(kd, KERN_PROC_PID, p->pid, &num_procs) == NULL) { // process doesn't exist anymore
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
