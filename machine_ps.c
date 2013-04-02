const char cmd[] = "ps ax -f | head -1";

char **pipe_in(const char *cmd, size_t *pn)
{
	FILE *f = popen(cmd, "r");
	if(!f)
		return NULL;

	char **list = NULL;
	size_t n = 0;

	char buf[1024]; // TODO: getline, etc
	while(fgets(buf, sizeof(buf), f)){
		list = urealloc(list, ++n * sizeof *list);
		list[n-1] = strdup(buf);
	}

	// TODO: check ferror

	pclose(cmd); // TODO: check

	*pn = n;
	return list;
}

// headers:
// UID        PID  PPID  C STIME TTY      STAT   TIME CMD
