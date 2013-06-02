#include "build.h"
#include "islands.h"
#include "merge.h"


static const gboolean build_debug = FALSE;

typedef void (*InitMergesFunc)(Build *);
typedef void (*AddMergesFunc)(Build *, Tree *, guint, guint);
typedef void (*FiniMergesFunc)(Build *);

struct Build_t {
	gboolean verbose;
	GRand * rng;
	Params * params;
	Tree * best_tree;
	guint num_restarts;
	guint cur_restart;

	/* work in progress storage */
	GPtrArray * trees;
	GSequence * merges;

	InitMergesFunc init_merges;
	AddMergesFunc add_merges;
	FiniMergesFunc fini_merges;
	gpointer merges_data;
};

static void build_extract_best_tree(Build * build);
static void build_greedy(Build * build);
static void build_init_merges(Build * build);
static void build_add_merges(Build * build, Tree * tkk, guint ii, guint jj);
static void build_fini_merges(Build * build);
static void build_sparse_init_merges(Build * build);
static void build_sparse_add_merges(Build * build, Tree * tkk, guint ii, guint jj);
static void build_sparse_fini_merges(Build * build);
static void build_init_trees(Build * build, Dataset * dataset);
static void build_remove_tree(Build * build, guint ii);
static void build_cleanup(Build * build);
static void build_assert(Build * build);
static void build_flatten_trees(Build * build);
static void build_println(Build * build);


Build * build_new(GRand *rng, Params * params, guint num_restarts, gboolean sparse) {
	Build * build;

	build = g_new(Build, 1);
	build->verbose = FALSE;
	build->rng = rng;
	build->params = params;
	params_ref(params);
	build->num_restarts = num_restarts;

	build->trees = NULL;
	build->merges = NULL;
	build->best_tree = NULL;
	build->merges_data = NULL;
	if (sparse) {
		build->init_merges = build_sparse_init_merges;
		build->add_merges = build_sparse_add_merges;
		build->fini_merges = build_sparse_fini_merges;
	} else {
		build->init_merges = build_init_merges;
		build->add_merges = build_add_merges;
		build->fini_merges = build_fini_merges;
	}
	return build;
}

void build_free(Build * build) {
	build_cleanup(build);
	params_unref(build->params);
	tree_unref(build->best_tree);
	g_free(build);
}

static void build_assert(Build * build) {
	GSequenceIter * head;
	GSequenceIter * prev;
	if (!build_debug) {
		return;
	}
	if (build->merges != NULL) {
		// make sure merges is sorted
		prev = g_sequence_get_begin_iter(build->merges);
		for (head = g_sequence_iter_next(prev);
			!g_sequence_iter_is_end(head); 
			head = g_sequence_iter_next(head)) {

			Merge * prev_merge = g_sequence_get(prev);
			Merge * head_merge = g_sequence_get(head);
			assert_lefloat(head_merge->score, prev_merge->score, EQFLOAT_DEFAULT_PREC);
			prev = head;
		}
	}
}


Tree * build_get_best_tree(Build * build) {
	return build->best_tree;
}


void build_set_verbose(Build * build, gboolean value) {
	g_assert(value == FALSE || value == TRUE);
	build->verbose = value;
}

static void build_cleanup(Build * build) {
	if (build->merges_data != NULL) {
		build->fini_merges(build);
	}
	if (build->trees != NULL) {
		g_ptr_array_free(build->trees, TRUE);
		build->trees = NULL;
	}
	if (build->merges != NULL) {
		g_sequence_foreach(build->merges, merge_free1, NULL);
		g_sequence_free(build->merges);
		build->merges = NULL;
	}
}

static void build_println(Build * build) {
	guint ii;
	Tree * tt;

	g_print("%d in queue\n", g_sequence_get_length(build->merges));
	for (ii = 0; ii < build->trees->len; ii++) {
		tt = g_ptr_array_index(build->trees, ii);
		if (tt != NULL) {
			g_print("\t%d: ", ii);
			tree_println(tt,"");
		}
	}
}

