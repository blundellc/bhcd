#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <gsl/gsl_sf_gamma.h>
#include <gsl/gsl_sf_log.h>
#include <gsl/gsl_sf_exp.h>

typedef union {
	gpointer ptr;
	gconstpointer const_ptr;
} UnionPtr;

gdouble log_add_exp(gdouble xx, gdouble yy) {
	gdouble diff = xx - yy;
	if (diff >= 0) {
		return xx + gsl_sf_log_1plusx(gsl_sf_exp(-diff));
	} else if (diff < 0) {
		return yy + gsl_sf_log_1plusx(gsl_sf_exp(diff));
	} else {
		/* inf/nan with same sign */
		return xx + yy;
	}
}

gchar * num_to_string(guint ii) {
	static gchar base[] = "0123456789abcdefghijklmnopqrstuvwxyz";
	GString *out;

	out = g_string_new("");

	do {
		g_string_append_c(out, base[ii % sizeof(base)]);
		ii /= sizeof(base);
	} while (ii != 0);

	return g_string_free(out, FALSE);
}


gint cmp_quark(gconstpointer paa, gconstpointer pbb) {
	GQuark aa;
	GQuark bb;

	aa = GPOINTER_TO_INT(paa);
	bb = GPOINTER_TO_INT(pbb);
	return (gint)aa - (gint)bb;
}


#define	DATASET_VALUE_SHIFT	0x10
typedef struct {
	guint		ref_count;
	GHashTable *	labels;
	GHashTable *	cells;
} Dataset;

typedef struct {
	GQuark	src;
	GQuark	dst;
} Dataset_Key;


Dataset_Key * dataset_key(gpointer *psrc, gpointer *pdst, gboolean fast) {
	Dataset_Key * key;
	GQuark src;
	GQuark dst;

	key = g_new(Dataset_Key, 1);

	src = GPOINTER_TO_INT(psrc);
	dst = GPOINTER_TO_INT(pdst);

	if (src < dst) {
		key->src = src;
		key->dst = dst;
	} else {
		key->src = dst;
		key->dst = src;
	}
	return key;
}

guint dataset_key_hash(gconstpointer pkey) {
	const Dataset_Key *key = pkey;
	return 41*g_direct_hash(GINT_TO_POINTER(key->src))
		+ g_direct_hash(GINT_TO_POINTER(key->dst));
}

gboolean dataset_key_eq(gconstpointer paa, gconstpointer pbb) {
	const Dataset_Key * aa = paa;
	const Dataset_Key * bb = pbb;
	return aa->src == bb->src && aa->dst == bb->dst;
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
				NULL,
				NULL,
				NULL,
				NULL
			);
	return data;
}

guint dataset_num_labels(Dataset * dataset) {
	return g_hash_table_size(dataset->labels);
}


void dataset_add_label(Dataset * dataset, gpointer label) {
	if (g_hash_table_lookup_extended(dataset->labels, label, NULL, NULL)) {
		return;
	}
	/* key = value apparently enables some optimization in hash table for
	 * sets.
	 */
	g_hash_table_insert(dataset->labels, label, label);
}

gpointer dataset_add_string_label(Dataset * dataset, gchar * label) {
	GQuark qlabel;
	gpointer plabel;

	qlabel = g_quark_from_string(label);
	plabel = GINT_TO_POINTER(qlabel);
	dataset_add_label(dataset, plabel);
	return plabel;
}

gboolean dataset_is_missing(Dataset * dataset, gpointer src, gpointer dst) {
	Dataset_Key * key = dataset_key(src, dst, TRUE);
	gboolean is_missing = !g_hash_table_lookup_extended(dataset->cells, key, NULL, NULL);
	g_free(key);
	return is_missing;
}

void dataset_set(Dataset * dataset, gpointer src, gpointer dst, gboolean value) {
	Dataset_Key * key = dataset_key(src, dst, FALSE);
	g_hash_table_replace(dataset->cells, key, GINT_TO_POINTER(value+DATASET_VALUE_SHIFT));
	dataset_add_label(dataset, src);
	dataset_add_label(dataset, dst);
	g_assert(!dataset_is_missing(dataset, src, dst));
}

GList * dataset_get_labels(Dataset * dataset) {
	return g_hash_table_get_keys(dataset->labels);
}

GList * dataset_get_labels_string(Dataset * dataset) {
	GList * labels;
	GList * label;

	labels = g_hash_table_get_keys(dataset->labels);
	/* now convert each label back into a string... */
	for (label = labels; label != NULL; label = g_list_next(label)) {
		UnionPtr str;

		str.const_ptr = g_quark_to_string(GPOINTER_TO_INT(label->data));
		label->data = str.ptr;
	}

	return labels;
}

