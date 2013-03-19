#include "sscache.h"
#include "counts.h"


struct SSCache_t {
	guint		ref_count;
	Dataset *	dataset;
	GHashTable *	suffstats_labels;
};


static void suffstats_add_triangles(Counts *, Dataset * dataset, GList * srcs, GList * dsts);

SSCache * sscache_new(Dataset *dataset) {
	SSCache * cache;

	cache = g_new(SSCache, 1);
	cache->ref_count = 1;
	cache->dataset = dataset;
	dataset_ref(cache->dataset);
	cache->suffstats_labels = g_hash_table_new_full(NULL, NULL, NULL, suffstats_unref);
	return cache;
}

void sscache_unref(SSCache *cache) {
	if (cache->ref_count <= 1) {
		g_hash_table_unref(cache->suffstats_labels);
		dataset_unref(cache->dataset);
		g_free(cache);
	} else {
		cache->ref_count--;
	}
}

gpointer sscache_get_label(SSCache *cache, gpointer label) {
	gpointer suffstats;

	if (!g_hash_table_lookup_extended(cache->suffstats_labels,
				label, NULL, &suffstats)) {
		gboolean missing;
		gboolean value;

		value = dataset_get(cache->dataset, label, label, &missing);
		if (missing) {
			suffstats = suffstats_new_empty();
		} else {
			suffstats = counts_new(value, 1);
		}
		g_hash_table_insert(cache->suffstats_labels, label, suffstats);
	}
	return suffstats;

}


gpointer sscache_get_offblock(SSCache *cache, gpointer root, GList * children) {
	return NULL;
}

gpointer suffstats_new_empty(void) {
	return counts_new(0, 0);
}

gpointer suffstats_copy(gpointer src) {
	if (src == NULL) {
		return NULL;
	}
	return counts_copy(src);
}

void suffstats_ref(gpointer ss) {
	counts_ref(ss);
}

void suffstats_unref(gpointer ss) {
	counts_unref(ss);
}

void suffstats_add(gpointer pdst, gpointer psrc) {
	Counts * dst = pdst;
	Counts * src = psrc;
	counts_add(dst, src);
}


static void suffstats_add_triangles(Counts * counts, Dataset * dataset, GList * srcs, GList * dsts) {
	GList * src;
	GList * dst;
	gboolean missing;
	gboolean value;

	for (src = srcs; src != NULL; src = g_list_next(src)) {
		for (dst = g_list_first(dsts); dst != NULL; dst = g_list_next(dst)) {
			value = dataset_get(dataset, src->data, dst->data, &missing);
			/*
			g_print("off: %s -> %s = %d, %d\n",
					dataset_get_label_string(params->dataset, src->data),
					dataset_get_label_string(params->dataset, dst->data),
					value,
					missing);
					*/
			if (!missing) {
				counts->num_ones += value;
				counts->num_total++;
			}
		}
	}
}

gpointer suffstats_new_offblock(Dataset * dataset, GList * srcs, GList * dsts) {
	Counts * counts;

	counts = counts_new(0, 0);
	suffstats_add_triangles(counts, dataset, srcs, dsts);
	if (!dataset_is_symmetric(dataset)) {
		/* see how the other half live. */
		suffstats_add_triangles(counts, dataset, dsts, srcs);
	}
	return counts;
}



