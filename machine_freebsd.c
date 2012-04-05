#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>

#include <sys/types.h>
#ifdef __FreeBSD__
#  include <kvm.h>
#endif
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/user.h>

#include <paths.h>
#include <sys/stat.h> // S_IFCHR
#include <sys/file.h> // O_RDONLY

#include "util.h"
#include "machine.h"
#include "proc.h"
#include "main.h"

// Process states - short form
// SIDL 1   /* Process being created by fork. */
// SRUN 2   /* Currently runnable. */
// SSLEEP 3   /* Sleeping on an address. */
// SSTOP  4   /* Process debugging or suspension. */
// SZOMB  5   /* Awaiting collection by parent. */
// SWAIT  6   /* Waiting or interrupt. */
// SLOCK  7   /* Blocked on a lock. */

// Take from top(8)
const char *state_abbrev[] = {
  "", "START", "RUN\0\0\0", "SLEEP", "STOP", "ZOMB", "WAIT", "LOCK", NULL
};

/* these are for detailing the memory statistics */
const char *memorynames[] = {
  "Active, ", "Inact, ", "Wired, ", "Cache, ", "Buf, ",
  "Free", NULL
};

const char *cpustates[] = {
  "user", "nice", "system", "interrupt", "idle", NULL
};

#define LOG1024 6.9314718055994

kvm_t *kd = NULL; // kvm handle

#define GETSYSCTL(...) // TODO

void machine_init(struct sysinfo *info)
{
  int pageshift;
  int mib[2], pagesize, ncpus;
  struct timeval boottime;
  size_t bt_size;

  /* get the page size and calculate pageshift from it */
  pagesize = getpagesize();
  pageshift = 0;
  while (pagesize > 1) {
    pageshift++;
    pagesize >>= 1;
  }

  /* we only need the amount of log(2)1024 for our conversion */
  pageshift -= LOG1024;

  // Get the boottime from the kernel to calculate uptime
  mib[0] = CTL_KERN;
  mib[1] = KERN_BOOTTIME;
  bt_size = sizeof(boottime);
  if (sysctl(mib, 2, &boottime, &bt_size, NULL, 0) != -1 &&
      boottime.tv_sec != 0) {
    info->boottime = boottime;
  } else {
    info->boottime.tv_sec = -1;
  }

  // Number of cpus
  ncpus = 0;
  GETSYSCTL("kern.smp.cpus", ncpus);
  info->ncpus = ncpus;

  // Populate cpu states once:
  GETSYSCTL("kern.cp_time", info->cpu_cycles);

  // Finally, open kvm handle
  if((kd = kvm_open(NULL, _PATH_DEVNULL, NULL, O_RDONLY, NULL)) == NULL){
    perror("kd");
    abort();
  }
}

void machine_term()
{
  if(kd)
    kvm_close(kd);
}

// Taken from top(8)
void getsysctl(const char *name, void *ptr, size_t len)
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

void get_load_average(struct sysinfo *info)
{
#if 0
// Flo?
  struct loadavg sysload;
  int i;
  extern int pageshift; // defined in machine.h

  // Load average
  GETSYSCTL("vm.loadavg", sysload);
  for (i = 0; i < 3; i++)
    info->loadavg[i] = (double)sysload.ldavg[i] / sysload.fscale;

  //info->fscale = sysload.fscale; TODO
#endif
}

void get_mem_usage(struct sysinfo *info)
{
    // Memory stuff
  long bufspace = 0;
  int memory_stats[6];

  GETSYSCTL("vfs.bufspace", bufspace);
  GETSYSCTL("vm.stats.vm.v_active_count", memory_stats[0]);
  GETSYSCTL("vm.stats.vm.v_inactive_count", memory_stats[1]);
  GETSYSCTL("vm.stats.vm.v_wire_count", memory_stats[2]);
  GETSYSCTL("vm.stats.vm.v_cache_count", memory_stats[3]);
  GETSYSCTL("vm.stats.vm.v_free_count", memory_stats[5]);
  /* convert memory stats to Kbytes */
#if 0
	// TODO
  info->memory[0] = pagetok(memory_stats[0]);
  info->memory[1] = pagetok(memory_stats[1]);
  info->memory[2] = pagetok(memory_stats[2]);
  info->memory[3] = pagetok(memory_stats[3]);
  info->memory[4] = bufspace / 1024;
  info->memory[5] = pagetok(memory_stats[5]);
  info->memory[6] = -1;
#endif
}

