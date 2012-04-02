#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <signal.h>

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

// convert fixpt_t value to percentages
double pctdouble(fixpt_t pc_cpu, double fscale)
{
  return ((double)pc_cpu/fscale)*100; // in %
}


/* Convert a kbyte value to either Mega or GigaBytes */
const char *format_kbytes(long unsigned val)
{
  char prefix;
  static char buf[64];

  if(val/1024/1024 > 10){ // display GigaBytes
    prefix = 'G';
    val = val/(1024*1024);
  }
  else if(val/1024 > 10){ // display MegaBytes
    prefix = 'M';
    val /= 1024;
  } else { // KiloBytes
    prefix = 'K';
  }

  snprintf(buf, sizeof buf, "%lu%c", val, prefix);

  return buf;
}

const char *format_seconds(long unsigned timeval)
{
  unsigned long int rest;
  unsigned int days, hours, minutes, seconds;
  static char buf[128];
  char *p;

  p = &buf[0];

  days = timeval/86400;
  rest = timeval % 86400;
  hours = rest / 3600;
  rest = rest % 3600;
  minutes = rest/60;
  rest = rest % 60;
  seconds = (unsigned int)rest;

  if(days)
    p += snprintf(p, sizeof p, "%d+", days);
  if(hours)
    p += snprintf(p, sizeof p, "%02d:", hours);

  p += snprintf(p, sizeof p, "%02d:%02d", minutes, seconds);

  return buf;
}
