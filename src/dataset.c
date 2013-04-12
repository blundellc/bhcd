#include <string.h>
#include "dataset.h"
#include "util.h"

#define	DATASET_VALUE_SHIFT	0x10
struct Dataset_t {
	guint		ref_count;
	gchar *		filename;
	gboolean	symmetric;
	gint		omitted;
	GQuark		max_qlabel;
	GHashTable *	labels;
	GHashTable *	cells;
};


typedef struct {
	GQuark	src;
	GQuark	dst;
} Dataset_Key;


static Dataset_Key * dataset_key(Dataset *, gconstpointer, gconstpointer);
static guint dataset_key_hash(gconstpointer);
static gboolean dataset_key_eq(gconstpointer, gconstpointer);
static gint dataset_label_cmp(gconstpointer, gconstpointer);
static void dataset_set_full(Dataset *, gpointer, gpointer, gint);

Dataset* dataset_new(gboolean symmetric) {
	Dataset * data = g_new(Dataset, 1);
	data->ref_count = 1;
	data->filename = NULL;
	data->symmetric = symmetric;
	data->omitted = -1;
	data->cells = g_hash_table_new_full(
				dataset_key_hash,
				dataset_key_eq,
				g_free,
				NULL
			);
	data->max_qlabel = 0;
	data->labels = g_hash_table_new_full(
				NULL,
				NULL,
				NULL,
				NULL
			);
	return data;
}

void dataset_ref(Dataset* dataset) {
	dataset->ref_count++;
}

void dataset_unref(Dataset* dataset) {
	if (dataset->ref_count <= 1) {
		g_hash_table_unref(dataset->cells);
		g_hash_table_unref(dataset->labels);
		g_free(dataset->filename);
		g_free(dataset);
	} else {
		dataset->ref_count--;
	}
}

gboolean dataset_is_symmetric(Dataset * dataset) {
	return dataset->symmetric;
}

void dataset_set_filename(Dataset *dataset, const gchar *filename) {
	dataset->filename = g_strdup(filename);
}


const gchar * dataset_get_filename(Dataset * dataset) {
	return dataset->filename;
}

void dataset_set_omitted(Dataset * dataset, gboolean omitted) {
	g_assert(omitted == FALSE || omitted == TRUE);
	dataset->omitted = omitted;
}

gboolean dataset_get_sparse(Dataset * dataset, gboolean * omitted) {
	if (omitted != NULL) {
		*omitted = dataset->omitted;
	}
	if (dataset->omitted < 0) {
		return FALSE;
	}
	return TRUE;
}

gboolean dataset_get(Dataset * dataset, gconstpointer src, gconstpointer dst, gboolean *missing) {
	gint value;
	Dataset_Key * key;
	gpointer ptr;

	key = dataset_key(dataset, src, dst);
	ptr = g_hash_table_lookup(dataset->cells, key);
	g_free(key);
	if (ptr == NULL) {
		value = dataset->omitted;
	} else {
		value = GPOINTER_TO_INT(ptr) - DATASET_VALUE_SHIFT;
		g_assert(value != dataset->omitted);
	}

	if (value < 0) {
		g_assert(missing != NULL);
		*missing = TRUE;
		return FALSE;
	}
	if (missing != NULL) {
		*missing = FALSE;
	}
	return value;
}

gboolean dataset_is_missing(Dataset * dataset, gpointer src, gpointer dst) {
	gboolean missing;
	dataset_get(dataset, src, dst, &missing);
	return missing;
}

void dataset_set(Dataset * dataset, gpointer src, gpointer dst, gboolean value) {
	g_assert(value == FALSE || value == TRUE);
	dataset_set_full(dataset, src, dst, value);
	g_assert(!dataset_is_missing(dataset, src, dst));
}

void dataset_set_missing(Dataset * dataset, gpointer src, gpointer dst) {
	dataset_set_full(dataset, src, dst, -1);
	g_assert(dataset_is_missing(dataset, src, dst));
}

static void dataset_set_full(Dataset * dataset, gpointer src, gpointer dst, gint value) {
	Dataset_Key * key;

	key = dataset_key(dataset, src, dst);
	if (value == dataset->omitted) {
		g_hash_table_remove(dataset->cells, key);
		g_free(key);
		return;
	}
	g_hash_table_replace(dataset->cells, key, GINT_TO_POINTER(value+DATASET_VALUE_SHIFT));
}

guint dataset_num_labels(Dataset * dataset) {
	return g_hash_table_size(dataset->labels);
}


void dataset_labels_iter_init(Dataset *dataset, DatasetLabelIter *iter) {
	g_hash_table_iter_init(&iter->iter, dataset->labels);
}

gboolean dataset_labels_iter_next(DatasetLabelIter *iter, gpointer *plabel) {
	return g_hash_table_iter_next(&iter->iter, plabel, NULL);

}


gpointer dataset_get_max_label(Dataset * dataset) {
	return GINT_TO_POINTER(dataset->max_qlabel);
}


void dataset_label_pairs_iter_init(Dataset * dataset, DatasetPairIter * iter) {
	iter->dataset = dataset;
	iter->dense = dataset->omitted < 0;
	if (iter->dense) {
		/* dense data */
		g_hash_table_iter_init(&iter->u.cell_iter, dataset->cells);
	} else {
		/* sparse data--brute force */
		iter->u.sp.src = NULL;
		dataset_labels_iter_init(iter->dataset, &iter->u.sp.src_iter);
		dataset_labels_iter_init(iter->dataset, &iter->u.sp.dst_iter);
	}
}

