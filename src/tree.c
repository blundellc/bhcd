#include <stdarg.h>
#include <gsl/gsl_sf_log.h>
#include <gsl/gsl_sf_exp.h>
#include "tree.h"
#include "sscache.h"
#include "util.h"
#include "counts.h"

static const gboolean tree_debug = FALSE;


struct Tree_t {
	guint		ref_count;
	gboolean	is_leaf;
	/* shared */
	Params *	params;
	gpointer	suffstats_on;
	gpointer	suffstats_off;
	/* elements shared */
	GList *		children;

	Labelset *	labels;
	Labelset *	merge_left;
	Labelset *	merge_right;

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
	if (!tree_debug) {
		return;
	}
	g_assert(tree->ref_count >= 1);
	g_assert(tree->params != NULL);
	g_assert(tree->suffstats_on != NULL);
	g_assert(tree->suffstats_off != NULL);
	assert_lefloat(tree->logprob, 0.0, EQFLOAT_DEFAULT_PREC);
	if (tree_is_leaf(tree)) {
		g_assert(labelset_count(tree->labels) == 1);
		g_assert(labelset_count(tree->merge_left) == 1);
		g_assert(labelset_count(tree->merge_right) == 0);
	} else {
		GList *child;
		Labelset *combined;

		combined = labelset_new(tree->params->dataset);
		g_assert(tree->merge_left != NULL);
		g_assert(tree->merge_right != NULL);
		labelset_union(combined, tree->merge_left);
		labelset_union(combined, tree->merge_right);
		g_assert(labelset_equal(combined, tree->labels));
		labelset_unref(combined);

		for (child = tree->children; child != NULL; child = g_list_next(child)) {
			tree_assert(child->data);
		}
	}
}


static Tree * tree_new(Params * params) {
	Tree * tree;
       
	tree = g_slice_new(Tree);
	tree->ref_count = 1;
	tree->is_leaf = TRUE;
	tree->params = params;
	params_ref(tree->params);
	tree->suffstats_on = NULL;
	tree->suffstats_off = NULL;
	tree->children = NULL;
	tree->labels = NULL;
	tree->merge_left = NULL;
	tree->merge_right = NULL;

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
       
	tree = g_slice_new(Tree);
	tree->ref_count = 1;
	tree->is_leaf = orig->is_leaf;
	tree->params = orig->params;
	params_ref(tree->params);

	tree->suffstats_on = suffstats_copy(orig->suffstats_on);
	tree->suffstats_off = suffstats_copy(orig->suffstats_off);
	tree->labels = labelset_copy(orig->labels);
	tree->merge_left = labelset_copy(orig->merge_left);
	tree->merge_right = labelset_copy(orig->merge_right);
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

	tree_assert(tree);
	return tree;
}

guint32 tree_hash(Tree * tree) {
	guint32 hash;
	guint32 shift;

	hash = labelset_hash(tree->labels);
	return hash;
	shift = 0;
	for (GList * child = tree->children; child != NULL; child = g_list_next(child)) {
		guint32 child_hash = tree_hash(child->data);
		hash ^= child_hash >> shift;
		hash ^= (child_hash & (0xffffff >> shift)) << (32-shift);
		shift = (shift+1) & 31;
	}
	return hash;
}

Tree * leaf_new(Params * params, gconstpointer label) {
	Tree * leaf;

	leaf = tree_new(params);
	leaf->suffstats_on = sscache_get_label(params->sscache, label);
	suffstats_ref(leaf->suffstats_on);
	leaf->suffstats_off = suffstats_new_empty();
	leaf->labels = labelset_new(params->dataset, label);
	leaf->merge_left = labelset_new(params->dataset, label);
	leaf->merge_right = labelset_new(params->dataset);
	leaf->logprob = tree_get_logprob(leaf);
	tree_assert(leaf);
	return leaf;
}

Tree * branch_new(Params * params) {
	Tree * branch;

	branch = tree_new(params);
	branch->is_leaf = FALSE;
	branch->suffstats_on = suffstats_new_empty();
	branch->suffstats_off = suffstats_new_empty();
	branch->children = NULL;
	branch->labels = labelset_new(params->dataset);
	branch->merge_left = labelset_new(params->dataset);
	branch->merge_right = labelset_new(params->dataset);
	branch->logprob = tree_get_logprob(branch);
	tree_assert(branch);
	return branch;
}

