#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>

#include "proc.h"
#include "util.h"

#define ITER_PROCS(i, p, ps) \
	for(i = 0; i < NPROCS; i++) \
		for(p = ps[i]; p; p = p->hash_next)

void proc_update_single(struct proc *proc, struct proc **procs);

void proc_free(struct proc *p)
{
	char **iter;
	for(iter = p->argv; *iter; iter++)
		free(*iter);
	free(p->argv);
	free(p->proc_path);
	free(p->cmd);
	free(p->basename);
	free(p->unam);
	free(p->gnam);
	free(p);
}

struct proc *proc_new(pid_t pid)
{
	struct proc *this = NULL;
	char cmdln[32];
	int len;
	char *buffer, *p;

	snprintf(cmdln, sizeof cmdln, "/proc/%d/cmdline", pid);

	if(fline(cmdln, &buffer, &len)){
		struct stat st;
		int argc;
		int i;

		this = umalloc(sizeof *this);

		for(i = 0, argc = 1; i < len; i++)
			if(!buffer[i])
				argc++;

		this->argv = umalloc((argc + 1) * sizeof *this->argv);
		for(p = buffer, i = argc = 0; i < len; i++)
			if(!buffer[i]){
				this->argv[argc++] = ustrdup(p);
				p = buffer + i + 1;
			}
		if(!argc){
			this->argv[argc] = umalloc(8);
			snprintf(this->argv[argc++], 8, "%d", pid);
		}

		this->argv[argc] = NULL;

		this->proc_path = ustrdup(cmdln);
		*strrchr(this->proc_path, '/') = '\0';

		if((p = strchr(this->argv[0], ':'))){
			*p = '\0';
			this->basename = ustrdup(this->argv[0]);
			*p = ':';
			this->basename_offset = 0;
		}else{
			this->basename = strrchr(this->argv[0], '/');
			if(!this->basename++)
				this->basename = this->argv[0];
			this->basename_offset = this->basename - this->argv[0];

			this->basename = ustrdup(this->basename);
		}

		snprintf(cmdln, sizeof cmdln, "/proc/%d/task/%d/", pid, pid);
		if(!stat(cmdln, &st)){
			struct passwd *passwd;
			struct group  *group;

#define GETPW(idvar, var, truct, fn, member, id) \
			truct = fn(id); \
			idvar = id; \
			if(truct){ \
				var = ustrdup(truct->member); \
			}else{ \
				char buf[8]; \
				snprintf(buf, sizeof buf, "%d", id); \
				var = ustrdup(buf); \
			} \

			GETPW(this->uid, this->unam, passwd, getpwuid, pw_name, st.st_uid)
			GETPW(this->gid, this->gnam,  group, getgrgid, gr_name, st.st_gid)
		}

		for(i = 0; i < len; i++)
			switch(buffer[i]){
				case '\0':
				case '\n':
					buffer[i] = ' ';
					break;
			}
		buffer[len] = '\0';

		this->cmd       = buffer;

		this->pid       = pid;
		this->ppid      = -1;
	}

	return this;
}

struct proc *proc_get(struct proc **procs, pid_t pid)
{
	struct proc *p;

	if(pid >= 0)
		for(p = procs[pid % NPROCS]; p; p = p->hash_next)
			if(p->pid == pid)
				return p;

	return NULL;
}

int proc_listcontains(struct proc **procs, pid_t pid)
{
	return !!proc_get(procs, pid);
}

void proc_addto(struct proc **procs, struct proc *p)
{
	struct proc *last;

	last = procs[p->pid % NPROCS];
	if(last){
		while(last->hash_next)
			last = last->hash_next;
		last->hash_next = p;
	}else{
		procs[p->pid % NPROCS] = p;
	}
}

struct proc **proc_init()
{
	int n = sizeof(struct proc *) * NPROCS;
	struct proc **p = umalloc(n);
	memset(p, 0, n);
	return p;
}

void proc_listall(struct proc **procs, struct procstat *stat)
{
	/* TODO: kernel threads */
	DIR *d = opendir("/proc");
	struct dirent *ent;

	if(!d){
		perror("opendir()");
		exit(1);
	}

	errno = 0;
	while((errno = 0, ent = readdir(d))){
		int pid;

		if(sscanf(ent->d_name, "%d", &pid) && !proc_listcontains(procs, pid)){
			struct proc *p = proc_new(pid);

			if(p){
				proc_addto(procs, p);
				stat->count++;
			}
		}
	}

	if(errno){
		fprintf(stderr, "readdir(): %s\n", strerror(errno));
		exit(1);
	}

	closedir(d);
}

