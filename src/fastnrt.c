#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <gsl/gsl_sf_gamma.h>
#include <gsl/gsl_sf_log.h>
#include <gsl/gsl_sf_exp.h>
#include "nrt.h"


typedef struct {
	guint		ref_count;
	guint		num_ones;
	guint		num_total;
} Counts;

Counts * counts_new(guint num_ones, guint num_total) {
	Counts * counts;

	counts = g_new(Counts, 1);
	counts->ref_count = 1;
	counts->num_ones = num_ones;
	counts->num_total = num_total;
	return counts;
}

Counts * counts_copy(Counts * orig) {
	return counts_new(orig->num_ones, orig->num_total);
}

void counts_add(Counts * dst, Counts * src) {
	dst->num_ones += src->num_ones;
	dst->num_total += src->num_total;
}

void counts_ref(Counts * counts) {
	counts->ref_count++;
}

void counts_unref(Counts * counts) {
	if (counts->ref_count <= 1) {
		g_free(counts);
	} else {
		counts->ref_count--;
	}
}


typedef struct {
	guint		ref_count;
	Dataset *	dataset;
	gdouble		gamma;
	gdouble		loggamma; /* really log(1-gamma) */
	gdouble		alpha;
	gdouble		beta;
	gdouble		delta;
	gdouble		lambda;
} Params;

Params * params_new(Dataset * dataset, gdouble gamma, gdouble alpha, gdouble beta, gdouble delta, gdouble lambda) {
	Params * params = g_new(Params, 1);
	params->ref_count = 1;
	params->dataset = dataset;
	dataset_ref(dataset);
	params->gamma = gamma;
	params->loggamma = gsl_sf_log(1.0 - gamma);
	params->alpha = alpha;
	params->beta = beta;
	params->delta = delta;
	params->lambda = lambda;
	return params;
}

Params * params_default(Dataset * dataset) {
	return params_new(dataset,
			  0.4,
			  1.0, 0.2,
			  0.2, 1.0);
}

void params_ref(Params * params) {
	params->ref_count++;
}

void params_unref(Params * params) {
	if (params->ref_count <= 1) {
		dataset_unref(params->dataset);
		g_free(params);
	} else {
		params->ref_count--;
	}
}

gdouble params_logprob_on(Params * params, gpointer pcounts) {
	Counts * counts = pcounts;
	gdouble a1 = params->alpha + counts->num_ones;
	gdouble b0 = params->beta  + counts->num_total - counts->num_ones;
	return gsl_sf_lnbeta(a1, b0) - gsl_sf_lnbeta(params->alpha, params->beta);
}

gdouble params_logprob_off(Params * params, gpointer pcounts) {
	Counts * counts = pcounts;
	gdouble a1 = params->delta + counts->num_ones;
	gdouble b0 = params->lambda  + counts->num_total - counts->num_ones;
	return gsl_sf_lnbeta(a1, b0) - gsl_sf_lnbeta(params->delta, params->lambda);
}

gpointer suff_stats_from_label(Params * params, gpointer label) {
	gboolean missing;
	gboolean value;

	value = dataset_get(params->dataset, label, label, &missing);
	if (missing) {
		return counts_new(0, 0);
	}
	return counts_new(value, 1);
}

gpointer suff_stats_copy(gpointer src) {
	return counts_copy(src);
}

gpointer suff_stats_empty(Params * params) {
	return counts_new(0, 0);
}

void suff_stats_add(gpointer pdst, gpointer psrc) {
	Counts * dst = pdst;
	Counts * src = psrc;
	counts_add(dst, src);
}


gpointer params_suff_stats_off_lookup(Params * params, GList * srcs, GList * dsts) {
	Counts * counts;
	GList * src;
	GList * dst;
	gboolean missing;
	gboolean value;

	counts = counts_new(0, 0);

	for (src = g_list_first(srcs); src != NULL; src = g_list_next(src)) {
		for (dst = g_list_first(dsts); dst != NULL; dst = g_list_next(dst)) {
			value = dataset_get(params->dataset, src->data, dst->data, &missing);
			if (!missing) {
				counts->num_ones += value;
				counts->num_total++;
			}
		}
	}
	return counts;
}

void suff_stats_unref(gpointer ss) {
	counts_unref(ss);
}


typedef struct {
	guint		ref_count;
	gboolean	is_leaf;
	Params *	params;
	gpointer	suff_stats_on;
	gpointer	suff_stats_off;
	GList *		children;
	GList *		labels;

	gboolean	dirty;
	gdouble		log_pi;
	gdouble		log_not_pi;
	gdouble		logprob_cluster;
	gdouble		logprob_children;

	gdouble		logprob;
} Tree;

