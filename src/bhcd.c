#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib/gprintf.h>
#include "bhcd.h"


static gboolean binary_only = FALSE;
static gboolean sparse_greedy = FALSE;
static gboolean lua_shell = FALSE;
static gboolean verbose = FALSE;
static gboolean disable_fit_file = FALSE;
static guint build_restarts = 1;
static guint seed = 0x2a23b6bb;
static gdouble param_gamma = 0.4;
static gdouble param_alpha = 1.0;
static gdouble param_beta = 0.2;
static gdouble param_delta = 0.2;
static gdouble param_lambda = 1.0;
static gchar *	test_fname = NULL;
static gchar 	output_prefix_default[] = "output/out";
static gchar *	output_prefix = output_prefix_default;
static gchar *	output_tree_fname = NULL;
static gchar *	output_pred_fname = NULL;
static gchar *	output_fit_fname = NULL;
static gchar *	output_time_fname = NULL;
static gchar *	output_hypers_fname = NULL;

static GOptionEntry options[] = {
	{ "seed",	 's', 0, G_OPTION_ARG_INT,	&seed,		"set RNG seed to S",		"S" },
	{ "lua",	 'L', 0, G_OPTION_ARG_NONE,	&lua_shell,	"drop to lua shell",		NULL },
	{ "verbose",	 'v', 0, G_OPTION_ARG_NONE,	&verbose,	"be verbose",			NULL },

	{ "sparse",	 'S', 0, G_OPTION_ARG_NONE,	&sparse_greedy,	"use sparse greedy algorithm",	NULL },
	{ "binary-only", 'B', 0, G_OPTION_ARG_NONE,	&binary_only, 	"only construct binary trees",	NULL },
	{ "restarts",	 'R', 0, G_OPTION_ARG_INT,	&build_restarts,"take best of N restarts",	"N" },

	{ "no-fit-file",   0, 0, G_OPTION_ARG_NONE,	&disable_fit_file, "do not generate .fit file",	NULL },
	{ "test-file",	 't', 0, G_OPTION_ARG_FILENAME,	&test_fname,	"test dataset", NULL },
	{ "prefix",	 'p', 0, G_OPTION_ARG_STRING,	&output_prefix,	"prefix for output filenames", NULL },
	{ "sample-hypers", 0, 0, G_OPTION_ARG_STRING,	&output_hypers_fname,
									"slice sample hyperparameters, output to this file", NULL },

	{ "gamma",	 'g', 0, G_OPTION_ARG_DOUBLE,	&param_gamma,	"set mixture parameter to GAMMA","GAMMA" },
	{ "alpha",	 'a', 0, G_OPTION_ARG_DOUBLE,	&param_alpha,	"set on-diagonal one hyperparameter to ALPHA", "ALPHA" },
	{ "beta",	 'b', 0, G_OPTION_ARG_DOUBLE,	&param_beta,	"set on-diadonal zero hyperparameter to BETA", "BETA" },
	{ "delta",	 'd', 0, G_OPTION_ARG_DOUBLE,	&param_delta,	"set off-diagonal one hyperparameter to DELTA", "DELTA" },
	{ "lambda",	 'l', 0, G_OPTION_ARG_DOUBLE,	&param_lambda,	"set off-diadonal zero hyperparameter to LAMBDA", "LAMBDA" },
	{ NULL,		   0, 0, 0,			NULL,		NULL, NULL },
};

static gchar * parse_args(int *argc, char ***argv);
static Tree * run(GRand * rng, Dataset * dataset);
static void save_pred(Pair * tree_dataset, GIOChannel * io);
static void timer_save_io(GTimer * timer, GIOChannel * io);


static gchar * parse_args(int *argc, char ***argv) {
	GError * error;
	GOptionContext * ctx;

	ctx = g_option_context_new("FILE -- fit an BHCD (" BHCD_VERSION ")");
	g_option_context_add_main_entries(ctx, options, NULL);
	error = NULL;
	if (!g_option_context_parse(ctx, argc, argv, &error)) {
		g_print("option parsing failed: %s\n", error->message);
		goto error;
	}
	if (*argc < 2) {
		g_print("missing argument FILE\n");
		goto error;
	}
	if (*argc > 2) {
		g_print("too many arguments\n");
		goto error;
	}
	g_option_context_free(ctx);
	output_tree_fname = g_strdup_printf("%s.tree", output_prefix);
	output_pred_fname = g_strdup_printf("%s.pred", output_prefix);
	output_fit_fname  = g_strdup_printf("%s.fit", output_prefix);
	output_time_fname = g_strdup_printf("%s.time", output_prefix);
	return (*argv)[1];
error:
	g_print("%s", g_option_context_get_help(ctx, TRUE, NULL));
	g_option_context_free(ctx);
	exit(1);
}

