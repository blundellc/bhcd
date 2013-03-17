#ifndef	MERGE_H
#define	MERGE_H

#include "params.h"
#include "tree.h"

typedef struct {
	guint ii;
	guint jj;
	Tree * tree;
	gdouble score;
} Merge;


Merge * merge_new(Params * params, guint ii, Tree * aa, guint jj, Tree * bb, Tree * mm);
void merge_free(Merge * merge);
void merge_free1(gpointer merge, gpointer data);

Merge * merge_best(Params * params, guint ii, Tree * aa, guint jj, Tree * bb);
gint merge_cmp_score(gconstpointer paa, gconstpointer pbb, gpointer userdata);
Merge * merge_absorb(Params * params, guint ii, Tree * aa, guint jj, Tree * bb);
Merge * merge_join(Params * params, guint ii, Tree * aa, guint jj, Tree * bb);

#endif /*MERGE_H*/
