#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/sysctl.h>

#include "util.h"
#include "machine.h"
#include "proc.h"

// Process states - short form
// SIDL 1   /* Process being created by fork. */
// SRUN 2   /* Currently runnable. */
// SSLEEP 3   /* Sleeping on an address. */
// SSTOP  4   /* Process debugging or suspension. */
// SZOMB  5   /* Awaiting collection by parent. */
// SWAIT  6   /* Waiting or interrupt. */
// SLOCK  7   /* Blocked on a lock. */

// Take from top(8)
const char *state_abbrev[] = {
  "", "START", "RUN\0\0\0", "SLEEP", "STOP", "ZOMB", "WAIT", "LOCK"
};

/* these are for detailing the memory statistics */
char *memorynames[] = {
  "Active, ", "Inact, ", "Wired, ", "Cache, ", "Buf, ",
  "Free", NULL
};

void init_machine(struct procstat *pst)
{
  extern int pageshift; // defined in machine.h
  int mib[2], pagesize, ncpus;
  struct timeval boottime;
  size_t bt_size;

  /* get the page size and calculate pageshift from it */
  pagesize = getpagesize();
  pageshift = 0;
  while (pagesize > 1) {
    pageshift++;
    pagesize >>= 1;
  }

  /* we only need the amount of log(2)1024 for our conversion */
  pageshift -= LOG1024;

  // TODO: do this only once, not continously
  // Get the boottime from the kernel to calculate uptime
  mib[0] = CTL_KERN;
  mib[1] = KERN_BOOTTIME;
  bt_size = sizeof(boottime);
  if (sysctl(mib, 2, &boottime, &bt_size, NULL, 0) != -1 &&
      boottime.tv_sec != 0) {
    pst->boottime = boottime;
  } else {
    pst->boottime.tv_sec = -1;
  }

  /* // Number of cpus */
  /* ncpus = 0; */
  /* GETSYSCTL("kern.smp.cpus", ncpus); */
  /* pst->ncpus = ncpus; */
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

// Taken from top(8)
void getsysctl(const char *name, void *ptr, size_t len)
{
  size_t nlen = len;

  if (sysctlbyname(name, ptr, &nlen, NULL, 0) == -1) {
    fprintf(stderr, "top: sysctl(%s...) failed: %s\n", name,
            strerror(errno));
    abort();
  }
  if (nlen != len) {
    fprintf(stderr, "top: sysctl(%s...) expected %lu, got %lu\n",
            name, (unsigned long)len, (unsigned long)nlen);
    abort();
  }
}
