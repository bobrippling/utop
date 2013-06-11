#include <stdio.h>
#include <time.h>

#include <sys/types.h>

#include <pwd.h>
#include <grp.h>

#include "structs.h"
#include "proc.h"
#include "main.h"

const char *machine_proc_display_line_default(struct myproc *p);
int machine_proc_display_width_default(void);

void machine_update_unam_gnam(struct myproc *, uid_t uid, uid_t gid);

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

void machine_update_unam_gnam(struct myproc *this, uid_t uid, uid_t gid)
{
	struct passwd *passwd;
	struct group  *group;

#define GETPW(idvar, var, truct, fn, member, id)\
	truct = fn(id);                               \
	idvar = id;                                   \
	if(var) free(var);                            \
	if(truct){                                    \
		var = ustrdup(truct->member);               \
	}else{                                        \
		char buf[8];                                \
		snprintf(buf, sizeof buf, "%d", id);        \
		var = ustrdup(buf);                         \
	}

	GETPW(this->uid, this->unam, passwd, getpwuid, pw_name, uid)
	GETPW(this->gid, this->gnam,  group, getgrgid, gr_name, gid)
}

const char *machine_proc_display_line_default(struct myproc *p)
{
	static char buf[64];

	if(globals.thin){
		snprintf(buf, sizeof buf,
			"% 7d %-1s "
			"%-*s "
			//"%3.1f"
			,
			p->pid, proc_state_str(p),
			max_unam_len, p->unam
			//p->pc_cpu
		);
	}else{
		snprintf(buf, sizeof buf,
			"% 7d % 7d %-1s " // 18
			"%-*s %-*s "      // max_unam_len + max_gnam_len + 1
			"%3.1f"           // 5
			,
			p->pid, p->ppid, proc_state_str(p),
			max_unam_len, p->unam,
			max_gnam_len, p->gnam,
			p->pc_cpu
		);
	}

	return buf;
}

int machine_proc_display_width_default()
{
	if(globals.thin)
		return 9 + max_unam_len;
	else
		return 18 + max_unam_len + max_gnam_len + 1 + 5;
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
