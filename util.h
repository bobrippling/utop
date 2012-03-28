#ifndef UTIL_H
#define UTIL_H

long mstime();
char *fline(const char *path, char **buf, int *len);

void *umalloc(size_t l);
void *urealloc(void *p, size_t l);
char *ustrdup(const char *s);

int str_to_sig(const char *);

int longest_passwd_line(const char *fname);

const char *uptime_from_boottime(time_t boottime);
const char* format_memory(int memory[6]);
#endif
