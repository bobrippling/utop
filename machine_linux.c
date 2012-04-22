#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <paths.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/stat.h> // S_IFCHR
#include <sys/file.h> // O_RDONLY
#include <time.h>
#include <dirent.h>
#include <ctype.h>

#include "util.h"
#include "proc.h"
#include "machine.h"
#include "main.h"
#include "structs.h"

/* these are for detailing the memory statistics */
const char *memorynames[] = {
  "Active, ", "Inact, ", "Wired, ", "Cache, ", "Buf, ",
  "Free", NULL
};

void machine_init(struct sysinfo *info)
{
  // get uptime and calculate bootime from it
  FILE *f;
  char buf[64];
  time_t now;

  time(&now);

  if((f = fopen("/proc/uptime", "r"))){
    for(;;){
      unsigned long uptime_secs;
      if(!fgets(buf, sizeof buf - 1, f))
        break;

      if(sscanf(buf, "%lu", &uptime_secs))
        info->boottime.tv_sec = now - uptime_secs;

      break;
    }
    fclose(f);
  }

  machine_update(info);
}

void machine_term()
{
}

static void get_load_average(struct sysinfo *info)
{
  FILE *f;
  char buf[64];

  if((f = fopen("/proc/loadavg", "r"))){
    for(;;){
      double avg_1, avg_5, avg_15;

      if(!fgets(buf, sizeof buf -1, f))
        break;

      if(sscanf(buf, "%lf %lf %lf", &avg_1, &avg_5, &avg_15)){
        info->loadavg[0] = avg_1;
        info->loadavg[1] = avg_5;
        info->loadavg[2] = avg_15;
      }
      break;
    }
  }
  fclose(f);
}

static void get_mem_usage(struct sysinfo *info)
{
  FILE *f;
  char buf[256];
  long unsigned mem_val = 0;

  if((f = fopen("/proc/meminfo", "r"))){
    while (!feof(f)) {

      char *c, *key, *mem;

      if(!fgets(buf, sizeof buf -1, f))
        break;

      /* Format:
         Key:     val kB
      */

      c = strchr(buf, ':'); // find ':' seperator

      if(!c)
        break;

      key = buf; // pointer to the beginning of the line
      mem = c+1; // mem points to char after ':'
      *c = '\0'; // terminate key string

      while(isspace(*c++)); // Skip whitespaces
      mem = c+1; // Pointer to the beginning of the numerical value

      if (key && mem) {
        if (!strcmp("Active", key)) {
          if (sscanf(mem, "%lu", &mem_val))
            info->memory[0] = mem_val;
        }
        if (!strcmp("Inactive", key)) {
          if (sscanf(mem, "%lu", &mem_val))
            info->memory[1] = mem_val;
        }
        // TODO: Wired?
        info->memory[2] = 0;

        if (!strcmp("Cached", key)) {
          if (sscanf(mem, "%lu", &mem_val))
            info->memory[3] = mem_val;
        }
        if (!strcmp("Buffers", key)) {
          if (sscanf(mem, "%lu", &mem_val))
            info->memory[4] = mem_val;
        }
        if (!strcmp("MemFree", key)) {
          if (sscanf(mem, "%lu", &mem_val))
            info->memory[5] = mem_val;
        }
      }
    }
  }
  fclose(f);
}

static void get_cpu_stats(struct sysinfo *info)
{
	// TODO
	(void)info;
}

void machine_update(struct sysinfo *info)
{
	get_load_average(info);
	get_mem_usage(info);
	get_cpu_stats(info);
}

int machine_proc_exists(struct myproc *p)
{
	char buf[64];
	snprintf(buf, sizeof buf, "/proc/%d", p->pid);
	return access(buf, F_OK) == 0;
}

void machine_read_argv(struct myproc *p)
{
	// cmdline
	char path[64];
	char *cmd;
	int len;

	snprintf(path, sizeof path, "/proc/%d/cmdline", p->pid);

	if(fline(path, &cmd, &len)){
		int i, nuls;
		char *last, *pos;

		for(i = nuls = 0; i < len; i++)
			if(cmd[i] == '\0')
				nuls++;

		argv_free(p->argc, p->argv);

		p->argc = nuls;
		p->argv = umalloc((p->argc + 1) * sizeof *p->argv);

		for(i = nuls = 0, last = cmd; i < len; i++){
			// nuls is the argv index
			if(cmd[i] == '\0'){
				p->argv[nuls++] = ustrdup(last);
				last = cmd + i + 1;
			}
		}
		p->argv[p->argc] = NULL;

		if(p->argv[0]){
			if((pos = strchr(p->argv[0], ':'))){
				/* sshd: ... */
				p->argv0_basename = p->argv[0];
			}else{
				pos = strrchr(p->argv[0], '/'); /* locate the last '/' in argv[0], which is the full command path */

				/* malloc in just a second.. */
				if(!pos++)
					pos = p->argv[0];
				p->argv0_basename = pos;
			}
		}

		free(cmd);
	}
}