const char *proc_str(struct proc *p)
{
	static char buf[256];

	snprintf(buf, sizeof buf,
			"{ pid=%d, ppid=%d, state=%c, cmd=\"%s\" }",
			p->pid, p->ppid, p->state, p->cmd);

	return buf;
}

void proc_update_single(struct proc *proc, struct proc **procs)
{
	char *buf;
	char path[32];

	snprintf(path, sizeof path, "%s/stat", proc->proc_path);

	if(fline(path, &buf, NULL)){
		char *start = strrchr(buf, ')') + 2;
		char *iter;
		int i = 0;
		pid_t oldppid = proc->ppid;

		for(iter = strtok(start, " \t"); iter; iter = strtok(NULL, " \t")){
#define INT(n, fmt, x) case n: sscanf(iter, fmt, x); break
			switch(i++){
				INT(0, "%c", (char *)&proc->state);
				INT(1, "%d", &proc->ppid);
				INT(4, "%d", &proc->tty);
				INT(5, "%d", &proc->pgrp);
#undef INT
			}
		}

		free(buf);

		if(proc->ppid && oldppid != proc->ppid){
			struct proc *parent = proc_get(procs, proc->ppid);
			struct proc *iter;

			if(parent->child_first){
				iter = parent->child_first;

				while(iter->child_next)
					iter = iter->child_next;

				iter->child_next = proc;
			}else{
				parent->child_first = proc;
			}
		}
	}
#if 0
					cpu     = cpuusage(root);
					mem     = int(head("%s/statm" % root).split(' ')[0]);
					time    = self.time(stats[11]) # usermode time;
					proc->pc_cpu = 0;
#endif
}

void proc_update(struct proc **procs, struct procstat *pst)
{
	int i;
	int count, running, owned;

	count = 1; /* init */
	running = owned = 0;

	for(i = 0; i < NPROCS; i++){
		struct proc **changeme;
		struct proc *p;

		changeme = &procs[i];
		for(p = procs[i]; p; ){
			if(access(p->proc_path, F_OK)){
				struct proc *next = p->hash_next;
				struct proc *parent = proc_get(procs, p->ppid);

				if(parent){
					struct proc *iter, **prev;

					prev = &parent->child_first;
					for(iter = parent->child_first; iter; iter = iter->child_next)
						if(iter->pid == p->pid){
							*prev = iter->child_next;
							break;
						}else{
							prev = &iter->child_next;
						}
				}

				*changeme = next;
				proc_free(p);
				p = next;
			}else{
				if(p){
					extern int global_uid;

					proc_update_single(p, procs);
					if(p->ppid != 0 && p->ppid != 2)
						count++; /* else it's kthreadd or init */

					if(p->state == 'R')
						running++;
					if(p->uid == global_uid)
						owned++;
				}

				changeme = &p->hash_next;
				p = p->hash_next;
			}
		}
	}

	pst->count   = count;
	pst->running = running;
	pst->owned   = owned;

	proc_listall(procs, pst);
}

void proc_dump(struct proc **ps, FILE *f)
{
	struct proc *p;
	int i;

	ITER_PROCS(i, p, ps)
		fprintf(f, "%s\n", proc_str(p));
}

struct proc *proc_find(const char *str, struct proc **ps)
{
	return proc_find_n(str, ps, 0);
}

struct proc *proc_find_n(const char *str, struct proc **ps, int n)
{
	struct proc *p;
	int i;

	ITER_PROCS(i, p, ps)
		if(strstr(p->cmd, str) && n-- <= 0)
			return p;

	return NULL;
}

int proc_to_idx(struct proc *p, struct proc *parent, int *py)
{
	struct proc *iter;
	int ret = 0;
	int y;

	if(p == parent)
		return 1;

	y = *py;
	for(iter = parent->child_first; iter; iter = iter->child_next)
		if(p == iter || (++y, proc_to_idx(p, iter, &y))){
			ret = 1;
			break;
		}

	*py = y;
	return ret;
}

struct proc *proc_from_idx(struct proc *parent, int *idx)
{
	struct proc *iter, *ret = NULL;
	int i = *idx;
#define RET(x) do{ ret = x; goto fin; }while(0)

	if(i <= 0)
		return parent;

	for(iter = parent->child_first; iter; iter = iter->child_next){
		if(--i <= 0){
			RET(iter);
		}else{
			struct proc *p = proc_from_idx(iter, &i);
			if(p)
				RET(p);
		}
	}

fin:
	*idx = i;
	return ret;
}
