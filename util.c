#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "util.h"

long mstime()
{
	struct timeval t;
	gettimeofday(&t, NULL);
	return t.tv_sec * 1000 + t.tv_usec / 1000;
}

int fline(const char *path, char *buf, int len)
{
	FILE *f = fopen(path, "r");
	int ret = 0;

	if(!f)
		return 1;

	if(fgets(buf, len, f) == NULL)
		ret = 1;

	fclose(f);
	return ret;
}
