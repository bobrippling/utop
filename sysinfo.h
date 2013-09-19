#ifndef SYSINFO_H
#define SYSINFO_H

#include <sys/time.h>

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
