#ifndef SYS_H
#define SYS_H

struct sysinfo
{
	// process info
	int count, running, owned, zombies;

	// machine info
	float loadavg[3];
	int memory[6];

	float cpu_pct;

	int ncpus;

	struct timeval boottime;
};

#endif
