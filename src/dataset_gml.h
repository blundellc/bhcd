#ifndef	DATASET_GML_H
#define	DATASET_GML_H
#include "dataset.h"

Dataset * dataset_gml_load(const gchar *fname);
void dataset_gml_save(Dataset * dataset, const gchar *fname);

#endif /*DATASET_GML_H*/
