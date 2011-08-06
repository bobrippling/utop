#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/types.h>

#include "proc.h"
#include "gui.h"

#define MS_TO_US(n) ((n) * 1000)

int global_uid   = 0;
int global_force = 0;

int main(int argc, char **argv)
{
	static struct proc **proclist;
	int i;

	for(i = 1; i < argc; i++)
		if(!strcmp(argv[i], "-f")){
			global_force = 1;
		}else{
			fprintf(stderr,
					"Usage: %s [-f]\n"
					" -f: Don't prompt for lsof and strace\n"
					, *argv);
			return 1;
		}

	global_uid = getuid();

	gui_init();

	proclist = proc_init();

	gui_run(proclist);

	gui_term();

	return 0;
}
