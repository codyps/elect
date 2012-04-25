#include "proto.h"

#include <string.h>
#include <stdlib.h>
#include <stddef.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <unistd.h>

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

static int sane_send(int fd, void *buf, size_t len)
{
	ssize_t r = send(fd, buf, len, MSG_NOSIGNAL);
	if (r == -1) {
		return -1;
	} else if (r == 0) {
		return 1;
	} else if (r != len) {
		return -2;
	}

	return 0;
}

int proto_send_op(int fd, frame_op_t op)
{
	unsigned char buf[FRAME_OP_BYTES];
	unsigned i;
	for (i = 0; i < FRAME_OP_BYTES; i++) {
		buf[i] = op >> (FRAME_OP_BYTES - i - 1);
	}

	return sane_send(fd, buf, FRAME_OP_BYTES);
}

int proto_send_len(int fd, frame_len_t fl)
{
	unsigned char buf[FRAME_LEN_BYTES];
	unsigned i;
	for (i = 0; i < FRAME_LEN_BYTES; i++) {
		buf[i] = fl >> (FRAME_LEN_BYTES - i - 1);
	}

	return sane_send(fd, buf, FRAME_LEN_BYTES);
}

int proto_send_valid_num(int fd, valid_num_t *vn)
{
	return -1;
}

int proto_send_ident_num(int fd, ident_num_t *in)
{
	return -1;
}

int proto_frame_op(int fd, frame_op_t op)
{
	int r = proto_send_len(fd, sizeof(op));
	if (r) {
		return 1;
	}

	return proto_send_op (fd, op);
}

void ident_num_init(ident_num_t *in)
{
	unsigned i;
	for (i = 0; i < IDENT_NUM_BYTES; i++) {
		in->data[i] = rand();
	}
}

int  cla_get_vnum(int fd, char const *name, char const *pass, valid_num_t *vn)
{
	/* FIXME */
	return -1;
}

int  ctf_send_vote(int fd, char const *vote, valid_num_t const *vn, ident_num_t const *in)
{
	/* FIXME */
	return -1;
}
