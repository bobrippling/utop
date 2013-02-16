#include "machine.h"

#warning machine::ps incomplete

// headers:
const char *ps_cmd = "ps -e -o pid,ppid,stat,tty,cmd ";

static char **pipe_in(const char *cmd, size_t *pn)
{
	FILE *f = popen(cmd, "r");
	if(!f)
		return NULL;

	char **list = NULL;
	size_t n = 0;

	char buf[1024]; // TODO: getline, etc
	while(fgets(buf, sizeof(buf), f)){
		list = urealloc(list, ++n * sizeof *list);
		list[n-1] = strdup(buf);
	}

	// TODO: check ferror

	pclose(cmd); // TODO: check

	*pn = n;
	return list;
}

int machine_proc_exists(struct myproc *p)
{
}

int machine_update_proc(struct myproc *proc, struct myproc **procs)
{
}

void machine_proc_get_more(struct myproc **ps)
{
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

const char *uptime_from_boottime(time_t boottime)
{
	return "?";
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

const char *machine_format_memory( struct sysinfo *)
{
}

const char *machine_format_cpu_pct(struct sysinfo *)
{
}


const char *machine_proc_display_line(struct myproc *p)
{
}

int machine_proc_display_width(void)
{
}
