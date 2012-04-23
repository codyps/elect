
#define _GNU_SOURCE

#include "tabulate.h"
#include "ballot.h"

#include <search.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>

static void ident_num_rec_init(struct ident_num_rec *inr, struct vote *v)
{
	list_init(&inr->l);
	memcpy(&inr->id, &v->id, sizeof(inr->id));
}

static void vote_rec_init(struct vote_rec *vr, struct ballot_option *ba)
{
	vr->opt = bo_ref_inc(ba);
	vr->vote_count = 0;
	list_init(&vr->ident_nums);
}

static void valid_num_rec_init(
		struct valid_num_rec *vnr,
		valid_num_t *vn)
{
	memcpy(&vnr->vn, vn, sizeof(*vn));
	vnr->used = false;
}

static int valid_num_cmp(struct valid_num_rec *r1, struct valid_num_rec *r2)
{
	return memcmp(&r1->vn, &r2->vn, sizeof(valid_num_t));
}

#if 0
struct valid_num_rec *vns_insert(struct vote_num_store *vns, valid_num_t *vn)
{
	struct valid_num_rec *vnr = malloc(sizeof(*vnr));
	if (!vnr) {
		return NULL;
	}

	valid_num_rec_init(vnr, vn);

	struct valid_num_rec *res = *(struct valid_num_rec **)tsearch(vns->root,
			(comparison_fn_t)valid_num_cmp);

	if (res == vnr) {
		/* is new */
	}

	return NULL;
}
#endif

static struct valid_num_rec *vns_find_vn(
		struct valid_num_store *vns,
		valid_num_t *vn)
{
	struct valid_num_rec vnr;
	valid_num_rec_init(&vnr, vn);

	struct valid_num_rec **res = (struct valid_num_rec **)tfind(&vnr,
			&vns->root,
			(comparison_fn_t)valid_num_cmp);

	if (!res) {
		/* is new... */
		return NULL;
	}

	/* is old */
	return *res;
}

static int vote_rec_cmp(struct vote_rec *v1, struct vote_rec *v2)
{
	struct ballot_option *b1 = v1->opt, *b2 = v2->opt;
	if (b1->len > b2->len)
		return -1;
	else if (b1->len < b2->len)
		return 1;
	else
		return memcmp(b1->data, b2->data, b1->len);
}

static int vs_add_vote(struct vote_store *vs, struct vote *v)
{
	struct vote_rec *vr = malloc(sizeof(*vr));
	if (!vr) {
		return -ENOMEM;
	}

	vote_rec_init(vr, v->opt);

	struct vote_rec *res = *(struct vote_rec **)tsearch(
			vr,
			&vs->root,
			(comparison_fn_t)vote_rec_cmp);

	if (res != vr) {
		free(vr);
	}


	struct ident_num_rec *ir = malloc(sizeof(*ir));
	if (!ir) {
		return -ENOMEM;
	}

	ident_num_rec_init(ir, v);

	list_add(&res->ident_nums, &ir->l);

	res->vote_count ++;

	return 0;
}

static int vote_store_init(struct vote_store *vs)
{
	vs->ct = 0;
	vs->root = NULL;
	return 0;
}

static int valid_num_store_init(struct valid_num_store *vns)
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

	r = vote_store_init(&t->vs);
	if (r)
		return r;

	return pthread_mutex_init(&t->mut, NULL);
}

int tabu_insert_vote(tabu_t *t, struct vote *v)
{
	pthread_mutex_lock(&t->mut);

	/* check if validation number is valid and unused */
	struct valid_num_rec *vnr = vns_find_vn(&t->vns, &v->vn);
	if (!vnr) {
		/* no such validation number. */
		pthread_mutex_unlock(&t->mut);
		return TABU_BAD_VALIDATION;
	}

	if (vnr->used) {
		pthread_mutex_unlock(&t->mut);
		return TABU_ALREADY_VOTED;
	}

	/* add vote */
	int r = vs_add_vote(&t->vs, v);

	if (!r) {
		vnr->used = true;
	}

	pthread_mutex_unlock(&t->mut);
	return r;
}
