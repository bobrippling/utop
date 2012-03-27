#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/times.h>
#include <sys/vtimes.h>

#include "cpu.h"

static clock_t lastcpu, lastsyscpu, lastusercpu;
static int ncpu;

/*
 * TODO:
 * s/times()/getrusage()/
 */

void cpu_init()
{
	FILE *f;
	struct tms timesample;
	char line[128];

	lastcpu     = times(&timesample);

	if(lastcpu == -1){
		perror("times()");
		abort();
	}

	lastsyscpu  = timesample.tms_stime;
	lastusercpu = timesample.tms_utime;

	f = fopen("/proc/cpuinfo", "r");
	if(!f){
		perror("open cpuinfo");
		abort();
	}

	ncpu = 0;
	while(fgets(line, sizeof line, f))
		if(strncmp(line, "processor", 9) == 0)
			ncpu++;

	fclose(f);
}

long more_stuff_todo()
{
	struct tms timesample;
	clock_t now;
	double percent;

	now = times(&timesample);
	if(now <= lastcpu || timesample.tms_stime < lastsyscpu || timesample.tms_utime < lastusercpu)
		// overflow detection, skip
		percent = -1.0;
	else{
		percent = (timesample.tms_stime - lastsyscpu) + (timesample.tms_utime - lastusercpu);
		percent /= now - lastcpu;
		percent /= ncpu;
		percent *= 100;
	}
	lastcpu = now;
	lastsyscpu = timesample.tms_stime;
	lastusercpu = timesample.tms_utime;

	return percent;
}
