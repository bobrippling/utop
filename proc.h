#ifndef PROC_H
#define PROC_H

#include <sys/param.h>
#include <sys/errno.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/user.h>
#include <sys/vmmeter.h>

#include <err.h>
#include <kvm.h>
#include <math.h>
#include <nlist.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <vis.h>
#include <grp.h>

// defined in proc.c
//extern char *state_abbrev[];

struct myproc
{
	pid_t pid, ppid;
	uid_t uid;
  gid_t gid;
  int jid; // Jail ID
	char *unam, *gnam;

	char *cmd;
	char *basename;
	char **argv;
  int argc;
	int basename_offset;

  char state;
	char *state_str;
  char *tty;
  gid_t pgrp;
  signed char nice;
  long flag; // P* flags

	double pc_cpu;
	unsigned long utime, stime, cutime, cstime;

	/* important */
	struct myproc *hash_next;
	struct myproc *child_first, *child_next;

	/* only used for arrangement */
	struct myproc *next;
};

struct procstat
{
	int count, running, owned, zombies, ncpus;
	struct timeval boottime;
  double loadavg[3];
  double fscale;
  long cpu_cycles[CPUSTATES]; // raw cpu counter
  double cpu_pct[CPUSTATES]; // percentage
  int memory[7];
};

struct myproc **proc_init();
struct myproc  *proc_get(struct myproc **, pid_t);
void          proc_update(struct myproc **, struct procstat *);
void          proc_handle_renames(struct myproc **);
void          proc_cleanup(struct myproc **);
void          proc_handle_rename(struct myproc *this);
void          proc_addto(struct myproc **procs, struct myproc *p);

struct myproc  *proc_to_list(struct myproc **);
struct myproc  *proc_to_tree(struct myproc **);
struct myproc  *proc_find(  const char *, struct myproc **);
struct myproc  *proc_find_n(const char *, struct myproc **, int);
const char     *proc_str(struct myproc *p);
int            proc_listcontains(struct myproc **procs, pid_t pid);
int            proc_to_idx(struct myproc *p, struct myproc *parent, int *y);
struct myproc  *proc_from_idx(struct myproc *parent, int *idx);
struct myproc  *procs_find_root(struct myproc **procs);

void proc_dump(struct myproc **ps, FILE *f);

#define HASH_TABLE_SIZE 128

#endif
