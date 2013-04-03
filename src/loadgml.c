#include <string.h>
#include "dataset.h"
#include "dataset_gml.h"
#include "util.h"

void write_names(gpointer arg, GIOChannel *io) {
	Pair * pair = arg;
	GHashTable * label_index = pair->fst;
	Dataset * dataset = pair->snd;
	GList * labels;
	int index = 0;

	labels = dataset_get_labels(dataset);
	for (GList * xx = labels; xx != NULL; xx = g_list_next(xx)) {
		g_hash_table_insert(label_index, xx->data, GINT_TO_POINTER(index));
		io_printf(io, "%d %s\n", index, dataset_label_to_string(dataset, xx->data));
		index++;
	}
	dataset_get_labels_free(labels);
}

void write_graph(gpointer arg, GIOChannel *io) {
	Pair * argpair = arg;
	GHashTable * label_index = argpair->fst;
	Dataset * dataset = argpair->snd;
	GList * pairs;

	pairs = dataset_get_label_pairs(dataset);
	for (GList * xx = pairs; xx != NULL; xx = g_list_next(xx)) {
		Pair * pair = xx->data;
		gpointer src_ptr = g_hash_table_lookup(label_index, pair->fst);
		gpointer dst_ptr = g_hash_table_lookup(label_index, pair->snd);
		guint src = GPOINTER_TO_INT(src_ptr);
		guint dst = GPOINTER_TO_INT(dst_ptr);
		gboolean value = dataset_get(dataset, pair->fst, pair->snd, NULL);
		if (src == dst) {
			g_warning("skipping self-link");
			continue;
		}
		io_printf(io, "0 %u %u %u\n", src, dst, value);
	}
	dataset_get_label_pairs_free(pairs);
}


void usage(const gchar *name) {
	g_error("usage: %s <filename> cmd\n"
		"\twrite_irm <output graph> <output names>\n"
		"\twrite_gml <output gml> <output adj>\n"
		"\tnum_items\n", name);
}

int main(int argc, char *argv[]) {
	Dataset * dataset;

	if (argc < 3) {
		usage(argv[0]);
	}

	dataset = dataset_gml_load(argv[1]);
	if (strcmp(argv[2], "num_items") == 0) {
		g_print("%u\n", dataset_num_labels(dataset));
	} else if (strcmp(argv[2], "write_gml") == 0 && argc == 5) {
		io_writefile(argv[3], (IOFunc) dataset_gml_save_io, dataset);
		io_writefile(argv[4], (IOFunc) dataset_adj_save_io, dataset);
	} else if (strcmp(argv[2], "write_irm") == 0 && argc == 5) {
		Pair * indices_dataset = pair_new(g_hash_table_new(NULL, NULL), dataset);
		io_writefile(argv[4], (IOFunc) write_names, indices_dataset);
		io_writefile(argv[3], (IOFunc) write_graph, indices_dataset);
	} else {
		usage(argv[0]);
	}
	dataset_unref(dataset);
	return 0;
}