void tree_assert(Tree * tree) {
	g_assert(tree->ref_count >= 1);
	g_assert(tree->params != NULL);
	g_assert(tree->suff_stats_on != NULL);
	g_assert(tree->suff_stats_off != NULL);
	g_assert(tree->logprob <= 0.0);
	if (tree->children == NULL) {
		g_assert(g_list_length(tree->labels) == 1);
	} else {
		GList *child;

		for (child = tree->children; child != NULL; child = g_list_next(child)) {
			tree_assert(child->data);
		}
	}
}

void tree_unref(Tree *);
void tree_ref(Tree *);
gdouble tree_logprob(Tree *);

Tree * tree_new(Params * params) {
	Tree * tree;
       
	tree = g_new(Tree, 1);
	tree->ref_count = 1;
	tree->is_leaf = TRUE;
	tree->params = params;
	params_ref(tree->params);
	tree->suff_stats_on = NULL;
	tree->suff_stats_off = NULL;
	tree->children = NULL;
	tree->labels = NULL;

	tree->dirty = TRUE;
	tree->log_pi = 0.0;
	tree->log_not_pi = 0.0;
	tree->logprob_cluster = 0.0;
	tree->logprob_children = 0.0;
	tree->logprob = 0.0;

	return tree;
}

Tree * tree_copy(Tree * template) {
	Tree * tree;
	GList * child;
       
	tree = g_new(Tree, 1);
	tree->ref_count = 1;
	tree->is_leaf = template->is_leaf;
	tree->params = template->params;
	params_ref(tree->params);

	tree->suff_stats_on = suff_stats_copy(template->suff_stats_on);
	tree->suff_stats_off = suff_stats_copy(template->suff_stats_off);
	tree->labels = g_list_copy(template->labels);
	tree->children = g_list_copy(template->children);
	for (child = tree->children; child != NULL; child = g_list_next(child)) {
		tree_ref(child->data);
	}

	tree->dirty = template->dirty;
	tree->log_pi = template->log_pi;
	tree->log_not_pi = template->log_not_pi;
	tree->logprob_cluster = template->logprob_cluster;
	tree->logprob_children = template->logprob_children;
	tree->logprob = template->logprob;

	return tree;
}

Tree * leaf_new(Params * params, gpointer label) {
	Tree * leaf;

	leaf = tree_new(params);
	leaf->suff_stats_on = suff_stats_from_label(params, label);
	leaf->suff_stats_off = suff_stats_empty(params);
	leaf->labels = g_list_append(NULL, label);
	leaf->logprob = tree_logprob(leaf);
	return leaf;
}

Tree * branch_new(Params * params) {
	Tree * branch;

	branch = tree_new(params);
	branch->is_leaf = FALSE;
	branch->suff_stats_on = suff_stats_empty(params);
	branch->suff_stats_off = suff_stats_empty(params);
	branch->children = NULL;
	branch->logprob = tree_logprob(branch);
	return branch;
}

gboolean tree_is_leaf(Tree * tree) {
	return tree->is_leaf;
}

guint tree_num_leaves(Tree * tree) {
	guint total;
	GList *child;

	if (tree_is_leaf(tree)) {
		return 1;
	}

	total = 0;
	for (child = tree->children; child != NULL; child = g_list_next(child)) {
		total += tree_num_leaves(child->data);
	}
	return total;
}

guint tree_num_intern(Tree * tree) {
	guint total;
	GList *child;

	if (tree_is_leaf(tree)) {
		return 0;
	}

	total = 1;
	for (child = tree->children; child != NULL; child = g_list_next(child)) {
		total += tree_num_intern(child->data);
	}
	return total;
}

void tree_struct_print(Tree * tree, GString *str) {
	GList * child;

	if (tree_is_leaf(tree)) {
		guint32 qlabel;

		qlabel = GPOINTER_TO_INT(tree->labels->data);
		g_string_append_printf(str, "%s", g_quark_to_string(qlabel));
		return;
	}
	g_string_append_printf(str, "%2.2e:{", tree->logprob);
	for (child = tree->children; child != NULL; child = g_list_next(child)) {
		tree_struct_print(child->data, str);
		if (g_list_next(child) != NULL) {
			g_string_append_printf(str, ", ");
		}
	}
	g_string_append_printf(str, "}");
}

void tree_print(Tree * tree, GString *str) {
	g_string_append_printf(str, "logprob: %2.2e #intern: %u ", tree->logprob, tree_num_intern(tree));
	tree_struct_print(tree, str);
}

void tree_ref(Tree * tree) {
	tree->ref_count++;
}

