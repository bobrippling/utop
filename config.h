#ifndef _CONFIG_H_
#define _CONFIG_H_


#define HALF_DELAY_TIME 10

/* 1000ms */
#define WAIT_TIME (HALF_DELAY_TIME * 100)
/* 60s */
#define FULL_WAIT_TIME (WAIT_TIME * 60)

#define INDENT "    "

#define TRACE_TOOL "ktrace"
// Key bindings
#define CTRL_AND(c) ((c) & 037)

#define QUIT_CHAR 'q'
#define UP_CHAR 'k'
#define DOWN_CHAR 'j'
#define INFO_CHAR 'i'
#define KILL_CHAR 'd'
#define RENICE_CHAR 'r'
#define LSOF_CHAR 'l'
#define TRACE_CHAR 's'
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
// #define KILL_CHAR CTRL_AND('d')
#define EXPOSE_ONE_MORE_LINE_BOTTOM_CHAR CTRL_AND('e')
#define EXPOSE_ONE_MORE_LINE_TOP_CHAR CTRL_AND('y')
#define BACKWARD_WINDOW_CHAR CTRL_AND('b')
#define FORWARD_WINDOW_CHAR CTRL_AND('f')
#define BACKWARD_HALF_WINDOW_CHAR CTRL_AND('u')
#define FORWARD_HALF_WINDOW_CHAR CTRL_AND('d')

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

#define COLOR_RUNNING  COLOR_GREEN
#define ATTR_NOT_OWNED A_BOLD | COLOR_PAIR(1 + COLOR_BLACK)
#define ATTR_SEARCH    A_BOLD | COLOR_PAIR(1 + COLOR_RED)
#define ATTR_BASENAME  A_BOLD | COLOR_PAIR(1 + COLOR_CYAN)
#define ATTR_LOCK      A_BOLD | COLOR_PAIR(1 + COLOR_YELLOW)
#endif
