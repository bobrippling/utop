#ifndef _MACHINE_H_
#define _MACHINE_H_

struct sysinfo;
struct myproc;

void machine_init(struct sysinfo *info);
void machine_term(void);
void machine_update(struct sysinfo *info);

const char *uptime_from_boottime(time_t boottime);
const char *format_memory(int memory[6]);
const char *format_cpu_pct(double *cpu_pct);

int    machine_proc_exists(struct myproc *);

int    machine_update_proc(struct myproc *proc);
/* 0 on success, non-zero on error */

void machine_proc_get_more(struct myproc **);

const char *machine_proc_display_line(struct myproc *p);
int machine_proc_display_width(void);

const char *machine_format_memory( struct sysinfo *);
const char *machine_format_cpu_pct(struct sysinfo *);
#endif
