#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util.h"
#include "structs.h"
#include "machine.h"
#include "proc.h"
#include "main.h"

static char *in_line(FILE *f)
{
	char *s = NULL;
	size_t n = 0;
	int ch;

	while((ch = fgetc(f)) != EOF){
		s = urealloc(s, ++n + 1);
		s[n-1] = ch;
		if(ch == '\n')
			break;
	}
	if(s)
		s[n] = '\0';

	return s;
}

static char **pipe_in(const char *cmd, size_t *pn, size_t skip)
{
	FILE *f = ps_from_file ? fopen("__ps", "r") : popen(cmd, "r");
	if(!f)
		return NULL;

	char **list = NULL;
	size_t n = 0;
	char *line;

	while((line = in_line(f))){
		if(skip > 0){
			free(line);
			skip--;
			continue;
		}
		list = urealloc(list, ++n * sizeof *list);

		char *nl = strrchr(line, '\n');
		if(nl)
			*nl = '\0';
		list[n-1] = line;
	}

	// TODO: check ferror
	if(ps_from_file)
		fclose(f);
	else
		pclose(f);

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

	static const char *ps_cmd = "ps -e -o pid,ppid,uid,gid,state,nice,tty,command";
	ps_list = pipe_in(ps_cmd, &ps_n, 1);
}

static const char *ps_find(pid_t search_pid)
{
	if(!ps_list)
		return NULL;

	for(size_t i = 0; i < ps_n; i++){
		pid_t pid;
		if(sscanf(ps_list[i], " %d", &pid) == 1 && pid == search_pid)
			return ps_list[i];
	}
	return NULL;
}

static char **ps_parse_argv(char *cmd, size_t *pargc)
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

int machine_update_proc(struct myproc *p)
{
	const char *l = ps_find(p->pid);
	pid_t pid, ppid;
	uid_t uid, gid;
	char stat[8] = { 0 };
	char tty[16];
	char cmd[256] = { 0 }; /* %c doesn't 0-terminate */
	int nice;

	/*                                       /[ \n\t]*(.*)/ */
	if(l && sscanf(l, " %d %d %d %d "
				"%7s %d %15s%*[ \n\t]%255c",
				&pid, &ppid, &uid, &gid,
				stat, &nice, tty, cmd) == 8)
	{
		p->uid = uid;
		p->gid = gid;

		argv_free(p->argc, p->argv);
		p->argv = ps_parse_argv(cmd, &p->argc);

		char *slash = strrchr(p->argv[0], '/');
		p->argv0_basename = slash ? slash + 1 : p->argv[0];

		if(!p->tty || strcmp(p->tty, tty))
			free(p->tty), p->tty = ustrdup(tty);

		p->nice = nice;

		p->state = proc_state_parse(stat[0]);

		machine_update_unam_gnam(p, p->uid, p->gid);
		/*
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

	if(!ps_list)
		return;

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
			machine_update_proc(p);
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
	(void)info;
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
	(void)si;
	return "abc??";
}

const char *machine_format_cpu_pct(struct sysinfo *si)
{
	(void)si;
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
