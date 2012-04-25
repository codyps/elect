#ifndef BALLOT_H_
#define BALLOT_H_

#include <stdlib.h>

#define VALID_NUM_BYTES 16 /* 128 bit */
#define IDENT_NUM_BYTES 16 /* 128 bit */
typedef struct valid_num_t {
	unsigned char data[VALID_NUM_BYTES];
} valid_num_t;

typedef struct ident_num_t {
	unsigned char data[IDENT_NUM_BYTES];
} ident_num_t;

struct vote {
	valid_num_t vn; /* handled out by cla */
	ident_num_t id; /* randomly generated, used by voter */
	struct ballot_option *opt;
};

struct ballot_option {
	int ref;
	size_t len;
	unsigned char data[];
};

#endif
