#ifndef _MACHINE_H_
#define _MACHINE_H_

void        machine_init(struct procstat *pst);
void        machine_term(void);

const char *uptime_from_boottime(time_t boottime);
const char *format_memory(int memory[6]);
const char *format_cpu_pct(double cpu_pct[CPUSTATES]);

void get_load_average(struct procstat *pst);
void get_mem_usage(struct procstat *pst);
void get_cpu_stats(struct procstat *pst);

char **machine_get_argv(pid_t pid);

int    machine_proc_exists(pid_t pid);
int    machine_update_proc(struct myproc *proc, struct procstat *pst);
void   machine_proc_listall(struct myproc **procs, struct procstat *stat);

#endif /* _MACHINE_H_ */
