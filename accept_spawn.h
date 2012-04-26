#ifndef ACCEPT_SPAWN_H_
#define ACCEPT_SPAWN_H_

#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>

struct con_arg {
	int c_id;
	int cfd;
	struct sockaddr_storage address;
	socklen_t address_len;
	pthread_t th;
	void *pdata;
};

int accept_spawn_loop(int fd, void *(*thread)(void *), void *pdata);

#endif
