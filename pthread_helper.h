#include "warn.h"
#include <pthread.h>
#include <string.h>

__attribute__((__unused__))
static int c_pthread_attr_init_detach(pthread_attr_t *th_attr)
{
	int r = pthread_attr_init(th_attr);
	if (r) {
		w_prt("pthread_attr_init: %s\n", strerror(r));
		return 1;
	}

	r = pthread_attr_setdetachstate(th_attr, PTHREAD_CREATE_DETACHED);
	if (r) {
		w_prt("pthread_attr_setdetachstate: %s\n", strerror(r));
		return 2;
	}

	return 0;
}
