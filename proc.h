#ifndef PROC_H
#define PROC_H

#include <sys/time.h>
#define CPUSTATES 5 // TODO

struct myproc
{
	pid_t pid, ppid;
  int jid;

	uid_t uid;
  gid_t gid;
  gid_t pgrp;
	char *unam, *gnam;

	char *shell_cmd;      /* allocated, from argv */
	char **argv;          /* allocated */
  int argc;
	char *argv0_basename; /* pointer to somewhere in argv[0] */

  enum
	{
		PROC_STATE_RUN,
		PROC_STATE_SLEEP,
		PROC_STATE_DISK,
		PROC_STATE_STOPPED,
		PROC_STATE_ZOMBIE,
		PROC_STATE_DEAD,
		PROC_STATE_OTHER,
	} state;

  char *tty;
  signed char nice;

	double pc_cpu;
	unsigned long utime, stime, cutime, cstime;

	/* important */
	struct myproc *hash_next;
	struct myproc *child_first, *child_next;

	/* only used for arrangement */
	struct myproc *next;

	union
	{
		struct
		{
			int flag;
		} freebsd;
		struct
		{
			int unused;
		} linux;
	} machine;
};

struct myproc **proc_init();
struct myproc  *proc_get(struct myproc **, pid_t);
void          proc_update(struct myproc **);
void          proc_handle_rename(struct myproc *this, struct myproc **procs);
void          proc_handle_renames(struct myproc **ps);
void          proc_cleanup(struct myproc **);
void          proc_addto(struct myproc **procs, struct myproc *p);

struct myproc  *proc_to_list(struct myproc **);
struct myproc  *proc_to_tree(struct myproc **);
struct myproc  *proc_find(  const char *, struct myproc **);
struct myproc  *proc_find_n(const char *, struct myproc **, int);
const char     *proc_str(struct myproc *p);
const char     *proc_state_str(struct myproc *p);
int            proc_listcontains(struct myproc **procs, pid_t pid);
int            proc_to_idx(struct myproc *p, struct myproc *parent, int *y);
struct myproc  *proc_from_idx(struct myproc *parent, int *idx);

struct myproc  *proc_first(struct myproc **procs);

void proc_dump(struct myproc **ps, FILE *f);

#define HASH_TABLE_SIZE 128

#endif
