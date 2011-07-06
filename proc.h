#ifndef PROC_H
#define PROC_H

struct proc
{
	char *proc_path;
	char *cmd, *argv0;
	pid_t pid;

	int pc_cpu;

	struct proc *hash_next, *next; /* hash list */
};

struct proc **proc_init();
struct proc  *proc_to_list(struct proc **);
void          proc_update(struct proc **list, int *);

#define NPROCS 128

#endif