void tree_unref(Tree * tree) {
	if (tree->ref_count <= 1) {
		if (!tree_is_leaf(tree)) {
			g_list_free_full(tree->children, (GDestroyNotify)tree_unref);
		}
		g_list_free(tree->labels);
		suff_stats_unref(tree->suff_stats_on);
		suff_stats_unref(tree->suff_stats_off);
		params_unref(tree->params);
		g_free(tree);
	} else {
		tree->ref_count--;
	}
}

gdouble leaf_logprob(Tree * leaf) {
	g_assert(tree_is_leaf(leaf));
	leaf->logprob = params_logprob_on(leaf->params, leaf->suff_stats_on);
	leaf->dirty = FALSE;
	return leaf->logprob;
}

void branch_add_child(Tree * branch, Tree * child) {
	GList *labels;
	gpointer new_off;

	g_assert(!tree_is_leaf(branch));
	tree_ref(child);
	branch->children = g_list_append(branch->children, child);

	new_off = params_suff_stats_off_lookup(branch->params, branch->labels, child->labels);
	suff_stats_add(branch->suff_stats_off, new_off);
	suff_stats_unref(new_off);
	suff_stats_add(branch->suff_stats_on, child->suff_stats_on);
	for (labels = child->labels; labels != NULL; labels = g_list_next(labels)) {
		branch->labels = g_list_insert_sorted(branch->labels,
				labels->data, cmp_quark);
	}
	branch->dirty = TRUE;
	branch->logprob = tree_logprob(branch);
}

gdouble branch_log_not_pi(Tree * branch) {
	gdouble num_children;

	num_children = g_list_length(branch->children);

	return (num_children-1)*branch->params->loggamma;
}

gdouble branch_log_pi(Tree * branch, gdouble log_not_pi) {
	if (log_not_pi > gsl_sf_log(2.0)) {
		return gsl_sf_log(-gsl_sf_expm1(log_not_pi));
	} else {
		return gsl_sf_log_1plusx(-gsl_sf_exp(log_not_pi));
	}
}

gdouble branch_logprob(Tree * branch) {
	GList * child;

	if (g_list_length(branch->children) < 2) {
		return 0.0;
	}

	branch->log_not_pi = branch_log_not_pi(branch);
	branch->log_pi = branch_log_pi(branch, branch->log_not_pi);
	branch->logprob_cluster = params_logprob_on(branch->params, branch->suff_stats_on);
	branch->logprob_children = params_logprob_off(branch->params, branch->suff_stats_off);
	for (child = branch->children; child != NULL; child = g_list_next(child)) {
		branch->logprob_children += tree_logprob(child->data);
	}
	branch->logprob = log_add_exp(
			branch->log_pi + branch->logprob_cluster,
			branch->log_not_pi + branch->logprob_children);
	branch->dirty = FALSE;
	return branch->logprob;
}

gdouble tree_logprob(Tree *tree) {
	if (!tree->dirty) {
		return tree->logprob;
	}

	if (tree_is_leaf(tree)) {
		return leaf_logprob(tree);
	}
	return branch_logprob(tree);
}

typedef struct {
	guint ii;
	guint jj;
	Tree * tree;
	gdouble score;
} Merge;

gdouble merge_calc_logprob_rel(Params *, Tree *, Tree *);


