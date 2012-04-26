/* Central Legitimization Agency
 * - responds to requests from voters with a validation number
 * - track pair of (voter,validation number), don't give 1 voter
 *   more than 1 validation number
 *
 * Must listen for requests from voters. (TLS, password auth)
 * - for vnums.
 * - for who has voted and not voted.
 * Push validation numbers to CTF. (TLS, auth as client)
 * Request results data from CTF. (PULBIC)
 * Request vote/non-vote data from CTF. (TLS, auth as client)
 */
#define _GNU_SOURCE

#include "warn.h"
#include "tcp.h"
#include "proto.h"

#include "pthread_helper.h"
#include "accept_spawn.h"

#include <pthread.h>
#include <search.h>
#include <stdbool.h>
#include <unistd.h>

struct voter_rec {
	char *name;
	char *pass;
	valid_num_t vn;
	bool has_voted;
};

struct voters {
	void *root;
	unsigned ct;
};

struct pcc_arg {
	struct addrinfo *ctf_ai;
	char *ctf_addr;
	char *ctf_port;
	struct voters *vs;
	pthread_t th;
};

static void *periodic_check_ctf(void *v_arg)
{
	/* TODO: periodically check the results from the CTF aren't lieing */
	struct pcc_arg *arg = v_arg;
	return NULL;
}

static void *periodic_voters_ctf(void *v_arg)
{
	/* TODO: update our knowledge of who voted */
	struct pcc_arg *arg = v_arg;

	for(;;) {
		sleep(5);

		int fd = tcpw_connect("ctf", arg->ctf_addr, arg->ctf_port, ctf_ai);
		if (fd < 0)
			continue;

		proto_frame_op(fd, OP_REQ_VOTERS);

		unsigned char ct_buf[FRAME_LEN_BYTES + FRAME_OP_BYTES + FRAME_LEN_BYTES];
		ssize_t ct_len = recv(fd, ct_buf, sizeof(ct_buf), MSG_WAITALL);

		if (ct_len < 0) {
			w_prt("periodic voters: error on recv: %s\n", strerror(errno));
			goto clean_fd;
		} else if (ct_len == 0) {
			w_prt("periodic voters: len = 0, assuming other end died.");
			goto clean_fd;
		} else if (ct_len != sizeof(ct_buf)) {
			w_prt("periodic voters: recv has bad size: got %d != %d wanted\n",
					ct_len, sizeof(ct_buf));
			goto clean_fd;
		}

		frame_len_t frame_len = proto_decode_len(ct_buf);

		if (frame_len != FRAME_OP_BYTES + FRAME_LEN_BYTES) {
			w_prt("periodic voters: recved bad frame_len: %d\n", frame_len);
			goto clean_fd;
		}


		frame_op_t  op = proto_decode_op(ct_buf + FRAME_LEN_BYTES);

		if (op != OP_BALLOT_OPTION_CT) {
			w_prt("periodic voters: bad op: got %d wanted %d\n",
					op, OP_BALLOT_OPTION_CT);
			goto clean_fd;
		}

		frame_len_t option_ct = proto_decode_len(ct_buf + FRAME_LEN_BYTES + FRAME_OP_BYTES);

		unsigned i;
		for (i = 0; i < option_ct; i++) {
			/* recv OP_RESULTS */
			unsigned char res_base_buf[FRAME_LEN_BYTES + FRAME_OP_BYTES + FRAME_LEN_BYTES];
			ssize_t rb_len = recv(fd, res_base_buf, sizeof(res_base_buf), MSG_WAITALL);

			if (rb_len != sizeof(res_base_buf)) {
				w_prt("periodic voters: OP_RES %d: bad recv len: %d\n",
						i, rb_len);
				goto clean_fd;
			}

			frame_len_t frame_len = proto_decode_len(res_base_buf);
			frame_op_t  frame_op  = proto_decode_op(res_base_buf + FRAME_LEN_BYTES);
			frame_len_t bo_len    = proto_decode_len(res_base_buf +
							FRAME_LEN_BYTES + FRAME_OP_BYTES);

			struct ballot_option *bo = ballot_option_create(bo_len);
			if (!bo) {
				w_prt("ballot option alloc failed.");
			}

			ssize_t bo_recv_len = recv(fd, bo->data, bo->len, MSG_WAITALL);

			if (bo_recv_len != bo_len) {
				w_prt("periodic voters: OP_RES %d: ballot option rl bad: got %d wanted\n",
						i, bo_recv_len, bo_len);
				goto clean_bo;
			}

			size_t payload_len = frame_len - FRAME_OP_BYTES;
			size_t ident_num_bytes = payload_len - FRAME_LEN_BYTES - bo_len;
			size_t ident_num_ct = ident_num_bytes / IDENT_NUM_BYTES;
			size_t rem_bytes = ident_num_bytes % IDENT_NUM_BYTES;

			if (rem_bytes) {
				w_prt("periodic voters: OP_RES %d: remainder: %d\n",
						i, rem_bytes);
				goto clean_bo;
			}

			unsigned j;
			for (j = 0; j < ident_num_ct; j++) {
				ident_num_t in;
				ssize_t in_len = recv(fd, in.data, sizeof(in.data), MSG_WAITALL);
				if (in_len != sizeof(in.data)) {
					w_prt("periodic voters: %d: %d: bad len: got %d, want %d\n",
							i, j, in_len, sizeof(in.data));
					goto clean_bo;
				}

				/* TODO: do something with in */

			}

			continue;
clean_bo:
			bo_ref_dec(bo);
			goto clean_fd;
		}
clean_fd:
		close(fd);
	}
	return NULL;
}

