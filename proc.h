#ifndef PROC_H
#define PROC_H

struct sysinfo;

struct myproc **proc_init(void);
struct myproc  *proc_get(struct myproc **, pid_t);
void          proc_update(struct myproc **procs, struct sysinfo *info);
void          proc_cleanup(struct myproc **);
void          proc_addto(struct myproc **procs, struct myproc *p);
void          proc_create_shell_cmd(struct myproc *this);

struct myproc  *proc_to_list(struct myproc **);
struct myproc  *proc_to_tree(struct myproc **);
struct myproc  *proc_find(  const char *, struct myproc **);
struct myproc  *proc_find_n(const char *, struct myproc **, int);
const char     *proc_str(struct myproc *p);
const char     *proc_state_str(struct myproc *p);
int            proc_listcontains(struct myproc **procs, pid_t pid);
int            proc_to_idx(struct myproc **procs, struct myproc *searchee, int *py);
struct myproc  *proc_from_idx(struct myproc **procs, int *idx);

struct myproc  *proc_first(     struct myproc **procs);
struct myproc  *proc_first_next(struct myproc **procs);
void            proc_unmark(struct myproc **procs);
void            proc_mark_kernel(struct myproc **procs);

void proc_dump(struct myproc **ps, FILE *f);

enum proc_state proc_state_parse(char c);

#define HASH_TABLE_SIZE 128

#define ITER_PROC_HEADS(ty, p, procs)  \
	for(ty p = proc_first(procs);        \
			p;                               \
			p = proc_first_next(procs))

#define PROC_IS_KERNEL(p) ((p)->ppid == 0 || (p)->ppid == 2)
#define PROC_IS_OWNED(p) ((p)->uid == globals.uid)
#endif
