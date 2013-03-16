#include <stdio.h>
#include <glib.h>
#include <gsl/gsl_sf_gamma.h>


typedef struct {
	guint		ref_count;
	GHashTable *	labels;
	GHashTable *	cells;
} Dataset;

typedef struct {
	const gchar *	src;
	const gchar *	dst;
} Dataset_Key;


Dataset_Key * dataset_key(const gchar *src, const gchar *dst) {
	Dataset_Key * key = g_new(Dataset_Key, 1);
	key->src = src;
	key->dst = dst;
	return key;
}

guint dataset_key_hash(gconstpointer pkey) {
	const Dataset_Key *key = pkey;
	return 41*g_str_hash(key->src) + g_str_hash(key->dst);
}

gboolean dataset_key_eq(gconstpointer paa, gconstpointer pbb) {
	const Dataset_Key * aa = paa;
	const Dataset_Key * bb = pbb;
	return g_str_equal(aa->src, bb->src) && g_str_equal(aa->dst, bb->dst);
}

Dataset* dataset_new(void) {
	Dataset * data = g_new(Dataset, 1);
	data->ref_count = 1;
	data->cells = g_hash_table_new_full(
				dataset_key_hash,
				dataset_key_eq,
				g_free,
				NULL
			);
	data->labels = g_hash_table_new_full(
				g_str_hash,
				g_str_equal,
				g_free,
				NULL
			);
	return data;
}

void dataset_add_label(Dataset * dataset, const gchar * label) {
	if (g_hash_table_contains(dataset->labels, label)) {
		return;
	}
	g_hash_table_insert(dataset->labels, g_strdup(label), NULL);
}

void dataset_set(Dataset * dataset, const gchar *src, const gchar *dst, gboolean value) {
	Dataset_Key * key = dataset_key(src, dst);
	g_hash_table_replace(dataset->cells, key, GINT_TO_POINTER(value));
	dataset_add_label(dataset, src);
	dataset_add_label(dataset, dst);
}

GList * dataset_get_labels(Dataset * dataset) {
	return g_hash_table_get_keys(dataset->labels);
}

gboolean dataset_is_missing(Dataset * dataset, const gchar *src, const gchar *dst) {
	Dataset_Key * key = dataset_key(src, dst);
	gboolean is_missing = !g_hash_table_contains(dataset->cells, key);
	g_free(key);
	return is_missing;
}

gboolean dataset_get(Dataset * dataset, const gchar *src, const gchar *dst, gboolean *missing) {
	gboolean value;
	Dataset_Key * key;
	gpointer ptr;

	key = dataset_key(src, dst);
	ptr = g_hash_table_lookup(dataset->cells, key);
	g_free(key);
	if (ptr == NULL) {
		g_assert(missing != NULL);
		*missing = TRUE;
		return FALSE;
	}

	if (missing != NULL) {
		*missing = FALSE;
	}
	value = GPOINTER_TO_INT(ptr);
	return value;
}

void dataset_ref(Dataset* dataset) {
	dataset->ref_count++;
}

void dataset_unref(Dataset* dataset) {
	if (dataset->ref_count <= 1) {
		g_hash_table_unref(dataset->cells);
		g_hash_table_unref(dataset->labels);
		g_free(dataset);
	} else {
		dataset->ref_count--;
	}
}

typedef struct {
	guint		num_ones;
	guint		num_total;
} Counts;

Counts * counts_new(guint num_ones, guint num_total) {
	Counts * counts = g_new(Counts, 1);
	counts->num_ones = num_ones;
	counts->num_total = num_total;
	return counts;
}

Counts * counts_copy(Counts * orig) {
	return counts_new(orig->num_ones, orig->num_total);
}

void counts_add(Counts * dst, Counts * src) {
	dst->num_ones += src->num_ones;
	dst->num_total += src->num_total;
}

void counts_free(Counts * counts) {
	g_free(counts);
}


typedef struct {
	guint		ref_count;
	Dataset *	dataset;
	gdouble		gamma;
	gdouble		alpha;
	gdouble		beta;
	gdouble		delta;
	gdouble		lambda;
} Params;

Params * params_new(Dataset * dataset, gdouble gamma, gdouble alpha, gdouble beta, gdouble delta, gdouble lambda) {
	Params * params = g_new(Params, 1);
	params->ref_count = 1;
	params->dataset = dataset;
	dataset_ref(dataset);
	params->gamma = gamma;
	params->alpha = alpha;
	params->beta = beta;
	params->delta = delta;
	params->lambda = lambda;
	return params;
}

Params * params_default(Dataset * dataset) {
	return params_new(dataset,
			  0.4,
			  1.0, 0.2,
			  0.2, 1.0);
}

