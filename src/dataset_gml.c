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
	/* not symmetric(!) */
	dd = dataset_new(FALSE);
	dataset_set_filename(dd, fname);
	toks = tokens_open(fname);
	while (tokens_has_next(toks)) {
		next = tokens_next(toks);
		if (strcmp(next, "graph") == 0) {
			tokens_expect(toks, "[");
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
	label = dataset_label_lookup(dd, strip_quotes(node_label));
	g_free(node_label);
	g_hash_table_insert(id_labels, id, label);
}

static void parse_edge(Tokens * toks, Dataset * dd, GHashTable * id_labels) {
	gpointer src;
	gpointer dst;
	gint weight;
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
			weight = tokens_next_int(toks);
		} else {
			tokens_fail(toks, "unexpected token `%s'", next);
		}
		g_free(next);
	}
	tokens_expect(toks, "]");
	if (src == NULL || dst == NULL) {
		tokens_fail(toks, "missing source/target");
	}

	dataset_set(dd, src, dst, weight > 0);
}



void dataset_gml_save(Dataset * dataset, const gchar *fname) {
	GIOChannel *io;
	GError *error;
	error = NULL;
	io = g_io_channel_new_file(fname, "w", &error);
	if (error != NULL) {
		g_error("open `%s': %s", fname, error->message);
	}
	dataset_gml_save_io(dataset, io);
	g_io_channel_shutdown(io, TRUE, &error);
	if (error != NULL) {
		g_error("shutdown `%s': %s", fname, error->message);
	}
	g_io_channel_unref(io);
}

void dataset_gml_save_io(Dataset * dataset, GIOChannel * io) {
	GList * labels;
	GList * src;
	GList * dst;

	io_printf(io, "graph [\n");

	labels = dataset_get_labels(dataset);
	for (src = labels; src != NULL; src = g_list_next(src))  {
		io_printf(io, "\tnode [ id %d label \"%s\" ]\n",
				GPOINTER_TO_INT(src->data),
				dataset_label_to_string(dataset, src->data));
	}
	for (src = labels; src != NULL; src = g_list_next(src)) {
		for (dst = labels; dst != NULL; dst = g_list_next(dst)) {
			gboolean missing;
			gboolean value;

			value = dataset_get(dataset, src->data, dst->data, &missing);
			if (!missing) {
				io_printf(io, "\tedge [ source %d target %d weight %d ]\n",
						GPOINTER_TO_INT(src->data),
						GPOINTER_TO_INT(dst->data),
						value);
			}
		}
	}
	io_printf(io, "]\n");
}

