#ifndef	DATASET_GML_H
#define	DATASET_GML_H
#include <glib.h>
#include "dataset.h"

Dataset * dataset_gml_load(const gchar *fname);
void dataset_gml_save(Dataset * dataset, const gchar *fname);
void dataset_gml_save_io(Dataset * dataset, GIOChannel * io);
void dataset_adj_save_io(Dataset * dataset, GIOChannel * io);

#endif /*DATASET_GML_H*/
