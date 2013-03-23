#include <stdio.h>
#include <string.h>
#include <glib.h>
#include "nrt.h"

Tree * run_rand(GRand * rng, guint num_items, gdouble sparsity, guint verbose) {
	Dataset * dataset;
	Params * params;
	Tree * root;
	Build * build;

	/* dataset = dataset_gen_speckle(rng, num_items, 1.0-sparsity); */
	dataset = dataset_gen_blocks(rng, num_items, 3, sparsity);
	if (verbose > 1) {
		dataset_println(dataset, "");
	}

	params = params_default(dataset);
	dataset_unref(dataset);

	build = build_new(rng, params, 200, FALSE);
	build_run(build);
	root = build_get_best_tree(build);
	tree_ref(root);
	build_free(build);

	if (verbose) {
		tree_println(root, "result: ");
	}

	g_assert(tree_num_leaves(root) == dataset_num_labels(dataset));
	params_unref(params);
	return root;
}

int main(int argc, char * argv[]) {
	GRand * rng;
	guint num_items;
	guint repeat;
	gdouble sparsity;
	gdouble max_time;
	GTimer *timer;
	Tree * root;
	Tree * best;

	rng = g_rand_new();
	timer = g_timer_new();

	sparsity = 0.2;
	for (num_items = 10; num_items < 1000; num_items += 2) {
		max_time = 0.0;
		best = NULL;
		for (repeat = 0; repeat < 100; repeat++) {
			g_timer_start(timer);
			root = run_rand(rng, num_items, sparsity, 0);
			g_timer_stop(timer);
			if (g_timer_elapsed(timer, NULL) > max_time) {
				max_time = g_timer_elapsed(timer, NULL);
			}
			if (best == NULL || tree_get_logprob(root) > tree_get_logprob(best)) {
				tree_unref(best);
				best = root;
			} else {
				tree_unref(root);
			}
		}
		g_print("%d %2.2e ", num_items, max_time);
		tree_println(best, "");
		tree_unref(best);
	}
	g_timer_destroy(timer);
	g_rand_free(rng);
	return 0;
}

