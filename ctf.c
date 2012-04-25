/* Central Tabulating Facility
 * - maintain a list of validation numbers from CLA (CLA pushes these over).
 * - recieves a (vote,random id number,validation number) from voter
 *   - crosses off validation num.
 *   - adds random id num to vote
 *
 * Listen to vote data from voters. (TLS, no client validation)
 * Listen for requests for election results from all. (PUBLIC)
 * Listen for requests from CLA for vote/non-vote data. (TLS, client auth)
 */

#include "tcp.h"
#include "warn.h"
#include "proto.h"
#include "ballot.h"
#include "tabulate.h"

#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>

#include <unistd.h>

struct con_arg {
	int c_id;
	int cfd;
	struct sockaddr_storage address;
	socklen_t address_len;
	pthread_t th;
	tabu_t *tab;
};

static pthread_mutex_t con_prt_mut = PTHREAD_MUTEX_INITIALIZER;

static void con_prt(struct con_arg const *arg, char const *fmt, ...)
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

static int send_voters_cb(struct valid_num_rec *vnr, void *pdata)
{
	struct con_arg *arg = pdata;
	if (vnr->used) {
		proto_send_valid_num(arg->cfd, &vnr->vn);
	}
	return 0;
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
			/* FIXME: bailout (cleanup?) */
			break;
		}

		if (need_more) {
			ssize_t r = recv(cfd, buf + buf_occ, sizeof(buf) - buf_occ, 0);
			if (r == -1) {
				con_prt(arg, "recv failed %d\n", errno);
				/* FIXME: bailout as needed */
				continue;
			} else if (r == 0) {
				con_prt(arg, "recv got 0, %d\n", errno);
				/* FIXME: handle */
				continue;
			}
			buf_occ += r;
		}

		if (buf_occ < FRAME_LEN_BYTES + FRAME_OP_BYTES) {
			/* minimal frame has only LEN & OP */
			need_more = true;
			continue;
		}

		frame_len_t frame_len = decode_len(buf);
		if (frame_len > sizeof(buf) - FRAME_LEN_BYTES
				|| frame_len < FRAME_OP_BYTES) {
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
		frame_op_t op = decode_op(buf);

		unsigned char *payload = buf + FRAME_LEN_BYTES + FRAME_OP_BYTES;
		size_t     payload_len = frame_len - FRAME_OP_BYTES;

		switch(op) {
		case OP_VOTE: {
			struct vote v;
			int r = decode_vote(payload, payload_len, &v);
			if (r) {
				con_prt(arg, "decode_vote: fail: %d\n", r);
				int p = proto_frame_op(cfd, OP_FAIL);
				if (p) {
					con_prt(arg,
					"proto_frame_op: fail %d\n", p);
				}
			}

			r = tabu_insert_vote(arg->tab, &v);
			if (r) {
				con_prt(arg, "tabu_insert_vote: fail: %d\n", r);
				int p = proto_frame_op(cfd, OP_FAIL);
				if (p) {
					con_prt(arg,
					"proto_frame_op: fail %d\n", p);
				}
			}

			int p = proto_frame_op(cfd, OP_SUCC);
			if (p) {
				con_prt(arg,
				"proto_frame_op: succ %d\n", p);
			}
		}
			break;
		case OP_REQ_RESULTS:
			// TODO: send results.
			break;
		case OP_REQ_VOTERS:
			// TODO: send voters.
			proto_send_len(cfd, FRAME_OP_BYTES + tabu_vote_ct(arg->tab) * VALID_NUM_BYTES);
			proto_send_op (cfd, OP_VOTERS);
			tabu_for_each_valid_num_rec(arg->tab, send_voters_cb, arg);
			break;
		}

		// handle packet advancing.
		size_t whole_frame_len = frame_len + FRAME_LEN_BYTES;
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

int main(int argc, char *argv[])
{
	if (argc != 3) {
		w_prt("usage: %s <listen addr> <listen port>\n",
				argc?argv[0]:"ctf");
		return 1;
	}

	struct addrinfo *res;
	int r = tcp_resolve_listen(argv[1], argv[2], &res);

	if (r) {
		/* error resolving. */
		w_prt("listen addr resolve error: %s\n", tcp_resolve_strerror(r));
		return 2;
	}

	int tl = tcp_bind(res);
	freeaddrinfo(res);
	if (tl == -1) {
		w_prt("could create listener: %s\n", strerror(errno));
		return 3;
	}

	r = listen(tl, 128);
	if (r == -1) {
		w_prt("failed to start listening: %s\n", strerror(errno));
		return 4;
	}

	pthread_attr_t th_attr;
	r = pthread_attr_init(&th_attr);
	if (r) {
		w_prt("pthread_attr_init: %s\n", strerror(r));
		return 5;
	}

	r = pthread_attr_setdetachstate(&th_attr, PTHREAD_CREATE_DETACHED);
	if (r) {
		w_prt("pthread_attr_setdetachstate: %s\n", strerror(r));
		return 6;
	}

	tabu_t tab;
	r = tabu_init(&tab);
	if (r) {
		w_prt("tabu_init: %s\n", strerror(r));
		return 7;
	}

	int c_id = 0;
	struct con_arg *ca = malloc(sizeof(*ca));

	for(;;) {
		int cfd = accept(tl,
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

		ca->tab  = &tab;
		ca->c_id = c_id;
		ca->cfd  = cfd;

		r = pthread_create(&ca->th, &th_attr, con_th, ca);
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
