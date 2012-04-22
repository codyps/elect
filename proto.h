#ifndef PROTO_H_
#define PROTO_H_

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
	OP_SUCC,
	OP_FAIL,


	/* Serviced by the CTF */
	OP_VOTE,	/* returns OP_FAIL or OP_SUCC. */
	OP_STARTTLS,
	OP_REQ_VOTERS,
	OP_REQ_RESULTS
};

#define VALID_NUM_BYTES 16 /* 128 bit */
#define IDENT_NUM_BYTES 16 /* 128 bit */
typedef unsigned char valid_num_t[VALID_NUM_BYTES];
typedef unsigned char ident_num_t[IDENT_NUM_BYTES];

struct vote {
	valid_num_t vn; /* handled out by cla */
	ident_num_t id; /* randomly generated, used by voter */
	unsigned char *vote;
	size_t         vote_len; /* a vote is just viewed as
				    a binary string. */
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

int decode_vote(unsigned char *buf, size_t len)
{
	
}

#endif
