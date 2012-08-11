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
#include "list.h"

#include "pthread_helper.h"
#include "accept_spawn.h"

#include <pthread.h>
#include <search.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <gnutls/gnutls.h>

struct voter_rec {
	struct list_head l;

	char *name;
	size_t name_len;

	char *pass;
	size_t pass_len;

	valid_num_t vn;
	bool has_voted;
};

struct voters {
	void *root_by_name;
	void *root_by_vn;
	unsigned ct;
	unsigned num_voted;
	struct list_head v_list;
};

struct pcc_arg {
	struct addrinfo *ctf_ai;
	char *ctf_addr;
	char *ctf_port;
	struct voters *vs;
	pthread_t th;
};

int voter_vn_cmp(struct voter_rec *v1, struct voter_rec *v2)
{
	return memcmp(v1->vn.data, v2->vn.data, VALID_NUM_BYTES);
}

struct voter_rec *voters_find_by_vn(struct voters *v, valid_num_t *vn)
{
	struct voter_rec vr;
	memset(&vr, 0, sizeof(vr));
	memcpy(&(vr.vn.data), vn->data, VALID_NUM_BYTES);

	struct voter_rec **res = (struct voter_rec **)tfind(
			&vr,
			&v->root_by_vn,
			(comparison_fn_t)voter_vn_cmp);

	if (!res) {
		/* is new... */
		return NULL;
	}

	/* is old */
	return *res;
}

static int voter_name_cmp(struct voter_rec *v1, struct voter_rec *v2)
{
	if (v1->name_len < v2->name_len)
		return 1;
	else if (v1->name_len > v2->name_len)
		return -1;
	else
		return memcmp(v1->name, v2->name, v2->name_len);
}

static struct voter_rec *voters_find_by_name(struct voters const *v,
		unsigned char const *name, size_t name_len)
{
	struct voter_rec vr;
	memset(&vr, 0, sizeof(vr));
	vr.name = (char *)name;
	vr.name_len = name_len;

	struct voter_rec **res = (struct voter_rec **)tfind(
			&vr,
			&v->root_by_name,
			(comparison_fn_t)voter_name_cmp);

	if (!res) {
		/* does not exsist */
		w_prt("no such name\n");
		return NULL;
	}

	/* new */
	return *res;
}

static int voters_add_voter(struct voters *vs, struct voter_rec *vr)
{
	struct voter_rec **r1 = (struct voter_rec **)tsearch(
			vr,
			&vs->root_by_name,
			(comparison_fn_t)voter_name_cmp);

	if (r1 == NULL) {
		return -ENOMEM;
	}

	struct voter_rec **r2 = (struct voter_rec **)tsearch(
			vr,
			&vs->root_by_vn,
			(comparison_fn_t)voter_vn_cmp);

	if (r2 == NULL) {
		w_prt("out of mem for voters (2)\n");
		return -ENOMEM;
	}

	if (*r1 != vr) {
		/* already exsists */
		return 1;
	}

	if (*r2 != vr) {
		/* already exsists, should never get here. */
		return 2;
	}

	/* new */
	list_add(&vs->v_list, &(*r1)->l);
	vs->ct ++;
	return 0;
}

static int voters_mark_as_voted(struct voters *vs, valid_num_t *vn)
{
	struct voter_rec *vr = voters_find_by_vn(vs, vn);
	if (vr) {
		if (!vr->has_voted) {
			vr->has_voted = true;
			vs->num_voted ++;
		}
		return 0;
	} else {
		return EINVAL;
	}
}

static void *periodic_voters_ctf(void *v_arg)
{
	/* update our knowledge of who voted */
	struct pcc_arg *arg = v_arg;

	for(;;) {
		//w_prt("periodic: sleeping\n");
		sleep(5);

		//w_prt("periodic: connecting\n");

		int fd = tcpw_connect("ctf", arg->ctf_addr, arg->ctf_port, arg->ctf_ai);
		if (fd < 0) {
			w_prt("periodic: failed to connect\n");
			continue;
		}

		//w_prt("periodic: connected!\n");

		int r = proto_frame_op(fd, OP_REQ_VOTERS);
		if (r) {
			w_prt("periodic: OP_REQ_VOTERS frame failed to send.\n");
			goto clean_fd;
		}

		//w_prt("periodic: request sent.\n");

		unsigned char ct_buf[FRAME_LEN_BYTES + FRAME_OP_BYTES];
		ssize_t ct_len = recv(fd, ct_buf, sizeof(ct_buf), MSG_WAITALL);

		//w_prt("periodic: recved responce.\n");

		if (ct_len < 0) {
			w_prt("periodic voters: error on recv: %s\n", strerror(errno));
			goto clean_fd;
		} else if (ct_len == 0) {
			w_prt("periodic voters: len = 0, assuming other end died.\n");
			goto clean_fd;
		} else if (ct_len != sizeof(ct_buf)) {
			w_prt("periodic voters: recv has bad size: got %zd != %zu wanted\n",
					ct_len, sizeof(ct_buf));
			goto clean_fd;
		}

		frame_len_t frame_len = proto_decode_len(ct_buf);

		if (frame_len < FRAME_OP_BYTES) {
			w_prt("periodic voters: recved bad frame_len: %llu\n", frame_len);
			goto clean_fd;
		}


		frame_op_t  op = proto_decode_op(ct_buf + FRAME_LEN_BYTES);

		if (op != OP_VOTERS) {
			w_prt("periodic voters: bad op: got %zu wanted %zu\n",
					op, OP_VOTERS);
			goto clean_fd;
		}

		/* the rest of the data is vnums */
		size_t payload_len = frame_len - FRAME_OP_BYTES;
		size_t vnum_ct = payload_len / VALID_NUM_BYTES;
		size_t vnum_rem = payload_len % VALID_NUM_BYTES;

		if (vnum_rem) {
			w_prt("periodic voters: remainder %zu, ct %zu x %zu, len %zu\n",
					vnum_rem, vnum_ct, VALID_NUM_BYTES, payload_len);
			goto clean_fd;
		}

		size_t i;
		for (i = 0; i < vnum_ct; i++) {
			//w_prt("periodic: examined vnum\n");
			valid_num_t vn;
			ssize_t vn_r_len = recv(fd, vn.data, sizeof(vn.data), MSG_WAITALL);
			if (vn_r_len != sizeof(vn.data)) {
				w_prt("periodic voters: %zu: vn len got %zd want %zu\n",
						i, vn_r_len, sizeof(vn.data));
				goto clean_fd;
			}

			int r = voters_mark_as_voted(arg->vs, &vn);
			if (r) {
				w_prt("Invalid Validation Number reported: ");
				valid_num_print(&vn, stderr);
				putc('\n', stderr);
			}
		}

clean_fd:
		close(fd);
	}
	return NULL;
}

