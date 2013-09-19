#ifndef GUI_H
#define GUI_H

void gui_init(void);
void gui_term(void);
void gui_run(struct myproc **);

extern pid_t lock_proc_pid;
extern struct myproc *search_proc;
extern char search_str[32];
extern int pos_x;

#endif
