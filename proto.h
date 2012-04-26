#ifndef PROTO_H_
#define PROTO_H_

#include "ballot.h"


#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/*
 * frames are composed like so:
 *
 * | frame len (64 bits) | op (16 bits) | payload (0 to lots of bits) |
 * | (not counted in len)|       frame                                |
 *
 * Except for the OP_VOTERS and OP_RESULTS frames, payload is small.
 *
 * XXX: special care should be taken while parsing and generating OP_VOTERS and
 *      OP_RESULTS to limit unnecissary memmory usage.
 *
 * OP_VOTE:  | validation num | ident num | ballot option          |
 *           | fixed len      | fixed len | variable len           |
 *           |                |           | entire rest of payload |
 *
 * OP_VOTERS:
 *
 * OP_RESULTS:
 *
 *
 */
typedef uint_fast16_t frame_op_t;
typedef uint_fast64_t frame_len_t;
#define FRAME_LEN_BYTES 8
#define FRAME_OP_BYTES  2

enum {
	OP_SUCC,
	OP_FAIL,

	/* Serviced by the CTF */
	OP_VOTE,	/* serviced by CTF, returns OP_FAIL or OP_SUCC. */
	OP_REQ_VOTERS,  /* serviced by CTF */
	OP_VOTERS,	/* reply from CTF */
	OP_REQ_RESULTS, /* serviced by CTF */
	OP_BALLOT_OPTION_CT, /* reply from CTF */
	OP_RESULTS,	     /* reply from CTF */

	/* CLA */
	OP_REQ_VNUM,	/* serviced by CLA */
	OP_VNUM,	/* responce */
	OP_REQ_VOTER_NAMES,  /* service */
	OP_VOTER_NAMES_CT,      /* responce */
	OP_VOTER_NAME		/* also responce */
};

/* sends a simple frame containing only an op */
int proto_frame_op(int fd, frame_op_t op);

/* serializes a len */
int proto_send_len(int fd, frame_len_t len);

/* serializes a op, proto_send_len must be used prior. */
int proto_send_op(int fd, frame_op_t op);

/* the op that only contains a vnum */
int proto_frame_vnum(int fd, valid_num_t *vn);

int proto_frame_voter(int fd, char const *name, size_t name_len);

int proto_send_bytes(int fd, void const *data, size_t bytes);

int proto_send_valid_num(int fd, valid_num_t const *vn);
int proto_send_ident_num(int fd, ident_num_t const *in);
int proto_send_ballot_option(int fd, struct ballot_option const *opt);

frame_len_t proto_decode_len(unsigned char *buf);
frame_op_t  proto_decode_op(unsigned char *buf);
int         proto_decode_vote(unsigned char *buf, size_t len, struct vote *res);
void        proto_decode_valid_num(unsigned char *buf, valid_num_t *vn);

int  cla_get_vnum( int fd, char const *name, size_t name_len,
		char const *pass, size_t pass_len,
		valid_num_t *vn);
int  ctf_send_vote(int fd, char const *vote, size_t vote_len,
		valid_num_t const *vn, ident_num_t const *in);

#endif
