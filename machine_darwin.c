/* extend freebsd */

#include <sys/types.h>

#define ki_uid  kp_eproc.e_pcred.p_ruid
#define ki_rgid kp_eproc.e_pcred.p_rgid

#define ki_pid  kp_proc.p_pid
#define ki_stat kp_proc.p_stat
#define ki_flag kp_proc.p_flag

/* kvm interface */
typedef struct kvm_t kvm_t;

kvm_t *kvm_open(void *, const char *path /* /dev/null */, void *, int flags, void *);
int    kvm_close(kvm_t *);

struct kinfo_proc   *kvm_getprocs(kvm_t *, int what, int flag, int *n);
char               **kvm_getargv( kvm_t *, struct kinfo_proc *, int flag);

typedef unsigned long  u_long;
typedef unsigned int   u_int;
typedef unsigned short u_short;
typedef unsigned char  u_char;

#define KERN_PROC_PROC KERN_PROC_PID

/* implementation */
#include "machine_freebsd.c"


kvm_t *kvm_open(void *unused1, const char *path, void *unused2, int flags, void *unused3)
{
	(void)unused1;
	(void)unused2;
	(void)unused3;
	(void)path;
	(void)flags;
	return (void *)0x1; // hack? hack.
}

int kvm_close(kvm_t *kvm)
{
	(void)kvm;
	return 0;
}

struct kinfo_proc *kvm_getprocs(kvm_t *kvm, int what, int flag, int *n)
{
	int eno;
	struct kinfo_proc *kp;
	int mib[4] = {
		CTL_KERN,
		KERN_PROC,
		what,
		flag
	};
	size_t sz = sizeof *kp;

	/*
	 * what = KERN_PROC_RGID KERN_PROC_PGRP KERN_PROC_PID KERN_PROC_RUID
	 *        KERN_PROC_SESSION KERN_PROC_TTY KERN_PROC_RUID KERN_PROC_UID
	 * flag = pid
	 */

	kp = malloc(sizeof *kp);

	eno = sysctl(mib, 4, kp, &sz, NULL, 0);
	if(eno){
		free(kp);
		return NULL;
	}

	return kp;
}

char **kvm_getargv(kvm_t *kvm, struct kinfo_proc *kp, int flag)
{
	static char *dummy[] = { "TODO_argv", NULL };
	return dummy;
}
