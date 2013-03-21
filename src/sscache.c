#include "sscache.h"
#include "util.h"
#include "counts.h"
#include "labelset.h"


struct SSCache_t {
	guint		ref_count;
	gboolean	debug;
	Dataset *	dataset;
	GHashTable *	suffstats_labels;
	GHashTable *	suffstats_offblocks;
};


typedef struct {
	guint hash;
	Labelset *fst;
	Labelset *snd;
} Offblock_Key;

static Offblock_Key * offblock_key_new(Labelset * fst, Labelset * snd);
static void offblock_key_free(gpointer pkey);
static gboolean offblock_key_equal(gconstpointer paa, gconstpointer pbb);
static guint offblock_key_hash(gconstpointer pkey);
static gpointer sscache_get_offblock_ordered(SSCache *cache, GList * kk, GList * zz);

SSCache * sscache_new(Dataset *dataset) {
	SSCache * cache;

	cache = g_new(SSCache, 1);
	cache->ref_count = 1;
	cache->debug = FALSE;
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

static Offblock_Key * offblock_key_new(Labelset * fst, Labelset * snd) {
	Offblock_Key * key;
	guint hash_fst;
	guint hash_snd;

	hash_fst = labelset_hash(fst);
	hash_snd = labelset_hash(snd);
	if (hash_fst < hash_snd) {
		Labelset * tmp;
		tmp = fst;
		fst = snd;
		snd = tmp;
	}
	key = g_new(Offblock_Key, 1);
	key->hash = hash_fst ^ hash_snd;
	key->fst = fst;
	labelset_ref(key->fst);
	key->snd = snd;
	labelset_ref(key->snd);
	return key;
}

static void offblock_key_free(gpointer pkey) {
	Offblock_Key *key = pkey;

	labelset_unref(key->fst);
	labelset_unref(key->snd);
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
	return	labelset_equal(aa->fst, bb->fst) &&
		labelset_equal(aa->snd, bb->snd);
}


gpointer sscache_get_offblock(SSCache *cache, GList * kk, GList * zz) {
	gpointer suffstats;

	g_assert(kk != NULL);
	g_assert(zz != NULL);

	if (cache->debug) {
		g_print("sscache_get_offblock: {");
		list_labelset_print(zz);
		g_print("} ** {");
		list_labelset_print(kk);
		g_print("}\n");
	}

	/* if both are singletons, then let's go visit the full data matrix
	 * not really any way around that...
	 */
	if (g_list_next(kk) == NULL && g_list_next(zz) == NULL &&
			labelset_count(kk->data) == 1 &&
			labelset_count(zz->data) == 1) {
		return sscache_get_offblock_full(cache,
				labelset_any_label(kk->data),
				labelset_any_label(zz->data));
	}

	/*
	 * thm. if k = {i,j} and z = {x, y}
	 * then either sigma_{kx} and sigma_{ky} 
	 *          or sigma_{zi} and sigma_{zj}
	 * have been computed already.
	 *
	 * likewise we will say that
	 * if k = {k_i} and z = {z_x}
	 * then either sigma_{k z_x} for all x
	 *          or sigma_{z k_i} for all i
	 * have been computed.
	 *
	 * note that i,j,x,y and k_i and z_x are all sets of labels.
	 * ergo, we need two lists of sets of labels: kk and zz.
	 */
	suffstats = sscache_get_offblock_ordered(cache, kk, zz);
	if (suffstats == NULL) {
		suffstats = sscache_get_offblock_ordered(cache, zz, kk);
	}
	// thm failure
	g_assert(suffstats != NULL);
	return suffstats;
}

static gpointer sscache_get_offblock_ordered(SSCache *cache, GList * kkii, GList * zzxx) {
	Offblock_Key * key;

	Labelset * kk = labelset_new(cache->dataset);
	Labelset * zz = labelset_new(cache->dataset);
	gpointer suffstats = suffstats_new_empty();
	for (; kkii != NULL; kkii = g_list_next(kkii)) {
		labelset_union(kk, kkii->data);
	}
	for (; zzxx != NULL; zzxx = g_list_next(zzxx)) {
		gpointer offblock_suffstats = sscache_get_offblock_simple(cache, kk, zzxx->data);
		if (offblock_suffstats == NULL) {
			goto error;
		}
		suffstats_add(suffstats, offblock_suffstats);
		labelset_union(zz, zzxx->data);
	}
	key = offblock_key_new(kk, zz);
	g_hash_table_insert(cache->suffstats_offblocks, key, suffstats);
	labelset_unref(kk);
	labelset_unref(zz);
	return suffstats;
error:
	labelset_unref(kk);
	labelset_unref(zz);
	suffstats_unref(suffstats);
	return NULL;
}

gpointer sscache_get_offblock_simple(SSCache *cache, Labelset * ii, Labelset * jj) {
	gpointer suffstats;
	Offblock_Key * key;

	if (cache->debug) {
		g_print("sscache_get_offblock_simple: {");
		labelset_print(ii);
		g_print("} ** {");
		labelset_print(jj);
		g_print("}\n");
	}

	key = offblock_key_new(ii, jj);
	if (!g_hash_table_lookup_extended(cache->suffstats_offblocks,
				key, NULL, &suffstats)) {
		offblock_key_free(key);
		if (cache->debug) {
			g_print("sscache_get_offblock_simple end: fail\n");
		}
		return NULL;
	}
	if (cache->debug) {
		g_print("sscache_get_offblock_simple end: ");
		suffstats_print(suffstats);
		g_print("\n");
	}
	offblock_key_free(key);
	return suffstats;
}

gpointer sscache_get_offblock_full(SSCache *cache, gconstpointer ii, gconstpointer jj) {
	Counts * suffstats;
	Offblock_Key * key;
	gboolean missing;
	gboolean value;
	Labelset * xx;
	Labelset * yy;

	dataset_label_assert(cache->dataset, ii);
	dataset_label_assert(cache->dataset, jj);
	g_assert(ii != jj);

	/* look up the element in dataset. we should only ever have to do this
	 * for singletons.
	 */
	xx = labelset_new(cache->dataset, ii);
	yy = labelset_new(cache->dataset, jj);
	suffstats = sscache_get_offblock_simple(cache, xx, yy);
	if (suffstats != NULL) {
		goto out;
	}
	value = dataset_get(cache->dataset, ii, jj, &missing);
	if (missing) {
		suffstats = suffstats_new_empty();
	} else {
		suffstats = counts_new(value, 1);
	}
	// now add in the opposing direction... if not symmetric:
	if (!dataset_is_symmetric(cache->dataset)) {
		value = dataset_get(cache->dataset, jj, ii, &missing);
		if (!missing) {
			suffstats->num_ones += value;
			suffstats->num_total += 1;
		}
	}
	if (cache->debug) {
		g_print("sscache_get_offblock_full: ");
		suffstats_print(suffstats);
		g_print("\n");
	}
	key = offblock_key_new(xx, yy);
	g_hash_table_insert(cache->suffstats_offblocks, key, suffstats);
out:
	labelset_unref(xx);
	labelset_unref(yy);
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


void suffstats_print(gpointer ss) {
	Counts * cc = ss;
	g_print("<0: %d, 1: %d>", cc->num_ones, cc->num_total-cc->num_ones);
}

