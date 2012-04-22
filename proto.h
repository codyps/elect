#ifndef FRAME_H_
#define FRAME_H_

#include <stdint.h>

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
	/* Serviced by the CTF */
	OP_VOTE,
	OP_STARTTLS,
	OP_REQ_VOTERS,
	OP_REQ_RESULTS
};

struct vote {
	unsigned char *vote;
	size_t         vote_len; /* a vote is just viewed as
				    a binary string. */
	vaidation_num_t vn; /* handled out by cla */
	ident_num_t     id; /* randomly generated, used by voter */
};


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


#endif
