#include "merge.h"
#include "sscache.h"
#include "counts.h"

static const gboolean merge_debug = FALSE;
gboolean merge_local_score = FALSE;

static void merge_notify_parent(Merge * merge, gpointer global_suffstats, gpointer ss_aa, gpointer ss_bb);
static void merge_calc_score(Merge * merge);


Merge * merge_new(GRand * rng, Merge * parent, Params * params, guint ii, Tree * aa, guint jj, Tree * bb, Tree * mm) {
	Merge * merge;

	merge = g_slice_new(Merge);
	merge->ii = ii;
	merge->jj = jj;
	merge->tree = mm;
	tree_ref(merge->tree);

	merge->tree_score = tree_get_logprob(merge->tree)
	       		- tree_get_logprob(aa)
			- tree_get_logprob(bb);

	merge->ss_offblock = sscache_get_offblock(params->sscache,
			tree_get_merge_left(aa),
			tree_get_merge_right(aa),
			tree_get_merge_left(bb),
			tree_get_merge_right(bb));
	suffstats_ref(merge->ss_offblock);
	merge->ss_all = NULL;
	merge->ss_parent = NULL;
	merge->ss_self = NULL;
	merge->sym_break = g_rand_double(rng);
	if (parent != NULL && parent->ss_all != NULL) {
		merge_notify_parent(merge, parent->ss_all, tree_get_suffstats(aa), tree_get_suffstats(bb));
	}
	merge_calc_score(merge);
	return merge;
}

void merge_free(Merge * merge) {
	suffstats_unref(merge->ss_offblock);
	if (merge->ss_all != NULL) {
		suffstats_unref(merge->ss_all);
	}
	if (merge->ss_parent != NULL) {
		suffstats_unref(merge->ss_parent);
	}
	if (merge->ss_self != NULL) {
		suffstats_unref(merge->ss_self);
	}
	tree_unref(merge->tree);
	g_slice_free(Merge, merge);
}


void merge_notify_pair(Merge * merge, gpointer global_suffstats) {
	g_assert(merge->ss_all == NULL);
	merge_notify_parent(merge, global_suffstats, NULL, NULL);
}

static void merge_notify_parent(Merge * merge, gpointer global_suffstats, gpointer ss_aa, gpointer ss_bb) {
	merge->ss_all = global_suffstats;
	suffstats_ref(merge->ss_all);

	if (ss_aa == NULL && ss_bb == NULL) {
		merge->ss_parent = merge->ss_all;
		suffstats_ref(merge->ss_parent);
	} else {
		merge->ss_parent = suffstats_copy(merge->ss_all);
		suffstats_sub(merge->ss_parent, ss_aa);
		suffstats_sub(merge->ss_parent, ss_bb);
	}

	merge->ss_self = suffstats_copy(merge->ss_all);
	suffstats_sub(merge->ss_self, tree_get_suffstats(merge->tree));
	merge_calc_score(merge);
}

static void merge_calc_score(Merge * merge) {
	Params * params;

	params = tree_get_params(merge->tree);
	if (merge_local_score || merge->ss_parent == NULL) {
		/* local score */
		merge->score = merge->tree_score - params_logprob_offscore(params, merge->ss_offblock);
	} else {
		merge->score = merge->tree_score
			+ params_logprob_offscore(params, merge->ss_self)
			- params_logprob_offscore(params, merge->ss_parent);
	}
}

void merge_println(const Merge * merge, const gchar * prefix) {
	GString * out;

	out = g_string_new(prefix);
	merge_tostring(merge, out);
	g_print("%s\n", out->str);
	g_string_free(out, TRUE);
}

