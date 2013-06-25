#include <string.h>
#include "dataset.h"
#include "dataset_gml.h"
#include "util.h"

int main(int argc, char *argv[]) {
	Dataset * dataset;

	dataset = dataset_gen_toy3();
	io_writefile("output/toy3.gml", (IOFunc) dataset_gml_save_io, dataset);
	io_writefile("output/toy3.adj", (IOFunc) dataset_adj_save_io, dataset);
	dataset_unref(dataset);

	dataset = dataset_gen_toy4(FALSE);
	io_writefile("output/toy4dense.gml", (IOFunc) dataset_gml_save_io, dataset);
	io_writefile("output/toy4dense.adj", (IOFunc) dataset_adj_save_io, dataset);
	dataset_unref(dataset);

	dataset = dataset_gen_toy4(TRUE);
	io_writefile("output/toy4sparse.gml", (IOFunc) dataset_gml_save_io, dataset);
	io_writefile("output/toy4sparse.adj", (IOFunc) dataset_adj_save_io, dataset);
	dataset_unref(dataset);
	return 0;
}
