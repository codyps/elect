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
#include <pthread.h>

#include <unistd.h>


static int send_voters_cb(struct valid_num_rec *vnr, void *pdata)
{
	struct con_arg *arg = pdata;
	if (vnr->used) {
		proto_send_valid_num(arg->cfd, &vnr->vn);
	}
	return 0;
}

int ctf_handle_packet(struct con_arg *arg, frame_op_t op,
		unsigned char *payload, size_t payload_len)
{
	int cfd = arg->cfd;
	tabu_t *tab = arg->pdata;

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

	return 0;
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

	return accept_spawn_loop(tl, ctf_handle_packet, &tab);
}
