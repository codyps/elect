#ifndef TABULATE_H_
#define TABULATE_H_

#include "proto.h"  /* struct vote */
#include "list.h"

#include <stdbool.h>

struct valid_num_rec {
	valid_num_t vn;
	bool        used;
};

struct valid_num_store {
	void *root;
	unsigned ct;
};

struct ident_num_rec {
	struct list_head l;
	ident_num_t id;
};

struct vote_rec {
	unsigned vote_count;
	struct list_head ident_nums;
	struct ballot_option *opt;
};

static inline struct ballot_option *bo_ref_inc(struct ballot_option *ba)
{
	ba->ref++;
	return ba;
}

static inline void bo_ref_dec(struct ballot_option *ba)
{
	ba->ref--;
	if (ba->ref == 0) {
		free(ba);
	}
}

struct vote_store {
	void *root;
	unsigned ct;
};

/* I'm not typing "tabulation_t" out every time */
typedef struct tabu_t {
	struct valid_num_store vns;
	struct vote_store      vs;
	pthread_mutex_t mut;
} tabu_t;

/* returns 0 on success, something else on error */
int tabu_init(tabu_t *t);

/* returns 0 on success or one of the following error codes:
 * TABU_ALREADY_VOTED, TABU_BAD_VALIDATION, or -ENOMEM */
int tabu_insert_vote(tabu_t *t, struct vote *v);

int tabu_add_valid_num(tabu_t *tab, valid_num_t *vn);

void tabu_destroy(tabu_t *tab);

void tabu_for_each_vote_rec(tabu_t *tab,
		void (*cb)(void *vote_rec, void *data));

#define TABU_ALREADY_VOTED  256
#define TABU_BAD_VALIDATION 257
#define TABU_ALREADY_EXISTS 258

#endif

