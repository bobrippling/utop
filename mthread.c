#include <pthread.h>

void *f(void *p)
{
	pause();
	return 0;
}

main()
{
	pthread_t pt;
	if(pthread_create(&pt, NULL, f, NULL))
		return 1;

	pthread_join(pt, NULL);

	return 0;
}
