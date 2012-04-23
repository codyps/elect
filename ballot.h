#ifndef BALLOT_H_
#define BALLOT_H_

#include <stdlib.h>

struct ballot_option {
	int refs;
	size_t len;
	unsigned char data[];
};

#endif
