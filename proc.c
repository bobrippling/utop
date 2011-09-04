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
#include <time.h>

#include "proc.h"
#include "util.h"

#define ITER_PROCS(i, p, ps) \
	for(i = 0; i < NPROCS; i++) \
		for(p = ps[i]; p; p = p->hash_next)

static void proc_update_single(struct proc *proc, struct proc **procs, struct procstat *pst);
static void proc_handle_rename(struct proc *p);

void proc_free(struct proc *p)
{
	char **iter;
	if(p->argv)
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

static void getprocstat(struct procstat *pst)
{
	static time_t last;
	time_t now;

	if(last + 3 < (now = time(NULL))){
		FILE *f;
		char buf[1024];
		unsigned long usertime, nicetime, systemtime, irq, sirq, idletime, iowait, steal, guest;

		last = now;

		if((f = fopen("/proc/stat", "r"))){
			for(;;){
				unsigned long totaltime, totalperiod;

				if(!fgets(buf, sizeof buf - 1, f))
					break;

				if(sscanf(buf, "cpu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
							&usertime, &nicetime, &systemtime, &idletime, &iowait,
							&irq, &sirq, &steal, &guest) == 10){

					totaltime =
						usertime +
						nicetime +
						systemtime +
						irq +
						sirq +
						idletime +
						iowait +
						steal +
						guest;

					totalperiod = totaltime - pst->cputime_total;

					pst->cputime_total  = totaltime;
					pst->cputime_period = totalperiod;

					break;
				}
			}
			fclose(f);
		}
	}

#define NIL(x) if(!x) x = 1
	NIL(pst->cputime_total);
	NIL(pst->cputime_period);
#undef NIL
}

struct proc *proc_new(pid_t pid)
{
	struct proc *this = NULL;
	char cmdln[32];
	struct stat st;

	snprintf(cmdln, sizeof cmdln, "/proc/%d/cmdline", pid);

	this = umalloc(sizeof *this);
	memset(this, 0, sizeof *this);

	this->proc_path = ustrdup(cmdln);
	*strrchr(this->proc_path, '/') = '\0';

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

	this->pid       = pid;
	this->ppid      = -1;

	proc_handle_rename(this);

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

static void proc_handle_rename(struct proc *this)
{
	char cmdln[32];
	char *buffer, *p, *argv0end;
	int len, i, argc;

	buffer = argv0end = NULL;
	len = 0;

	snprintf(cmdln, sizeof cmdln, "/proc/%d/cmdline", this->pid);

	if(fline(cmdln, &buffer, &len)){
		argc = 0;
		for(i = 0, argc = 1; i < len; i++)
			if(!buffer[i])
				argc++;

		if(this->argv){
			for(i = 0; this->argv[i]; i++)
				free(this->argv[i]);
			free(this->argv);
		}

		this->argv = umalloc((argc + 1) * sizeof *this->argv);
		for(p = buffer, i = argc = 0; i < len; i++)
			switch(buffer[i]){
				case '\0':
					if(!argv0end)
						argv0end = buffer + i;

					this->argv[argc++] = ustrdup(p);
					p = buffer + i + 1;
				case '\n':
					buffer[i] = ' ';
			}
		buffer[len] = '\0';

		free(this->basename);
		if(!argv0end)
			argv0end = buffer + len;

		*argv0end = '\0';
		if((p = strchr(buffer, ':'))){
			*p = '\0';
			this->basename = ustrdup(buffer);
			*p = ':';
			this->basename_offset = 0;
		}else{
			this->basename = strrchr(buffer, '/');
			if(!this->basename++)
				this->basename = buffer;
			this->basename_offset = this->basename - buffer;

			this->basename = ustrdup(this->basename);
		}
		*argv0end = ' ';

		if(!argc)
			this->argv[argc++] = ustrdup(this->basename);
		this->argv[argc] = NULL;

		free(this->cmd);
		this->cmd       = buffer;
	}
}

static void proc_update_single(struct proc *proc, struct proc **procs, struct procstat *pst)
{
	char *buf;
	char path[32];

	snprintf(path, sizeof path, "%s/stat", proc->proc_path);

	if(fline(path, &buf, NULL)){
		char *start = strrchr(buf, ')') + 2;
		char *iter;
		int i = 0;
		unsigned long prevtime;
		pid_t oldppid = proc->ppid;

		prevtime = proc->utime + proc->stime;

		for(iter = strtok(start, " \t"); iter; iter = strtok(NULL, " \t")){
#define INT(n, fmt, x) case n: sscanf(iter, fmt, x); break
			switch(i++){
				INT(0,  "%c", (char *)&proc->state);
				INT(1,  "%d", &proc->ppid);
				INT(4,  "%d", &proc->tty);
				INT(5,  "%d", &proc->pgrp);

				INT(11, "%lu", &proc->utime);
				INT(12, "%lu", &proc->stime);
				INT(13, "%lu", &proc->cutime);
				INT(14, "%lu", &proc->cstime);
#undef INT
			}
		}

		proc->pc_cpu =
			(proc->utime + proc->stime - prevtime) /
			pst->cputime_period * 100.f;

#if 0
		fprintf(stderr, "proc[%d]->cpu = (%lu + %lu - %lu) / %lu = %d\n",
				proc->pid,
				proc->utime, proc->stime, prevtime,
				pst->cputime_total, proc->pc_cpu);
#endif

#if 0
		cpu     = cpuusage(root);
		mem     = int(head("%s/statm" % root).split(' ')[0]);
		time    = self.time(stats[11]) # usermode time;
		proc->pc_cpu = 0;
#endif

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
}

void proc_update(struct proc **procs, struct procstat *pst)
{
	int i;
	int count, running, owned;

	getprocstat(pst);

	count = 1; /* init */
	running = owned = 0;

	for(i = 0; i < NPROCS; i++){
		struct proc **change_me;
		struct proc *p;

		change_me = &procs[i];
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

				*change_me = next;
				proc_free(p);
				p = next;
			}else{
				if(p){
					extern int global_uid;

					proc_update_single(p, procs, pst);
					if(p->ppid != 0 && p->ppid != 2)
						count++; /* else it's kthreadd or init */

					if(p->state == 'R')
						running++;
					if(p->uid == global_uid)
						owned++;
				}

				change_me = &p->hash_next;
				p = p->hash_next;
			}
		}
	}

	pst->count   = count;
	pst->running = running;
	pst->owned   = owned;

	proc_listall(procs, pst);
}

void proc_handle_renames(struct proc **ps)
{
	struct proc *p;
	int i;

	ITER_PROCS(i, p, ps)
		proc_handle_rename(p);
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
