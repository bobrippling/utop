#ifndef _MACHINE_H_
#define _MACHINE_H_

#include "proc.h"

#define GETSYSCTL(name, var) getsysctl(name, &(var), sizeof(var))

int pageshift;    /* log base 2 of the pagesize */

/* define pagetok in terms of pageshift */
#define LOG1024 10 // log_2(1024)
#define pagetok(size) ((size) << pageshift)

void init_machine(struct procstat *pst);
void machine_cleanup();
const char *uptime_from_boottime(time_t boottime);
const char* format_memory(int memory[6]);
void getsysctl(const char *name, void *ptr, size_t len);
void get_load_average(struct procstat *pst);
void get_mem_usage(struct procstat *pst);
void get_cpu_stats(struct procstat *pst);
const char *proc_state_str(struct kinfo_proc *pp);
#endif /* _MACHINE_H_ */
