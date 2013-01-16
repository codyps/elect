/* Query - list who voted and who did not.
 */
#include "warn.h"
#include "tcp.h"
#include "proto.h"

int show_voters(int fd, frame_len_t voter_count)
{
	frame_len_t i;
	unsigned char *voter = NULL;
	for (i = 0; i < voter_count; i++) {
		unsigned char base_buf[FRAME_LEN_BYTES + FRAME_OP_BYTES];
		ssize_t rl = recv(fd, base_buf, sizeof(base_buf), MSG_WAITALL);
		if (rl == 0) {
			return 6;
		} else if (rl < 0) {
			return 7;
		} else if (rl != sizeof(base_buf)) {
			return 8;
		}

		frame_len_t fl = proto_decode_len(base_buf);

		frame_op_t  op = proto_decode_op(base_buf + FRAME_LEN_BYTES);
		if (op != OP_VOTER_NAME) {
			w_prt("invalid op, got %zu needed %d\n", op, OP_VOTER_NAME);
			return 8;
		}

		ssize_t voter_len = fl - FRAME_OP_BYTES;
		voter = realloc(voter, voter_len);
		rl = recv(fd, voter, voter_len, MSG_WAITALL);
		if (rl == 0) {
			return 9;
		} else if (rl < 0) {
			return 10;
		} else if (rl != voter_len) {
			return 11;
		}

		putchar('\t');
		fwrite(voter, voter_len, 1, stdout);
		putchar('\n');
	}

	free(voter);
	return 0;
}

int main(int argc, char *argv[])
{
	if (argc != 3) {
		w_prt("usage: %s <cla addr> <cla port>\n",
				argc?argv[0]:"query");
		return 1;
	}

	int fd = tcpw_resolve_and_connect("cla", argv[1], argv[2]);
	if (fd < 0)
		return 2;

	proto_frame_op(fd, OP_REQ_VOTER_NAMES);

	unsigned char buf[FRAME_LEN_BYTES + FRAME_OP_BYTES + FRAME_LEN_BYTES + FRAME_LEN_BYTES];
	ssize_t rl = recv(fd, buf, sizeof(buf), MSG_WAITALL);
	if (rl == 0) {
		return 3;
	} else if (rl < 0) {
		return 4;
	} else if (rl != sizeof(buf)) {
		return 5;
	}

	frame_len_t fl = proto_decode_len(buf);
	if (fl != FRAME_OP_BYTES + FRAME_LEN_BYTES + FRAME_LEN_BYTES) {
		w_prt("bad len: %"PRI_FRAMELEN", need %d\n",
				fl,
				FRAME_OP_BYTES + FRAME_LEN_BYTES + FRAME_LEN_BYTES);
		return 42;
	}

	frame_op_t  op = proto_decode_op(buf + FRAME_LEN_BYTES);
	if (op != OP_VOTER_NAMES_CT) {
		w_prt("invalid op, got %zu needed %d\n", op, OP_VOTER_NAMES_CT);
		return 8;
	}

	frame_len_t voter_count = proto_decode_len(buf + FRAME_LEN_BYTES + FRAME_OP_BYTES);
	frame_len_t non_voter_count = proto_decode_len(
			buf + FRAME_LEN_BYTES + FRAME_OP_BYTES + FRAME_LEN_BYTES);

	printf("voted: %"PRI_FRAMELEN"\n", voter_count);
	if (show_voters(fd, voter_count)) {
		w_prt("show voters failed.");
		return 6;
	}
	printf("did not vote: %"PRI_FRAMELEN"\n", non_voter_count);
	if (show_voters(fd, non_voter_count)) {
		w_prt("show voters failed.");
		return 7;
	}

	return 0;
}
