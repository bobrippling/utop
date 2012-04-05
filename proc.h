#ifndef PROC_H
#define PROC_H

struct sysinfo;

struct myproc **proc_init();
struct myproc  *proc_get(struct myproc **, pid_t);
void          proc_update(struct myproc **procs, struct sysinfo *info);
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
