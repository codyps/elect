#include "ballot.h"
#include <stddef.h>


struct ballot_option *ballot_option_create(size_t len)
{
	struct ballot_option *opt = malloc(offsetof(typeof(*opt), data[len]));
	if (!opt) {
		return NULL;
	}

	opt->len = len;
	opt->ref = 1;

	return opt;
}

void ident_num_init(ident_num_t *in)
{
	unsigned i;
	for (i = 0; i < IDENT_NUM_BYTES; i++) {
		in->data[i] = rand();
	}
}

void valid_num_init(valid_num_t *vn)
{
	unsigned i;
	for (i = 0; i < VALID_NUM_BYTES; i++) {
		vn->data[i] = rand();
	}
}

static const char hexc[] = "0123456789abcdef";

void valid_num_print(valid_num_t *vn, FILE *out)
{
	unsigned i;
	for (i = 0; i < VALID_NUM_BYTES; i++) {
		putc(hexc[vn->data[i] >> 4], out);
		putc(hexc[vn->data[i] & 0xf], out);
	}
}

void ident_num_print(ident_num_t *in, FILE *out)
{
	unsigned i;
	for (i = 0; i < IDENT_NUM_BYTES; i++) {
		putc(hexc[in->data[i] >> 4], out);
		putc(hexc[in->data[i] & 0xf], out);
	}
}

void ballot_option_print(struct ballot_option *bo, FILE *out)
{
	/* assume plain text */
	fwrite(bo->data, bo->len, 1, out);
}
