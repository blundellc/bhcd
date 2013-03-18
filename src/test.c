#include <glib.h>
#include <gsl/gsl_sf_log.h>
#include <gsl/gsl_sf_gamma.h>
#include "nrt.h"


void init_test_toy3(Tree **laa, Tree **lbb, Tree **lcc) {
	Dataset *dd;
	Params *params;
	gpointer aa, bb, cc;

	dd = dataset_gen_toy3();
	/*dataset_println(dd, "");*/
	aa = dataset_get_string_label(dd, "aa");
	bb = dataset_get_string_label(dd, "bb");
	cc = dataset_get_string_label(dd, "cc");
	params = params_default(dd);

	/* leaves */
	*laa = leaf_new(params, aa);
	*lbb = leaf_new(params, bb);
	*lcc = leaf_new(params, cc);

	params_unref(params);
	dataset_unref(dd);
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


void test_merge_score3(void) {
	Params * params;
	Tree *laa, *lbb, *lcc;
	Tree *tab, *tabc;
	Merge *merge;
	gdouble prec;
	gdouble score_tabc, correct_tab;

	prec = 1e-4;
	init_test_toy3(&laa, &lbb, &lcc);
	params = tree_get_params(laa);

	tab = branch_new(params);
	branch_add_child(tab, laa);
	branch_add_child(tab, lbb);

	tabc = branch_new(params);
	branch_add_child(tabc, tab);
	branch_add_child(tabc, lcc);

	merge = merge_new(params, 0, tab, 1, lcc, tabc);

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
}


int main(int argc, char *argv[]) {
	g_test_init(&argc, &argv, NULL);
	g_test_add_func("/tree/logprob3", test_tree_logprob3);
	g_test_add_func("/merge/score3", test_merge_score3);
	g_test_run();
	return 0;
}
