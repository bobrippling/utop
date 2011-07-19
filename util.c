#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>

#include "util.h"

void *umalloc(size_t l)
{
	void *p = malloc(l);
	if(!p){
		perror("malloc()");
		exit(1);
	}
	memset(p, 0, l);
	return p;
}

void *urealloc(void *p, size_t l)
{
	void *r = realloc(p, l);
	if(!r){
		perror("realloc()");
		exit(1);
	}
	return r;
}

char *ustrdup(const char *s)
{
	char *r = umalloc(strlen(s) + 1);
	strcpy(r, s);
	return r;
}

long mstime()
{
	struct timeval t;
	gettimeofday(&t, NULL);
	return t.tv_sec * 1000 + t.tv_usec / 1000;
}

char *fline(const char *path, char **pbuf, int *plen)
{
	FILE *f;
	char *ret;
	int c;
	int len;
	int i;

	f = fopen(path, "r");
	if(!f)
		return NULL;

	len = 64;
	ret = umalloc(len + 1);
	memset(ret, 0, len);

	i = 0;
	while((c = fgetc(f)) != EOF){
		ret[i++] = c;
		if(i == len){
			len += 64;
			ret = urealloc(ret, len + 1);
			memset(ret + i + 1, 0, 64);
		}
	}

	if(ferror(f)){
		perror("read()");
		exit(1);
	}
	fclose(f);

	*pbuf = ret;
	if(plen)
		*plen = i;

	return ret;
}