void build_once(Build * build) {
	params_reset_cache(build->params);
	build_init_trees(build, build->params->dataset);
	build->init_merges(build);
	build_greedy(build);
	build_extract_best_tree(build);
	build_cleanup(build);
}

void build_run(Build * build) {
	for (build->cur_restart = 0; build->cur_restart < build->num_restarts; build->cur_restart++) {
		build_once(build);
	}
}


static void build_init_trees(Build * build, Dataset * dataset) {
	DatasetLabelIter iter;
	gpointer label;

	g_assert(build->trees == NULL);
	build->trees = g_ptr_array_new_full(dataset_num_labels(dataset), (GDestroyNotify)tree_unref);
	dataset_labels_iter_init(dataset, &iter);
	while (dataset_labels_iter_next(&iter, &label)) {
		Tree * leaf = leaf_new(build->params, label);
		g_ptr_array_add(build->trees, leaf);
	}
}


static void build_remove_tree(Build * build, guint ii) {
	gpointer * tii;

	tii = &g_ptr_array_index(build->trees, ii);
	tree_unref(*tii);
	*tii = NULL;
}

static void build_init_merges(Build * build) {
	Merge * new_merge;
	Tree * aa;
	Tree * bb;
	guint ii;
	guint jj;

	g_assert(build->trees != NULL);
	g_assert(build->merges == NULL);
	g_assert(build->merges_data == NULL);
	build->merges = g_sequence_new(NULL);

	for (ii = 0; ii < build->trees->len; ii++) {
		aa = g_ptr_array_index(build->trees, ii);
		for (jj = ii + 1; jj < build->trees->len; jj++) {
			bb = g_ptr_array_index(build->trees, jj);
			new_merge = merge_join(build->rng, build->params, ii, aa, jj, bb);
			if (build_debug) {
				merge_println(new_merge, "\tadd init merge: ");
			}
			g_sequence_insert_sorted(build->merges, new_merge, merge_cmp_score, NULL);
		}
	}
}

static void build_add_merges(Build * build, Tree * tkk, guint ii, guint jj) {
	Tree *tll;
	Merge * new_merge;
	guint ll, kk;

	g_assert(build->trees != NULL);
	g_assert(build->merges != NULL);
	g_assert(build->merges_data == NULL);
	kk = build->trees->len;
	for (ll = 0; ll < build->trees->len; ll++) {
		if (g_ptr_array_index(build->trees, ll) == NULL) {
			continue;
		}

		tll = g_ptr_array_index(build->trees, ll);
		new_merge = merge_best(build->rng, build->params, kk, tkk, ll, tll);
		if (build_debug) {
			merge_println(new_merge, "\tadd merge: ");
		}
		g_sequence_insert_sorted(build->merges, new_merge, merge_cmp_score, NULL);
	}
}

static void build_fini_merges(Build * build) {
	return;
}

static void build_sparse_init_merges(Build * build) {
	Islands * islands;
	Merge * new_merge;
	GList * edges;

	g_assert(build->trees != NULL);
	g_assert(build->merges == NULL);
	g_assert(build->merges_data == NULL);

	build->merges = g_sequence_new(NULL);
	islands = islands_new(build->params->dataset, build->trees);
	build->merges_data = islands;

	edges = islands_get_edges(islands);
	for (GList * xx = edges; xx != NULL; xx = g_list_next(xx)) {
		Pair * pair = xx->data;
		guint ii = GPOINTER_TO_INT(pair->fst);
		guint jj = GPOINTER_TO_INT(pair->snd);
		Tree * aa = g_ptr_array_index(build->trees, ii);
		Tree * bb = g_ptr_array_index(build->trees, jj);
		g_assert(ii < build->trees->len);
		g_assert(jj < build->trees->len);

		new_merge = merge_join(build->rng, build->params, ii, aa, jj, bb);
		if (build_debug) {
			merge_println(new_merge, "\tadd init merge: ");
		}
		g_sequence_insert_sorted(build->merges, new_merge, merge_cmp_score, NULL);
	}
	islands_get_edges_free(edges);
}

