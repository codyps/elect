#ifndef TABULATE_H_
#define TABULATE_H_

#include "proto.h"  /* struct vote */

#define _GNU_SOURCE
#include <search.h>
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
	unsigned char *vote;
	size_t vote_len;
	unsigned vote_count;
	struct list_head ident_nums;
};

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

struct valid_num_rec *vns_insert(struct vote_num_store *vns, valid_num_t *vn)
{
	return NULL;
}

int vote_store_init(struct vote_store *vs)
{
	vs->ct = 0;
	vs->root = NULL;
}

int valid_num_store_init(struct valid_num_store *vns)
{
	vns->ct = 0;
	vns->root = NULL;
	return 0;
}

int tabu_init(tabu_t *t)
{
	int r = valid_num_store_init(&t->vns);
	if (r)
		return r;

	return pthread_mutex_init(&t->mut);
}

int tabu_insert_vote(tabu_t *t, struct vote *v)
{
	pthread_mutex_lock(&t->mut);

	/* check if validation number is valid and unused */
	struct valid_num_rec *vnr = vns_lookup(&t->vns, &v->vn);
	if (!vnr) {
		/* no such validation number. */
		pthread_mutex_unlock(&t->mut);
		return 10;
	}

	/* add vote */
	int r = vs_insert(&t->vs, 


}
