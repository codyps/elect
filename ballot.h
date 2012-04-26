#ifndef BALLOT_H_
#define BALLOT_H_

#include <stdio.h>  /* FILE */
#include <stdlib.h> /* size_t */

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

void ident_num_init(ident_num_t *in);
void ident_num_print(ident_num_t *in, FILE *out);

struct ballot_option *ballot_option_create(size_t len);
void ballot_option_print(struct ballot_option *bo, FILE *out);


#endif