static void build_sparse_add_merges(Build * build, Tree * tkk, guint ii, guint jj) {
	Islands * islands;
	Tree *tll;
	Merge * new_merge;
	GList * neigh;
	guint ll, kk;

	g_assert(build->trees != NULL);
	g_assert(build->merges != NULL);
	g_assert(build->merges_data != NULL);
	islands = build->merges_data;
	kk = build->trees->len;

	islands_merge(islands, kk, ii, jj);

	neigh = islands_get_neigh(islands, kk);
	for (GList * xx = neigh; xx != NULL; xx = g_list_next(xx)) {
		ll = GPOINTER_TO_INT(xx->data);
		if (g_ptr_array_index(build->trees, ll) == NULL) {
			continue;
		}

		tll = g_ptr_array_index(build->trees, ll);
		new_merge = merge_best(build->rng, build->params, kk, tkk, ll, tll);
		if (build_debug) {
			merge_println(new_merge, "\tadd merge: ");
		}
		g_sequence_insert_sorted(build->merges, new_merge, merge_cmp_score, NULL);
	}
	islands_get_neigh_free(neigh);
}

static void build_sparse_fini_merges(Build * build) {
	islands_free(build->merges_data);
	build->merges_data = NULL;
}

static void build_greedy(Build * build) {
	Merge * cur;
	GSequenceIter * head;
	guint iter;

	iter = 0;
	for (head = g_sequence_get_begin_iter(build->merges);
	    !g_sequence_iter_is_end(head);
	    head = g_sequence_get_begin_iter(build->merges)) {

		build_assert(build);
		cur = g_sequence_get(head);
		g_sequence_remove(head);

		/*
		if (build_debug && len > 0) {
			Merge * cur_next = g_sequence_get(g_sequence_get_begin_iter(build->merges));
			g_assert(merge_cmp_score(cur, cur_next, NULL) != 0);
		}
		*/

		if (g_ptr_array_index(build->trees, cur->ii) == NULL ||
		    g_ptr_array_index(build->trees, cur->jj) == NULL) {
			goto again;
		}

		if (build_debug) {
			merge_println(cur, "best merge: ");
		}

		build_remove_tree(build, cur->ii);
		build_remove_tree(build, cur->jj);
		build->add_merges(build, cur->tree, cur->ii, cur->jj);
		g_ptr_array_add(build->trees, cur->tree);
		tree_ref(cur->tree);
		if (build->verbose && (iter++ % 100) == 0) {
			g_print("%d: ", iter);
			build_println(build);
		}
again:
		merge_free(cur);
	}
	build_flatten_trees(build);
}

static void build_flatten_trees(Build * build) {
	/* we've agglomerated as much as we can by merging,
	 * but in the sparse case, we might have more than one connected
	 * component--so just connect them in a rose tree
	 */
	Tree * new_root;
	Tree * child;
	guint num_children;

	new_root = branch_new(build->params);
	num_children = 0;
	for (guint ii = 0; ii < build->trees->len; ii++) {
		child = g_ptr_array_index(build->trees, ii);
		if (child == NULL) {
			continue;
		}
		branch_add_child(new_root, child);
		build_remove_tree(build, ii);
		num_children++;
	}
	g_assert(num_children != 0);
	if (num_children == 1) {
		tree_ref(child);
		tree_unref(new_root);
		g_ptr_array_index(build->trees, 0) = child;
	} else {
		g_ptr_array_index(build->trees, 0) = new_root;
	}
}


static void build_extract_best_tree(Build * build) {
	Tree * root;

	g_assert(build->merges != NULL);
	g_assert(build->trees != NULL);

	root = g_ptr_array_index(build->trees, 0);
	g_assert(root != NULL);
	if (build->best_tree == NULL) {
		build->best_tree = root;
		tree_ref(build->best_tree);
	} else if (tree_get_logprob(root) > tree_get_logprob(build->best_tree)) {
		if (build->verbose) {
			g_print("better(%d): ", build->cur_restart);
			tree_println(root, "");
		}
		tree_unref(build->best_tree);
		build->best_tree = root;
		tree_ref(build->best_tree);
	}
}