gboolean dataset_label_pairs_iter_next(DatasetPairIter *iter, gpointer * psrc, gpointer * pdst) {
	if (iter->dense) {
		/* dst_iter is unused: dense */
		Dataset_Key * key;
		gpointer pkey;
		gboolean ret = g_hash_table_iter_next(&iter->u.cell_iter, &pkey, NULL);
		key = pkey;
		if (ret) {
			*psrc = GINT_TO_POINTER(key->src);
			*pdst = GINT_TO_POINTER(key->dst);
		}
		return ret;
	} else {
		/* sparse: brute force */
		gboolean dst_next;
		if (iter->u.sp.src == NULL) {
			if (!dataset_labels_iter_next(&iter->u.sp.src_iter, &iter->u.sp.src)) {
				return FALSE;
			}
			dataset_labels_iter_init(iter->dataset, &iter->u.sp.dst_iter);
		}
		while (1) {
			dst_next = dataset_labels_iter_next(&iter->u.sp.dst_iter, pdst);
			while (dst_next && dataset_is_missing(iter->dataset, iter->u.sp.src, *pdst)) {
				dst_next = dataset_labels_iter_next(&iter->u.sp.dst_iter, pdst);
			}
			if (dst_next) {
				break;
			}
			if (!dataset_labels_iter_next(&iter->u.sp.src_iter, &iter->u.sp.src)) {
				return FALSE;
			}
			dataset_labels_iter_init(iter->dataset, &iter->u.sp.dst_iter);
		}
		*psrc = iter->u.sp.src;
		return dst_next;
	}
}


void dataset_label_assert(Dataset *dataset, gconstpointer label) {
	g_assert(g_hash_table_lookup_extended(dataset->labels, label, NULL, NULL));
}

gpointer dataset_label_create(Dataset * dataset, const gchar * slabel) {
	gpointer label;
	GQuark qlabel;

	qlabel = g_quark_from_string(slabel);
	label = GINT_TO_POINTER(qlabel);
	if (!g_hash_table_lookup_extended(dataset->labels, label, NULL, NULL)) {
		/* key = value apparently enables some optimization in hash
		 * table for sets.
		 */
		if (qlabel > dataset->max_qlabel) {
			dataset->max_qlabel = qlabel;
		}
		g_hash_table_insert(dataset->labels, label, label);
	}
	return label;
}

gpointer dataset_label_lookup(Dataset * dataset, const gchar * slabel) {
	gpointer label;
	GQuark qlabel;

	qlabel = g_quark_from_string(slabel);
	label = GINT_TO_POINTER(qlabel);
	g_return_val_if_fail(g_hash_table_lookup_extended(dataset->labels, label, NULL, NULL),
			     NULL);
	return label;
}

const gchar * dataset_label_to_string(Dataset * dataset, gconstpointer label) {
	dataset_label_assert(dataset, label);
	return g_quark_to_string(GPOINTER_TO_INT(label));
}


void dataset_println(Dataset * dataset, const gchar *prefix) {
	GString * out;

	out = g_string_new("");
	g_string_append_printf(out, "dataset(%s): \n", prefix);
	dataset_tostring(dataset, out);
	g_print("%s\n", out->str);
	g_string_free(out, TRUE);
}

void dataset_tostring(Dataset * dataset, GString *str) {
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
	labels = g_list_sort(labels, dataset_label_cmp);
	max_len = 1;
	for (xx = labels; xx != NULL; xx = g_list_next(xx)) {
		qxx = GPOINTER_TO_INT(xx->data);
		len = (guint)strlen(g_quark_to_string(qxx));
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


static Dataset_Key * dataset_key(Dataset * dd, gconstpointer psrc, gconstpointer pdst) {
	Dataset_Key * key;
	GQuark src;
	GQuark dst;

	dataset_label_assert(dd, psrc);
	dataset_label_assert(dd, pdst);

	key = g_new(Dataset_Key, 1);

	src = GPOINTER_TO_INT(psrc);
	dst = GPOINTER_TO_INT(pdst);

	if (!dd->symmetric || src < dst) {
		key->src = src;
		key->dst = dst;
	} else {
		key->src = dst;
		key->dst = src;
	}
	return key;
}

static guint dataset_key_hash(gconstpointer pkey) {
	const Dataset_Key *key = pkey;
	return 41*g_direct_hash(GINT_TO_POINTER(key->src))
		+ g_direct_hash(GINT_TO_POINTER(key->dst));
}

static gboolean dataset_key_eq(gconstpointer paa, gconstpointer pbb) {
	const Dataset_Key * aa = paa;
	const Dataset_Key * bb = pbb;
	return aa->src == bb->src && aa->dst == bb->dst;
}

static gint dataset_label_cmp(gconstpointer paa, gconstpointer pbb) {
	/* compare the strings of the labels pointed to lexically */
	const gchar *aa;
	const gchar *bb;

	aa = g_quark_to_string(GPOINTER_TO_INT(paa));
	bb = g_quark_to_string(GPOINTER_TO_INT(pbb));
	return strcmp(aa, bb);
}
