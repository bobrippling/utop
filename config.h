#ifndef _CONFIG_H_
#define _CONFIG_H_


#define HALF_DELAY_TIME 10

/* 1000ms */
#define WAIT_TIME (HALF_DELAY_TIME * 100)
/* 60s */
#define FULL_WAIT_TIME (WAIT_TIME * 60)

#define SPACE_CMDLINE 4
#define SPACE_INDENT  4

#define TRACE_TOOL "strace"
// Key bindings
#define CTRL_AND(c) ((c) & 037)

/* Navigation */
#define UP_CHAR 'k'
#define DOWN_CHAR 'j'
#define LEFT_CHAR  'h'
#define RIGHT_CHAR 'l'

#define COL_0_CHAR '0'
#define SOL_CHAR '^'

#define INFO_CHAR 'i'
#define QUIT_CHAR 'q'
#define GOTO_LOCKED_CHAR 'o'
#define GOTO_$$_CHAR 'O'
#define SCROLL_TO_TOP_CHAR 'g'
#define SCROLL_TO_BOTTOM_CHAR 'G'
#define SCROLL_TO_FIRST_CHAR 'H'
#define SCROLL_TO_LAST_CHAR 'L'
#define SCROLL_TO_MIDDLE_CHAR 'M'
#define LOCK_CHAR CTRL_AND('k')
#define REDRAW_CHAR CTRL_AND('l')
#define SEARCH_FORWARD '/'
#define SEARCH_BACKWARD '?'
#define SEARCH_NEXT_CHAR CTRL_AND('n')
#define SEARCH_PREVIOUS_CHAR CTRL_AND('p')
#define RESET_SEARCH_CHAR CTRL_AND('u')
#define EXPOSE_ONE_MORE_LINE_BOTTOM_CHAR CTRL_AND('e')
#define EXPOSE_ONE_MORE_LINE_TOP_CHAR CTRL_AND('y')
#define BACKWARD_WINDOW_CHAR CTRL_AND('b')
#define FORWARD_WINDOW_CHAR CTRL_AND('f')
#define BACKWARD_HALF_WINDOW_CHAR CTRL_AND('u')
#define FORWARD_HALF_WINDOW_CHAR CTRL_AND('d')
#define FREEZE_CHAR 'f'
#define BASENAME_TOGGLE_CHAR 'b'

// Colors

// Default ncurses colors
// COLOR_BLACK   0
// COLOR_RED     1
// COLOR_GREEN   2
// COLOR_YELLOW  3
// COLOR_BLUE    4
// COLOR_MAGENTA 5
// COLOR_CYAN    6
// COLOR_WHITE   7

#define ATTR_SEARCH         COLOR_PAIR(1 + COLOR_YELLOW)
#define ATTR_SEARCH_ALT     COLOR_PAIR(1 + COLOR_BLUE)

#define ATTR_NOT_OWNED      COLOR_PAIR(1 + COLOR_BLACK) | A_BOLD
#define ATTR_LOCK           COLOR_PAIR(1 + COLOR_MAGENTA)
#define ATTR_JAILED         COLOR_PAIR(1 + COLOR_BLUE) | A_BOLD

#define BASENAME_COL   (1 + COLOR_CYAN)
#define BASENAME_ATTR  A_BOLD

typedef void proc_handler(struct myproc *, struct myproc **);

proc_handler delete, renice, lsof, strace, gdb, shell;

struct
{
	proc_handler *handler;
	char ch;

} externals[] = {
	{ delete,  'd' },
	{ renice,  'r' },
	{ lsof,    'q' },
	{ strace,  's' },
	{ gdb,     'a' },
	{ shell,   '!' },
	{ NULL,     0  }
};

#endif
