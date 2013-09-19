#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include <ncurses.h>

#include "proc_state.h"
#include "sysinfo.h"
#include "proc.h"

#include "main.h"
#include "util.h"

#include "gui.h"
#include "gui_tree.h"

#include "machine.h"

#include "config_ui.h"

void show_proc_tree(struct myproc *proc, int *py, int indent, const int top)
{
	int y = *py;

	proc->mark = 1;

	if(y >= LINES)
		return;

	if(y >= top){ // otherwise we're iterating over a process that's above pos_top
		const int is_owned         = PROC_IS_OWNED(proc);
		const int is_locked        = proc->pid == lock_proc_pid;
		const int is_searched      = proc      == search_proc;
		const int is_searched_alt  = *search_str
			                           && proc->shell_cmd
			                           && strstr(proc->shell_cmd, search_str);

		const unsigned linebuf_len = COLS + pos_x + 1;
		char *linebuf = umalloc(linebuf_len);
		memset(linebuf, ' ', linebuf_len);
		linebuf[linebuf_len-1] = '\0';

		int linebuf_used = snprintf(linebuf, linebuf_len, "%s",
				machine_proc_display_line(proc));

		const unsigned total_indent = SPACE_CMDLINE + SPACE_INDENT * indent;
		if(linebuf_used >= 0 && (unsigned)linebuf_used < linebuf_len){
			char *end = linebuf + linebuf_used;
			*end = ' ';

			if((unsigned)linebuf_used + total_indent < linebuf_len){
				char *linepos = end + total_indent;

				snprintf(linepos, linebuf_len - (linepos - linebuf),
						"%s", globals.basename ? proc->argv0_basename : proc->shell_cmd);
			}
		}

		move(y, 0);
		clrtoeol();

		if(is_searched)
			attron(ATTR_SEARCH);
		else if(is_locked)
			attron(ATTR_LOCK);
		else if(is_searched_alt)
			attron(ATTR_SEARCH_ALT);
		else if(!is_owned)
			attron(ATTR_NOT_OWNED);

		printw("% 7d %s", proc->pid, linebuf + pos_x);

		if(is_searched)
			attroff(ATTR_SEARCH);
		else if(is_locked)
			attroff(ATTR_LOCK);
		else if(is_searched_alt)
			attroff(ATTR_SEARCH_ALT);
		else if(!is_owned)
			attroff(ATTR_NOT_OWNED);

		/* basename shading */
		if(!globals.basename
		&& is_owned
		&& !is_locked
		&& !is_searched
		&& !is_searched_alt
		&& proc->argv)
		{
			const int bname_off = proc->argv0_basename - proc->argv[0];
			int off = machine_proc_display_width()
				+ bname_off + total_indent - pos_x;

			if(2 <= off && off < COLS){
				const int bn_len = strlen(proc->argv0_basename);

				/* y, x, n, attr, color, opts */
				/* + 6 for "% 7d" above */
				mvchgat(y, off + 6, bn_len, BASENAME_ATTR, BASENAME_COL, NULL);
			}
		}

		free(linebuf);
	}

	// done with our proc, increment y
	y++;

	/*
	 * need to iterate over all children,
	 * since we may currently be on a process above the top
	 */
	for(struct myproc **iter = proc->children; iter && *iter; iter++)
		show_proc_tree(*iter, &y, indent + 1, top);

	*py = y;
}
