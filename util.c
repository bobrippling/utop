#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

#include "util.h"

void *umalloc(size_t l)
{
	void *p = malloc(l);
	if(!p){
		perror("malloc()");
		abort();
	}
	memset(p, 0, l);
	return p;
}

void *urealloc(void *p, size_t l)
{
	void *r = realloc(p, l);
	if(!r){
		perror("realloc()");
		abort();
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
		/* process exited while we were reading, for e.g. */
		/*perror("read()");*/
		/*abort();*/
		free(ret);
		ret = NULL;
		i = 0;
	}
	fclose(f);

	*pbuf = ret;
	if(plen)
		*plen = i;

	return ret;
}

int str_to_sig(const char *s)
{
#define SIG(i) { #i, SIG##i }
	struct
	{
		const char *nam;
		int sig;
	} sigs[] = {
		SIG(HUP),   SIG(INT),   SIG(QUIT),  SIG(ILL),
		SIG(ABRT),  SIG(FPE),   SIG(KILL),  SIG(USR1),
		SIG(SEGV),  SIG(USR2),  SIG(PIPE),  SIG(ALRM),
		SIG(TERM),  SIG(CHLD),  SIG(CONT),  SIG(STOP),
		SIG(TSTP),  SIG(TTIN),  SIG(TTOU),
#ifndef __FreeBSD__
		SIG(PWR),   SIG(SYS),   SIG(POLL),  SIG(WINCH),
		SIG(PROF),  SIG(TRAP),  SIG(BUS),   SIG(STKFLT),
		SIG(URG),   SIG(XCPU),  SIG(XFSZ),  SIG(VTALRM),
#endif

	};
	unsigned int i;



	for(i = 0; i < sizeof(sigs)/sizeof(sigs[0]); i++)
		if(!strcmp(sigs[i].nam, s))
			return sigs[i].sig;
	return -1;
}

int longest_passwd_line(const char *fname)
{
	FILE *f = fopen(fname, "r");
	int max = 8; /* default */
	char buf[128];

	if(!f)
		return max;

	while(fgets(buf, sizeof buf, f)){
		char *colon = strchr(buf, ':');
		if(colon){
			int len = colon - buf;
			if(len > max)
				max = len;
		}
	}

	fclose(f);

	return max;
}
