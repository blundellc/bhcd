#include <stdio.h>
#include <string.h>
#include <glib.h>
#include "nrt.h"



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
			g_sequence_insert_sorted(merges, new_merge, merge_cmp_score, NULL);
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
		g_sequence_insert_sorted(merges, new_merge, merge_cmp_score, NULL);
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

