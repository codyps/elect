#include "proto.h"

#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#include <sys/types.h>
#include <sys/socket.h>

/**
 * @buf - must have at least FRAME_LEN_BYTES bytes.
 */
frame_len_t decode_len(unsigned char *buf)
{
	frame_len_t l = 0;

	unsigned i;
	for (i = 0; i < FRAME_LEN_BYTES; i++) {
		l = l << 8 | buf[i];
	}

	return l;
}

frame_op_t  decode_op(unsigned char *buf)
{
	frame_op_t o = 0;

	unsigned i;
	for (i = 0; i < FRAME_OP_BYTES; i++) {
		o = o << 8 | buf[i + FRAME_LEN_BYTES];
	}

	return o;
}

int decode_vote(unsigned char *buf, size_t len, struct vote *res)
{
	if (len < VALID_NUM_BYTES + IDENT_NUM_BYTES + 1) {
		return EINVAL;
	}

	memcpy(&res->vn, buf, VALID_NUM_BYTES);
	memcpy(&res->id, buf + VALID_NUM_BYTES, IDENT_NUM_BYTES);

	size_t bo_len = len - VALID_NUM_BYTES - IDENT_NUM_BYTES;
	unsigned char *bo_buf = buf + VALID_NUM_BYTES + IDENT_NUM_BYTES;


	struct ballot_option *opt = malloc(offsetof(typeof(*opt), data[bo_len]));
	if (!opt) {
		return ENOMEM;
	}

	opt->len = bo_len;
	opt->ref = 1;
	memcpy(&opt->data, bo_buf, bo_len);

	res->opt = opt;

	return 0;
}

/* FIXME: error handling */
int proto_send_op(int fd, frame_op_t op)
{
	// writev?
	frame_len_t fl = sizeof(op);
	ssize_t r = send(fd, &fl, sizeof(fl), MSG_NOSIGNAL);
	if (r == -1) {
		return -1;
	} else if (r == 0) {
		return 1;
	} else if (r != sizeof(fl)) {
		return -2;
	}

	r = send(fd, &op, sizeof(op), MSG_NOSIGNAL);

	if (r == -1) {
		return -1;
	} else if (r == 0) {
		return -2;
	} else if (r != sizeof(op)) {
		return -3;
	}

	return 0;
}
