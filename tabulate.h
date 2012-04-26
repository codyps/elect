#ifndef TABULATE_H_
#define TABULATE_H_

#include "proto.h"  /* struct vote */
#include "list.h"

#include <stdbool.h>

struct valid_num_rec {
	struct list_head l;
	valid_num_t vn;
};

struct valid_num_store {
	struct list_head used;
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
	struct list_head l;
};

struct vote_store {
	void *root;
	unsigned vote_recs; /* ie: number of ballot options */
	unsigned votes;     /* total votes recieved */
	struct list_head vr_list; /* list of vote_res */
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
unsigned tabu_vote_ct(tabu_t *tab);

void tabu_destroy(tabu_t *tab);

typedef int (*vote_rec_cb)(struct vote_rec *vr, void *pdata);
typedef int (*valid_num_rec_cb)(struct valid_num_rec *vnr, void *pdata);

//int tabu_for_each_valid_num_rec(tabu_t *tab, valid_num_rec_cb cb, void *pdata);
bool tabu_has_results(tabu_t *tab);
int tabu_for_each_vote_rec(tabu_t *tab, vote_rec_cb cb, void *pdata);
int tabu_for_each_voted_valid_num_rec(tabu_t *tab, valid_num_rec_cb cb, void *pdata);

#define TABU_ALREADY_VOTED  256
#define TABU_BAD_VALIDATION 257
#define TABU_ALREADY_EXISTS 258

#endif

