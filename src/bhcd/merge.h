#ifndef	MERGE_H
#define	MERGE_H

#include <glib.h>
#include "params.h"
#include "tree.h"

typedef struct {
	guint ii;
	guint jj;
	Tree * tree;
	gdouble tree_score;
	gdouble score;
	/* break equal scores at random */
	gdouble sym_break;
	/* suff stats between leaves of this tree */
	gpointer ss_offblock;
	/* suff stats for elements not in parent forest */
	gpointer ss_parent;
	/* suff stats for elements not in this forest */
	gpointer ss_self;
} Merge;


Merge * merge_new(GRand * rng, Merge * parent, Params * params, guint ii, Tree * aa, guint jj, Tree * bb, Tree * mm);
void merge_free(Merge * merge);
void merge_notify_global_suffstats(Merge *, gpointer);

Merge * merge_best(GRand *, Merge * parent, Params * params, guint ii, Tree * aa, guint jj, Tree * bb);
Merge * merge_absorb(GRand *, Merge * parent, Params * params, guint ii, Tree * aa, guint jj, Tree * bb);
Merge * merge_join(GRand *, Merge * parent, Params * params, guint ii, Tree * aa, guint jj, Tree * bb);
Merge * merge_collapse(GRand *, Merge * parent, Params * params, guint ii, Tree * aa, guint jj, Tree * bb);

void merge_println(const Merge * merge, const gchar * prefix);
void merge_tostring(const Merge * merge, GString * out);
gint merge_cmp_neg_score(gconstpointer paa, gconstpointer pbb);

#endif /*MERGE_H*/
