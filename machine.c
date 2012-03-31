#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <paths.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/stat.h> // S_IFCHR
#include <sys/file.h> // O_RDONLY
#include <time.h>

#include "util.h"
#include "machine.h"
#include "proc.h"

void init_machine(struct procstat *pst)
{
	(void)pst;
	// nothing to do... for now.. dun dun..
}

void machine_cleanup()
{
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
	(void)memory;
	return "";
}

// Taken from top(8)
void get_load_average(struct procstat *pst)
{
	// TODO
  for (int i = 0; i < 3; i++)
    pst->loadavg[i] = 1;

  pst->fscale = 1;
}

void get_mem_usage(struct procstat *pst)
{
  pst->memory[0] = 1;
  pst->memory[1] = 1;
  pst->memory[2] = 1;
  pst->memory[3] = 1;
  pst->memory[4] = 1;
  pst->memory[5] = 1;
  pst->memory[6] = -1;
}

void get_cpu_stats(struct procstat *pst)
{
  for(int state=0; state < CPUSTATES; state++) {
    pst->cpu_cycles[state] = 1;
  }

  for(int state=0; state < CPUSTATES; state++)
    pst->cpu_pct[state] = 1.f;//(double)(diff[state] * 1000 + half_total)/total_change/10.0L;
}

const char* format_cpu_pct(double cpu_pct[CPUSTATES])
{
	(void)cpu_pct;
	return "";
}

const char *proc_state_str(void *pp) {
	(void)pp;
	return "";
}

struct kinfo_proc *machine_proc_exists(pid_t pid)
{
	(void)pid;
	return 0;
}

char **machine_get_argv(pid_t pid)
{
	(void)pid;
  return NULL;
}

/* Update process fields. */
/* Returns 0 on success, -1 on error */
int machine_update_proc(struct myproc *proc, struct procstat *pst)
{
	char *buf;
	char path[32];

	return 0;

	//snprintf(path, sizeof path, "%s/stat", proc->proc_path);

	(void)pst;

	if(fline(path, &buf, NULL)){
		char *start = strrchr(buf, ')') + 2;
		char *iter;
		int i = 0;
		/*unsigned long prevtime;*/
		pid_t oldppid = proc->ppid;

		/*prevtime = proc->utime + proc->stime;*/

		for(iter = strtok(start, " \t"); iter; iter = strtok(NULL, " \t")){
#define INT(n, fmt, x) case n: sscanf(iter, fmt, x); break
			switch(i++){
				INT(0,  "%c", (char *)&proc->state);
				INT(1,  "%d", &proc->ppid);
				//INT(4,  "%d", &proc->tty);
				//INT(5,  "%d", &proc->pgrp);

				INT(11, "%lu", &proc->utime);
				INT(12, "%lu", &proc->stime);
				INT(13, "%lu", &proc->cutime);
				INT(14, "%lu", &proc->cstime);
#undef INT
			}
		}

#ifdef CPU_PERCENTAGE
		proc->pc_cpu =
			(proc->utime + proc->stime - prevtime) /
			pst->cputime_period * 100.f;
#endif

#if 0
		fprintf(stderr, "proc[%d]->cpu = (%lu + %lu - %lu) / %lu = %d\n",
				proc->pid,
				proc->utime, proc->stime, prevtime,
				pst->cputime_total, proc->pc_cpu);
#endif

#if 0
		cpu     = cpuusage(root);
		mem     = int(head("%s/statm" % root).split(' ')[0]);
		time    = self.time(stats[11]) # usermode time;
		proc->pc_cpu = 0;
#endif

		free(buf);

		if(proc->ppid && oldppid != proc->ppid){
			struct myproc *parent = NULL;//proc_get(procs, proc->ppid);
			struct myproc *iter;

			if(parent->child_first){
				iter = parent->child_first;

				while(iter->child_next)
					iter = iter->child_next;

				iter->child_next = proc;
			}else{
				parent->child_first = proc;
			}
		}
		return 0;
	}else{
		return -1;
	}
}

struct myproc *machine_proc_new(struct kinfo_proc *pp) {
  struct myproc *this = NULL;

  this = umalloc(sizeof(*this));

  //this->basename = ustrdup(pp->ki_comm);
  //this->uid = pp->ki_uid;
  //this->gid = pp->ki_rgid;

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

  //this->pid  = pp->ki_pid;
  this->ppid = -1;
  //this->jid = pp->ki_jid;
  //this->state = pp->ki_stat;
  //this->flag = pp->ki_flag;

  proc_handle_rename(this);

  return this;
}

void machine_proc_listall(struct myproc **procs, struct procstat *stat)
{
	// TODO
}
