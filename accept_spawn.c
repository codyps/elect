
#include "accept_spawn.h"
#include "pthread_helper.h"
#include "warn.h"

#include <string.h>
#include <stdlib.h>

#include <errno.h>
#include <pthread.h>

int accept_spawn_loop(int fd, void *(*thread)(void *), void *pdata)
{
	int c_id = 0;
	struct con_arg *ca = malloc(sizeof(*ca));

	pthread_attr_t th_attr;
	if (c_pthread_attr_init_detach(&th_attr))
		return 5;

	for(;;) {
		int cfd = accept(fd,
				(struct sockaddr *)&ca->address,
				&ca->address_len);
		if (cfd == -1) {
			w_prt("accept failed: %s\n", strerror(errno));
			switch(errno) {
			case ECONNABORTED:
			case EINTR:
				/* definitely retry */
				continue;
			case EMFILE:
			case ENFILE:
			case ENOMEM:
			case ENOBUFS:
				/* indicate overloaded system */
				continue;

			case EAGAIN:
				/* should never occur */

			case EBADF:
			case EINVAL:
			case ENOTSOCK:
			case EOPNOTSUPP:
			case EPROTO:
			default:
				/* actually (probably) fatal */
				break;
			}

			return -1;
		}

		ca->c_id  = c_id;
		ca->cfd   = cfd;
		ca->pdata = pdata;

		int r = pthread_create(&ca->th, &th_attr, thread, ca);
		if (r) {
			w_prt("pthread_create: %s\n", strerror(r));
			continue;
		}

		/* TODO: track created threads? */
		c_id ++;
		ca = malloc(sizeof(*ca));
	}

	return 0;
}
