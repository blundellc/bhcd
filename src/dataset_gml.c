#include <string.h>
#include "dataset_gml.h"
#include "tokens.h"
#include "util.h"


static void parse_node(Tokens * toks, Dataset * dd, GHashTable * id_labels);
static void parse_edge(Tokens * toks, Dataset * dd, GHashTable * id_labels);

Dataset * dataset_gml_load(const gchar *fname) {
	Dataset * dd;
	Tokens * toks;
	GHashTable * id_labels;
	gchar *next;

	id_labels = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	dd = dataset_new(TRUE);
	dataset_set_filename(dd, fname);
	toks = tokens_open(fname);
	while (tokens_has_next(toks)) {
		next = tokens_next(toks);
		if (strcmp(next, "graph") == 0) {
			tokens_expect(toks, "[");
		} else if (strcmp(next, "sparse") == 0) {
			dataset_set_omitted(dd, tokens_next_int(toks) > 0);
		} else if (strcmp(next, "node") == 0) {
			parse_node(toks, dd, id_labels);
		} else if (strcmp(next, "edge") == 0) {
			parse_edge(toks, dd, id_labels);
		} else if (strcmp(next, "]") == 0) {
			tokens_expect_end(toks);
		} else {
			tokens_fail(toks, "unexpected token `%s'", next);
		}
		g_free(next);
	}
	tokens_close(toks);
	g_hash_table_unref(id_labels);
	return dd;
}

static void parse_node(Tokens * toks, Dataset * dd, GHashTable * id_labels) {
	gchar *id;
	gchar *node_label;
	gpointer label;
	gchar *next;

	id = NULL;
	node_label = NULL;
	tokens_expect(toks, "[");
	while (!tokens_peek_test(toks, "]")) {
		next = tokens_next(toks);
		if (strcmp(next, "id") == 0) {
			id = tokens_next(toks);
		} else if (strcmp(next, "label") == 0) {
			node_label = tokens_next_quoted(toks);
		} else {
			tokens_fail(toks, "unexpected token `%s'", next);
		}
		g_free(next);
	}
	tokens_expect(toks, "]");
	if (id == NULL || node_label == NULL) {
		tokens_fail(toks, "missing id/label");
	}
	label = dataset_label_create(dd, strip_quotes(node_label));
	g_free(node_label);
	g_hash_table_insert(id_labels, id, label);
}

static void parse_edge(Tokens * toks, Dataset * dd, GHashTable * id_labels) {
	gpointer src;
	gpointer dst;
	gint64 weight;
	gchar *next;

	src = NULL;
	dst = NULL;
	weight = TRUE;

	tokens_expect(toks, "[");
	while (!tokens_peek_test(toks, "]")) {
		next = tokens_next(toks);
		if (strcmp(next, "source") == 0) {
			gchar *src_id;

			src_id = tokens_next(toks);
			src = g_hash_table_lookup(id_labels, src_id);
			if (src == NULL) {
				g_print("unknown source %s\n", src_id);
			}
			g_free(src_id);
		} else if (strcmp(next, "target") == 0) {
			gchar *dst_id;

			dst_id = tokens_next(toks);
			dst = g_hash_table_lookup(id_labels, dst_id);
			if (dst == NULL) {
				g_print("unknown target %s\n", dst_id);
			}
			g_free(dst_id);
		} else if (strcmp(next, "weight") == 0) {
			if (tokens_peek_test(toks, "NA")) {
				g_free(tokens_next(toks));
				weight = -1;
			} else {
				weight = tokens_next_int(toks);
				weight = weight > 0;
				/*
				if (weight <= 0) {
					tokens_fail(toks, "unexpected negative weight `%d'", weight);
				}
				if (weight >= 1) {
					tokens_fail(toks, "unexpected weight `%d'", weight);
				}
				*/
			}
		} else {
			tokens_fail(toks, "unexpected token `%s'", next);
		}
		g_free(next);
	}
	tokens_expect(toks, "]");
	if (src == NULL || dst == NULL) {
		tokens_fail(toks, "missing source/target");
	}

	if (weight < 0) {
		dataset_set_missing(dd, src, dst);
	} else {
		dataset_set(dd, src, dst, weight > 0);
	}
}



void dataset_gml_save(Dataset * dataset, const gchar *fname) {
	io_writefile(fname, (IOFunc)dataset_gml_save_io, dataset);
}

void dataset_gml_save_io(Dataset * dataset, GIOChannel * io) {
	gboolean sparse;
	gboolean omitted;
	DatasetLabelIter iter_src, iter_dst;
	gpointer src, dst;

	io_printf(io, "graph [\n");

	sparse = dataset_get_sparse(dataset, &omitted);
	if (sparse) {
		io_printf(io, "\tsparse %d\n", omitted);
	}

	dataset_labels_iter_init(dataset, &iter_src);
	while (dataset_labels_iter_next(&iter_src, &src)) {
		io_printf(io, "\tnode [ id %d label \"%s\" ]\n",
				GPOINTER_TO_INT(src),
				dataset_label_to_string(dataset, src));
	}

	dataset_labels_iter_init(dataset, &iter_src);
	while (dataset_labels_iter_next(&iter_src, &src)) {
		dataset_labels_iter_init(dataset, &iter_dst);
		while (dataset_labels_iter_next(&iter_dst, &dst)) {
			gboolean missing;
			gboolean value;

			value = dataset_get(dataset, src, dst, &missing);
			if ((!sparse && !missing) ||
			    (sparse && value != omitted)) {
				io_printf(io, "\tedge [ source %d target %d weight %d ]\n",
						GPOINTER_TO_INT(src),
						GPOINTER_TO_INT(dst),
						value);
			} else if (sparse && missing) {
				io_printf(io, "\tedge [ source %d target %d weight NA ]\n",
						GPOINTER_TO_INT(src),
						GPOINTER_TO_INT(dst));
			}
		}
	}
	io_printf(io, "]\n");
}

void dataset_adj_save_io(Dataset * dataset, GIOChannel * io) {
	GString * out;
	GError * error;

	out = g_string_new("");
	dataset_tostring(dataset, out);
	error = NULL;
	g_io_channel_write_chars(io, out->str, out->len, NULL, &error);
	if (error != NULL) {
		g_error("g_io_channel_write_chars: %s", error->message);
	}
	g_string_free(out, TRUE);
}

