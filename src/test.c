#include <glib.h>
#include <gsl/gsl_sf_log.h>
#include <gsl/gsl_sf_gamma.h>
#include <gsl/gsl_sf_pow_int.h>
#include "nrt.h"


void init_test_toy3(Tree **laa, Tree **lbb, Tree **lcc) {
	Dataset *dataset;
	Params *params;
	gpointer aa, bb, cc;

	dataset = dataset_gen_toy3();
	/*dataset_println(dataset, "");*/
	aa = dataset_get_string_label(dataset, "aa");
	bb = dataset_get_string_label(dataset, "bb");
	cc = dataset_get_string_label(dataset, "cc");
	params = params_default(dataset);

	/* leaves */
	*laa = leaf_new(params, aa);
	*lbb = leaf_new(params, bb);
	*lcc = leaf_new(params, cc);

	params_unref(params);
	dataset_unref(dataset);
}

void init_test_toy4(Tree **laa, Tree **lbb, Tree **lcc, Tree **ldd) {
	Dataset *dataset;
	Params *params;
	gpointer aa, bb, cc, dd;

	dataset = dataset_gen_toy4();
	/* dataset_println(dataset, ""); */
	aa = dataset_get_string_label(dataset, "aa");
	bb = dataset_get_string_label(dataset, "bb");
	cc = dataset_get_string_label(dataset, "cc");
	dd = dataset_get_string_label(dataset, "dd");
	params = params_default(dataset);

	/* leaves */
	*laa = leaf_new(params, aa);
	*lbb = leaf_new(params, bb);
	*lcc = leaf_new(params, cc);
	*ldd = leaf_new(params, dd);

	params_unref(params);
	dataset_unref(dataset);
}

void test_tree_logprob3(void) {
	Params * params;
	Tree *laa, *lbb, *lcc;
	Tree *tab, *tac, *tbc, *tabc;
	gdouble prec;
	gdouble correct_tab, correct_tac, correct_tabc;

	prec = 1e-4;
	init_test_toy3(&laa, &lbb, &lcc);

	g_assert(tree_get_params(laa) == tree_get_params(lbb));
	g_assert(tree_get_params(lcc) == tree_get_params(lbb));
	params = tree_get_params(laa);
	params_ref(params);

	assert_eqfloat(tree_get_logprob(laa), 0.0, prec);
	assert_eqfloat(tree_get_logprob(lbb), 0.0, prec);
	assert_eqfloat(tree_get_logprob(lcc), 0.0, prec);

	/* pairs */
	tab = branch_new(params);
	branch_add_child(tab, laa);
	branch_add_child(tab, lbb);

	correct_tab =
		log_add_exp(gsl_sf_log(0.4) + gsl_sf_lnbeta(1.0+1, 0.2+0) - gsl_sf_lnbeta(1.0, 0.2)
			   ,gsl_sf_log(1.0 - 0.4) + gsl_sf_lnbeta(0.2+1, 1.0+0) - gsl_sf_lnbeta(0.2, 1.0)
			   );
	assert_eqfloat(tree_get_logprob(tab), correct_tab, prec);

	tac = branch_new(params);
	branch_add_child(tac, laa);
	branch_add_child(tac, lcc);

	correct_tac =
		log_add_exp(gsl_sf_log(0.4) + gsl_sf_lnbeta(1.0+0, 0.2+1) - gsl_sf_lnbeta(1.0, 0.2)
			   ,gsl_sf_log(1.0 - 0.4) + gsl_sf_lnbeta(0.2+0, 1.0+1) - gsl_sf_lnbeta(0.2, 1.0)
			   );
	assert_eqfloat(tree_get_logprob(tac), correct_tac, prec);

	tbc = branch_new(params);
	branch_add_child(tbc, lbb);
	branch_add_child(tbc, lcc);
	assert_eqfloat(tree_get_logprob(tac), correct_tac, prec);

	/* 3-trees */
	tabc = branch_new(params);
	branch_add_child(tabc, tab);
	branch_add_child(tabc, lcc);

	correct_tabc =
		log_add_exp(gsl_sf_log(0.4) + gsl_sf_lnbeta(1.0+1, 0.2+2) - gsl_sf_lnbeta(1.0, 0.2)
			   ,gsl_sf_log(1.0 - 0.4) + correct_tab + 0.0 + gsl_sf_lnbeta(0.2, 1.0+2) - gsl_sf_lnbeta(0.2, 1.0)
			   );
	assert_eqfloat(tree_get_logprob(tabc), correct_tabc, prec);
	tree_unref(tabc);

	tabc = branch_new(params);
	branch_add_child(tabc, laa);
	branch_add_child(tabc, lcc);
	branch_add_child(tabc, lbb);

	correct_tabc =
		log_add_exp(gsl_sf_log(1.-(1.-0.4)*(1.-0.4)) + gsl_sf_lnbeta(1.0+1, 0.2+2) - gsl_sf_lnbeta(1.0, 0.2)
			   ,2*gsl_sf_log(1.0 - 0.4) + gsl_sf_lnbeta(0.2+1, 1.0+2) - gsl_sf_lnbeta(0.2, 1.0)
			   );
	assert_eqfloat(tree_get_logprob(tabc), correct_tabc, prec);

	/* deallocate */
	tree_unref(laa);
	tree_unref(lbb);
	tree_unref(lcc);
	tree_unref(tab);
	tree_unref(tac);
	tree_unref(tbc);
	tree_unref(tabc);
	params_unref(params);
}

