#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <alloca.h>

#include "proc.h"

/*#define PHONY 1*/

struct proc *proc_to_list(struct proc **procs)
{
	struct proc *first, **next = &first;
	int i;

	for(i = 0; i < NPROCS; i++){
		struct proc *p;
		for(p = procs[i]; p; p = p->hash_next){
			*next = p;
			next = &p->next;
		}
	}
	*next = NULL;
	return first;
}

void proc_free(struct proc *p)
{
	free(p->argv0);
	free(p->proc_path);
	free(p->cmd);
	free(p);
}

struct proc *proc_frompid(pid_t pid)
{
	struct proc *this = NULL;
	FILE *f;
	char cmdln[32];

	snprintf(cmdln, sizeof cmdln, "/proc/%d/cmdline", pid);

	f = fopen(cmdln, "r");
	if(f){
		char buffer[256];

		if(fgets(buffer, sizeof buffer, f)){
			char *p;

			this = malloc(sizeof *this);
			if(!this){
				perror("malloc()");
				exit(1);
			}
			memset(this, 0, sizeof *this);

			this->pid       = pid;
			this->argv0     = strdup(buffer);
			this->proc_path = strdup(cmdln);
			this->cmd       = strdup(buffer);

			if(!this->argv0 || !this->proc_path || !this->cmd){
				perror("malloc()");
				exit(1);
			}

			p = strchr(this->cmd, ' ');
			if(p)
				*p = '\0';
		}

		fclose(f);
	}

	return this;
}

int proc_listcontains(struct proc **list, pid_t pid)
{
	struct proc *p;

	for(p = list[pid % NPROCS]; p; p = p->hash_next)
		if(p->pid == pid)
			return 1;

	return 0;
}

void proc_addto(struct proc **list, struct proc *p)
{
	struct proc *last;

	last = list[p->pid % NPROCS];
	if(last){
		while(last->hash_next)
			last = last->hash_next;
		last->hash_next = p;
	}else{
		list[p->pid % NPROCS] = p;
	}
}

struct proc **proc_init()
{
	int n = sizeof(struct proc *) * NPROCS;
	struct proc **p = malloc(n);
	memset(p, 0, n);
#ifdef PHONY
	p[0] = proc_frompid(1);
	p[1 % NPROCS] = proc_frompid(getpid());
#endif
	return p;
}

void proc_listall(struct proc **list, int *np)
{
	/* TODO: kernel threads */
	DIR *d = opendir("/proc");
	struct dirent *ent;
	int n = *np;

	if(!d){
		perror("opendir()");
		exit(1);
	}

	errno = 0;
	while((errno = 0, ent = readdir(d))){
		int pid;

		if(sscanf(ent->d_name, "%d", &pid) && !proc_listcontains(list, pid)){
			struct proc *p = proc_frompid(pid);

			if(p){
				proc_addto(list, p);
				n++;
			}
		}
	}

	if(errno){
		fprintf(stderr, "readdir(): %s\n", strerror(errno));
		exit(1);
	}

	*np = n;
	closedir(d);
}

void proc_update_single(struct proc *proc)
{
	/*
	char buf[256];
	char path[32];

	snprintf(path, sizeof path, "%s/stat", proc->proc_path);
	if(!fline(path, buf, sizeof buf)){
		char *start = strrchr(buf, ')') + 2;


	}
	*/

	proc->pc_cpu = rand();
}

void proc_update(struct proc **list, int *np)
{
	int i;
	int n;

#ifdef PHONY
	*np = 2;
	return;
#endif

	for(i = n = 0; i < NPROCS; i++){
		struct proc *p;
		struct proc **changeme = &list[i];

		for(p = list[i]; p; p = p->hash_next){
			struct stat st;

			if(stat(p->proc_path, &st)){
				*changeme = p->hash_next;

				proc_free(p);
			}else{
				proc_update_single(p);
				n++;
			}

			changeme = &p->hash_next;
		}
	}

	*np = n;
	proc_listall(list, np);
}
