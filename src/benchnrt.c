#include <stdio.h>
#include <string.h>
#include <glib.h>
#include "nrt.h"

void run_rand(GRand * rng, guint num_items, gdouble sparsity, guint verbose) {
	Dataset * dataset;
	Params * params;
	GList * labels;
	Tree * root;

	/* dataset = dataset_gen_speckle(rng, num_items, 1.0-sparsity); */
	dataset = dataset_gen_blocks(rng, num_items, 3, sparsity);
	if (verbose > 1) {
		dataset_println(dataset, "");
	}

	params = params_default(dataset);

	labels = dataset_get_labels(dataset);

	dataset_unref(dataset);

	root = build(rng, params, labels);

	if (verbose) {
		tree_println(root, "result: ");
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