void test_tree_logprob4(void) {
	Params * params;
	Merge * merge;
	GRand *rng;
	Tree *laa, *lbb, *lcc, *ldd;
	Tree *tab, *tac, *tcd, *tbalance;
	Tree *tflat, *tcascade, *tcascade_intern;
	gdouble correct_laa, correct_lcc, correct_tab, correct_tac, correct_tcd;
	gdouble correct_tbalance, correct_tbalance_children;
	gdouble correct_tflat, correct_tcascade, correct_tcascade_intern;
	gdouble prec;

	prec = 1e-4;
	rng = g_rand_new();
	init_test_toy4(&laa, &lbb, &lcc, &ldd);

	g_assert(tree_get_params(laa) == tree_get_params(lbb));
	g_assert(tree_get_params(lcc) == tree_get_params(lbb));
	g_assert(tree_get_params(ldd) == tree_get_params(lbb));
	params = tree_get_params(laa);
	params_ref(params);

	/* leaves */
	correct_laa = gsl_sf_lnbeta(1.0+1, 0.2) - gsl_sf_lnbeta(1.0, 0.2);
	correct_lcc = gsl_sf_lnbeta(1.0, 0.2+1) - gsl_sf_lnbeta(1.0, 0.2);
	assert_eqfloat(tree_get_logprob(laa), correct_laa, prec);
	assert_eqfloat(tree_get_logprob(lbb), 0.0, prec);
	assert_eqfloat(tree_get_logprob(lcc), correct_lcc, prec);
	assert_eqfloat(tree_get_logprob(ldd), 0.0, prec);

	/* pair */
	tab = branch_new(params);
	branch_add_child(tab, laa);
	branch_add_child(tab, lbb);

	correct_tab =
		log_add_exp(gsl_sf_log(0.4) + gsl_sf_lnbeta(1.0+1, 0.2+2) - gsl_sf_lnbeta(1.0, 0.2)
			   ,gsl_sf_log(1.0 - 0.4) + gsl_sf_lnbeta(0.2, 1.0+2) - gsl_sf_lnbeta(0.2, 1.0)
				  + correct_laa
			   );
	assert_eqfloat(tree_get_logprob(tab), correct_tab, prec);

	tac = branch_new(params);
	branch_add_child(tac, laa);
	branch_add_child(tac, lcc);
	correct_tac =
		log_add_exp(gsl_sf_log(0.4) + gsl_sf_lnbeta(1.0+2, 0.2+2) - gsl_sf_lnbeta(1.0, 0.2)
			   ,gsl_sf_log(1.0 - 0.4) + gsl_sf_lnbeta(0.2+1, 1.0+1) - gsl_sf_lnbeta(0.2, 1.0)
				  + correct_laa + correct_lcc
			   );
	assert_eqfloat(tree_get_logprob(tac), correct_tac, prec);

	tcd = branch_new(params);
	branch_add_child(tcd, lcc);
	branch_add_child(tcd, ldd);
	correct_tcd =
		log_add_exp(gsl_sf_log(0.4) + gsl_sf_lnbeta(1.0+1, 0.2+2) - gsl_sf_lnbeta(1.0, 0.2)
			   ,gsl_sf_log(1.0 - 0.4) + gsl_sf_lnbeta(0.2+1, 1.0+1) - gsl_sf_lnbeta(0.2, 1.0)
				 + correct_lcc + 0.0
			   );
	assert_eqfloat(tree_get_logprob(tcd), correct_tcd, prec);


	/* 4-balance */
	correct_tbalance_children = gsl_sf_lnbeta(0.2+4, 1.0+3) - gsl_sf_lnbeta(0.2, 1.0) + correct_tab + correct_tcd;
	correct_tbalance =
		log_add_exp(gsl_sf_log(0.4) + gsl_sf_lnbeta(1.0+6, 0.2+7) - gsl_sf_lnbeta(1.0, 0.2)
			   ,gsl_sf_log(1.0 - 0.4) + correct_tbalance_children
			   );
	tbalance = branch_new(params);
	branch_add_child(tbalance, tab);
	branch_add_child(tbalance, tcd);
	assert_eqfloat(tree_get_logprob(tbalance), correct_tbalance, prec);

	/* 4-flat */
	correct_tflat =
		log_add_exp(gsl_sf_log(1.0 - gsl_sf_pow_int(1-0.4, 3)) + gsl_sf_lnbeta(1.0+6, 0.2+7) - gsl_sf_lnbeta(1.0, 0.2)
			   ,3*gsl_sf_log(1.0 - 0.4) + gsl_sf_lnbeta(0.2+5, 1.0+6) - gsl_sf_lnbeta(0.2, 1.0)
				+ gsl_sf_lnbeta(1.0+1, 0.2) - gsl_sf_lnbeta(1.0, 0.2)
				+ gsl_sf_lnbeta(1.0, 0.2+1) - gsl_sf_lnbeta(1.0, 0.2)
			   );
	tflat = branch_new(params);
	branch_add_child(tflat, laa);
	branch_add_child(tflat, lbb);
	branch_add_child(tflat, lcc);
	branch_add_child(tflat, ldd);
	assert_eqfloat(tree_get_logprob(tflat), correct_tflat, prec);

	merge = merge_collapse(rng, params, 0, tcd, 1, tab);
	assert_eqfloat(tree_get_logprob(merge->tree), correct_tflat, prec);
	merge_free(merge);

	/* 4-cascade */
	correct_tcascade_intern =
		log_add_exp(
			  gsl_sf_log(0.4) + gsl_sf_lnbeta(1.0+4, 0.2+4) - gsl_sf_lnbeta(1.0, 0.2)
			, gsl_sf_log(1-0.4) + gsl_sf_lnbeta(0.2+3, 1.0+1) - gsl_sf_lnbeta(0.2, 1.0)
				+ gsl_sf_lnbeta(1.0, 0.2+1) - gsl_sf_lnbeta(1.0, 0.2)
				+ correct_tab);
	correct_tcascade =
		log_add_exp(gsl_sf_log(0.4) + gsl_sf_lnbeta(1.0+6, 0.2+7) - gsl_sf_lnbeta(1.0, 0.2)
			, gsl_sf_log(1-0.4) + gsl_sf_lnbeta(0.2+2, 1.0+3) - gsl_sf_lnbeta(0.2, 1.0)
				+ correct_tcascade_intern);
	tcascade_intern = branch_new(params);
	branch_add_child(tcascade_intern, tab);
	branch_add_child(tcascade_intern, lcc);
	assert_eqfloat(tree_get_logprob(tcascade_intern), correct_tcascade_intern, prec);
	tcascade = branch_new(params);
	branch_add_child(tcascade, tcascade_intern);
	branch_add_child(tcascade, ldd);
	assert_eqfloat(tree_get_logprob(tcascade), correct_tcascade, prec);

	merge = merge_join(rng, params, 0, tcascade_intern, 1, ldd);
	assert_eqfloat(tree_get_logprob(merge->tree), correct_tcascade, prec);
	merge_free(merge);

	/* deallocate */
	tree_unref(laa);
	tree_unref(lbb);
	tree_unref(lcc);
	tree_unref(tab);
	tree_unref(tcd);
	tree_unref(tbalance);
	tree_unref(tflat);
	tree_unref(tcascade);
	tree_unref(tcascade_intern);
	params_unref(params);
	g_rand_free(rng);
}