int machine_update_proc(struct myproc *proc, struct myproc **procs)
{
	char *buf;
	char path[64];

	(void)procs;

	snprintf(path, sizeof path, "/proc/%d/stat", proc->pid);

	if(fline(path, &buf, NULL)){
		int i;
		char *start = strrchr(buf, ')') + 2;
		char *iter;

		i = 0;
		for(iter = strtok(start, " \t"); iter; iter = strtok(NULL, " \t")){
			switch(i++){
				case 0:
				{
					/* state: convert to PROC_STATE */
					char c;
					sscanf(iter, "%c", &c);
					switch(c){
						case 'R': proc->state = PROC_STATE_RUN;     break;
						case 'S': proc->state = PROC_STATE_SLEEP;   break;
						case 'D': proc->state = PROC_STATE_DISK;    break;
						case 'T': proc->state = PROC_STATE_STOPPED; break;
						case 'Z': proc->state = PROC_STATE_ZOMBIE;  break;
						case 'X': proc->state = PROC_STATE_DEAD;    break;
						default:  proc->state = PROC_STATE_OTHER;   break;
					}
				}

#define INT(n, fmt, x) case n: sscanf(iter, fmt, x); break
				INT(1,  "%d", &proc->ppid);
				INT(4,  "%s",  proc->tty);
				INT(5,  "%u", &proc->pgrp);

				INT(11, "%lu", &proc->utime);
				INT(12, "%lu", &proc->stime);
				INT(13, "%lu", &proc->cutime);
				INT(14, "%lu", &proc->cstime);
#undef INT
			}
		}
		free(buf);

    machine_read_argv(proc);
    return 0;
	}else{
		return -1;
	}
}

const char *machine_proc_display_line(struct myproc *p)
{
	static char buf[64];

	if(global_thin){
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

int machine_proc_display_width()
{
	if(global_thin)
		return 9 + max_unam_len;
	else
		return 18 + max_unam_len + max_gnam_len + 1 + 5;
}

struct myproc *machine_proc_new(pid_t pid)
{
	struct myproc *this = NULL;
	char cmdln[32];
	struct stat st;

	this = umalloc(sizeof *this);
	memset(this, 0, sizeof *this);

	snprintf(cmdln, sizeof cmdln, "/proc/%d/task/%d/", pid, pid);
	if(stat(cmdln, &st) == 0){
		struct passwd *passwd;
		struct group  *group;

#define GETPW(idvar, var, truct, fn, member, id)  \
		truct = fn(id);                               \
		idvar = id;                                   \
		if(truct){                                    \
			var = ustrdup(truct->member);               \
		}else{                                        \
			char buf[8];                                \
			snprintf(buf, sizeof buf, "%d", id);        \
			var = ustrdup(buf);                         \
		}

		GETPW(this->uid, this->unam, passwd, getpwuid, pw_name, st.st_uid)
		GETPW(this->gid, this->gnam,  group, getgrgid, gr_name, st.st_gid)
	}

	this->pid       = pid;
	this->ppid      = -1;

	return this;
}

void machine_proc_get_more(struct myproc **procs)
{
	/* TODO: kernel threads */
	DIR *d = opendir("/proc");
	struct dirent *ent;

	if(!d){
		perror("opendir()");
		exit(1);
	}

	while((errno = 0, ent = readdir(d))){
		int pid;

		if(sscanf(ent->d_name, "%d", &pid) == 1
		&& !proc_get(procs, pid)){
			struct myproc *p = machine_proc_new(pid);

			if(p){
				proc_addto(procs, p);
			}
		}
	}

	if(errno){
		fprintf(stderr, "readdir(): %s\n", strerror(errno));
		exit(1);
	}

	closedir(d);
}

const char *machine_format_memory(struct sysinfo *info)
{
  static char memory_string[128];
  char *p;
  int i;

  p = memory_string;

  for(i=0; i<6; i++)
    p += snprintf(p, (sizeof memory_string) - (p - memory_string),
				"%s %s", format_kbytes(info->memory[i]), memorynames[i]);

  return memory_string;
}

const char *machine_format_cpu_pct(struct sysinfo *info)
{
	(void)info;
	return "todo: linux cpu pct";
}
