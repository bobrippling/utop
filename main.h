#ifndef MAIN_H
#define MAIN_H

extern int max_unam_len, max_gnam_len;

extern struct globals
{
	uid_t uid;
	int force;
	int debug;
	int kernel;
	int basename;
} globals;

extern int ps_from_file;

#endif
