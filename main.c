#include <stdio.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/types.h>

#include "proc.h"
#include "gui.h"

#define MS_TO_US(n) ((n) * 1000)

int global_uid;

int main(int argc, char **argv)
{
	static struct proc **proclist;

	if(argc != 1){
		fprintf(stderr, "Usage: %s\n", *argv);
		return 1;
	}

	global_uid = getuid();

	gui_init();

	proclist = proc_init();

	gui_run(proclist);

	gui_term();

	return 0;
}
