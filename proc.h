#ifndef PROC_H
#define PROC_H

struct proc
{
	char *proc_path;
	pid_t pid, ppid;
	int uid, gid;
	char *unam, *gnam;

	char *cmd;
	char *argv0, *basename;
	int basename_offset;

	int state, tty, pgrp;

	int pc_cpu;

	struct proc *hash_next; /* important */

	/* only used for arrangement */
	struct proc *child_first, *child_next;
	struct proc *next;
};

struct procstat
{
	int count, running;
};

struct proc **proc_init();
struct proc  *proc_get(   struct proc **, pid_t);
void          proc_update(struct proc **, struct procstat *);

struct proc  *proc_to_list(struct proc **);
struct proc  *proc_to_tree(struct proc **);
struct proc *proc_find(const char *, struct proc **);
const char   *proc_str(struct proc *p);
int           proc_offset(struct proc *p, struct proc *parent, int *found);

void proc_dump(struct proc **ps, FILE *f);

#define NPROCS 128

#endif
