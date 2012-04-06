#ifndef _MACHINE_H_
#define _MACHINE_H_

struct sysinfo;
struct myproc;

void machine_init(struct sysinfo *info);
void machine_term(void);

const char *uptime_from_boottime(time_t boottime);
const char *format_memory(int memory[6]);
const char *format_cpu_pct(double *cpu_pct);

void machine_update(struct sysinfo *);

int    machine_proc_exists(struct myproc *);

int    machine_update_proc(struct myproc *proc, struct myproc **procs);
/* 0 on success, non-zero on error */

void machine_proc_get_more(struct myproc **);

const char *machine_proc_display_line(struct myproc *p);
int machine_display_width(void);

const char *machine_format_memory( struct sysinfo *);
const char *machine_format_cpu_pct(struct sysinfo *);

void machine_update(struct sysinfo *);

#endif