gboolean dataset_get(Dataset * dataset, gpointer src, gpointer dst, gboolean *missing) {
	gboolean value;
	Dataset_Key * key;
	gpointer ptr;

	key = dataset_key(src, dst, TRUE);
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
	value = GPOINTER_TO_INT(ptr) - DATASET_VALUE_SHIFT;
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


Dataset * dataset_gen_speckle(GRand * rng, guint num_items, gdouble prob_one) {
	Dataset * dd;
	guint ii, jj;

	dd = dataset_new();
	for (ii = 0; ii < num_items; ii++) {
		gchar *name_ii = num_to_string(ii);
		gpointer label_ii = dataset_add_string_label(dd, name_ii);
		g_free(name_ii);
		for (jj = ii; jj < num_items; jj++) {
			gchar *name_jj = num_to_string(jj);
			gpointer label_jj = dataset_add_string_label(dd, name_jj);
			g_free(name_jj);
			dataset_set(dd, label_ii, label_jj,
					g_rand_double(rng) < prob_one);
		}
	}
	return dd;
}


Dataset * dataset_gen_blocks(GRand * rng, guint num_items, guint block_width, gdouble prob_flip) {
	Dataset *dd;
	guint ii, jj;
	guint value;

	dd = dataset_new();
	for (ii = 0; ii < num_items; ii++) {
		gchar *name_ii = num_to_string(ii);
		gpointer label_ii = dataset_add_string_label(dd, name_ii);
		g_free(name_ii);
		for (jj = ii; jj < num_items; jj++) {
			gchar *name_jj = num_to_string(jj);
			gpointer label_jj = dataset_add_string_label(dd, name_jj);
			g_free(name_jj);

			value = (ii/block_width % 2) == (jj/block_width % 2)
				&& ABS((gint)ii-(gint)jj) < block_width;
			if (g_rand_double(rng) < prob_flip) {
				value = !value;
			}
			dataset_set(dd, label_ii, label_jj, value);
		}
	}
	return dd;
}


void dataset_print(Dataset * dataset, GString *str) {
	GList * labels;
	GList * xx;
	GList * yy;
	GQuark qxx;
	GQuark qyy;
	guint max_len;
	guint len;
	gboolean missing;
	gboolean value;

	labels = g_hash_table_get_keys(dataset->labels);
	max_len = 5;
	for (xx = labels; xx != NULL; xx = g_list_next(xx)) {
		qxx = GPOINTER_TO_INT(xx->data);
		len = strlen(g_quark_to_string(qxx));
		if (len > max_len) {
			max_len = len;
		}
	}
	/* header */
	g_string_append_printf(str, "%*s ", max_len, "");
	for (yy = labels; yy != NULL; yy = g_list_next(yy)) {
		qyy = GPOINTER_TO_INT(yy->data);
		g_string_append_printf(str, "%*s ", max_len, g_quark_to_string(qyy));
	}
	g_string_append(str, "\n");
	for (xx = labels; xx != NULL; xx = g_list_next(xx)) {
		qxx = GPOINTER_TO_INT(xx->data);
		g_string_append_printf(str, "%*s ", max_len, g_quark_to_string(qxx));
		/* content */
		for (yy = labels; yy != NULL; yy = g_list_next(yy)) {
			value = dataset_get(dataset, xx->data, yy->data, &missing);
			if (missing) {
				g_string_append_printf(str, "%*s ", max_len, "_");
			} else {
				g_string_append_printf(str, "%*d ", max_len, value);
			}
		}
		g_string_append(str, "\n");
	}
	g_list_free(labels);
}

Dataset * dataset_toy3(void) {
	Dataset * dataset;

	dataset = dataset_new();
	/*     aa   bb   cc
	 * aa   _    1    0
	 * bb   1    _    0
	 * cc   0    0    _
	 */
	dataset_set(dataset, "aa", "bb", TRUE);
	dataset_set(dataset, "aa", "cc", FALSE);
	dataset_set(dataset, "bb", "cc", FALSE);
	return dataset;
}


typedef struct {
	guint		ref_count;
	guint		num_ones;
	guint		num_total;
} Counts;

Counts * counts_new(guint num_ones, guint num_total) {
	Counts * counts;

	counts = g_new(Counts, 1);
	counts->ref_count = 1;
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

void counts_ref(Counts * counts) {
	counts->ref_count++;
}

void counts_unref(Counts * counts) {
	if (counts->ref_count <= 1) {
		g_free(counts);
	} else {
		counts->ref_count--;
	}
}


typedef struct {
	guint		ref_count;
	Dataset *	dataset;
	gdouble		gamma;
	gdouble		loggamma; /* really log(1-gamma) */
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
	params->loggamma = gsl_sf_log(1.0 - gamma);
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

gdouble params_logprob_on(Params * params, gpointer pcounts) {
	Counts * counts = pcounts;
	gdouble a1 = params->alpha + counts->num_ones;
	gdouble b0 = params->beta  + counts->num_total - counts->num_ones;
	return gsl_sf_lnbeta(a1, b0) - gsl_sf_lnbeta(params->alpha, params->beta);
}

gdouble params_logprob_off(Params * params, gpointer pcounts) {
	Counts * counts = pcounts;
	gdouble a1 = params->delta + counts->num_ones;
	gdouble b0 = params->lambda  + counts->num_total - counts->num_ones;
	return gsl_sf_lnbeta(a1, b0) - gsl_sf_lnbeta(params->delta, params->lambda);
}

gpointer suff_stats_from_label(Params * params, gpointer label) {
	gboolean missing;
	gboolean value;

	value = dataset_get(params->dataset, label, label, &missing);
	if (missing) {
		return counts_new(0, 0);
	}
	return counts_new(value, 1);
}

gpointer suff_stats_copy(gpointer src) {
	return counts_copy(src);
}

gpointer suff_stats_empty(Params * params) {
	return counts_new(0, 0);
}

void suff_stats_add(gpointer pdst, gpointer psrc) {
	Counts * dst = pdst;
	Counts * src = psrc;
	counts_add(dst, src);
}


gpointer params_suff_stats_off_lookup(Params * params, GList * srcs, GList * dsts) {
	Counts * counts;
	GList * src;
	GList * dst;
	gboolean missing;
	gboolean value;

	counts = counts_new(0, 0);

	for (src = g_list_first(srcs); src != NULL; src = g_list_next(src)) {
		for (dst = g_list_first(dsts); dst != NULL; dst = g_list_next(dst)) {
			value = dataset_get(params->dataset, src->data, dst->data, &missing);
			if (!missing) {
				counts->num_ones += value;
				counts->num_total++;
			}
		}
	}
	return counts;
}

void suff_stats_unref(gpointer ss) {
	counts_unref(ss);
}


typedef struct {
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
} Tree;

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

void tree_unref(Tree *);
void tree_ref(Tree *);
gdouble tree_logprob(Tree *);

Tree * tree_new(Params * params) {
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

Tree * tree_copy(Tree * template) {
	Tree * tree;
	GList * child;
       
	tree = g_new(Tree, 1);
	tree->ref_count = 1;
	tree->is_leaf = template->is_leaf;
	tree->params = template->params;
	params_ref(tree->params);

	tree->suff_stats_on = suff_stats_copy(template->suff_stats_on);
	tree->suff_stats_off = suff_stats_copy(template->suff_stats_off);
	tree->labels = g_list_copy(template->labels);
	tree->children = g_list_copy(template->children);
	for (child = tree->children; child != NULL; child = g_list_next(child)) {
		tree_ref(child->data);
	}

	tree->dirty = template->dirty;
	tree->log_pi = template->log_pi;
	tree->log_not_pi = template->log_not_pi;
	tree->logprob_cluster = template->logprob_cluster;
	tree->logprob_children = template->logprob_children;
	tree->logprob = template->logprob;

	return tree;
}

Tree * leaf_new(Params * params, gpointer label) {
	Tree * leaf;

	leaf = tree_new(params);
	leaf->suff_stats_on = suff_stats_from_label(params, label);
	leaf->suff_stats_off = suff_stats_empty(params);
	leaf->labels = g_list_append(NULL, label);
	leaf->logprob = tree_logprob(leaf);
	return leaf;
}

Tree * branch_new(Params * params) {
	Tree * branch;

	branch = tree_new(params);
	branch->is_leaf = FALSE;
	branch->suff_stats_on = suff_stats_empty(params);
	branch->suff_stats_off = suff_stats_empty(params);
	branch->children = NULL;
	branch->logprob = tree_logprob(branch);
	return branch;
}

gboolean tree_is_leaf(Tree * tree) {
	return tree->is_leaf;
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

gdouble leaf_logprob(Tree * leaf) {
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

	new_off = params_suff_stats_off_lookup(branch->params, branch->labels, child->labels);
	suff_stats_add(branch->suff_stats_off, new_off);
	suff_stats_unref(new_off);
	suff_stats_add(branch->suff_stats_on, child->suff_stats_on);
	for (labels = child->labels; labels != NULL; labels = g_list_next(labels)) {
		branch->labels = g_list_insert_sorted(branch->labels,
				labels->data, cmp_quark);
	}
	branch->dirty = TRUE;
	branch->logprob = tree_logprob(branch);
}

gdouble branch_log_not_pi(Tree * branch) {
	gdouble num_children;

	num_children = g_list_length(branch->children);

	return (num_children-1)*branch->params->loggamma;
}

gdouble branch_log_pi(Tree * branch, gdouble log_not_pi) {
	if (log_not_pi > gsl_sf_log(2.0)) {
		return gsl_sf_log(-gsl_sf_expm1(log_not_pi));
	} else {
		return gsl_sf_log_1plusx(-gsl_sf_exp(log_not_pi));
	}
}

gdouble branch_logprob(Tree * branch) {
	GList * child;

	if (g_list_length(branch->children) < 2) {
		return 0.0;
	}

	branch->log_not_pi = branch_log_not_pi(branch);
	branch->log_pi = branch_log_pi(branch, branch->log_not_pi);
	branch->logprob_cluster = params_logprob_on(branch->params, branch->suff_stats_on);
	branch->logprob_children = params_logprob_off(branch->params, branch->suff_stats_off);
	for (child = branch->children; child != NULL; child = g_list_next(child)) {
		branch->logprob_children += tree_logprob(child->data);
	}
	branch->logprob = log_add_exp(
			branch->log_pi + branch->logprob_cluster,
			branch->log_not_pi + branch->logprob_children);
	branch->dirty = FALSE;
	return branch->logprob;
}

gdouble tree_logprob(Tree *tree) {
	if (!tree->dirty) {
		return tree->logprob;
	}

	if (tree_is_leaf(tree)) {
		return leaf_logprob(tree);
	}
	return branch_logprob(tree);
}

typedef struct {
	guint ii;
	guint jj;
	Tree * tree;
	gdouble score;
} Merge;

gdouble merge_calc_logprob_rel(Params *, Tree *, Tree *);


Merge * merge_new(Params * params, guint ii, Tree * aa, guint jj, Tree * bb, Tree * mm) {
	Merge * merge;
	gdouble logprob_rel;

	merge = g_new(Merge, 1);
	merge->ii = ii;
	merge->jj = jj;
	merge->tree = mm;
	logprob_rel = merge_calc_logprob_rel(params, aa, bb);
	merge->score = merge->tree->logprob - aa->logprob - bb->logprob - logprob_rel;
	return merge;
}

void merge_free(Merge * merge) {
	tree_unref(merge->tree);
	g_free(merge);
}

void merge_free1(gpointer merge, gpointer data) {
	merge_free(merge);
}

Merge * merge_join(Params * params, guint ii, Tree * aa, guint jj, Tree * bb) {
	Tree * tree;

	tree = branch_new(params);
	branch_add_child(tree, aa);
	branch_add_child(tree, bb);
	return merge_new(params, ii, aa, jj, bb, tree);
}

Merge * merge_absorb(Params * params, guint ii, Tree * aa, guint jj, Tree * bb) {
	/* absorb bb as a child of aa */
	Tree * tree;

	if (tree_is_leaf(aa)) {
		return NULL;
	}

	tree = tree_copy(aa);
	branch_add_child(tree, bb);
	return merge_new(params, ii, aa, jj, bb, tree);
}

Merge * merge_best(Params * params, guint ii, Tree * aa, guint jj, Tree * bb) {
	Merge * merge;
	Merge * best_merge;

	best_merge = merge_join(params, ii, aa, jj, bb);
	merge = merge_absorb(params, ii, aa, jj, bb);
	if (merge != NULL) {
		if (merge->score > best_merge->score) {
			merge_free(best_merge);
			best_merge = merge;
		} else {
			merge_free(merge);
		}
	}
	merge = merge_absorb(params, jj, bb, ii, aa);
	if (merge != NULL) {
		if (merge->score > best_merge->score) {
			merge_free(best_merge);
			best_merge = merge;
		} else {
			merge_free(merge);
		}
	}
	return best_merge;
}

gdouble merge_calc_logprob_rel(Params * params, Tree * aa, Tree * bb) {
	gpointer offblock;
	gdouble logprob_rel;

	offblock = params_suff_stats_off_lookup(params, aa->labels, bb->labels);
	logprob_rel = params_logprob_off(params, offblock);
	suff_stats_unref(offblock);
	return logprob_rel;
}


gint cmp_score(gconstpointer paa, gconstpointer pbb, gpointer userdata) {
	const Merge * aa = paa;
	const Merge * bb = pbb;
	return bb->score - aa->score;
}


GPtrArray * build_init_trees(Params * params, GList * labels) {
	GPtrArray * trees;

	trees = g_ptr_array_new();
	for (labels = g_list_first(labels); labels != NULL; labels = g_list_next(labels)) {
		Tree * leaf = leaf_new(params, labels->data);
		g_ptr_array_add(trees, leaf);
	}
	return trees;
}


GSequence * build_init_merges(Params * params, GPtrArray * trees) {
	GSequence * merges;
	Merge * new_merge;
	Tree * aa;
	Tree * bb;
	guint ii;
	guint jj;

	merges = g_sequence_new(NULL);

	for (ii = 0; ii < trees->len; ii++) {
		aa = g_ptr_array_index(trees, ii);
		for (jj = ii + 1; jj < trees->len; jj++) {
			bb = g_ptr_array_index(trees, jj);
			new_merge = merge_best(params, ii, aa, jj, bb);
			g_sequence_insert_sorted(merges, new_merge, cmp_score, NULL);
		}
	}
	return merges;
}

void build_remove_tree(GPtrArray * trees, guint ii) {
	gpointer * tii;

	tii = &g_ptr_array_index(trees, ii);
	tree_unref(*tii);
	*tii = NULL;
}

void build_add_merges(Params * params, GSequence * merges, GPtrArray * trees, Tree * tkk) {
	Tree *tll;
	Merge * new_merge;
	guint ll, kk;

	kk = trees->len;
	for (ll = 0; ll < trees->len; ll++) {
		if (g_ptr_array_index(trees, ll) == NULL) {
			continue;
		}

		tll = g_ptr_array_index(trees, ll);
		new_merge = merge_best(params, kk, tkk, ll, tll);
		g_sequence_insert_sorted(merges, new_merge, cmp_score, NULL);
	}
}

void build_greedy(Params * params, GPtrArray * trees, GSequence * merges) {
	Merge * cur;
	GSequenceIter * head;
	guint live_trees;

	live_trees = trees->len;
	while (live_trees > 1) {
		g_assert(g_sequence_get_length(merges) > 0);
		head = g_sequence_get_begin_iter(merges);
		cur = g_sequence_get(head);
		g_sequence_remove(head);

		if (g_ptr_array_index(trees, cur->ii) == NULL ||
		    g_ptr_array_index(trees, cur->jj) == NULL) {
			goto again;
		}

		build_remove_tree(trees, cur->ii);
		build_remove_tree(trees, cur->jj);
		live_trees--;
		build_add_merges(params, merges, trees, cur->tree);
		g_ptr_array_add(trees, cur->tree);
		tree_ref(cur->tree);
again:
		merge_free(cur);
	}
}

Tree * build(Params * params, GList * labels) {
	GPtrArray * trees;
	GSequence * merges;
	Tree * root;

	trees = build_init_trees(params, labels);
	merges = build_init_merges(params, trees);

	build_greedy(params, trees, merges);

	root = g_ptr_array_index(trees, trees->len - 1);
	g_assert(root != NULL);
	g_ptr_array_free(trees, TRUE);
	g_sequence_foreach(merges, merge_free1, NULL);
	g_sequence_free(merges);
	return root;
}


void run_rand(GRand * rng, guint num_items, gdouble sparsity, guint verbose) {
	Dataset * dataset;
	Params * params;
	GList * labels;
	Tree * root;
	GString * out;

	/* dataset = dataset_gen_speckle(rng, num_items, 1.0-sparsity); */
	dataset = dataset_gen_blocks(rng, num_items, 3, sparsity);
	if (verbose > 1) {
		out = g_string_new("dataset: \n");
		dataset_print(dataset, out);
		g_print("%s\n", out->str);
		g_string_free(out, TRUE);
	}

	params = params_default(dataset);

	labels = dataset_get_labels(dataset);

	dataset_unref(dataset);

	root = build(params, labels);

	if (verbose) {
		out = g_string_new("result: ");
		tree_print(root, out);
		g_print("%s\n", out->str);
		g_string_free(out, TRUE);
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

