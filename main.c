#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/types.h>
#include <signal.h>

#include "proc.h"
#include "gui.h"
#include "util.h"
#include "machine.h"

#define MS_TO_US(n) ((n) * 1000)

uid_t global_uid     = 0;
int   global_force   = 0;
int   global_debug   = 0;
int   global_thin    = 0;
int   global_kernel  = 0;

int max_unam_len, max_gnam_len;

static struct myproc **proclist;

void extra_init()
{
	global_uid = getuid();

	/* for layout - username length */
	max_unam_len = longest_passwd_line("/etc/passwd");
	max_gnam_len = longest_passwd_line("/etc/group");
}

void signal_handler(int sig)
{
	gui_term();
	fprintf(stderr, "Caught signal %d. Bye!\n", sig);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	int i;

	signal(SIGINT,  signal_handler);
	signal(SIGTERM, signal_handler);

	for(i = 1; i < argc; i++){
		if(!strcmp(argv[i], "-f")){
			global_force = 1;
		}else if(!strcmp(argv[i], "-d")){
			global_debug = 1;
		}else if(!strcmp(argv[i], "-t")){
			global_thin = 1;
		}else if(!strcmp(argv[i], "-k")){
			global_kernel = 1;
		}else if(!strcmp(argv[i], "-v")){
			fprintf(stderr, "utop %s\n", "0.9");
			return 0;
		}else{
			fprintf(stderr,
							"Usage: %s [-f] [-d]\n"
							" -f: Don't prompt for lsof and strace\n"
							" -d: Debug mode\n"
							" -t: Thin mode (for small screens)\n"
							" -k: Show kernel threads\n"
							, *argv);
			return 1;
		}
	}

	extra_init();
	gui_init();

	proclist = proc_init();

	gui_run(proclist);

	gui_term();
	machine_term();

	return EXIT_SUCCESS;
}
