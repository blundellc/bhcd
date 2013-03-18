#include "dataset.h"
#include "dataset_gml.h"

int main(int argc, char *argv[]) {
	Dataset * dataset;

	if (argc < 2) {
		g_error("usage: %s <filename>", argv[0]);
	}

	dataset = dataset_gml_load(argv[1]);
	dataset_println(dataset, "");
	dataset_unref(dataset);
	return 0;
}
