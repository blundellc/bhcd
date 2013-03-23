#include <string.h>
#include "dataset.h"
#include "dataset_gml.h"
#include "util.h"

int main(int argc, char *argv[]) {
	Dataset * dataset;

	if (argc < 4) {
		g_error("usage: %s <filename> <output gml> <output adj>", argv[0]);
	}

	dataset = dataset_gml_load(argv[1]);
	io_writefile(argv[2], (IOFunc) dataset_gml_save_io, dataset);
	io_writefile(argv[3], (IOFunc) dataset_adj_save_io, dataset);
	dataset_unref(dataset);
	return 0;
}
