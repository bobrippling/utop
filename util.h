#ifndef UTIL_H
#define UTIL_H

long mstime(void);
char *fline(const char *path, char **buf, int *len);

void *umalloc(size_t l);
void *urealloc(void *p, size_t l);
char *ustrdup(const char *s);

int str_to_sig(const char *);

int longest_passwd_line(const char *fname);

const char *format_kbytes(long unsigned val);
const char *format_seconds(long unsigned timeval);

void argv_free(int argc, char **argv);

#endif
