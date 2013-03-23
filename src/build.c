#include "build.h"
#include "merge.h"

typedef void (*InitMergesFunc)(Build *);
typedef void (*AddMergesFunc)(Build *, Tree *);

struct Build_t {
	gboolean debug;
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
};

static void build_add_merges(Build * build, Tree * tkk);
static void build_extract_best_tree(Build * build);
static void build_greedy(Build * build);
static void build_init_merges(Build * build);
static void build_init_trees(Build * build, GList * labels);
static void build_remove_tree(Build * build, guint ii);
static void build_cleanup(Build * build);


Build * build_new(GRand *rng, Params * params, guint num_restarts, gboolean sparse) {
	Build * build;

	build = g_new(Build, 1);
	build->debug = FALSE;
	build->verbose = TRUE;
	build->rng = rng;
	build->params = params;
	params_ref(params);
	build->num_restarts = num_restarts;

	build->trees = NULL;
	build->merges = NULL;
	build->best_tree = NULL;

	build->init_merges = build_init_merges;
	build->add_merges = build_add_merges;
	return build;
}

void build_free(Build * build) {
	build_cleanup(build);
	params_unref(build->params);
	tree_unref(build->best_tree);
	g_free(build);
}



Tree * build_get_best_tree(Build * build) {
	return build->best_tree;
}


void build_set_verbose(Build * build, gboolean value) {
	g_assert(value == FALSE || value == TRUE);
	build->verbose = value;
}

static void build_cleanup(Build * build) {
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


void build_once(Build * build) {
	GList * labels;

	params_reset_cache(build->params);
	labels = dataset_get_labels(build->params->dataset);
	build_init_trees(build, labels);
	dataset_get_labels_free(labels);
	build_init_merges(build);
	build_greedy(build);
	build_extract_best_tree(build);
	build_cleanup(build);
}

void build_run(Build * build) {
	for (build->cur_restart = 0; build->cur_restart < build->num_restarts; build->cur_restart++) {
		build_once(build);
	}
}


static void build_init_trees(Build * build, GList * labels) {
	g_assert(build->trees == NULL);
	build->trees = g_ptr_array_new_full(g_list_length(labels), (GDestroyNotify)tree_unref);
	for (labels = g_list_first(labels); labels != NULL; labels = g_list_next(labels)) {
		Tree * leaf = leaf_new(build->params, labels->data);
		g_ptr_array_add(build->trees, leaf);
	}
}


static void build_init_merges(Build * build) {
	Merge * new_merge;
	Tree * aa;
	Tree * bb;
	guint ii;
	guint jj;

	g_assert(build->trees != NULL);
	g_assert(build->merges == NULL);
	build->merges = g_sequence_new(NULL);

	for (ii = 0; ii < build->trees->len; ii++) {
		aa = g_ptr_array_index(build->trees, ii);
		for (jj = ii + 1; jj < build->trees->len; jj++) {
			bb = g_ptr_array_index(build->trees, jj);
			new_merge = merge_join(build->rng, build->params, ii, aa, jj, bb);
			if (build->debug) {
				merge_println(new_merge, "\tadd init merge: ");
			}
			g_sequence_insert_sorted(build->merges, new_merge, merge_cmp_score, NULL);
		}
	}
}

static void build_remove_tree(Build * build, guint ii) {
	gpointer * tii;

	tii = &g_ptr_array_index(build->trees, ii);
	tree_unref(*tii);
	*tii = NULL;
}

static void build_add_merges(Build * build, Tree * tkk) {
	Tree *tll;
	Merge * new_merge;
	guint ll, kk;

	g_assert(build->trees != NULL);
	g_assert(build->merges != NULL);
	kk = build->trees->len;
	for (ll = 0; ll < build->trees->len; ll++) {
		if (g_ptr_array_index(build->trees, ll) == NULL) {
			continue;
		}

		tll = g_ptr_array_index(build->trees, ll);
		new_merge = merge_best(build->rng, build->params, kk, tkk, ll, tll);
		if (build->debug) {
			merge_println(new_merge, "\tadd merge: ");
		}
		g_sequence_insert_sorted(build->merges, new_merge, merge_cmp_score, NULL);
	}
}

static void build_greedy(Build * build) {
	Merge * cur;
	GSequenceIter * head;
	guint live_trees;

	live_trees = build->trees->len;
	while (live_trees > 1) {
		g_assert(g_sequence_get_length(build->merges) > 0);
		head = g_sequence_get_begin_iter(build->merges);
		cur = g_sequence_get(head);
		g_sequence_remove(head);

		if (build->debug && g_sequence_get_length(build->merges) > 0) {
			Merge * cur_next = g_sequence_get(g_sequence_get_begin_iter(build->merges));
			g_assert(merge_cmp_score(cur, cur_next, NULL) != 0);
		}

		if (g_ptr_array_index(build->trees, cur->ii) == NULL ||
		    g_ptr_array_index(build->trees, cur->jj) == NULL) {
			goto again;
		}

		if (build->debug) {
			merge_println(cur, "best merge: ");
		}

		build_remove_tree(build, cur->ii);
		build_remove_tree(build, cur->jj);
		live_trees--;
		build->add_merges(build, cur->tree);
		g_ptr_array_add(build->trees, cur->tree);
		tree_ref(cur->tree);
again:
		merge_free(cur);
	}
}

static void build_extract_best_tree(Build * build) {
	Tree * root;

	g_assert(build->merges != NULL);
	g_assert(build->trees != NULL);

	root = g_ptr_array_index(build->trees, build->trees->len - 1);
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


