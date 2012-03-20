#ifndef UTIL_H
#define UTIL_H

long mstime();
char *fline(const char *path, char **buf, int *len);

void *umalloc(size_t l);
char *ustrdup(const char *s);

int str_to_sig(const char *);

int longest_passwd_line(const char *fname);

#endif
