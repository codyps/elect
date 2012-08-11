#include "warn.h"

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

void proto_decode_valid_num(unsigned char *buf, valid_num_t *vn)
{
	memcpy(&vn->data, buf, VALID_NUM_BYTES);
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

static int sane_send(int fd, void const *buf, size_t len)
{
	while(len) {
		ssize_t r = send(fd, buf, len, MSG_NOSIGNAL);
		if (r == -1) {
			return -1;
		} else if (r == 0) {
			return 1;
		} else if (r < 0) {
			return -2;
		}

		len -= r;
		buf += r;
	}

	return 0;
}

int proto_send_bytes(int fd, void const *buf, size_t len)
{
	return sane_send(fd, buf, len);
}

#define DEF_PROTO_SEND(name, type, bytes)	\
int proto_send_##name(int fd, type it)		\
{						\
	unsigned char buf[bytes];		\
	unsigned i;				\
	for (i = 0; i < bytes; i++) {		\
		buf[i] = it >> ((bytes - i - 1) * 8);	\
	}					\
	return sane_send(fd, buf, bytes);	\
}

DEF_PROTO_SEND(op, frame_op_t, FRAME_OP_BYTES)
DEF_PROTO_SEND(len, frame_len_t, FRAME_LEN_BYTES)

#define DEF_PROTO_SEND_FARRAY(name, struct_n, field)		\
int proto_send_##name(int fd, struct_n const *it)			\
{								\
	return sane_send(fd, it->field, sizeof(it->field));	\
}

DEF_PROTO_SEND_FARRAY(valid_num, valid_num_t, data)
DEF_PROTO_SEND_FARRAY(ident_num, ident_num_t, data)

#define DEF_PROTO_SEND_EARRAY(name, struct_n, data_field, size_field)	\
int proto_send_##name(int fd, struct_n const *it)			\
{									\
	int r = proto_send_len(fd, it->size_field);			\
	if (r)								\
		return 1;						\
	return sane_send(fd, it->data_field, it->size_field);		\
}

DEF_PROTO_SEND_EARRAY(ballot_option, struct ballot_option, data, len)

int proto_frame_op(int fd, frame_op_t op)
{
	int r = proto_send_len(fd, FRAME_OP_BYTES);
	if (r) {
		w_prt("proto frame op failed\n");
		return 1;
	}

	return proto_send_op (fd, op);
}

int proto_frame_vnum(int fd, valid_num_t *vn)
{
	int r = proto_send_len(fd, FRAME_OP_BYTES + VALID_NUM_BYTES);
	if (r) {
		return 1;
	}

	r = proto_send_op(fd, OP_VNUM);
	if (r) {
		return 2;
	}

	r = proto_send_valid_num(fd, vn);
	if (r) {
		return 3;
	}

	return 0;
}

int proto_frame_voter(int fd, char const *name, size_t name_len)
{
	int r = proto_send_len(fd, FRAME_OP_BYTES + name_len);
	if (r) {
		return 1;
	}

	r = proto_send_op(fd, OP_VOTER_NAME);
	if (r) {
		return 2;
	}

	r = proto_send_bytes(fd, name, name_len);
	if (r) {
		return 3;
	}

	return 0;
}

int  cla_get_vnum(int fd, char const *name, size_t name_len,
		char const *pass, size_t pass_len, valid_num_t *vn)
{
	/*                    op             len of name       name        pass */
	proto_send_len(fd, FRAME_OP_BYTES + FRAME_LEN_BYTES + name_len + pass_len);
	proto_send_op(fd, OP_REQ_VNUM);
	proto_send_len(fd, name_len);

	proto_send_bytes(fd, name, name_len);
	proto_send_bytes(fd, pass, pass_len);

	unsigned char buf[FRAME_LEN_BYTES + FRAME_OP_BYTES + VALID_NUM_BYTES];
	ssize_t rl = recv(fd, buf, sizeof(buf), MSG_WAITALL);
	if (rl < 0) {
		return 1;
	} else if (rl == 0) {
		return 2;
	} else if (rl != sizeof(buf)) {
		return 3;
	}

	frame_len_t fl = proto_decode_len(buf);

	if (fl != VALID_NUM_BYTES + FRAME_OP_BYTES) {
		return 4;
	}

	frame_op_t op = proto_decode_op(buf + FRAME_LEN_BYTES);
	if (op != OP_VNUM) {
		return 5;
	}

	memcpy(vn->data, buf + FRAME_LEN_BYTES + FRAME_OP_BYTES, VALID_NUM_BYTES);

	return 0;
}

int  ctf_send_vote(int fd, char const *vote, size_t vote_len,
		valid_num_t const *vn, ident_num_t const *in)
{
	proto_send_len(fd, FRAME_OP_BYTES + VALID_NUM_BYTES + IDENT_NUM_BYTES + vote_len);
	proto_send_op(fd, OP_VOTE);
	proto_send_valid_num(fd, vn);
	proto_send_ident_num(fd, in);
	proto_send_bytes(fd, vote, vote_len);


	unsigned char buf[FRAME_LEN_BYTES + FRAME_OP_BYTES];
	ssize_t rl = recv(fd, buf, sizeof(buf), MSG_WAITALL);
	if (rl < 0) {
		return -1;
	} else if (rl == 0) {
		return -2;
	} else if (rl != sizeof(buf)) {
		return -3;
	}

	frame_len_t fl = proto_decode_len(buf);

	if (fl != FRAME_OP_BYTES) {
		return -4;
	}

	frame_op_t op = proto_decode_op(buf + FRAME_LEN_BYTES);

	if (op == OP_SUCC) {
		return 0;
	} else if (op == OP_FAIL) {
		return 1;
	} else {
		return -5;
	}
}
