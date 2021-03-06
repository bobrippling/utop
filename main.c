#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/types.h>
#include <signal.h>

#include "structs.h"
#include "proc.h"
#include "gui.h"
#include "util.h"
#include "machine.h"
#include "main.h"

struct globals globals;

int max_unam_len, max_gnam_len;
int ps_from_file;

static struct myproc **proclist;

static void extra_init()
{
	globals.uid = getuid();

	/* for layout - username length */
	max_unam_len = longest_passwd_line("/etc/passwd");
	max_gnam_len = longest_passwd_line("/etc/group");
}

static void signal_handler(int sig)
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
			globals.force = 1;
		}else if(!strcmp(argv[i], "-d")){
			globals.debug = 1;
		}else if(!strcmp(argv[i], "-b")){
			globals.basename = 1;
		}else if(!strcmp(argv[i], "-k")){
			globals.kernel = 1;
		}else if(!strcmp(argv[i], "-P")){
			ps_from_file ^= 1;
		}else if(!strcmp(argv[i], "-v")){
			fprintf(stderr, "utop %s\n", "0.9");
			return 0;
		}else{
			fprintf(stderr,
							"Usage: %s [-f] [-d] [-P]\n"
							" -f: Don't prompt for lsof and strace\n"
							" -d: Debug mode\n"
							" -b: Only show program basenames\n"
							" -k: Show kernel threads\n"
							" -P: Read ps listing from ./__ps\n"
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
