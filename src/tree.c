#include <gsl/gsl_sf_log.h>
#include <gsl/gsl_sf_exp.h>
#include "tree.h"
#include "util.h"

struct Tree_t {
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
};


static Tree * tree_new(Params * params);
static gdouble branch_log_not_pi(Tree * branch);
static gdouble branch_log_pi(Tree * branch, gdouble log_not_pi);
static gdouble branch_logprob(Tree * branch);
static gdouble leaf_logprob(Tree * leaf);

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


static Tree * tree_new(Params * params) {
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

Tree * tree_copy(Tree * orig) {
	Tree * tree;
	GList * child;
       
	tree = g_new(Tree, 1);
	tree->ref_count = 1;
	tree->is_leaf = orig->is_leaf;
	tree->params = orig->params;
	params_ref(tree->params);

	tree->suff_stats_on = suff_stats_copy(orig->suff_stats_on);
	tree->suff_stats_off = suff_stats_copy(orig->suff_stats_off);
	tree->labels = g_list_copy(orig->labels);
	tree->children = g_list_copy(orig->children);
	for (child = tree->children; child != NULL; child = g_list_next(child)) {
		tree_ref(child->data);
	}

	tree->dirty = orig->dirty;
	tree->log_pi = orig->log_pi;
	tree->log_not_pi = orig->log_not_pi;
	tree->logprob_cluster = orig->logprob_cluster;
	tree->logprob_children = orig->logprob_children;
	tree->logprob = orig->logprob;

	return tree;
}

Tree * leaf_new(Params * params, gpointer label) {
	Tree * leaf;

	leaf = tree_new(params);
	leaf->suff_stats_on = suff_stats_from_label(params, label);
	leaf->suff_stats_off = suff_stats_empty(params);
	leaf->labels = g_list_append(NULL, label);
	leaf->logprob = tree_get_logprob(leaf);
	return leaf;
}

Tree * branch_new(Params * params) {
	Tree * branch;

	branch = tree_new(params);
	branch->is_leaf = FALSE;
	branch->suff_stats_on = suff_stats_empty(params);
	branch->suff_stats_off = suff_stats_empty(params);
	branch->children = NULL;
	branch->logprob = tree_get_logprob(branch);
	return branch;
}

gboolean tree_is_leaf(Tree * tree) {
	return tree->is_leaf;
}

GList * tree_get_labels(Tree * tree) {
	return tree->labels;
}

Params * tree_get_params(Tree * tree) {
	return tree->params;
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

void tree_println(Tree * tree, const gchar *prefix) {
	GString * out;
	out = g_string_new(prefix);
	tree_tostring(tree, out);
	g_print("%s\n", out->str);
	g_string_free(out, TRUE);
}

void tree_tostring(Tree * tree, GString *str) {
	g_string_append_printf(str, "logprob: %2.2e #intern: %u ", tree->logprob, tree_num_intern(tree));
	tree_struct_print(tree, str);
}

void tree_ref(Tree * tree) {
	tree->ref_count++;
}

void tree_unref(Tree * tree) {
	if (tree == NULL) {
		return;
	}
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


gconstpointer leaf_get_label(Tree * leaf) {
	g_assert(tree_is_leaf(leaf));
	return leaf->labels->data;
}

static gdouble leaf_logprob(Tree * leaf) {
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

	/* data on the new offset par takes in both the off and on suff stats of
	 * the branch
	 */
	new_off = suff_stats_off_lookup(branch->params, branch->labels, child->labels);
	suff_stats_add(branch->suff_stats_off, new_off);
	suff_stats_add(branch->suff_stats_on, child->suff_stats_on);
	suff_stats_add(branch->suff_stats_on, new_off);
	suff_stats_unref(new_off);

	for (labels = child->labels; labels != NULL; labels = g_list_next(labels)) {
		branch->labels = g_list_insert_sorted(branch->labels,
				labels->data, cmp_quark);
	}
	branch->dirty = TRUE;
	branch->logprob = tree_get_logprob(branch);
}

GList * branch_get_children(Tree * branch) {
	g_assert(!tree_is_leaf(branch));
	return branch->children;
}

static gdouble branch_log_not_pi(Tree * branch) {
	gdouble num_children;

	num_children = g_list_length(branch->children);

	return (num_children-1)*branch->params->loggamma;
}

static gdouble branch_log_pi(Tree * branch, gdouble log_not_pi) {
	if (log_not_pi > gsl_sf_log(2.0)) {
		return gsl_sf_log(-gsl_sf_expm1(log_not_pi));
	} else {
		return gsl_sf_log_1plusx(-gsl_sf_exp(log_not_pi));
	}
}

static gdouble branch_logprob(Tree * branch) {
	GList * child;

	if (g_list_length(branch->children) < 2) {
		return 0.0;
	}

	branch->log_not_pi = branch_log_not_pi(branch);
	branch->log_pi = branch_log_pi(branch, branch->log_not_pi);
	branch->logprob_cluster = params_logprob_on(branch->params, branch->suff_stats_on);
	branch->logprob_children = params_logprob_off(branch->params, branch->suff_stats_off);
	/* g_print("children 0: %2.2e\n", branch->logprob_children); */
	for (child = branch->children; child != NULL; child = g_list_next(child)) {
		branch->logprob_children += tree_get_logprob(child->data);
		/* g_print("children n: %2.2e\n", branch->logprob_children); */
	}
	branch->logprob = log_add_exp(
			branch->log_pi + branch->logprob_cluster,
			branch->log_not_pi + branch->logprob_children);
	branch->dirty = FALSE;
	return branch->logprob;
}

gdouble tree_get_logprob(Tree *tree) {
	if (!tree->dirty) {
		return tree->logprob;
	}

	if (tree_is_leaf(tree)) {
		return leaf_logprob(tree);
	}
	return branch_logprob(tree);
}

