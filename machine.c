#ifdef __FreeBSD__
#  include "machine_freebsd.c"
#else
#  ifdef __linux__
#    include "machine_linux.c"
#  else
#    include "machine_darwin.c"
#  endif
#endif

// common to all

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

  diff_secs = now - boottime;

  days = diff_secs/86400;
  rest = diff_secs % 86400;
  hours = rest / 3600;
  rest = rest % 3600;
  minutes = rest/60;
  rest = rest % 60;
  seconds = (unsigned int)rest;

  snprintf(buf, sizeof buf, "up %d+%02d:%02d:%02d  %02d:%02d:%02d",
			days, hours, minutes, seconds,
			ltime->tm_hour, ltime->tm_min, ltime->tm_sec);

  return buf;
}

const char *machine_format_memory(struct sysinfo *info)
{
	(void)info;
	return "todo: mem";
#if 0
  int i;
  char prefix;
  static char memory_string[128];
  char *p;

  p = memory_string;

  for(i=0; i<6; i++){
    int val = memory[i]; // default is KiloBytes

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

    p += snprintf(p, sizeof memory_string, "%d%c %s", val, prefix, memorynames[i]);
  }

  return memory_string;
#endif
}

const char *machine_format_cpu_pct(struct sysinfo *info)
{
	(void)info;
	return "todo: cpu pct";
}
