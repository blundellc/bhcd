#ifndef	MERGE_H
#define	MERGE_H

#include <glib.h>
#include "params.h"
#include "tree.h"

typedef struct {
	guint ii;
	guint jj;
	Tree * tree;
	gdouble score;
	/* break equal scores at random */
	gdouble sym_break;
} Merge;


Merge * merge_new(GRand * rng, Params * params, guint ii, Tree * aa, guint jj, Tree * bb, Tree * mm);
void merge_free(Merge * merge);
void merge_free1(gpointer merge, gpointer data);

Merge * merge_best(GRand *, Params * params, guint ii, Tree * aa, guint jj, Tree * bb);
Merge * merge_absorb(GRand *, Params * params, guint ii, Tree * aa, guint jj, Tree * bb);
Merge * merge_join(GRand *, Params * params, guint ii, Tree * aa, guint jj, Tree * bb);
Merge * merge_collapse(GRand *, Params * params, guint ii, Tree * aa, guint jj, Tree * bb);

void merge_println(const Merge * merge, const gchar * prefix);
void merge_tostring(const Merge * merge, GString * out);
gint merge_cmp_neg_score(gconstpointer paa, gconstpointer pbb);

#endif /*MERGE_H*/
