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

#include "util.h"
#include "proc.h"
#include "machine.h"
#include "main.h"
#include "structs.h"

void machine_init(struct sysinfo *info)
{
	// nothing to do... for now.. dun dun..
	(void)info;
}

void machine_term()
{
}

// Taken from top(8)
void get_load_average(struct sysinfo *info)
{
	// TODO
	(void)info;
}

void get_mem_usage(struct sysinfo *info)
{
	// TODO
	(void)info;
}

void get_cpu_stats(struct sysinfo *info)
{
	// TODO
	(void)info;
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

		for(i = 0; i < p->argc; i++)
			free(p->argv[i]);
		free(p->argv);

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

	snprintf(buf, sizeof buf,
		"% 7d % 7d %-7s "
		"%-*s %-*s "
		"%3.1f"
		,
		p->pid, p->ppid, proc_state_str(p),
		max_unam_len, p->unam,
		max_gnam_len, p->gnam,
		p->pc_cpu
	);

	return buf;
}

struct myproc *machine_proc_new(pid_t pid, struct myproc **procs)
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

	proc_handle_rename(this, procs);

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
			struct myproc *p = machine_proc_new(pid, procs);

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
