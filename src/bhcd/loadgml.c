#include <string.h>
#include "dataset.h"
#include "dataset_gml.h"
#include "util.h"

void read_names(gpointer arg, GIOChannel *io) {
	Pair * pair = arg;
	GHashTable * label_index = pair->fst;
	Dataset * dataset = pair->snd;
	GError * error = NULL;
	gchar * line;
	gpointer label;
	gint index;

	while (g_io_channel_read_line(io, &line, NULL, NULL, &error) == G_IO_STATUS_NORMAL) {
		gchar ** fields = g_strsplit(g_strstrip(line), " ", 2);
		g_assert(g_strv_length(fields) == 2);
		index = (gint)g_ascii_strtoull(fields[0], NULL, 0);
		label = dataset_label_lookup(dataset, fields[1]);
		g_hash_table_insert(label_index, label, GINT_TO_POINTER(index));
		g_strfreev(fields);
		g_free(line);
	}
	g_assert(g_hash_table_size(label_index) == dataset_num_labels(dataset));
}

void write_names(gpointer arg, GIOChannel *io) {
	Pair * pair = arg;
	GHashTable * label_index = pair->fst;
	Dataset * dataset = pair->snd;
	DatasetLabelIter iter;
	gpointer label;
	int index = 0;

	dataset_labels_iter_init(dataset, &iter);
	while (dataset_labels_iter_next(&iter, &label)) {
		g_hash_table_insert(label_index, label, GINT_TO_POINTER(index));
		io_printf(io, "%d %s\n", index, dataset_label_to_string(dataset, label));
		index++;
	}
}

void write_graph(gpointer arg, GIOChannel *io) {
	Pair * argpair = arg;
	GHashTable * label_index = argpair->fst;
	Dataset * dataset = argpair->snd;
	DatasetPairIter pairs;
	gpointer lsrc, ldst;

	dataset_label_pairs_iter_init(dataset, &pairs);
	while (dataset_label_pairs_iter_next(&pairs, &lsrc, &ldst)) {
		gpointer src_ptr = g_hash_table_lookup(label_index, lsrc);
		gpointer dst_ptr = g_hash_table_lookup(label_index, ldst);
		guint src = GPOINTER_TO_INT(src_ptr);
		guint dst = GPOINTER_TO_INT(dst_ptr);
		gboolean value = dataset_get(dataset, lsrc, ldst, NULL);
		if (src == dst) {
			g_warning("skipping self-link");
			continue;
		}
		io_printf(io, "0 %u %u %u\n", src, dst, value);
	}
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
		if (g_file_test(argv[4], G_FILE_TEST_EXISTS)) {
			g_print("names file exists already; using it.\n");
			io_readfile(argv[4], (IOFunc) read_names, indices_dataset);
		} else {
			io_writefile(argv[4], (IOFunc) write_names, indices_dataset);
		}
		io_writefile(argv[3], (IOFunc) write_graph, indices_dataset);
	} else {
		usage(argv[0]);
	}
	dataset_unref(dataset);
	return 0;
}