void params_ref(Params * params) {
	params->ref_count++;
}

void params_unref(Params * params) {
	if (params->ref_count <= 1) {
		dataset_unref(params->dataset);
		g_free(params);
	} else {
		params->ref_count--;
	}
}

gdouble params_logprob_cluster(Params * params, gpointer pcounts) {
	Counts * counts = pcounts;
	gdouble a1 = params->alpha + counts->num_ones;
	gdouble b0 = params->beta  + counts->num_total - counts->num_ones;
	return gsl_sf_lnbeta(a1, b0) - gsl_sf_lnbeta(params->alpha, params->beta);
}

gpointer suff_stats_from_label(Params * params, gpointer label) {
	gboolean missing;
	gboolean value = dataset_get(params->dataset, label, label, &missing);
	if (missing) {
		return counts_new(0, 0);
	}
	return counts_new(value, 1);
}

gpointer suff_stats_empty(Params * params) {
	return counts_new(0, 0);
}

void suff_stats_add(gpointer pdst, gpointer psrc) {
	Counts * dst = pdst;
	Counts * src = psrc;
	counts_add(dst, src);
}


void suff_stats_free(gpointer ss) {
	counts_free(ss);
}


typedef struct {
	guint		ref_count;
	Params *	params;
	gpointer	suff_stats_on;
	gpointer	suff_stats_off;
	gdouble		logprob;
	GPtrArray *	children;
	union {
		gpointer label;
	} u;
} Tree;

void tree_unref(Tree *);

Tree * leaf_new(Params * params, gpointer label) {
	Tree * leaf = g_new(Tree, 1);
	leaf->ref_count = 1;
	leaf->suff_stats_on = suff_stats_from_label(params, label);
	leaf->suff_stats_off = suff_stats_empty(params);
	leaf->params = params;
	params_ref(leaf->params);
	leaf->logprob = params_logprob_cluster(leaf->params, leaf->suff_stats_on);
	leaf->children = NULL;
	leaf->u.label = label;
	return leaf;
}

Tree * branch_new(Params * params) {
	Tree * branch = g_new(Tree, 1);
	branch->ref_count = 1;
	branch->suff_stats_on = suff_stats_empty(params);
	branch->suff_stats_off = suff_stats_empty(params);
	branch->params = params;
	params_ref(branch->params);
	branch->children = g_ptr_array_new_with_free_func((GDestroyNotify)tree_unref);
	branch->u.label = NULL;
	branch->logprob = 0.0;
	return branch;
}

gboolean tree_is_leaf(Tree * tree) {
	return tree->children == NULL;
}

guint tree_num_leaves(Tree * tree) {
	guint total, ii;

	if (tree_is_leaf(tree)) {
		return 1;
	}

	total = 0;
	for (ii = 0; ii < tree->children->len; ii++) {
		total += tree_num_leaves(g_ptr_array_index(tree->children, ii));
	}
	return total;
}

guint tree_num_intern(Tree * tree) {
	guint total, ii;

	if (tree_is_leaf(tree)) {
		return 0;
	}

	total = 1;
	for (ii = 0; ii < tree->children->len; ii++) {
		total += tree_num_intern(g_ptr_array_index(tree->children, ii));
	}
	return total;
}

void tree_struct_print(Tree * tree, GString *str) {
	guint ii;

	if (tree_is_leaf(tree)) {
		g_string_append_printf(str, "%s", (gchar *)tree->u.label);
		return;
	}
	g_string_append_printf(str, "%2.2e:{", tree->logprob);
	for (ii = 0; ii < tree->children->len; ii++) {
		tree_struct_print(g_ptr_array_index(tree->children, ii), str);
		if (ii != tree->children->len-1) {
			g_string_append_printf(str, ", ");
		}
	}
	g_string_append_printf(str, "}");
}

void tree_print(Tree * tree, GString *str) {
	g_string_append_printf(str, "logprob: %2.2e #intern: %u ", tree->logprob, tree_num_intern(tree));
	tree_struct_print(tree, str);
}

void tree_ref(Tree * tree) {
	tree->ref_count++;
}

void tree_unref(Tree * tree) {
	if (tree->ref_count <= 1) {
		if (!tree_is_leaf(tree)) {
			g_ptr_array_unref(tree->children);
		}
		suff_stats_free(tree->suff_stats_on);
		suff_stats_free(tree->suff_stats_off);
		params_unref(tree->params);
		g_free(tree);
	} else {
		tree->ref_count--;
	}
}

void branch_add_child(Tree * branch, Tree * child) {
	g_assert(!tree_is_leaf(branch));
	tree_ref(child);
	g_ptr_array_add(branch->children, child);
	suff_stats_add(branch->suff_stats_on, child->suff_stats_on);
	suff_stats_add(branch->suff_stats_off, child->suff_stats_off);
}

