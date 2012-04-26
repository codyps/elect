
#include "accept_spawn.h"
#include "pthread_helper.h"
#include "warn.h"

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include <errno.h>
#include <pthread.h>
#include <stdarg.h>

#include <unistd.h>

static pthread_mutex_t con_prt_mut = PTHREAD_MUTEX_INITIALIZER;

void con_prt(struct con_arg const *arg, char const *fmt, ...)
{
#if 0
	char host[128];
	char serv[32];

	int r = getnameinfo((struct sockaddr *)&arg->address,
			arg->address_len,
			host, sizeof(host), serv, sizeof(serv),
			NI_NUMERICHOST, NI_NUMERICSERV);
#endif

	pthread_mutex_lock(&con_prt_mut);

	fprintf(stderr, "con %03d: ", arg->c_id);

	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	pthread_mutex_unlock(&con_prt_mut);
}

static void *con_th(void *v_arg)
{
	struct con_arg *arg = v_arg;
	int cfd = arg->cfd;
	unsigned char buf[128];
	size_t buf_occ = 0;
	bool need_more = true;

	for (;;) {
		if (buf_occ + 1 > sizeof(buf)) {
			con_prt(arg, "overfilled buffer\n");
			/* bailout */
			break;
		}

		if (need_more) {
			ssize_t r = recv(cfd, buf + buf_occ, sizeof(buf) - buf_occ, 0);
			if (r == -1) {
				con_prt(arg, "recv failed: %s\n", strerror(errno));
				/* FIXME: bailout as needed */
				switch(errno) {
				case ECONNREFUSED:
				case EFAULT:
				case EBADF:
				case EINVAL:
				case ENOMEM:
				case ENOTCONN:
				case ENOTSOCK:
					/* some error we can't handle. */
					goto e_shutdown;

				case EINTR:
					/* retry */
					break;
				}
				continue;
			} else if (r == 0) {
				/* assuming the other end broke the connection */
				con_prt(arg, "closed\n");
				goto e_shutdown;
			}
			buf_occ += r;
		}

		if (buf_occ < FRAME_LEN_BYTES + FRAME_OP_BYTES) {
			/* minimal frame has only LEN & OP */
			need_more = true;
			continue;
		}

		frame_len_t frame_len  = proto_decode_len(buf);
		size_t whole_frame_len = frame_len + FRAME_LEN_BYTES;
		if (whole_frame_len > sizeof(buf) || frame_len < FRAME_OP_BYTES) {
			con_prt(arg, "frame has bad len: %u\n", frame_len);
			/* close and die, do
			 * not attempt to recover from possible
			 * desync. */
			goto e_shutdown;
		}

		if (buf_occ < (frame_len + FRAME_LEN_BYTES)) {
			/* not enough data to complete frame */
			need_more = true;
			continue;
		}

		/* we have a frame */
		frame_op_t op = proto_decode_op(buf + FRAME_LEN_BYTES);

		unsigned char *payload = buf + FRAME_LEN_BYTES + FRAME_OP_BYTES;
		size_t     payload_len = frame_len - FRAME_OP_BYTES;

		int r = arg->handle_packet(arg, op, payload, payload_len);
		if (r) {
			con_prt(arg, "packet handler requested shutdown: %d\n", r);
			goto e_shutdown;
		}

		// handle packet advancing.
		size_t left_len = buf_occ - whole_frame_len;
		memmove(buf, buf + whole_frame_len, left_len);
		need_more = false;
	}

e_shutdown:
	/* cleanup any alocations owned by this thread */
	close(cfd);
	free(v_arg);
	return NULL;
}

int accept_spawn_loop(int fd, handle_packet_cb handle_packet, void *pdata)
{
	int c_id = 0;
	struct con_arg *ca = malloc(sizeof(*ca));
	memset(&ca->address, 0, sizeof(ca->address));
	memset(&ca->address_len, 0, sizeof(ca->address_len));

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
				w_prt("system may be overloaded: %s\n", strerror(errno));
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

			w_prt("fatal accept error.\n");

			return -1;
		}

		ca->c_id  = c_id;
		ca->cfd   = cfd;
		ca->handle_packet = handle_packet;
		ca->pdata = pdata;

		int r = pthread_create(&ca->th, &th_attr, con_th, ca);
		if (r) {
			w_prt("pthread_create: %s\n", strerror(r));
			continue;
		}

		/* TODO: track created threads? */
		c_id ++;
		ca = malloc(sizeof(*ca));
		memset(&ca->address, 0, sizeof(ca->address));
		memset(&ca->address_len, 0, sizeof(ca->address_len));
	}

	return 0;
}
