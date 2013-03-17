#ifndef DATASET_H
#define DATASET_H

#include <glib.h>

struct Dataset_t;
typedef struct Dataset_t Dataset;

Dataset * dataset_new(void);
void dataset_ref(Dataset *);
void dataset_unref(Dataset *);

void dataset_set(Dataset *, gpointer, gpointer, gboolean);
gboolean dataset_get(Dataset *, gpointer, gpointer, gboolean *);

gboolean dataset_is_missing(Dataset *, gpointer, gpointer);
GList * dataset_get_labels(Dataset *);
const gchar * dataset_get_label_string(Dataset *, gpointer);

guint dataset_num_labels(Dataset *);
void dataset_print(Dataset *, GString *);

Dataset * dataset_gen_speckle(GRand *, guint, gdouble);
Dataset * dataset_gen_blocks(GRand *, guint, guint, gdouble);
Dataset * dataset_gen_toy3(void);


#endif
