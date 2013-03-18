#include <string.h>
#include "dataset.h"
#include "dataset_gml.h"

int main(int argc, char *argv[]) {
	Dataset * dataset;

	if (argc < 2) {
		g_error("usage: %s <filename>", argv[0]);
	}

	dataset = dataset_gml_load(argv[1]);
	dataset_println(dataset, "");

	if (argc > 2 && strcmp(argv[2], "gml") == 0) {
		GIOChannel *io;

		io = g_io_channel_unix_new(1);
		dataset_gml_save_io(dataset, io);
		g_io_channel_unref(io);
	}
	dataset_unref(dataset);
	return 0;
}
