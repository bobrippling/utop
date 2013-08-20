#ifndef STRUCTS_H
#define STRUCTS_H

#include <sys/time.h>

struct myproc
{
	pid_t pid, ppid;
  int jid;

	uid_t uid;
  gid_t gid;
  gid_t pgrp;
	char *unam, *gnam;

	char *shell_cmd;      /* allocated, from argv */
	char **argv;          /* allocated */
  int argc;
	char *argv0_basename; /* pointer to somewhere in argv[0] */

  enum proc_state
	{
		PROC_STATE_RUN,
		PROC_STATE_SLEEP,
		PROC_STATE_DISK,
		PROC_STATE_STOPPED,
		PROC_STATE_ZOMBIE,
		PROC_STATE_DEAD,
		PROC_STATE_TRACE,
		PROC_STATE_OTHER,
#define PROC_N_STATES (PROC_STATE_OTHER + 1)
	} state;

  char *tty;
  signed char nice;

	double pc_cpu;
	unsigned long utime, stime, cutime, cstime;
	unsigned long cputime;
	unsigned long memsize;

	/* important */
	struct myproc *hash_next;
	struct myproc **children;
	int mark;

	union
	{
		struct
		{
			int flag;
		} freebsd;
	} machine;
};

struct sysinfo
{
	// process info
	int count, count_kernel, owned;
	int procs_in_state[PROC_N_STATES]; /* number of PROC_STATE */

	// machine info
	float loadavg[3];

#ifndef __LP64__
	long
#endif
	unsigned memory[6];

	float cpu_pct;

	unsigned long cpu_cycles;
	int ncpus;

	struct timeval boottime;
};

#endif