static int read_auth_line(struct voter_rec *vr, char *line,
		__attribute__((__unused__)) size_t line_len)
{
	char *pass = line;
	char *name   = strsep(&pass, "\t");
	if (pass == NULL) {
		return 1;
	}

	vr->name = strdup(name);
	vr->name_len = strlen(name);
	vr->pass = strdup(pass);
	vr->pass_len = strlen(pass);
	w_prt("name: %s %d pass: %s %d\n", name, vr->name_len, pass, vr->pass_len);
	valid_num_init(&vr->vn);
	vr->has_voted = false;
	list_init(&vr->l);

	return 0;
}

static void voters_init(struct voters *vs)
{
	vs->root_by_name = NULL;
	vs->root_by_vn = NULL;
	vs->ct         = 0;
	vs->num_voted  = 0;
	list_init(&vs->v_list);
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
			w_prt("failed to add voter\n");
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

static int send_voters_to_ctf(struct addrinfo *ai, char *addr, char *port, struct voters *vs)
{
	int fd = tcpw_connect("ctf", addr, port, ai);
	if (fd < 0)
		return 1;

	struct voter_rec *vr;
	list_for_each_entry(vr, &vs->v_list, l) {
		/* serialize vnum and send */
		int r = proto_frame_vnum(fd, &vr->vn);
		if (r) {
			w_prt("error sending vnum");
		}
	}

	close(fd);
	return 0;
}

static int cla_handle_packet(struct con_arg *arg, frame_op_t op,
		unsigned char *payload, size_t payload_len)
{
	struct voters *vs = arg->pdata;

	switch(op) {
	case OP_REQ_VNUM: {
		if (payload_len < FRAME_LEN_BYTES + 1 + 1) {
			w_prt("vnum request too short: %d\n", payload_len);
			return 1;
		}

		frame_len_t name_len = proto_decode_len(payload);
		unsigned char *name  = payload + FRAME_LEN_BYTES;


		frame_len_t pass_len = payload_len - name_len - FRAME_LEN_BYTES;
		unsigned char *pass  = name + name_len;

		w_prt("name len: %llu pass len: %llu\n", name_len, pass_len);

		struct voter_rec *vr = voters_find_by_name(vs, name, name_len);
		if (!vr) {
			name[name_len] = '\0'; /* will overwrite a part of "pass len" */
			w_prt("got a req for invalid voter: %s %llu\n", name, name_len);
			return 1;
		}

		if (vr->pass_len != pass_len) {
			w_prt("invalid password");
			return 1;
		}

		if (memcmp(vr->pass, pass, pass_len)) {
			w_prt("invalid password");
			return 1;
		}

		/* validated, give out vnum */
		int r = proto_frame_vnum(arg->cfd, &vr->vn);
		if (r) {
			w_prt("failed to send vnum\n");
			return 1;
		}
		return 0;
	}
		break;
	case OP_REQ_VOTER_NAMES: {
		int cfd = arg->cfd;
		proto_send_len(cfd, FRAME_OP_BYTES + FRAME_LEN_BYTES + FRAME_LEN_BYTES);
		proto_send_op(cfd, OP_VOTER_NAMES_CT);
		proto_send_len(cfd, vs->num_voted);
		proto_send_len(cfd, vs->ct - vs->num_voted);

		struct voter_rec *vr;
		list_for_each_entry(vr, &vs->v_list, l) {
			if (vr->has_voted) {
				proto_frame_voter(cfd, vr->name, vr->name_len);
			}
		}

		list_for_each_entry(vr, &vs->v_list, l) {
			if (!vr->has_voted) {
				proto_frame_voter(cfd, vr->name, vr->name_len);
			}
		}

	}
		break;
	default:
		w_prt("unknown op: %d\n", op);
		return 1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	gnutls_global_init();

	srand(time(NULL));

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

	r = send_voters_to_ctf(ctf_ai, argv[3], argv[4], &vs);
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

	return accept_spawn_loop(tl, cla_handle_packet, &vs);
}