typedef struct {
	guint ii;
	guint jj;
	Tree * tree;
	gdouble score;
} Merge;

Merge * merge_new(Params * params, guint ii, Tree * aa, guint jj, Tree * bb) {
	Merge * merge = g_new(Merge, 1);
	merge->ii = ii;
	merge->jj = jj;
	merge->tree = branch_new(params);
	branch_add_child(merge->tree, aa);
	branch_add_child(merge->tree, bb);
	merge->score = -(gdouble)(ii * jj);
	return merge;
}

void merge_free(Merge * merge) {
	tree_unref(merge->tree);
	g_free(merge);
}


gint cmp_score(gconstpointer paa, gconstpointer pbb, gpointer userdata) {
	const Merge * aa = paa;
	const Merge * bb = pbb;
	return aa->score - bb->score;
}


Tree * build(Params * params, GList * labels) {
	GPtrArray * trees;
	GSequence * merges;
	Merge * new_merge;
	Merge * cur;
	GSequenceIter * head;
	guint live_trees;
	guint ii, jj, kk, ll;
	gpointer * tii;
	gpointer * tjj;
	Tree * tkk;
	Tree * tll;

	trees = g_ptr_array_new();
	live_trees = 0;
	for (labels = g_list_first(labels); labels != NULL; labels = g_list_next(labels)) {
		Tree * leaf = leaf_new(params, labels->data);
		g_ptr_array_add(trees, leaf);
		live_trees++;
	}

	merges = g_sequence_new(NULL);

	for (ii = 0; ii < trees->len; ii++) {
		Tree * aa;

		aa = g_ptr_array_index(trees, ii);

		for (jj = ii + 1; jj < trees->len; jj++) {
			Tree * bb;

			bb = g_ptr_array_index(trees, jj);
			new_merge = merge_new(params, ii, aa, jj, bb);
			g_sequence_insert_sorted(merges, new_merge, cmp_score, NULL);
		}
	}

	while (live_trees > 1) {
		g_assert(g_sequence_get_length(merges) > 0);
		head = g_sequence_get_begin_iter(merges);
		cur = (Merge *)g_sequence_get(head);
		g_sequence_remove(head);

		if (g_ptr_array_index(trees, cur->ii) == NULL ||
		    g_ptr_array_index(trees, cur->jj) == NULL) {
			goto again;
		}

		tii = &g_ptr_array_index(trees, cur->ii);
		tree_unref(*tii);
		*tii = NULL;
		tjj = &g_ptr_array_index(trees, cur->jj);
		tree_unref(*tjj);
		*tjj = NULL;
		tkk = cur->tree;
		kk = trees->len;
		live_trees--;

		for (ll = 0; ll < trees->len; ll++) {
			if (g_ptr_array_index(trees, ll) == NULL) {
				continue;
			}

			tll = g_ptr_array_index(trees, ll);
			new_merge = merge_new(params, kk, tkk, ll, tll);
			g_sequence_insert_sorted(merges, new_merge, cmp_score, NULL);
		}

		g_ptr_array_add(trees, tkk);
		tree_ref(tkk);
again:
		merge_free(cur);
	}

	{
		Tree * root;

		root = (Tree *)g_ptr_array_index(trees, trees->len - 1);
		tree_ref(root);
		g_assert(root != NULL);
		tree_unref(root);
		g_ptr_array_free(trees, TRUE);
		g_sequence_free(merges);
		return root;
	}
}

int main(int argc, char * argv[]) {
	Dataset * dataset;
	Params * params;
	GList * labels;
	Tree * root;
	GString * out;

	dataset = dataset_new();

	/*     aa   bb   cc
	 * aa   _    1    0
	 * bb   1    _    0
	 * cc   0    0    _
	 */
	dataset_set(dataset, "aa", "bb", TRUE);
	dataset_set(dataset, "bb", "aa", TRUE);
	dataset_set(dataset, "aa", "cc", FALSE);
	dataset_set(dataset, "cc", "aa", FALSE);
	dataset_set(dataset, "bb", "cc", FALSE);
	dataset_set(dataset, "cc", "bb", FALSE);

	params = params_default(dataset);

	labels = dataset_get_labels(dataset);

	dataset_unref(dataset);

	root = build(params, labels);

	out = g_string_new("result: ");
	tree_print(root, out);
	g_print("%s\n", out->str);
	g_string_free(out, TRUE);

	g_assert(tree_num_leaves(root) == 3);
	tree_unref(root);
	g_list_free(labels);
	params_unref(params);
	return 0;
}