void merge_tostring(const Merge * merge, GString * out) {
	g_string_append_printf(out, "%03d + %03d (%2.2e/%1.2e)-> ", merge->ii, merge->jj, merge->score, merge->sym_break);
	if (merge_debug && merge->ss_parent != NULL) {
		gpointer ss_tree = tree_get_suffstats(merge->tree);
		Params * params = tree_get_params(merge->tree);
		g_string_append_printf(out, "[total tree score: %e, new tree score: %e(%d,%d), self score: %e(%d,%d), parent score: %e(%d,%d), off score %e(%d,%d)]",
				merge->tree_score,
				tree_get_logprob(merge->tree),
				((Counts *)ss_tree)->num_ones,
				((Counts *)ss_tree)->num_total,
				params_logprob_offscore(params, merge->ss_self),
				((Counts *)merge->ss_self)->num_ones,
				((Counts *)merge->ss_self)->num_total,
				params_logprob_offscore(params, merge->ss_parent),
				((Counts *)merge->ss_parent)->num_ones,
				((Counts *)merge->ss_parent)->num_total,
				params_logprob_offscore(params, merge->ss_offblock),
				((Counts *)merge->ss_offblock)->num_ones,
				((Counts *)merge->ss_offblock)->num_total
		       );
	}
	tree_tostring(merge->tree, out);
}

Merge * merge_join(GRand * rng, Merge * parent, Params * params, guint ii, Tree * aa, guint jj, Tree * bb) {
	Tree * tree;
	Merge * merge;

	tree = branch_new(params);
	branch_add_child(tree, aa);
	branch_add_child(tree, bb);
	merge = merge_new(rng, parent, params, ii, aa, jj, bb, tree);
	tree_unref(tree);
	return merge;
}

Merge * merge_absorb(GRand * rng, Merge * parent, Params * params, guint ii, Tree * aa, guint jj, Tree * bb) {
	/* absorb bb as a child of aa */
	Tree * tree;
	Merge * merge;

	if (tree_is_leaf(aa) || params->binary_only) {
		return NULL;
	}

	tree = tree_copy(aa);
	branch_add_child(tree, bb);
	merge = merge_new(rng, parent, params, ii, aa, jj, bb, tree);
	tree_unref(tree);
	return merge;
}

Merge * merge_collapse(GRand * rng, Merge * parent, Params * params, guint ii, Tree * aa, guint jj, Tree * bb) {
	/* make children of aa and children of bb all children of a new node */
	Tree * tree;
	Merge * merge;
	GList * child;

	if (tree_is_leaf(aa) || params->binary_only) {
		return NULL;
	}

	tree = branch_new(params);
	for (child = branch_get_children(aa); child != NULL; child = g_list_next(child)) {
		branch_add_child(tree, child->data);
	}
	for (child = branch_get_children(bb); child != NULL; child = g_list_next(child)) {
		branch_add_child(tree, child->data);
	}
	merge = merge_new(rng, parent, params, ii, aa, jj, bb, tree);
	tree_unref(tree);
	return merge;
}

Merge * merge_best(GRand * rng, Merge * parent, Params * params, guint ii, Tree * aa, guint jj, Tree * bb) {
	Merge * merge;
	Merge * best_merge;

	best_merge = merge_join(rng, parent, params, ii, aa, jj, bb);
	merge = merge_absorb(rng, parent, params, ii, aa, jj, bb);
	if (merge != NULL) {
		if (merge->score > best_merge->score) {
			merge_free(best_merge);
			best_merge = merge;
		} else {
			merge_free(merge);
		}
	}
	merge = merge_absorb(rng, parent, params, jj, bb, ii, aa);
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


gint merge_cmp_neg_score(gconstpointer paa, gconstpointer pbb) {
	const Merge * aa = paa;
	const Merge * bb = pbb;
	gdouble diff = bb->score - aa->score;
	if (diff < 0) {
		return -1;
	} else if (diff > 0) {
		return 1;
	} else {
		if (bb->sym_break < aa->sym_break) {
			return -1;
		} else if (bb->sym_break > aa->sym_break) {
			return 1;
		} else {
			/*
			merge_println(aa, "aa: ");
			merge_println(bb, "bb: ");
			*/
			g_warning("unable to break tie!");
			// really no way to discriminate!
			return 0;
		}
	}
}


