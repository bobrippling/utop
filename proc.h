#ifndef PROC_H
#define PROC_H

struct proc
{
	char *proc_path;
	pid_t pid, ppid;
	int uid, gid;
	char *unam, *gnam;

	char *cmd;
	char *basename;
	char **argv;
	int basename_offset;

	int state, tty, pgrp;

	int pc_cpu;
	unsigned long utime, stime, cutime, cstime;

	struct proc *hash_next; /* important */

	/* only used for arrangement */
	struct proc *child_first, *child_next;
	struct proc *next;
};

struct procstat
{
	int count, running, owned, zombies;
	double loadavg[3];
	unsigned long cputime_total, cputime_period, uptime_secs;
};

struct proc **proc_init();
struct proc  *proc_get(   struct proc **, pid_t);
void          proc_update(struct proc **, struct procstat *);
void          proc_handle_renames(struct proc **);

struct proc  *proc_to_list(struct proc **);
struct proc  *proc_to_tree(struct proc **);
struct proc  *proc_find(  const char *, struct proc **);
struct proc  *proc_find_n(const char *, struct proc **, int);
const char   *proc_str(struct proc *p);
int           proc_to_idx(struct proc *p, struct proc *parent, int *y);
struct proc  *proc_from_idx(struct proc *parent, int *idx);

void proc_dump(struct proc **ps, FILE *f);

#define NPROCS 128

#endif
