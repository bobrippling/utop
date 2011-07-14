#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>

#include "util.h"

long mstime()
{
	struct timeval t;
	gettimeofday(&t, NULL);
	return t.tv_sec * 1000 + t.tv_usec / 1000;
}

char *fline(const char *path, char *buf, int len)
{
	FILE *f = fopen(path, "r");
	char *ret = buf;

	if(!f){
		fprintf(stderr, "open \"%s\": %s\n", path, strerror(errno));
		return NULL;
	}

	if(fgets(buf, len, f) == NULL)
		ret = NULL;

	fclose(f);
	return ret;
}
