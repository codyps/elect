#ifndef ACCEPT_SPAWN_H_
#define ACCEPT_SPAWN_H_


#include "proto.h"

#include <pthread.h>

#include <sys/types.h>
#include <sys/socket.h>

struct con_arg;

typedef int (*handle_packet_cb)(
		struct con_arg *arg,
		frame_op_t op,
		unsigned char *payload,
		size_t payload_len);

void con_prt(struct con_arg const *arg, char const *fmt, ...);

struct con_arg {
	int c_id;
	int cfd;
	struct sockaddr_storage address;
	socklen_t address_len;
	pthread_t th;
	handle_packet_cb handle_packet;
	void *pdata;
};

int accept_spawn_loop(int fd, handle_packet_cb, void *pdata);

#endif
