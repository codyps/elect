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
 * | len (64 bits) | op (16 bits) | payload (0 to lots of bits) |
 *
 * Except for the OP_VOTERS and OP_RESULTS frames, payload is small.
 *
 * XXX: special care should be taken while parsing and generating OP_VOTERS and
 *      OP_RESULTS to limit unnecissary memmory usage.
 *
 */
#define FRAME_LEN_BYTES 8
#define FRAME_OP_BYTES  2
typedef uint_fast16_t frame_op_t;
typedef uint_fast64_t frame_len_t;

enum {
	OP_SUCC,
	OP_FAIL,

	/* Serviced by the CTF */
	OP_VOTE,	/* returns OP_FAIL or OP_SUCC. */
	OP_STARTTLS,
	OP_REQ_VOTERS,
	OP_REQ_RESULTS
};

frame_len_t decode_len(unsigned char *buf);
frame_op_t  decode_op(unsigned char *buf);
int decode_vote(unsigned char *buf, size_t len, struct vote *res);

int proto_send_op(int fd, unsigned op);

#endif
