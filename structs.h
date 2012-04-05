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

  enum
	{
		PROC_STATE_RUN,
		PROC_STATE_SLEEP,
		PROC_STATE_DISK,
		PROC_STATE_STOPPED,
		PROC_STATE_ZOMBIE,
		PROC_STATE_DEAD,
		PROC_STATE_OTHER,
#define PROC_N_STATES (PROC_STATE_OTHER + 1)
	} state;

  char *tty;
  signed char nice;

	double pc_cpu;
	unsigned long utime, stime, cutime, cstime;

	/* important */
	struct myproc *hash_next;
	struct myproc **children;

	union
	{
		struct
		{
			int flag;
		} freebsd;
		struct
		{
			int unused;
		} linux;
	} machine;
};

struct sysinfo
{
	// process info
	int count, owned;
	int procs_in_state[PROC_N_STATES]; /* number of PROC_STATE */

	// machine info
	float loadavg[3];
	int memory[6];

	float cpu_pct;

	int ncpus;

	struct timeval boottime;
};

#endif
