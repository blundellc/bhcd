#include "merge.h"

static gdouble merge_calc_logprob_rel(Params * params, Tree * aa, Tree * bb);


Merge * merge_new(Params * params, guint ii, Tree * aa, guint jj, Tree * bb, Tree * mm) {
	Merge * merge;
	gdouble logprob_rel;

	merge = g_new(Merge, 1);
	merge->ii = ii;
	merge->jj = jj;
	merge->tree = mm;
	logprob_rel = merge_calc_logprob_rel(params, aa, bb);
	merge->score = tree_get_logprob(merge->tree)
	       		- tree_get_logprob(aa) - tree_get_logprob(bb) - logprob_rel;
	return merge;
}

void merge_free(Merge * merge) {
	tree_unref(merge->tree);
	g_free(merge);
}

void merge_free1(gpointer merge, gpointer data) {
	merge_free(merge);
}


Merge * merge_join(Params * params, guint ii, Tree * aa, guint jj, Tree * bb) {
	Tree * tree;

	tree = branch_new(params);
	branch_add_child(tree, aa);
	branch_add_child(tree, bb);
	return merge_new(params, ii, aa, jj, bb, tree);
}

Merge * merge_absorb(Params * params, guint ii, Tree * aa, guint jj, Tree * bb) {
	/* absorb bb as a child of aa */
	Tree * tree;

	if (tree_is_leaf(aa)) {
		return NULL;
	}

	tree = tree_copy(aa);
	branch_add_child(tree, bb);
	return merge_new(params, ii, aa, jj, bb, tree);
}

Merge * merge_best(Params * params, guint ii, Tree * aa, guint jj, Tree * bb) {
	Merge * merge;
	Merge * best_merge;

	best_merge = merge_join(params, ii, aa, jj, bb);
	merge = merge_absorb(params, ii, aa, jj, bb);
	if (merge != NULL) {
		if (merge->score > best_merge->score) {
			merge_free(best_merge);
			best_merge = merge;
		} else {
			merge_free(merge);
		}
	}
	merge = merge_absorb(params, jj, bb, ii, aa);
	if (merge != NULL) {
		if (merge->score > best_merge->score) {
			merge_free(best_merge);
			best_merge = merge;
		} else {
			merge_free(merge);
		}
	}
	return best_merge;
}

static gdouble merge_calc_logprob_rel(Params * params, Tree * aa, Tree * bb) {
	gpointer offblock;
	gdouble logprob_rel;

	offblock = suff_stats_off_lookup(params, tree_get_labels(aa), tree_get_labels(bb));
	logprob_rel = params_logprob_off(params, offblock);
	suff_stats_unref(offblock);
	return logprob_rel;
}

gint merge_cmp_score(gconstpointer paa, gconstpointer pbb, gpointer userdata) {
	const Merge * aa = paa;
	const Merge * bb = pbb;
	return bb->score - aa->score;
}

