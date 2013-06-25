#include <glib.h>
#include <string.h>
#include <math.h>
#include "dataset.h"
#include "dataset_gml.h"

typedef struct {
	guint		num_vertices;
	guint		next_vertex;
	gchar **	vertex_names;
	gdouble *	num_outgoing;
	/* private: */
	GHashTable *	incoming;
} AdjMtx;

typedef struct { gboolean empty; GHashTableIter iter; } AdjMtxIncomingIter;

void adjmtx_add(AdjMtx * adj, guint src, guint dst);
guint adjmtx_add_vertex(AdjMtx * adj, const gchar *name);
void adjmtx_free(AdjMtx * adj);
void adjmtx_incoming_iter_init(AdjMtx * adj, guint dst, AdjMtxIncomingIter * ivv);
gboolean adjmtx_incoming_iter_next(AdjMtxIncomingIter * ivv, guint *vv);
AdjMtx * adjmtx_new(guint num_vertices);
AdjMtx * adjmtx_new_dataset(Dataset * dataset);
void adjmtx_print(AdjMtx * adj);
gdouble * pagerank(AdjMtx * adj, guint max_steps, gdouble damp);


AdjMtx * adjmtx_new(guint num_vertices) {
	AdjMtx * adj;

	adj = g_new(AdjMtx, 1);
	adj->num_vertices = num_vertices;
	adj->next_vertex = 0;
	adj->vertex_names = g_new(gchar *, num_vertices+1);
	adj->num_outgoing = g_new(gdouble, num_vertices);
	for (guint uu = 0; uu < adj->num_vertices; uu++) {
		adj->vertex_names[uu] = NULL;
		adj->num_outgoing[uu] = 0.0;
	}
	adj->vertex_names[num_vertices] = NULL;
	adj->incoming = g_hash_table_new_full(NULL, NULL, NULL, (GDestroyNotify)g_hash_table_unref);
	return adj;
}

AdjMtx * adjmtx_new_dataset(Dataset * dataset) {
	AdjMtx * adj = adjmtx_new(dataset_num_labels(dataset));
	DatasetLabelIter labels;
	DatasetPairIter pairs;
	gpointer label, src, dst;
	GHashTable * label_to_index;

	label_to_index = g_hash_table_new(NULL, NULL);
	dataset_labels_iter_init(dataset, &labels);
	while (dataset_labels_iter_next(&labels, &label)) {
		guint uu = adjmtx_add_vertex(adj, dataset_label_to_string(dataset, label));
		g_hash_table_insert(label_to_index, label, GINT_TO_POINTER(uu));
	}

	dataset_label_pairs_iter_init(dataset, &pairs);
	while (dataset_label_pairs_iter_next(&pairs, &src, &dst)) {
		gpointer pp = g_hash_table_lookup(label_to_index, src);
		gpointer qq = g_hash_table_lookup(label_to_index, dst);
		guint uu = GPOINTER_TO_INT(pp);
		guint vv = GPOINTER_TO_INT(qq);
		if (dataset_get(dataset, src, dst, NULL)) {
			adjmtx_add(adj, uu, vv);
		}
	}
	g_hash_table_unref(label_to_index);
	return adj;
}

guint adjmtx_add_vertex(AdjMtx * adj, const gchar *name) {
	guint uu = adj->next_vertex;
	g_assert(uu < adj->num_vertices);
	adj->vertex_names[uu] = g_strdup(name);
	adj->next_vertex++;
	return uu;
}

void adjmtx_free(AdjMtx * adj) {
	g_hash_table_unref(adj->incoming);
	g_strfreev(adj->vertex_names);
	g_free(adj->num_outgoing);
	g_free(adj);
}

void adjmtx_incoming_iter_init(AdjMtx * adj, guint dst, AdjMtxIncomingIter * iter) {
	GHashTable * incoming = g_hash_table_lookup(adj->incoming, GINT_TO_POINTER(dst));
	iter->empty = incoming == NULL;
	if (!iter->empty) {
		g_hash_table_iter_init(&iter->iter, incoming);
	}
}

gboolean adjmtx_incoming_iter_next(AdjMtxIncomingIter * iter, guint *vv) {
	gpointer pvv;
	gboolean ret = !iter->empty && g_hash_table_iter_next(&iter->iter, &pvv, NULL);
	if (ret) {
		*vv = GPOINTER_TO_INT(pvv);
	}
	return ret;
}

void adjmtx_add(AdjMtx * adj, guint src, guint dst) {
	GHashTable *incoming;
	gpointer pincoming;
	gpointer psrc = GINT_TO_POINTER(src);
	gpointer pdst = GINT_TO_POINTER(dst);

	if (src == dst) {
		return;
	}

	if (!g_hash_table_lookup_extended(adj->incoming, pdst, NULL, &pincoming)) {
		incoming = g_hash_table_new(NULL, NULL);
		g_hash_table_insert(adj->incoming, pdst, incoming);
	} else {
		incoming = pincoming;
	}
	g_hash_table_insert(incoming, psrc, psrc);
	adj->num_outgoing[src]++;
}

gboolean adjmtx_incoming(AdjMtx * adj, guint src, guint dst) {
	gpointer psrc = GINT_TO_POINTER(src);
	gpointer pdst = GINT_TO_POINTER(dst);
	GHashTable *incoming;
	gpointer pincoming;

	if (!g_hash_table_lookup_extended(adj->incoming, pdst, NULL, &pincoming)) {
		return FALSE;
	}
	incoming = pincoming;
	return g_hash_table_lookup_extended(incoming, psrc, NULL, NULL);
}