void test_merge_score3(void) {
	GRand * rng;
	Params * params;
	Tree *laa, *lbb, *lcc;
	Tree *tab, *tabc;
	Merge *merge;
	gdouble prec;
	gdouble score_tabc, correct_tab;

	prec = 1e-4;
	rng = g_rand_new();
	init_test_toy3(&laa, &lbb, &lcc);
	params = tree_get_params(laa);

	tab = branch_new(params);
	branch_add_child(tab, laa);
	branch_add_child(tab, lbb);

	tabc = branch_new(params);
	branch_add_child(tabc, tab);
	branch_add_child(tabc, lcc);

	merge = merge_new(rng, params, 0, tab, 1, lcc, tabc);

	correct_tab =
		log_add_exp(gsl_sf_log(0.4) + gsl_sf_lnbeta(1.0+1, 0.2+0) - gsl_sf_lnbeta(1.0, 0.2)
			   ,gsl_sf_log(1.0 - 0.4) + gsl_sf_lnbeta(0.2+1, 1.0+0) - gsl_sf_lnbeta(0.2, 1.0)
			   );
	assert_eqfloat(tree_get_logprob(tab), correct_tab, prec);
	score_tabc =
		log_add_exp(gsl_sf_log(0.4) + gsl_sf_lnbeta(1.0+1, 0.2+2) - gsl_sf_lnbeta(1.0, 0.2)
			   ,gsl_sf_log(1.0 - 0.4) + correct_tab + 0.0 + gsl_sf_lnbeta(0.2, 1.0+2) - gsl_sf_lnbeta(0.2, 1.0)
			   )
	   	- (correct_tab + 0.0 + gsl_sf_lnbeta(0.2, 1.0+2) - gsl_sf_lnbeta(0.2, 1.0));
	assert_eqfloat(merge->score, score_tabc, prec);

	merge_free(merge);
	tree_unref(tab);
	tree_unref(tabc);
	tree_unref(laa);
	tree_unref(lbb);
	tree_unref(lcc);
	g_rand_free(rng);
}


int main(int argc, char *argv[]) {
	g_test_init(&argc, &argv, NULL);
	g_test_add_func("/tree/logprob3", test_tree_logprob3);
	g_test_add_func("/tree/logprob4", test_tree_logprob4);
	g_test_add_func("/merge/score3", test_merge_score3);
	g_test_run();
	return 0;
}
