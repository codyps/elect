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
#include "pthread_helper.h"
#include "accept_spawn.h"

#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>

#include <unistd.h>

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

static void *ctf_con_th(void *v_arg)
{
	struct con_arg *arg = v_arg;
	tabu_t *tab = arg->pdata;
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

		switch(op) {
		case OP_VOTE: {
			struct vote v;
			int r = proto_decode_vote(payload, payload_len, &v);
			if (r) {
				con_prt(arg, "decode_vote: fail: %d\n", r);
				int p = proto_frame_op(cfd, OP_FAIL);
				if (p) {
					con_prt(arg,
					"proto_frame_op: fail %d\n", p);
				}
			}

			r = tabu_insert_vote(tab, &v);
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
			proto_send_len(cfd, FRAME_OP_BYTES + tabu_vote_ct(tab) * VALID_NUM_BYTES);
			proto_send_op (cfd, OP_VOTERS);
			tabu_for_each_valid_num_rec(tab, send_voters_cb, arg);
			break;
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

int main(int argc, char *argv[])
{
	if (argc != 3) {
		w_prt("usage: %s <listen addr> <listen port>\n",
				argc?argv[0]:"ctf");
		return 1;
	}

	int tl = tcpw_listen(argv[1], argv[2]);
	if (tl == -1) {
		return 3;
	}

	tabu_t tab;
	int r = tabu_init(&tab);
	if (r) {
		w_prt("tabu_init: %s\n", strerror(r));
		return 7;
	}

	return accept_spawn_loop(tl, ctf_con_th, &tab);
}
