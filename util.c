#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <signal.h>


/* these are for detailing the memory statistics */
char *memorynames[] = {
	"Active, ", "Inact, ", "Wired, ", "Cache, ", "Buf, ",
	"Free", NULL
};


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

const char *uptime_from_boottime(time_t boottime)
{
  static char buf[64]; // Should be sufficient
  time_t now;
  struct tm *ltime;
  unsigned long int diff_secs; // The difference between now and the epoch
  unsigned long int rest;
  unsigned int days, hours, minutes, seconds;

  time(&now);
  ltime = localtime(&now);

  diff_secs = now-boottime;

  days = diff_secs/86400;
  rest = diff_secs % 86400;
  hours = rest / 3600;
  rest = rest % 3600;
  minutes = rest/60;
  rest = rest % 60;
  seconds = (unsigned int)rest;

  snprintf(buf, sizeof buf, "up %d+%02d:%02d:%02d  %02d:%02d:%02d", days, hours, minutes, seconds, ltime->tm_hour, ltime->tm_min, ltime->tm_sec);
  return buf;
}

const char* format_memory(int memory[6])
{
  int i, slen;
  char prefix;
  char buf[6][128];
  static char *memory_string;

  for(i=0; i<6; i++){
    int val = memory[i]; // default is KiloBytes

    if(val/1024/1024 > 1){ // display GigaBytes
      prefix = 'G';
      val = val/(1024*1024);
    }
    else if(val/1024 > 1){ // display MegaBytes
      prefix = 'M';
      val /= 1024;
    } else { // KiloBytes
      prefix = 'K';
    }
    snprintf(buf[i], sizeof buf[i], "%d%c %s", val, prefix, memorynames[i]);
    slen += strlen(buf[i]);
  }

  memory_string = umalloc(slen+1);
  for(i=0; i<6; i++){
    /* memory_string += sprintf(memory_string, "%s ", buf[i]); */
    memory_string = strncat(memory_string, buf[i], strlen(buf[i]));
  }

  return memory_string;
}
