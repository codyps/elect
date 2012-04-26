/* results - show results from ctf
 * querys and prints ctf results
 */

#include "warn.h"

int decode_results() {
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


int main(int argc, char **argv)
{
	if (argc != 3) {
		w_prt("usage: %s <ctf addr> <ctf port>\n",
				argc?argv[0]:"results");
		return 1;
	}


	int fd = tcpw_resolve_and_connect("ctf", argv[1], argv[2]);
	if (fd < 0) {
		return 2;
	}

	/* TODO: fill in */

	return 0;
}
