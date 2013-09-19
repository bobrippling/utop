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
	enum
	{
		disp_tree,
		disp_type
	} disp;
#define disp_COUNT (disp_type + 1)

} globals;

#endif
