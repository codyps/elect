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

struct src_arg {
	struct con_arg *arg;
	bool is_first;
};

static int send_results_cb(struct vote_rec *vr, void *pdata)
{
	struct src_arg *sarg = pdata;
	tabu_t *tab = sarg->arg->pdata;
	int cfd = sarg->arg->cfd;

	if (sarg->is_first) {
		/* send the number of options prior to any other data. */
		sarg->is_first = false;
		proto_send_len(cfd, FRAME_OP_BYTES + FRAME_LEN_BYTES);
		proto_send_op(cfd, OP_BALLOT_OPTION_CT);
		proto_send_len(cfd, tab->vs.vote_recs);
	}

	/*                |     op         | bo len          | bo          | */
	proto_send_len(cfd, FRAME_OP_BYTES + FRAME_LEN_BYTES + vr->opt->len
			/* ident nums                     */
			+  IDENT_NUM_BYTES * vr->vote_count);

	proto_send_op(cfd, OP_RESULTS);

	/* send ballot option */
	proto_send_ballot_option(cfd, vr->opt);

	/* send all ident_nums */
	struct ident_num_rec *inr;
	list_for_each_entry(inr, &vr->ident_nums, l) {
		proto_send_ident_num(cfd, &inr->id);
	}

	return 0;
}

static int send_voters_cb(struct valid_num_rec *vnr, void *pdata)
{
	struct con_arg *arg = pdata;
	proto_send_valid_num(arg->cfd, &vnr->vn);
	return 0;
}

static int ctf_handle_packet(struct con_arg *arg, frame_op_t op,
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
	case OP_REQ_RESULTS: {
		/* send results. */
		/* for each ballot option */
		struct src_arg sarg = {
			.arg = arg,
			.is_first = true
		};
		if (tabu_has_results(tab)) {
			tabu_for_each_vote_rec(tab, send_results_cb, &sarg);
		} else {
			proto_send_len(cfd, FRAME_OP_BYTES + FRAME_LEN_BYTES);
			proto_send_op(cfd, OP_BALLOT_OPTION_CT);
			proto_send_len(cfd, 0);
		}
	}
		break;
	case OP_REQ_VOTERS:
		/* send voters. */
		proto_send_len(cfd, FRAME_OP_BYTES + tabu_vote_ct(tab) * VALID_NUM_BYTES);
		proto_send_op (cfd, OP_VOTERS);
		tabu_for_each_voted_valid_num_rec(tab, send_voters_cb, arg);
		break;
	case OP_VNUM: {
		/* add vnum to list */
		valid_num_t vn;
		proto_decode_valid_num(payload, &vn);
		int r = tabu_add_valid_num(tab, &vn);
		if (r) {
			w_prt("failed to add vnum\n");
		} else {
			//w_prt("added vnum.\n");
		}
	}
		break;
	default:
		w_prt("unknown op %zu\n", op);
		return 1;
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