static int voter_name_cmp(struct voter_rec *v1, struct voter_rec *v2)
{
	return strcmp(v1->name, v2->name);
}

static int voters_add_voter(struct voters *vs, struct voter_rec *vr)
{
	struct voter_rec *res = *(struct voter_rec **)tsearch(
			vr,
			vs->root,
			(comparison_fn_t)voter_name_cmp);

	if (res != vr) {
		/* already exsists */
		return 1;
	}

	/* new */
	return 0;
}

static int read_auth_line(struct voter_rec *vr, char *line, size_t line_len)
{
	char *pass = line;
	char *name   = strsep(&pass, "\t");
	if (pass == NULL) {
		return 1;
	}


	vr->name = name;
	vr->pass = pass;
	w_prt("name: %s pass: %s\n", name, pass);
	memset(&vr->vn, 0, sizeof(vr->vn));
	vr->has_voted = false;

	return 0;
}

static void voters_init(struct voters *vs)
{
	vs->root = NULL;
	vs->ct   = 0;
}

static int read_auth_file(struct voters *vs, char *fname)
{
	FILE *f = fopen(fname, "r");
	if (!f) {
		w_prt("error opening file %s\n", fname);
		return 1;
	}

	char *line = NULL;
	size_t sz  = 0;
	struct voter_rec *vr = malloc(sizeof(*vr));
	if (!vr) {
		w_prt("allocation failed.");
		fclose(f);
		return -ENOMEM;
	}

	for(;;) {
		ssize_t line_len = getline(&line, &sz, f);
		if (line_len == -1) {
			if (feof(f)) {
				fclose(f);
				free(line);
				return 0;
			} else {
				w_prt("error reading file: %d\n",
						ferror(f));
				fclose(f);
				free(line);
				free(vr);
				return 4;
			}
		}


		/* strip the newline */
		if (line[line_len - 1] == '\n')
			line[line_len - 1] = '\0';

		int r = read_auth_line(vr, line, line_len);
		if (r) {
			w_prt("malformed auth file line\n");
			continue;
		}

		r = voters_add_voter(vs, vr);
		if (r) {
			w_prt("voter is a duplicate, skipping.\n");
			continue;
		}

		vr = malloc(sizeof(*vr));
		if (!vr) {
			w_prt("allocation failed.");
			fclose(f);
			free(line);
			return -ENOMEM;
		}
	}
}

static int send_voters_to_ctf(struct addrinfo *ai, struct voters *vs)
{
	return -1;
}

static int cla_handle_packet(struct con_arg *arg, frame_op_t op,
		unsigned char *payload, size_t payload_len)
{
	switch(op) {
	default:
		return 1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	if (argc != 6) {
		w_prt(
		"usage: %s <listen addr> <listen port> <ctf addr> <ctf port> <auth file>\n",
			argc?argv[0]:"cla");
		return 1;
	}

	struct addrinfo *ctf_ai;
	int r = tcpw_resolve_as_client("ctf", argv[3], argv[4], &ctf_ai);
	if (r) {
		return 2;
	}

	struct voters vs;
	voters_init(&vs);

	r = read_auth_file(&vs, argv[5]);
	if (r) {
		w_prt("read auth file failed: %d\n", r);
		return 3;
	}

	r = send_voters_to_ctf(ctf_ai, &vs);
	if (r) {
		w_prt("send voters to ctf failed: %d\n", r);
		return 4;
	}

	pthread_attr_t th_attr;
	r = c_pthread_attr_init_detach(&th_attr);
	if (r) {
		return 5;
	}

	struct pcc_arg pa = {
		.ctf_ai = ctf_ai,
		.vs     = &vs,
		.ctf_addr = argv[3],
		.ctf_port = argv[4]
	};

	r = pthread_create(&pa.th, &th_attr, periodic_check_ctf, &pa);
	if (r) {
		w_prt("could not start periodic ctf check thread: %s\n",
				strerror(r));
		return 5;
	}

	r = pthread_create(&pa.th, &th_attr, periodic_voters_ctf, &pa);
	if (r) {
		w_prt("could not start periodic voters thread: %s\n",
				strerror(r));
		return 6;
	}


	int tl = tcpw_listen(argv[1], argv[2]);
	if (tl == -1) {
		return 7;
	}

	return accept_spawn_loop(tl, cla_handle_packet, NULL);
}