Tree * branch_new_full1(Params * params, ...) {
	va_list ap;
	Tree * branch;

	branch = branch_new(params);
	va_start(ap, params);
	for (Tree * child = va_arg(ap, Tree *); child != NULL; child = va_arg(ap, Tree *)) {
		branch_add_child(branch, child);
	}
	va_end(ap);
	return branch;
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
		labelset_unref(tree->merge_left);
		labelset_unref(tree->merge_right);
		labelset_unref(tree->labels);
		suffstats_unref(tree->suffstats_on);
		suffstats_unref(tree->suffstats_off);
		params_unref(tree->params);
		g_slice_free(Tree, tree);
	} else {
		tree->ref_count--;
	}
}

gboolean tree_is_leaf(Tree * tree) {
	return tree->is_leaf;
}

Labelset * tree_get_labels(Tree * tree) {
	return tree->labels;
}

Labelset * tree_get_merge_left(Tree * tree) {
	return tree->merge_left;
}

Labelset * tree_get_merge_right(Tree * tree) {
	return tree->merge_right;
}

Params * tree_get_params(Tree * tree) {
	return tree->params;
}

void tree_set_params(Tree * tree, Params * params, gboolean recurse) {
	if (params != tree->params) {
		params_unref(tree->params);
		tree->params = params;
		params_ref(tree->params);
		tree->dirty = TRUE;
	}

	if (!recurse || tree_is_leaf(tree)) {
		return;
	}

	for (GList * child = tree->children; child != NULL; child = g_list_next(child)) {
		Tree * ch = child->data;
		tree_set_params(ch, params, recurse);
		/* propagate dirty bit. this is inefficient, as our
		 * params may not have changed, but by propagating dirty, we
		 * will still recalculate everything. but: simpler, uncommon use
		 * case.
		*/
		tree->dirty |= ch->dirty;
	}
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

void tree_struct_tostring(Tree * tree, GString *str) {
	GList * child;

	if (tree_is_leaf(tree)) {
		labelset_tostring(tree->labels, str);
		return;
	}
	g_string_append_printf(str, "%2.2e:{", tree->logprob);
	for (child = tree->children; child != NULL; child = g_list_next(child)) {
		tree_struct_tostring(child->data, str);
		if (g_list_next(child) != NULL) {
			g_string_append(str, ", ");
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
	g_string_append_printf(str, "logprob: %2.2e (0:%d, 1:%d) #intern: %u ",
			tree->logprob,
			counts_num_zeros(tree->suffstats_on),
			counts_num_ones(tree->suffstats_on),
			tree_num_intern(tree));
	tree_struct_tostring(tree, str);
}

gint tree_cmp_label(gconstpointer paa, gconstpointer pbb) {
	const Tree * aa = paa;
	const Tree * bb = pbb;
	return labelset_cmp(aa->labels, bb->labels);
}



gconstpointer leaf_get_label(Tree * leaf) {
	g_assert(tree_is_leaf(leaf));
	return labelset_any_label(leaf->labels);
}

static gdouble leaf_logprob(Tree * leaf) {
	g_assert(tree_is_leaf(leaf));
	leaf->logprob = params_logprob_on(leaf->params, leaf->suffstats_on);
	leaf->dirty = FALSE;
	return leaf->logprob;
}

void branch_add_child(Tree * branch, Tree * child) {
	gpointer new_off;

	g_assert(!tree_is_leaf(branch));
	tree_ref(child);

	if (branch->children != NULL) {
		/* once there is more than one child, we'd better start adding
		 * off diagonal terms.
		 */
		if (tree_debug) {
			tree_println(branch, "branch: ");
			labelset_print(branch->merge_left);
			g_print("/");
			labelset_print(branch->merge_right);
			g_print("\n");
			labelset_print(child->merge_left);
			g_print("/");
			labelset_print(child->merge_right);
			g_print("\n");
			tree_println(child, "child: ");
		}
		new_off = sscache_get_offblock(branch->params->sscache,
				branch->merge_left,
				branch->merge_right,
				child->merge_left,
				child->merge_right);
		if (tree_debug) {
			g_print("new off: ");
			suffstats_print(new_off);
			g_print("\n");
		}
		g_assert(new_off != NULL);
		suffstats_add(branch->suffstats_off, new_off);
		suffstats_add(branch->suffstats_on, new_off);
		new_off = NULL;

		labelset_set_equal(branch->merge_left, child->labels);
		labelset_set_equal(branch->merge_right, branch->labels);
	} else {
		labelset_set_equal(branch->merge_left, child->merge_left);
		labelset_set_equal(branch->merge_right, child->merge_right);
	}

	branch->children = g_list_prepend(branch->children, child);
	suffstats_add(branch->suffstats_on, child->suffstats_on);
	if (tree_debug) {
		g_print("branch on: "); suffstats_print(branch->suffstats_on); g_print("\n");
		g_print("branch off: "); suffstats_print(branch->suffstats_off); g_print("\n");
	}

	labelset_union(branch->labels, child->labels);

	if (tree_debug) {
		g_print("post merge: ");
		labelset_print(branch->merge_left);
		g_print("/");
		labelset_print(branch->merge_right);
		g_print("\n");
	}
	branch->dirty = TRUE;
	branch->logprob = tree_get_logprob(branch);
	tree_assert(branch);
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
	if (log_not_pi > G_LN2) {
		return gsl_sf_log(-gsl_sf_expm1(log_not_pi));
	} else {
		return gsl_sf_log_1plusx(-gsl_sf_exp(log_not_pi));
	}
}

static gdouble branch_logprob(Tree * branch) {
	GList * child;

	/* if fewer than two children, do nothing */
	if (branch->children == NULL || 
	    g_list_next(branch->children) == NULL) {
		branch->log_not_pi = 0.0;
		branch->log_pi = 0.0;
		branch->logprob_cluster = 0.0;
		branch->logprob_children = 0.0;
		branch->dirty = FALSE;
		return 0.0;
	}

	branch->log_not_pi = branch_log_not_pi(branch);
	branch->log_pi = branch_log_pi(branch, branch->log_not_pi);
	branch->logprob_cluster = params_logprob_on(branch->params, branch->suffstats_on);
	branch->logprob_children = params_logprob_off(branch->params, branch->suffstats_off);
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

gdouble tree_get_logresponse(Tree *tree) {
	gdouble logprob;

	if (tree_is_leaf(tree)) {
		return 0.0;
	} else {
		logprob = tree_get_logprob(tree);
		return tree->log_pi + tree->logprob_cluster - logprob;
	}
}

gdouble tree_get_lognotresponse(Tree *tree) {
	gdouble logprob;

	g_assert(!tree_is_leaf(tree));
	logprob = tree_get_logprob(tree);
	return tree->log_not_pi + tree->logprob_children - logprob;
}

gdouble tree_logpredict(Tree *tree, gconstpointer src, gconstpointer dst, gboolean value) {
	Tree * child;
	gdouble logpred_on;
	gdouble logpred_below;

	g_assert(labelset_contains(tree->labels, src));
	g_assert(labelset_contains(tree->labels, dst));

	logpred_on = params_logpred_on(tree->params, tree->suffstats_on, value);
	if (tree_is_leaf(tree)) {
		return logpred_on;
	}

	child = NULL;
	for (GList * xx = tree->children; xx != NULL; xx = g_list_next(xx)) {
		Tree * child_query = xx->data;
		if (labelset_contains(child_query->labels, src) &&
		    labelset_contains(child_query->labels, dst)) {
			child  = child_query;
			break;
		}
	}

	if (child == NULL) {
		/* src,dst lies in the off block */
		logpred_below = params_logpred_off(tree->params, tree->suffstats_off, value);
	} else {
		logpred_below = tree_logpredict(child, src, dst, value);
	}
	return log_add_exp(
		  tree_get_logresponse(tree)    + logpred_on
		, tree_get_lognotresponse(tree) + logpred_below
		);
}