void adjmtx_print(AdjMtx * adj) {
	size_t max_len = strlen("src/dst");
	gchar * fmt;
	AdjMtxIncomingIter iter;
	for (guint uu = 0; uu < adj->num_vertices; uu++) {
		size_t len = strlen(adj->vertex_names[uu]);
		if (len > max_len) {
			max_len = len;
		}
	}
	fmt = g_strdup_printf("%%%us ", (guint)max_len);
	g_print(fmt, "src/dst");
	for (guint uu = 0; uu < adj->num_vertices; uu++) {
		g_print(fmt, adj->vertex_names[uu]);
	}
	g_print("\n");
	for (guint src = 0; src < adj->num_vertices; src++) {
		g_print(fmt, adj->vertex_names[src]);
		for (guint dst = 0; dst < adj->num_vertices; dst++) {
			if (adjmtx_incoming(adj, src, dst)) {
				g_print(fmt, "1");
			}else {
				g_print(fmt, "0");
			}
		}
		g_print("\n");
	}
	g_print("\n");

	for (guint dst = 0; dst < adj->num_vertices; dst++) {
		guint src = 0;
		g_print(fmt, adj->vertex_names[dst]);
		g_print("%2.2f ", adj->num_outgoing[dst]);
		adjmtx_incoming_iter_init(adj, dst, &iter);
		while (adjmtx_incoming_iter_next(&iter, &src)) {
			g_print(fmt, adj->vertex_names[src]);
		}
		g_print("\n");
	}
	g_print("\n");
	g_free(fmt);
}

gdouble * pagerank(AdjMtx * adj, guint max_steps, gdouble damp) {
	gdouble * pr = g_new(gdouble, adj->num_vertices);
	gdouble * newpr = g_new(gdouble, adj->num_vertices);
	gdouble sum, change;
	AdjMtxIncomingIter ivv;
	guint vv;

	for (guint uu = 0; uu < adj->num_vertices; uu++) {
		pr[uu] = 1.0/((gdouble)adj->num_vertices);
		newpr[uu] = 0.0;
	}
	for (guint step = 0; step < max_steps; step++) {
		g_print("%d: ", step);
		sum = 0;
		for (guint uu = 0; uu < adj->num_vertices; uu++) {
			g_print("%d: %e ", uu, pr[uu]);
			sum += pr[uu];
		}
		g_print(" (%e)\n", sum);
		change = 0.0;
		for (guint uu = 0; uu < adj->num_vertices; uu++) {
			sum = 0.0;
			adjmtx_incoming_iter_init(adj, uu, &ivv);
			while (adjmtx_incoming_iter_next(&ivv, &vv)) {
				sum += pr[vv]/adj->num_outgoing[vv];
			}
			newpr[uu] = (1.0-damp)/((gdouble)adj->num_vertices);
			newpr[uu] += damp*sum;
			g_assert(newpr[uu] < 1.0);
			change += fabs(pr[uu] - newpr[uu]);
		}
		{ gdouble * tmp = pr; pr = newpr; newpr = tmp; }
		if (change < 1e-3) {
			break;
		}
	}
	g_free(newpr);
	return pr;
}


gdouble * rooted_pagerank(guint root, AdjMtx * adj, guint max_steps, gdouble damp) {
	gdouble * pr = g_new(gdouble, adj->num_vertices);
	gdouble * newpr = g_new(gdouble, adj->num_vertices);
	gdouble sum, change;
	AdjMtxIncomingIter ivv;
	guint vv;

	for (guint uu = 0; uu < adj->num_vertices; uu++) {
		pr[uu] = 1.0/((gdouble)adj->num_vertices);
		newpr[uu] = 0.0;
	}
	for (guint step = 0; step < max_steps; step++) {
		/*
		g_print("%d: ", step);
		sum = 0;
		for (guint uu = 0; uu < adj->num_vertices; uu++) {
			g_print("%d: %e ", uu, pr[uu]);
			sum += pr[uu];
		}
		g_print(" (%e)\n", sum);
		*/
		change = 0.0;
		for (guint uu = 0; uu < adj->num_vertices; uu++) {
			sum = 0.0;
			adjmtx_incoming_iter_init(adj, uu, &ivv);
			while (adjmtx_incoming_iter_next(&ivv, &vv)) {
				sum += pr[vv]/adj->num_outgoing[vv];
			}
			newpr[uu] = damp*sum;
			if (uu == root) {
				newpr[uu] += 1.0-damp;
			}
			g_assert(newpr[uu] < 1.0);
			change += fabs(pr[uu] - newpr[uu]);
		}
		{ gdouble * tmp = pr; pr = newpr; newpr = tmp; }
		if (change < 1e-3) {
			break;
		}
	}
	g_free(newpr);
	return pr;
}

int main(int argc, char *argv[]) {
	Dataset * train;
	AdjMtx * adj;
	gdouble * pr;
	gdouble ** roots;
	const gdouble damping = 0.85;
	const guint max_steps = 100;

	train = dataset_gml_load(argv[1]);
	adj = adjmtx_new_dataset(train);
	//adjmtx_print(adj);
	pr = pagerank(adj, max_steps, damping);
	roots = g_new(gdouble *, adj->num_vertices);
	for (guint uu = 0; uu < adj->num_vertices; uu++) {
		roots[uu] = rooted_pagerank(uu, adj, max_steps, damping);
		g_print("%s: %2.2f: ", adj->vertex_names[uu], pr[uu]);
		for (guint vv = 0; vv < adj->num_vertices; vv++) {
			g_print("%s:%2.2f ", adj->vertex_names[vv], roots[uu][vv]);
		}
		g_print("\n");
	}
	for (guint uu = 0; uu < adj->num_vertices; uu++) {
		g_free(roots[uu]);
	}
	g_free(roots);
	adjmtx_free(adj);
	dataset_unref(train);
	g_free(pr);
	return 0;
}

