#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/types.h>

#include "proc.h"
#include "gui.h"
#include "util.h"
#include "main.h"

#define MS_TO_US(n) ((n) * 1000)

uid_t global_uid   = 0;
int   global_force = 0;
int   global_debug = 0;

int max_unam_len, max_gnam_len;

void extra_init()
{
	global_uid = getuid();

	/* for layout - username length */
	max_unam_len = longest_passwd_line("/etc/passwd");
	max_gnam_len = longest_passwd_line("/etc/group");
}

int main(int argc, char **argv)
{
	static struct myproc **proclist;
	int i;

	for(i = 1; i < argc; i++){
		if(!strcmp(argv[i], "-f")){
			global_force = 1;
		}else if(!strcmp(argv[i], "-d")){
			global_debug = 1;
		}else{
			fprintf(stderr,
					"Usage: %s [-f] [-d]\n"
					" -f: Don't prompt for lsof and strace\n"
					" -d: Debug mode\n"
					, *argv);
			return 1;
		}
	}

	extra_init();

	gui_init();

	proclist = proc_init();

	gui_run(proclist);

	gui_term();

	return 0;
}
