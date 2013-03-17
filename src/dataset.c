#include <string.h>
#include "dataset.h"
#include "util.h"

#define	DATASET_VALUE_SHIFT	0x10
struct Dataset_t {
	guint		ref_count;
	GHashTable *	labels;
	GHashTable *	cells;
};

typedef struct {
	GQuark	src;
	GQuark	dst;
} Dataset_Key;


static Dataset_Key * dataset_key(gpointer *, gpointer *, gboolean);
static guint dataset_key_hash(gconstpointer);
static gboolean dataset_key_eq(gconstpointer, gconstpointer);
static void dataset_add_label(Dataset *, gpointer);
static gpointer dataset_add_string_label(Dataset *, gchar *);

static Dataset_Key * dataset_key(gpointer *psrc, gpointer *pdst, gboolean fast) {
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


static void dataset_add_label(Dataset * dataset, gpointer label) {
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


const gchar * dataset_get_label_string(Dataset * dataset, gpointer label) {
	UnionPtr str;

	str.const_ptr = g_quark_to_string(GPOINTER_TO_INT(label));
	return str.ptr;
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

Dataset * dataset_gen_toy3(void) {
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
