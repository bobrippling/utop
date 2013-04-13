#ifndef MACHINE_PS
#  ifdef __FreeBSD__
#    include "machine_freebsd.c"
#  else
#    ifdef __linux__
#      include "machine_linux.c"
#    else
#      ifdef __DARWIN__
#        include "machine_darwin.c"
#      else
#        define MACHINE_PS
#      endif
#    endif
#  endif
#endif

#ifdef MACHINE_PS
#  include "machine_ps.c"
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