static Tree * run(GRand * rng, Dataset * dataset) {
	Params * params;
	Tree * root;
	Build * build;

	if (verbose) {
		dataset_println(dataset, "");
	}

	params = params_new(dataset,
			param_gamma,
			param_alpha, param_beta,
			param_delta, param_lambda);
	params->binary_only = binary_only;

	build = build_new(rng, params, build_restarts, sparse_greedy);
	build_set_verbose(build, verbose);
	params_unref(params);
	build_run(build);
	root = build_get_best_tree(build);
	tree_ref(root);
	build_free(build);

	g_assert(tree_num_leaves(root) == dataset_num_labels(dataset));
	return root;
}

static void eval_test(Pair * root_timer, GIOChannel *io) {
	Dataset * test;
	Pair * root_timer_test;

	if (test_fname == NULL) {
		return;
	}
	test = dataset_gml_load(test_fname);
	root_timer_test = pair_new(root_timer, test);
	save_pred(root_timer_test, io);
	pair_free(root_timer_test);
	dataset_unref(test);
}

static void save_pred(Pair * root_timer_data, GIOChannel * io) {
	Pair * root_timer = root_timer_data->fst;
	Tree * const tree = root_timer->fst;
	GTimer * timer = root_timer->snd;
	Dataset * const dataset = root_timer_data->snd;
	DatasetPairIter pairs;
	gpointer src, dst;

	dataset_label_pairs_iter_init(dataset, &pairs);
	while (dataset_label_pairs_iter_next(&pairs, &src, &dst)) {
		gboolean missing;
		gboolean value = dataset_get(dataset, src, dst, &missing);
		gdouble logpred_true = tree_logpredict(tree, src, dst, TRUE);
		gdouble logpred_false = tree_logpredict(tree, src, dst, FALSE);
		g_assert(!missing);
		io_printf(io, "%e,%s,%s,%s,%1.17e,%1.17e\n",
				g_timer_elapsed(timer, NULL),
				dataset_label_to_string(dataset, src),
				dataset_label_to_string(dataset, dst),
				(value? "true": "false"),
				logpred_false,
				logpred_true);
	}
}

static void timer_save_io(GTimer * timer, GIOChannel * io) {
	io_printf(io, "time: %es\n", g_timer_elapsed(timer, NULL));
}

static gdouble eval_tree_params_logprob(Params * params, gpointer ptree) {
	Tree * tree = ptree;

	tree_set_params(tree, params, TRUE);
	return tree_get_logprob(tree);
}

static void sample_save_hypers(Tree * root, GIOChannel * io) {
	GRand * rng;
	Params * params;

	rng = g_rand_new();
	params = tree_get_params(root);
	io_printf(io, "step, gamma, alpha, beta, delta, lambda, logprob\n");
	for (guint step = 0; step < 1000; step++) {
		g_printf("%d, %e, %e, %e, %e, %e, %e\n",
				step, params->gamma,
				params->alpha, params->beta,
				params->delta, params->lambda,
				tree_get_logprob(root)
			 );
		io_printf(io, "%d, %e, %e, %e, %e, %e, %e\n",
				step, params->gamma,
				params->alpha, params->beta,
				params->delta, params->lambda,
				tree_get_logprob(root)
			 );
		params = params_sample(rng, params, eval_tree_params_logprob, root);
	}
}

int main(int argc, char * argv[]) {
	GRand * rng;
	GTimer * timer;
	Dataset * dataset;
	gchar * train_fname;
	Tree * root;
	Pair * root_timer;
	Pair * root_timer_train;

	train_fname = parse_args(&argc, &argv);

	g_print("seed: %x\n", seed);
	g_print("output prefix: %s\n", output_prefix);
	rng = g_rand_new_with_seed(seed);
	timer = g_timer_new();
	dataset = dataset_gml_load(train_fname);

	g_timer_start(timer);
	root = run(rng, dataset);
	g_timer_stop(timer);

	io_stdout((IOFunc)timer_save_io, timer);
	tree_println(root, "tree: ");

	io_writefile(output_time_fname, (IOFunc)timer_save_io, timer);
	tree_io_save(root, output_tree_fname);

	root_timer = pair_new(root, timer);
	io_writefile(output_pred_fname, (IOFunc)eval_test, root_timer);

	if (!disable_fit_file) {
		root_timer_train = pair_new(root_timer, dataset);
		io_writefile(output_fit_fname, (IOFunc)save_pred, root_timer_train);
		pair_free(root_timer_train);
	}
	pair_free(root_timer);

	if (output_hypers_fname != NULL) {
		io_writefile(output_hypers_fname, (IOFunc)sample_save_hypers, root);
	}

	if (lua_shell) {
		bhcd_lua_shell(root);
	}

	tree_unref(root);
	g_free(output_tree_fname);
	g_free(output_pred_fname);
	g_free(output_time_fname);
	dataset_unref(dataset);
	g_timer_destroy(timer);
	g_rand_free(rng);
	return 0;
}

