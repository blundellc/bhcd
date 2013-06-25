#include "islands.h"
#include "util.h"
#include "tree.h"

struct Islands_t {
	gboolean debug;
	GHashTable * neigh;
};

static void islands_add_edge_directed(Islands *islands, guint ii, guint jj);


Islands * islands_new(Dataset * dataset, GPtrArray *trees) {
	Islands * islands;
	gpointer src, dst;
	DatasetPairIter pairs;
	GHashTable * labels_to_trees;
	gpointer pp;
	gpointer qq;
	guint ii;
	guint jj;

	islands = g_new(Islands,1);
	islands->debug = FALSE;
	islands->neigh = g_hash_table_new_full(NULL, NULL, NULL, (GDestroyNotify)g_hash_table_unref);

	labels_to_trees = g_hash_table_new(NULL, NULL);
	for (ii = 0; ii < trees->len; ii++) {
		gconstpointer label = leaf_get_label(g_ptr_array_index(trees, ii));
		g_hash_table_insert(labels_to_trees, GINT_TO_POINTER(GPOINTER_TO_INT(label)), GINT_TO_POINTER(ii));
	}

	dataset_label_pairs_iter_init_full(dataset, DATASET_ITER_TRUE, &pairs);
	while (dataset_label_pairs_iter_next(&pairs, &src, &dst)) {
		pp = g_hash_table_lookup(labels_to_trees, src);
		ii = GPOINTER_TO_INT(pp);
		qq = g_hash_table_lookup(labels_to_trees, dst);
		jj = GPOINTER_TO_INT(qq);
		islands_add_edge(islands, ii, jj);
	}
	dataset_label_pairs_iter_init_full(dataset, DATASET_ITER_MISSING, &pairs);
	while (dataset_label_pairs_iter_next(&pairs, &src, &dst)) {
		pp = g_hash_table_lookup(labels_to_trees, src);
		ii = GPOINTER_TO_INT(pp);
		qq = g_hash_table_lookup(labels_to_trees, dst);
		jj = GPOINTER_TO_INT(qq);
		islands_add_edge(islands, ii, jj);
	}
	g_hash_table_unref(labels_to_trees);

	return islands;
}

void islands_free(Islands *islands) {
	g_hash_table_unref(islands->neigh);
	g_free(islands);
}

void islands_add_edge(Islands *islands, guint ii, guint jj) {
	islands_add_edge_directed(islands, ii, jj);
	islands_add_edge_directed(islands, jj, ii);
}

static void islands_add_edge_directed(Islands *islands, guint ii, guint jj) {
	gpointer hood;
	gpointer pp = GINT_TO_POINTER(ii);
	gpointer qq = GINT_TO_POINTER(jj);

	if (!g_hash_table_lookup_extended(islands->neigh, pp, NULL, &hood)) {
		hood = g_hash_table_new(NULL, NULL);
		g_hash_table_insert(islands->neigh, pp, hood);
	}
	g_hash_table_insert(hood, qq, qq);
	if (islands->debug) {
		g_print("island: %u -> %u\n", ii, jj);
	}
}

void islands_merge(Islands * islands, guint mm, guint ii, guint jj) {
	gpointer pp = GINT_TO_POINTER(ii);
	gpointer qq = GINT_TO_POINTER(jj);
	gpointer rr = GINT_TO_POINTER(mm);
	GHashTable * hood_ii = g_hash_table_lookup(islands->neigh, pp);
	GHashTable * hood_jj = g_hash_table_lookup(islands->neigh, qq);
	GHashTable * hood_rr;
	GList * elems_ii;
	GList * elems_jj;

	/* go through the neighbours of ii and jj, removing ii and jj, and
	 * adding mm.
	 */
	elems_ii = g_hash_table_get_keys(hood_ii);
	for (GList * xx = elems_ii; xx != NULL; xx = g_list_next(xx)) {
		gpointer kk = xx->data;
		GHashTable * hood_kk = g_hash_table_lookup(islands->neigh, kk);
		if (kk == pp || kk == qq) {
			continue;
		}
		g_hash_table_remove(hood_kk, pp);
		g_hash_table_remove(hood_kk, qq);
		g_hash_table_insert(hood_kk, rr, rr);
		if (islands->debug) {
			g_print("merge: %d: added %u\n", GPOINTER_TO_INT(kk), mm);
		}
	}
	elems_jj = g_hash_table_get_keys(hood_jj);
	for (GList * xx = elems_jj; xx != NULL; xx = g_list_next(xx)) {
		gpointer kk = xx->data;
		GHashTable * hood_kk = g_hash_table_lookup(islands->neigh, kk);
		if (kk == pp || kk == qq) {
			continue;
		}
		g_hash_table_remove(hood_kk, pp);
		g_hash_table_remove(hood_kk, qq);
		g_hash_table_insert(hood_kk, rr, rr);
		if (islands->debug) {
			g_print("merge: %d: added %u\n", GPOINTER_TO_INT(kk), mm);
		}
	}
	/* create a new entry for mm, which consists of the concatonation of the
	 * neighbourhoods of pp and qq
	 */
	g_list_free(elems_ii);
	hood_rr = hood_ii;
	g_hash_table_ref(hood_ii);
	for (GList * xx = elems_jj; xx != NULL; xx = g_list_next(xx)) {
		gpointer kk = xx->data;
		if (kk == pp || kk == qq) {
			continue;
		}
		g_hash_table_insert(hood_rr, kk, kk);
	}
	g_list_free(elems_jj);
	g_hash_table_remove(hood_rr, pp);
	g_hash_table_remove(hood_rr, qq);
	g_hash_table_insert(islands->neigh, rr, hood_rr);

	/* finally remove ii and jj completely. */
	g_hash_table_remove(islands->neigh, pp);
	g_hash_table_remove(islands->neigh, qq);
	if (islands->debug) {
		g_print("island: merge %u %u -> %u\n", ii, jj, mm);
	}
}

GList * islands_get_neigh(Islands * islands, guint ii) {
	GHashTable * hood;
	gpointer pp = GINT_TO_POINTER(ii);

	hood = g_hash_table_lookup(islands->neigh, pp);
	if (islands->debug) {
		g_print("get neighbours %u: %u\n", ii, g_hash_table_size(hood));
	}
	if (hood == NULL) {
		return NULL;
	}
	return g_hash_table_get_keys(hood);
}

void islands_get_neigh_free(GList * neighhood) {
	g_list_free(neighhood);
}


GList * islands_get_edges(Islands * islands) {
	GList * edges = NULL;
	GList * elems_ii = g_hash_table_get_keys(islands->neigh);
	for (GList * xx = elems_ii; xx != NULL; xx = g_list_next(xx)) {
		for (GList * yy = g_list_next(xx); yy != NULL; yy = g_list_next(yy)) {
			edges = g_list_prepend(edges, pair_new(xx->data, yy->data));
		}
	}
	g_list_free(elems_ii);
	return edges;
}

void islands_get_edges_free(GList * edges) {
	g_list_free_full(edges, (GDestroyNotify)pair_free);
}
