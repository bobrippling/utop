#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <kvm.h>
#include <unistd.h>
#include <paths.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/stat.h> // S_IFCHR
#include <sys/file.h> // O_RDONLY

#include "util.h"
#include "machine.h"
#include "proc.h"

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

kvm_t *kd = NULL; // kvm handle

void init_machine(struct procstat *pst)
{
  extern int pageshift; // defined in machine.h
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
    pst->boottime = boottime;
  } else {
    pst->boottime.tv_sec = -1;
  }

  // Number of cpus
  ncpus = 0;
  GETSYSCTL("kern.smp.cpus", ncpus);
  pst->ncpus = ncpus;

  // Populate cpu states once:
  GETSYSCTL("kern.cp_time", pst->cpu_cycles);

  // Finally, open kvm handle
  if((kd = kvm_open(NULL, _PATH_DEVNULL, NULL, O_RDONLY, NULL)) == NULL){
    perror("kd");
    abort();
  }
}

void machine_cleanup()
{
  if(kd)
    kvm_close(kd);
}

const char *uptime_from_boottime(time_t boottime)
{
  static char buf[64]; // Should be sufficient
  time_t now;
  struct tm *ltime;
  unsigned long int diff_secs; // The difference between now and the epoch
  unsigned long int rest;
  unsigned int days, hours, minutes, seconds;

  time(&now);
  ltime = localtime(&now);

  diff_secs = now-boottime;

  days = diff_secs/86400;
  rest = diff_secs % 86400;
  hours = rest / 3600;
  rest = rest % 3600;
  minutes = rest/60;
  rest = rest % 60;
  seconds = (unsigned int)rest;

  snprintf(buf, sizeof buf, "up %d+%02d:%02d:%02d  %02d:%02d:%02d", days, hours, minutes, seconds, ltime->tm_hour, ltime->tm_min, ltime->tm_sec);
  return buf;
}

const char* format_memory(int memory[6])
{
  int i;
  char prefix;
  static char memory_string[128];
  char *p;

  p = memory_string;

  for(i=0; i<6; i++){
    int val = memory[i]; // default is KiloBytes

    if(val/1024/1024 > 10){ // display GigaBytes
      prefix = 'G';
      val = val/(1024*1024);
    }
    else if(val/1024 > 10){ // display MegaBytes
      prefix = 'M';
      val /= 1024;
    } else { // KiloBytes
      prefix = 'K';
    }

    p += snprintf(p, sizeof memory_string, "%d%c %s", val, prefix, memorynames[i]);
  }

  return memory_string;
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

void get_load_average(struct procstat *pst)
{
  struct loadavg sysload;
  int i;
  extern int pageshift; // defined in machine.h

  // Load average
  GETSYSCTL("vm.loadavg", sysload);
  for (i = 0; i < 3; i++)
    pst->loadavg[i] = (double)sysload.ldavg[i] / sysload.fscale;

  pst->fscale = sysload.fscale;
}

void get_mem_usage(struct procstat *pst)
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
  pst->memory[0] = pagetok(memory_stats[0]);
  pst->memory[1] = pagetok(memory_stats[1]);
  pst->memory[2] = pagetok(memory_stats[2]);
  pst->memory[3] = pagetok(memory_stats[3]);
  pst->memory[4] = bufspace / 1024;
  pst->memory[5] = pagetok(memory_stats[5]);
  pst->memory[6] = -1;
}

void get_cpu_stats(struct procstat *pst)
{
  /* Calculate total cpu utilization in % user, %nice, %system, %interrupt, %idle */
  int state;
  long diff[CPUSTATES], cpu_cycles_now[CPUSTATES];
  long total_change = 0, half_total;

  GETSYSCTL("kern.cp_time", cpu_cycles_now); // old values in pst->cpu_time

  // top's weird algorithm
  for(state=0; state < CPUSTATES; state++) {
    diff[state] = cpu_cycles_now[state] - pst->cpu_cycles[state];
    pst->cpu_cycles[state] = cpu_cycles_now[state]; // copy new values to old ones
    total_change += diff[state];
  }
  // don't divide by zero
  if (total_change == 0)
    total_change = 1;

  half_total = total_change / 2l;

  for(state=0; state < CPUSTATES; state++)
    pst->cpu_pct[state] = (double)(diff[state] * 1000 + half_total)/total_change/10.0L;
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
}

struct kinfo_proc *machine_proc_exists(pid_t pid)
{
  struct kinfo_proc *proc = NULL;
  int num_procs = 0;

  proc = kvm_getprocs(kd, KERN_PROC_PID, pid, &num_procs);

  return proc;
}

char **machine_get_argv(pid_t pid)
{
  struct kinfo_proc *proc;
  char **argv = NULL;

  if((proc = machine_proc_exists(pid)) != NULL)
    argv = kvm_getargv(kd, proc, 0);

  return argv;
}

/* Update process fields. */
/* Returns 0 on success, -1 on error */
int machine_update_proc(struct myproc *proc, struct procstat *pst)
{
  struct kinfo_proc *pp;

  if ( (pp = machine_proc_exists(proc->pid)) != NULL ) {
    if(proc->basename)
      free(proc->basename);
    proc->basename = ustrdup(pp->ki_comm);
    proc->pid = pp->ki_pid;
    proc->ppid = pp->ki_ppid;
    proc->state = pp->ki_stat;
    if(proc->state_str)
      free(proc->state_str);
    proc->state_str = ustrdup(proc_state_str(pp));
    proc->uid = pp->ki_ruid;
    proc->gid = pp->ki_rgid;
    proc->nice = pp->ki_nice;
    proc->pc_cpu = pctdouble(pp->ki_pctcpu, pst->fscale);
    proc->flag = pp->ki_flag;

    // Set tty
    char buf[8];
    devname_r(pp->ki_tdev, S_IFCHR, buf, 8);
    if(proc->tty)
      free(proc->tty);
    proc->tty = ustrdup(buf);

    return 0;
  } else {
    return -1;
  }
}

struct myproc *machine_proc_new(struct kinfo_proc *pp) {
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

void machine_proc_listall(struct myproc **procs, struct procstat *stat)
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
      if(!proc_listcontains(procs, pp->ki_pid)){
        struct myproc *p = machine_proc_new(pp);

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
