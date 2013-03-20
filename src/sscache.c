#include "sscache.h"
#include "util.h"
#include "counts.h"


struct SSCache_t {
	guint		ref_count;
	Dataset *	dataset;
	GHashTable *	suffstats_labels;
	GHashTable *	suffstats_offblocks;
};


typedef struct {
	guint hash;
	gboolean copy;
	GList *fst;
	GList *snd;
} Offblock_Key;

static Offblock_Key * offblock_key_new(GList * fst, GList * snd, gboolean copy);
static void offblock_key_free(gpointer pkey);
static gboolean offblock_key_equal(gconstpointer paa, gconstpointer pbb);
static guint offblock_key_hash(gconstpointer pkey);

static void suffstats_add_triangles(Counts *, Dataset * dataset, GList * srcs, GList * dsts);

SSCache * sscache_new(Dataset *dataset) {
	SSCache * cache;

	cache = g_new(SSCache, 1);
	cache->ref_count = 1;
	cache->dataset = dataset;
	dataset_ref(cache->dataset);
	cache->suffstats_labels = g_hash_table_new_full(NULL, NULL, NULL, suffstats_unref);
	cache->suffstats_offblocks = g_hash_table_new_full(
			offblock_key_hash, offblock_key_equal,
			offblock_key_free, suffstats_unref);
	return cache;
}

void sscache_unref(SSCache *cache) {
	if (cache->ref_count <= 1) {
		g_hash_table_unref(cache->suffstats_labels);
		g_hash_table_unref(cache->suffstats_offblocks);
		dataset_unref(cache->dataset);
		g_free(cache);
	} else {
		cache->ref_count--;
	}
}

gpointer sscache_get_label(SSCache *cache, gconstpointer label) {
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
		/* the ptr2int int2ptr dance is to express that we are really
		 * respecting the const annotation above... oh gcc.
		 */
		g_hash_table_insert(cache->suffstats_labels,
				GINT_TO_POINTER(GPOINTER_TO_INT(label)),
				suffstats);
	}
	return suffstats;

}

static Offblock_Key * offblock_key_new(GList * fst, GList * snd, gboolean copy) {
	Offblock_Key * key;
	guint hash_fst;
	guint hash_snd;

	list_assert_sorted(fst, cmp_quark);
	list_assert_sorted(snd, cmp_quark);
	hash_fst = list_hash(fst, g_direct_hash);
	hash_snd = list_hash(fst, g_direct_hash);
	if (hash_fst < hash_snd) {
		GList * tmp;
		tmp = fst;
		fst = snd;
		snd = tmp;
	}
	if (copy) {
		fst = g_list_copy(fst);
		snd = g_list_copy(snd);
	}
	key = g_new(Offblock_Key, 1);
	key->copy = copy;
	key->hash = hash_fst ^ hash_snd;
	key->fst = fst;
	key->snd = snd;
	return key;
}

static void offblock_key_free(gpointer pkey) {
	Offblock_Key *key = pkey;

	if (key->copy) {
		g_list_free(key->fst);
		g_list_free(key->snd);
	}
	g_free(key);
}

static guint offblock_key_hash(gconstpointer pkey) {
	const Offblock_Key *key = pkey;
	return key->hash;
}

static gboolean offblock_key_equal(gconstpointer paa, gconstpointer pbb) {
	const Offblock_Key *aa = paa;
	const Offblock_Key *bb = pbb;

	if (aa->hash != bb->hash) {
		return FALSE;
	}
	return	list_equal(aa->fst, bb->fst, g_direct_equal) &&
		list_equal(aa->snd, bb->snd, g_direct_equal);
}

gpointer sscache_get_offblock(SSCache *cache, GList * root, GList * child) {
	return NULL;
}

gpointer sscache_get_offblock_simple(SSCache *cache, GList * xx, GList * yy) {
	gpointer suffstats;
	Offblock_Key * key;

	key = offblock_key_new(xx, yy, FALSE);
	if (!g_hash_table_lookup_extended(cache->suffstats_offblocks,
				key, NULL, &suffstats)) {
		offblock_key_free(key);
		return NULL;
	}
	offblock_key_free(key);
	return suffstats;
}

gpointer sscache_get_offblock_full(SSCache *cache, GList * xx, GList * yy) {
	Counts * suffstats;
	Offblock_Key * key;
	gboolean missing;
	gboolean value;


	suffstats = sscache_get_offblock(cache, xx, yy);
	if (suffstats != NULL) {
		return suffstats;
	}
	// look up the element in dataset. we should only ever have to do this
	// for singletons. so check...
	g_assert(g_list_next(xx) == NULL);
	g_assert(g_list_next(yy) == NULL);

	value = dataset_get(cache->dataset, xx->data, yy->data, &missing);
	if (missing) {
		suffstats = suffstats_new_empty();
	} else {
		suffstats = counts_new(value, 1);
	}
	// now add in the opposing direction... if not symmetric:
	if (!dataset_is_symmetric(cache->dataset)) {
		value = dataset_get(cache->dataset, yy->data, xx->data, &missing);
		if (!missing) {
			suffstats->num_ones += value;
			suffstats->num_total += 1;
		}
	}
	key = offblock_key_new(xx, yy, TRUE);
	g_hash_table_insert(cache->suffstats_offblocks, key, suffstats);
	return suffstats;
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



