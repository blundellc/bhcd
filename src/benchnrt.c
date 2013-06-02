#include <stdio.h>
#include <string.h>
#include <glib.h>
#include "nrt.h"


typedef struct {
	guint num_items;
	gdouble sparsity;
} Job;

typedef struct {
	Job * job;
	Tree * best_tree;
	gdouble worst_time;
} Result;

static void blocks_job(Job * job, GAsyncQueue * result_queue);
static void result_free(gpointer pres);
static Tree * run_blocks(guint num_items, gdouble sparsity);


static gint small_jobs_first(gconstpointer paa, gconstpointer pbb, gpointer junk) {
	const Job * aa = paa;
	const Job * bb = pbb;
	/* glib's internal g_thread_pool_wakeup_and_stop_all (up to now) seems to
	 * push 1 onto the internal thread pool queue... which we're sorting. uh
	 * oh.
	 */
	if (paa == GINT_TO_POINTER(1) || pbb == GINT_TO_POINTER(1)) {
		return 0;
	}
	return aa->num_items - bb->num_items;
}


static void result_free(gpointer pres) {
	Result * res = pres;
	g_free(res->job);
	tree_unref(res->best_tree);
	g_free(res);
}

static void blocks_job(Job * job, GAsyncQueue * result_queue) {
	Result * result;
	GTimer *timer;
	Tree * tree;

	timer = g_timer_new();
	result = NULL;

	for (guint repeat = 0; repeat < 100; repeat++) {
		g_timer_start(timer);
		tree = run_blocks(job->num_items, job->sparsity);
		g_timer_stop(timer);
		if (result == NULL) {
			result = g_new(Result, 1);
			result->job = job;
			result->best_tree = tree;
			result->worst_time = g_timer_elapsed(timer, NULL);
		} else if (g_timer_elapsed(timer, NULL) > result->worst_time) {
			result->worst_time = g_timer_elapsed(timer, NULL);
		} else if (tree_get_logprob(tree) > tree_get_logprob(result->best_tree)) {
			tree_unref(result->best_tree);
			result->best_tree = tree;
		} else {
			tree_unref(tree);
		}
	}
	g_timer_destroy(timer);
	g_async_queue_push(result_queue, result);
}


static Tree * run_blocks(guint num_items, gdouble sparsity) {
	Dataset * dataset;
	Params * params;
	Tree * root;
	Build * build;
	GRand * rng;

	rng = g_rand_new();
	/* dataset = dataset_gen_speckle(rng, num_items, 1.0-sparsity); */
	dataset = dataset_gen_blocks(rng, num_items, 5, sparsity);

	params = params_default(dataset);
	dataset_unref(dataset);

	build = build_new(rng, params, 200, FALSE);
	build_run(build);
	root = build_get_best_tree(build);
	tree_ref(root);
	build_free(build);

	g_assert(tree_num_leaves(root) == dataset_num_labels(dataset));
	params_unref(params);
	g_rand_free(rng);
	return root;
}

int main(int argc, char * argv[]) {
	guint num_items;
	gdouble sparsity;
	GAsyncQueue * result_queue;
	GThreadPool * pool;
	const guint num_cores = 5;
	GError * error;
	gint pending_jobs;

	error = NULL;
	g_thread_init(NULL);
	result_queue = g_async_queue_new_full(result_free);
	pool = g_thread_pool_new((GFunc)blocks_job, result_queue, num_cores, TRUE, &error);
	if (error != NULL) {
		g_error("g_thread_pool_new: %s", error->message);
	}
	g_thread_pool_set_sort_function(pool, small_jobs_first, NULL);
	sparsity = 0.2;
	pending_jobs = 0;
	for (num_items = 10; num_items < 512; num_items += 2) {
		Job * job = g_new(Job, 1);
		job->num_items = num_items;
		job->sparsity = sparsity;
		g_thread_pool_push(pool, job, &error);
		if (error != NULL) {
			g_error("g_thread_pool_push: %s", error->message);
		}
		pending_jobs++;
	}

	while (pending_jobs > 0) {
		Result * result;
		// wait 30s between attempts to pop from queue (timed_pop for
		// compat).
		GTimeVal timeout;
		g_get_current_time(&timeout);
		g_time_val_add(&timeout, 30000000);
		result = g_async_queue_timed_pop(result_queue, &timeout);
		while (result != NULL) {
			g_print("%d %2.2e ", result->job->num_items, result->worst_time);
			tree_println(result->best_tree, "");
			result_free(result);
			pending_jobs--;
			result = g_async_queue_try_pop(result_queue);
		}
		g_print("status: %d pending\n", pending_jobs);
	}
	g_thread_pool_free(pool, FALSE, TRUE);
	g_async_queue_unref(result_queue);
	return 0;
}

