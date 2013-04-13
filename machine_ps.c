#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/time.h>
#include <sys/types.h>

#include "util.h"
#include "structs.h"
#include "machine.h"
#include "proc.h"

static char **pipe_in(const char *cmd, size_t *pn, size_t skip)
{
	FILE *f = popen(cmd, "r");
	if(!f)
		return NULL;

	char **list = NULL;
	size_t n = 0;

	char buf[1024]; // TODO: getline, etc
	while(fgets(buf, sizeof(buf), f)){
		if(skip > 0){
			skip--;
			continue;
		}
		list = urealloc(list, ++n * sizeof *list);
		char *nl = strrchr(buf, '\n');
		if(nl)
			*nl = '\0';
		list[n-1] = ustrdup(buf);
	}

	// TODO: check ferror

	pclose(f); // TODO: check

	*pn = n;
	return list;
}

static char **ps_list;
static size_t ps_n;

static void ps_update(void)
{
	if(ps_list){
		for(size_t i = 0; i < ps_n; i++)
			free(ps_list[i]);
		free(ps_list);
	}

	static const char *ps_cmd = "ps -e -o pid,ppid,uid,gid,stat,tty,command";
	ps_list = pipe_in(ps_cmd, &ps_n, 1);
}

static const char *ps_find(pid_t search_pid)
{
	for(size_t i = 0; i < ps_n; i++){
		pid_t pid;
		if(sscanf(ps_list[i], " %d", &pid) == 1 && pid == search_pid)
			return ps_list[i];
	}
	return NULL;
}

static char **ps_parse_argv(char *cmd, int *pargc)
{
	char **argv = NULL;
	int argc = 0;

	for(char *s = strtok(cmd, " \t"); s; s = strtok(NULL, " \t")){
		argv = urealloc(argv, (++argc + 1) * sizeof *argv);
		argv[argc - 1] = ustrdup(s);
	}

	argv[argc] = NULL;

	*pargc = argc;
	return argv;
}

int machine_proc_exists(struct myproc *p)
{
	return !!ps_find(p->pid);
}

int machine_update_proc(struct myproc *p, struct myproc **procs)
{
	const char *l = ps_find(p->pid);
	pid_t pid, ppid;
	uid_t uid, gid;
	char stat[4];
	char tty[16];
	char cmd[256] = { 0 }; /* %c doesn't 0-terminate */

	/*                                       /[ \n\t]*(.*)/ */
	if(l && sscanf(l, " %d %d %d %d %3s %15s%*[ \n\t]%255c",
				&pid, &ppid, &uid, &gid,
				stat, tty, cmd) == 7)
	{
		p->uid  = uid;
		p->gid  = gid;

		argv_free(p->argc, p->argv);
		p->argv = ps_parse_argv(cmd, &p->argc);

		char *slash = strrchr(p->argv[0], '/');
		p->argv0_basename = slash ? slash + 1 : p->argv[0];

		if(!p->tty || strcmp(p->tty, tty))
			free(p->tty), p->tty = ustrdup(tty);

		p->state = PROC_STATE_OTHER;
		/*
			 gid_t pgrp;
			 char *unam, *gnam;

			 enum
			 {
			 PROC_STATE_RUN,
			 PROC_STATE_SLEEP,
			 PROC_STATE_DISK,
			 PROC_STATE_STOPPED,
			 PROC_STATE_ZOMBIE,
			 PROC_STATE_DEAD,
			 PROC_STATE_TRACE,
			 PROC_STATE_OTHER,
			 } state;

			 char *tty;
			 signed char nice;

			 double pc_cpu;
			 unsigned long utime, stime, cutime, cstime;
			 unsigned long cputime;
			 unsigned long memsize;
			 */
		return 0;
	}

	return -1;
}

void machine_proc_get_more(struct myproc **procs)
{
	ps_update();

	for(size_t i = 0; i < ps_n; i++){
		pid_t pid, ppid;

		if(sscanf(ps_list[i], " %d %d ", &pid, &ppid) == 2
		&& !proc_get(procs, pid))
		{
			struct myproc *p = umalloc(sizeof *p);

			/* bare minimum - rest is done in _update */
			p->pid  = pid;
			p->ppid = ppid;

			proc_addto(procs, p);
		}else{
			//fprintf(stderr, "couldn't sscanf(\"%s\")\n", ps[i]);
		}
	}
}

/* TODO */
void machine_init(struct sysinfo *info)
{
	memset(info, 0, sizeof *info);
}

void machine_term(void)
{
}

void machine_update(struct sysinfo *info)
{
}

const char *format_memory(int memory[6])
{
	*memory = 0;
	return "?";
}

const char *format_cpu_pct(double *cpu_pct)
{
	*cpu_pct = 0;
	return "?";
}

const char *machine_format_memory(struct sysinfo *si)
{
	return "abc??";
}

const char *machine_format_cpu_pct(struct sysinfo *si)
{
	return "xyz??";
}


const char *machine_proc_display_line(struct myproc *p)
{
	return machine_proc_display_line_default(p);
}

int machine_proc_display_width(void)
{
	return machine_proc_display_width_default();
}