void get_cpu_stats(struct sysinfo *info)
{
  /* Calculate total cpu utilization in % user, %nice, %system, %interrupt, %idle */
#if 0
  int state;
  long diff[CPUSTATES], cpu_cycles_now[CPUSTATES];
  long total_change = 0, half_total;

  GETSYSCTL("kern.cp_time", cpu_cycles_now); // old values in info->cpu_time

  // top's weird algorithm
  for(state=0; state < CPUSTATES; state++) {
    diff[state] = cpu_cycles_now[state] - info->cpu_cycles[state];
    info->cpu_cycles[state] = cpu_cycles_now[state]; // copy new values to old ones
    total_change += diff[state];
  }
  // don't divide by zero
  if (total_change == 0)
    total_change = 1;

  half_total = total_change / 2l;

  for(state=0; state < CPUSTATES; state++)
    info->cpu_pct[state] = (double)(diff[state] * 1000 + half_total)/total_change/10.0L;
#endif
}

const char* format_cpu_pct(double cpu_pct[CPUSTATES])
{
  static char buf[128];
  char *p = buf;

  int i;

  for(i=0; i < CPUSTATES; i++) {
    p += snprintf(p, sizeof buf, "%s %2.1f%%", cpustates[i], cpu_pct[i]);
    if (i != CPUSTATES -1 ) // not the last item
      p += snprintf(p, sizeof buf, ", ");
  }

  return buf;
}

const char *machine_proc_state_str(struct kinfo_proc *pp)
{
	return "";
	// TODO
#if 0
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
        if (state >= 0)
          sprintf(status, "%.6s", state_abbrev[(int)state]);
        else
          sprintf(status, "?%5d", state);
        break;
    }
  } else {
    strcpy(status, " ");
  }

  return status;
#endif
}

int machine_proc_exists(struct myproc *p)
{
  int num_procs = 0;

  return !!kvm_getprocs(kd, KERN_PROC_PID, p->pid, &num_procs);
}

char **machine_get_argv(struct myproc *p)
{
	int n = 0;
  struct kinfo_proc *proc = kvm_getprocs(kd, KERN_PROC_PID, p->pid, &n);
	return kvm_getargv(kd, proc, 0);
}

/* Update process fields. */
/* Returns 0 on success, -1 on error */
int machine_update_proc(struct myproc *proc, struct myproc **procs)
{
	int n = 0;
  struct kinfo_proc *pp;

  if((pp = kvm_getprocs(kd, KERN_PROC_PID, proc->pid, &n)) != NULL){

#if 0
    if(proc->basename)
      free(proc->basename);
    proc->basename = ustrdup(pp->ki_comm);

    if(proc->state_str)
      free(proc->state_str);
    proc->state_str = ustrdup(proc_state_str(pp));

    proc->pid = pp->ki_pid;
    proc->ppid = pp->ki_ppid;
    proc->state = pp->ki_stat;
    proc->uid = pp->ki_ruid;
    proc->gid = pp->ki_rgid;
    proc->nice = pp->ki_nice;
    proc->flag = pp->ki_flag;

    proc->pc_cpu = pctdouble(pp->ki_pctcpu, pst->fscale);

    // Set tty
    char buf[8];
    devname_r(pp->ki_tdev, S_IFCHR, buf, 8);
    if(proc->tty)
      free(proc->tty);
    proc->tty = ustrdup(buf);
#endif

    return 0;
  } else {
    return -1;
  }
}

struct myproc *machine_proc_new(struct kinfo_proc *pp, struct myproc **procs)
{
  struct myproc *this = NULL;

  this = umalloc(sizeof(*this));

#if 0
  this->basename = ustrdup(pp->ki_comm);
#endif
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
#ifdef __FreeBSD__
	// TODO
  this->jid = pp->ki_jid;
#endif
  this->state = pp->ki_stat;
  //this->flag = pp->ki_flag; TODO

  proc_handle_rename(this, procs);

  return this;
}

const char *machine_proc_display_line(struct myproc *p)
{
	// similar to linux, except with jid - TODO
	static char buf[64];

	snprintf(buf, sizeof buf,
		"% 7d			 %-7s "
		"%-*s %-*s "
		"%3.1f"
		,
		p->pid, proc_state_str(p),
		max_unam_len, p->unam,
		max_gnam_len, p->gnam,
		p->pc_cpu
	);

	return buf;
}

void machine_proc_get_more(struct myproc **procs)
{
  // This is the number of processes that kvm_getprocs returns
  int num_procs = 0;

  // get all processes
  struct kinfo_proc *pbase; // defined in /usr/include/sys/user.h

  if((pbase = kvm_getprocs(kd, KERN_PROC_PROC, 0, &num_procs) )){
    struct kinfo_proc *pp;
    int i;

    // We iterate over each kinfo_struct pointer and check if it
    // exists already in our hash table. If it is not present yet, add
    // it to the table and increase the global process counter
    for(pp = pbase, i = 0; i < num_procs; pp++, i++) {
      if(!proc_get(procs, pp->ki_pid)){
        struct myproc *p = machine_proc_new(pp, procs);

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
#if 0
          stat->count++;
          if(pp->ki_stat == SZOMB)
            stat->zombies++;
#endif
        }
      }
    }
  }
}
