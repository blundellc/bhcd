#include <string.h>
#include "dataset_gml.h"
#include "tokens.h"


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
			node_label = tokens_next(toks);
		}
		g_free(next);
	}
	tokens_expect(toks, "]");
	if (id == NULL || node_label == NULL) {
		tokens_fail(toks, "missing id/label");
	}
	if (node_label[0] == '"') {
		/* separate variable no_quotes because we will want to free
		 * node_label later.
		 */
		gchar *no_quotes;
		guint len;

		no_quotes = &node_label[1];
		len = strlen(no_quotes);
		if (len > 0 && no_quotes[len-1] == '"') {
			no_quotes[len-1] = '\0';
		}
		label = dataset_add_string_label(dd, no_quotes);
	} else {
		label = dataset_add_string_label(dd, node_label);
	}
	g_free(node_label);
	g_hash_table_insert(id_labels, id, label);
}

static void parse_edge(Tokens * toks, Dataset * dd, GHashTable * id_labels) {
	gchar * weight;
	gpointer src;
	gpointer dst;
	gboolean value_weight;
	gchar *next;

	src = NULL;
	dst = NULL;
	weight = NULL;

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
			weight = tokens_next(toks);
		}
		g_free(next);
	}
	tokens_expect(toks, "]");
	if (src == NULL || dst == NULL) {
		tokens_fail(toks, "missing source/target");
	}

	if (weight == NULL) {
		value_weight = TRUE;
	} else if (strcmp(weight, "0") == 0) {
		value_weight = FALSE;
	} else if (strcmp(weight, "1") == 0) {
		value_weight = TRUE;
	} else {
		value_weight = FALSE;
		tokens_fail(toks, "invalid weight");
	}

	dataset_set(dd, src, dst, value_weight);
	g_free(weight);
}

void dataset_gml_save(Dataset * dataset, const gchar *fname) {
}

