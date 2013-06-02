#ifndef DATASET_H
#define DATASET_H

#include <glib.h>

struct Dataset_t;
typedef struct Dataset_t Dataset;
typedef struct DatasetLabelIter_t {
	GHashTableIter iter;
	gint value;
} DatasetLabelIter;

#define	DATASET_ITER_ANY	(-2)
#define	DATASET_ITER_MISSING	(-1)
#define	DATASET_ITER_FALSE	FALSE
#define	DATASET_ITER_TRUE	TRUE
typedef struct DatasetPairIter_t {
	Dataset * dataset;
	gboolean brute;
	gint value;
	union {
		GHashTableIter cell_iter;
		struct {
			DatasetLabelIter src_iter;
			DatasetLabelIter dst_iter;
			gpointer src;
		} br;
	} u;
} DatasetPairIter;

Dataset * dataset_new(void);
void dataset_ref(Dataset *);
void dataset_unref(Dataset *);
const gchar * dataset_get_filename(Dataset *);
void dataset_set_filename(Dataset *, const gchar *);
void dataset_set_omitted(Dataset *, gboolean omitted);
gboolean dataset_get_sparse(Dataset *, gboolean *omitted);

void dataset_set(Dataset *, gpointer, gpointer, gboolean);
void dataset_set_missing(Dataset *, gpointer, gpointer);
gboolean dataset_is_missing(Dataset *, gpointer, gpointer);
gboolean dataset_get(Dataset *, gconstpointer, gconstpointer, gboolean *);

/* labels on rows/columns */
void dataset_label_assert(Dataset *, gconstpointer);
guint dataset_num_labels(Dataset *);
void dataset_labels_iter_init(Dataset *, DatasetLabelIter *);
gboolean dataset_labels_iter_next(DatasetLabelIter *, gpointer *);
gpointer dataset_label_create(Dataset *, const gchar *);
gpointer dataset_label_lookup(Dataset *, const gchar *);
gpointer dataset_get_max_label(Dataset *);
const gchar * dataset_label_to_string(Dataset *, gconstpointer);

void dataset_label_pairs_iter_init(Dataset *, DatasetPairIter *);
void dataset_label_pairs_iter_init_full(Dataset *, gint, DatasetPairIter *);
gboolean dataset_label_pairs_iter_next(DatasetPairIter *, gpointer *, gpointer *);

void dataset_println(Dataset *, const gchar *);
void dataset_tostring(Dataset *, GString *);

Dataset * dataset_gen_speckle(GRand *, guint, gdouble);
Dataset * dataset_gen_blocks(GRand *, guint, guint, gdouble);
Dataset * dataset_gen_toy3(void);
Dataset * dataset_gen_toy4(gboolean sparse);


#endif
