#ifndef SYS_H
#define SYS_H

struct sysinfo
{
	int count, running, owned, zombies;
	int loadavg[3];

	struct timeval boottime;
};

#endif
