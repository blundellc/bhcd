#include "dataset.h"
#include "dataset_gml.h"

int main(int argc, char *argv[]) {
	Dataset * dataset;
	GString * out;

	if (argc < 2) {
		g_error("usage: %s <filename>", argv[0]);
	}

	dataset = dataset_gml_load(argv[1]);

	out = g_string_new("");
	dataset_print(dataset, out);
	dataset_unref(dataset);
	g_print("%s\n", out->str);
	g_string_free(out, TRUE);
	return 0;
}
