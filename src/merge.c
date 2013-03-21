#include "merge.h"
#include "sscache.h"

static gdouble merge_calc_logprob_rel(Params * params, Tree * aa, Tree * bb);


Merge * merge_new(GRand * rng, Params * params, guint ii, Tree * aa, guint jj, Tree * bb, Tree * mm) {
	Merge * merge;
	gdouble logprob_rel;

	merge = g_new(Merge, 1);
	merge->ii = ii;
	merge->jj = jj;
	merge->tree = mm;
	tree_ref(merge->tree);
	logprob_rel = merge_calc_logprob_rel(params, aa, bb);
	merge->score = tree_get_logprob(merge->tree)
	       		- tree_get_logprob(aa) - tree_get_logprob(bb) - logprob_rel;
	merge->sym_break = g_rand_int(rng);
	return merge;
}

void merge_free(Merge * merge) {
	tree_unref(merge->tree);
	g_free(merge);
}

void merge_free1(gpointer merge, gpointer data) {
	merge_free(merge);
}


void merge_println(Merge * merge, const gchar * prefix) {
	GString * out;

	out = g_string_new(prefix);
	merge_tostring(merge, out);
	g_print("%s\n", out->str);
	g_string_free(out, TRUE);
}

void merge_tostring(Merge * merge, GString * out) {
	g_string_append_printf(out, "%d + %d (%2.2e/%d)-> ", merge->ii, merge->jj, merge->score, merge->sym_break);
	tree_tostring(merge->tree, out);
}

Merge * merge_join(GRand * rng, Params * params, guint ii, Tree * aa, guint jj, Tree * bb) {
	Tree * tree;
	Merge * merge;

	tree = branch_new(params);
	branch_add_child(tree, aa);
	branch_add_child(tree, bb);
	merge = merge_new(rng, params, ii, aa, jj, bb, tree);
	tree_unref(tree);
	return merge;
}

Merge * merge_absorb(GRand * rng, Params * params, guint ii, Tree * aa, guint jj, Tree * bb) {
	/* absorb bb as a child of aa */
	Tree * tree;
	Merge * merge;

	if (tree_is_leaf(aa)) {
		return NULL;
	}

	tree = tree_copy(aa);
	branch_add_child(tree, bb);
	merge = merge_new(rng, params, ii, aa, jj, bb, tree);
	tree_unref(tree);
	return merge;
}

Merge * merge_collapse(GRand * rng, Params * params, guint ii, Tree * aa, guint jj, Tree * bb) {
	/* make children of aa and children of bb all children of a new node */
	Tree * tree;
	Merge * merge;
	GList * child;

	if (tree_is_leaf(aa)) {
		return NULL;
	}

	tree = branch_new(params);
	for (child = branch_get_children(aa); child != NULL; child = g_list_next(child)) {
		branch_add_child(tree, child->data);
	}
	for (child = branch_get_children(bb); child != NULL; child = g_list_next(child)) {
		branch_add_child(tree, child->data);
	}
	merge = merge_new(rng, params, ii, aa, jj, bb, tree);
	tree_unref(tree);
	return merge;
}

Merge * merge_best(GRand * rng, Params * params, guint ii, Tree * aa, guint jj, Tree * bb) {
	Merge * merge;
	Merge * best_merge;

	best_merge = merge_join(rng, params, ii, aa, jj, bb);
	merge = merge_absorb(rng, params, ii, aa, jj, bb);
	if (merge != NULL) {
		if (merge->score > best_merge->score) {
			merge_free(best_merge);
			best_merge = merge;
		} else {
			merge_free(merge);
		}
	}
	merge = merge_absorb(rng, params, jj, bb, ii, aa);
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

	offblock = sscache_get_offblock(params->sscache, tree_get_labelsets(aa), tree_get_labelsets(bb));
	logprob_rel = params_logprob_off(params, offblock);
	return logprob_rel;
}

gint merge_cmp_score(gconstpointer paa, gconstpointer pbb, gpointer userdata) {
	const Merge * aa = paa;
	const Merge * bb = pbb;
	gint diff = bb->score - aa->score;
	if (diff == 0) {
		return bb->sym_break - aa->sym_break;
	}
	return diff;
}