Merge * merge_new(Params * params, guint ii, Tree * aa, guint jj, Tree * bb, Tree * mm) {
	Merge * merge;
	gdouble logprob_rel;

	merge = g_new(Merge, 1);
	merge->ii = ii;
	merge->jj = jj;
	merge->tree = mm;
	logprob_rel = merge_calc_logprob_rel(params, aa, bb);
	merge->score = merge->tree->logprob - aa->logprob - bb->logprob - logprob_rel;
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

gdouble merge_calc_logprob_rel(Params * params, Tree * aa, Tree * bb) {
	gpointer offblock;
	gdouble logprob_rel;

	offblock = params_suff_stats_off_lookup(params, aa->labels, bb->labels);
	logprob_rel = params_logprob_off(params, offblock);
	suff_stats_unref(offblock);
	return logprob_rel;
}


gint cmp_score(gconstpointer paa, gconstpointer pbb, gpointer userdata) {
	const Merge * aa = paa;
	const Merge * bb = pbb;
	return bb->score - aa->score;
}


GPtrArray * build_init_trees(Params * params, GList * labels) {
	GPtrArray * trees;

	trees = g_ptr_array_new();
	for (labels = g_list_first(labels); labels != NULL; labels = g_list_next(labels)) {
		Tree * leaf = leaf_new(params, labels->data);
		g_ptr_array_add(trees, leaf);
	}
	return trees;
}


GSequence * build_init_merges(Params * params, GPtrArray * trees) {
	GSequence * merges;
	Merge * new_merge;
	Tree * aa;
	Tree * bb;
	guint ii;
	guint jj;

	merges = g_sequence_new(NULL);

	for (ii = 0; ii < trees->len; ii++) {
		aa = g_ptr_array_index(trees, ii);
		for (jj = ii + 1; jj < trees->len; jj++) {
			bb = g_ptr_array_index(trees, jj);
			new_merge = merge_best(params, ii, aa, jj, bb);
			g_sequence_insert_sorted(merges, new_merge, cmp_score, NULL);
		}
	}
	return merges;
}

void build_remove_tree(GPtrArray * trees, guint ii) {
	gpointer * tii;

	tii = &g_ptr_array_index(trees, ii);
	tree_unref(*tii);
	*tii = NULL;
}

void build_add_merges(Params * params, GSequence * merges, GPtrArray * trees, Tree * tkk) {
	Tree *tll;
	Merge * new_merge;
	guint ll, kk;

	kk = trees->len;
	for (ll = 0; ll < trees->len; ll++) {
		if (g_ptr_array_index(trees, ll) == NULL) {
			continue;
		}

		tll = g_ptr_array_index(trees, ll);
		new_merge = merge_best(params, kk, tkk, ll, tll);
		g_sequence_insert_sorted(merges, new_merge, cmp_score, NULL);
	}
}

void build_greedy(Params * params, GPtrArray * trees, GSequence * merges) {
	Merge * cur;
	GSequenceIter * head;
	guint live_trees;

	live_trees = trees->len;
	while (live_trees > 1) {
		g_assert(g_sequence_get_length(merges) > 0);
		head = g_sequence_get_begin_iter(merges);
		cur = g_sequence_get(head);
		g_sequence_remove(head);

		if (g_ptr_array_index(trees, cur->ii) == NULL ||
		    g_ptr_array_index(trees, cur->jj) == NULL) {
			goto again;
		}

		build_remove_tree(trees, cur->ii);
		build_remove_tree(trees, cur->jj);
		live_trees--;
		build_add_merges(params, merges, trees, cur->tree);
		g_ptr_array_add(trees, cur->tree);
		tree_ref(cur->tree);
again:
		merge_free(cur);
	}
}

Tree * build(Params * params, GList * labels) {
	GPtrArray * trees;
	GSequence * merges;
	Tree * root;

	trees = build_init_trees(params, labels);
	merges = build_init_merges(params, trees);

	build_greedy(params, trees, merges);

	root = g_ptr_array_index(trees, trees->len - 1);
	g_assert(root != NULL);
	g_ptr_array_free(trees, TRUE);
	g_sequence_foreach(merges, merge_free1, NULL);
	g_sequence_free(merges);
	return root;
}


void run_rand(GRand * rng, guint num_items, gdouble sparsity, guint verbose) {
	Dataset * dataset;
	Params * params;
	GList * labels;
	Tree * root;
	GString * out;

	/* dataset = dataset_gen_speckle(rng, num_items, 1.0-sparsity); */
	dataset = dataset_gen_blocks(rng, num_items, 3, sparsity);
	if (verbose > 1) {
		out = g_string_new("dataset: \n");
		dataset_print(dataset, out);
		g_print("%s\n", out->str);
		g_string_free(out, TRUE);
	}

	params = params_default(dataset);

	labels = dataset_get_labels(dataset);

	dataset_unref(dataset);

	root = build(params, labels);

	if (verbose) {
		out = g_string_new("result: ");
		tree_print(root, out);
		g_print("%s\n", out->str);
		g_string_free(out, TRUE);
	}

	g_assert(tree_num_leaves(root) == dataset_num_labels(dataset));
	tree_unref(root);
	g_list_free(labels);
	params_unref(params);
}

int main(int argc, char * argv[]) {
	GRand * rng;
	guint num_items;
	guint repeat;
	gdouble sparsity;
	gdouble max_time;
	GTimer *timer;

	rng = g_rand_new();
	timer = g_timer_new();

	sparsity = 0.0;
	for (num_items = 10; num_items < 14; num_items += 2) {
		max_time = 0.0;
		for (repeat = 0; repeat < 100; repeat++) {
			g_timer_start(timer);
			run_rand(rng, num_items, sparsity, 2);
			g_timer_stop(timer);
			if (g_timer_elapsed(timer, NULL) > max_time) {
				max_time = g_timer_elapsed(timer, NULL);
			}
		}
		g_print("%d %2.2e\n", num_items, max_time);
	}
	g_timer_destroy(timer);
	g_rand_free(rng);
	return 0;
}

