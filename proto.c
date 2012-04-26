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
frame_len_t proto_decode_len(unsigned char *buf)
{
	frame_len_t l = 0;

	unsigned i;
	for (i = 0; i < FRAME_LEN_BYTES; i++) {
		l = l << 8 | buf[i];
	}

	return l;
}

frame_op_t proto_decode_op(unsigned char *buf)
{
	frame_op_t o = 0;

	unsigned i;
	for (i = 0; i < FRAME_OP_BYTES; i++) {
		o = o << 8 | buf[i];
	}

	return o;
}

int proto_decode_vote(unsigned char *buf, size_t len, struct vote *res)
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
	} else if (r != (ssize_t)len) {
		return -2;
	}

	return 0;
}

#define DEF_PROTO_SEND(name, type, bytes)	\
int proto_send_##name(int fd, type it)		\
{						\
	unsigned char buf[bytes];		\
	unsigned i;				\
	for (i = 0; i < bytes; i++) {		\
		buf[i] = it >> (bytes - i - 1);	\
	}					\
	return sane_send(fd, buf, bytes);	\
}

DEF_PROTO_SEND(op, frame_op_t, FRAME_OP_BYTES)
DEF_PROTO_SEND(len, frame_len_t, FRAME_LEN_BYTES)

#define DEF_PROTO_SEND_FARRAY(name, struct_n, field)		\
int proto_send_##name(int fd, struct_n *it)			\
{								\
	return sane_send(fd, it->field, sizeof(it->field));	\
}

DEF_PROTO_SEND_FARRAY(valid_num, valid_num_t, data)
DEF_PROTO_SEND_FARRAY(ident_num, ident_num_t, data)

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
