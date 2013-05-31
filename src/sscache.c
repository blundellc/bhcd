#include "sscache.h"
#include "util.h"
#include "counts.h"
#include "labelset.h"


struct SSCache_t {
	guint		ref_count;
	gboolean	debug;
	gboolean	not_found_zero;
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
static gpointer sscache_lookup_offblock_sparse(SSCache *cache, Labelset * kk, Labelset * zz);
static gpointer sscache_lookup_offblock_ordered(SSCache *cache, Labelset * kk, GList * zzxx);
static gpointer sscache_lookup_offblock_simple(SSCache *cache, Labelset * xx, Labelset * yy);
static gpointer sscache_lookup_offblock_full(SSCache *cache, gconstpointer ii, gconstpointer jj);

SSCache * sscache_new(Dataset *dataset) {
	SSCache * cache;

	cache = g_new(SSCache, 1);
	cache->ref_count = 1;
	cache->not_found_zero = TRUE;
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

	key = g_slice_new(Offblock_Key);
	key->fst = fst;
	labelset_ref(key->fst);
	key->snd = snd;
	labelset_ref(key->snd);
	key->hash  = labelset_hash(key->fst);
	key->hash ^= labelset_hash(key->snd);
	return key;
}

static void offblock_key_free(gpointer pkey) {
	Offblock_Key *key = pkey;

	labelset_unref(key->fst);
	labelset_unref(key->snd);
	g_slice_free(Offblock_Key, key);
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
	return	(labelset_equal(aa->fst, bb->fst) && labelset_equal(aa->snd, bb->snd)) ||
		(labelset_equal(aa->fst, bb->snd) && labelset_equal(aa->snd, bb->fst));
}


gpointer sscache_get_offblock_full(SSCache *cache, gconstpointer ii, gconstpointer jj) {
	Labelset * kk = labelset_new(cache->dataset, ii);
	Labelset * zz = labelset_new(cache->dataset, jj);
	GList * kkii = list_new(kk);
	GList * zzxx = list_new(zz);
	gpointer suffstats = sscache_get_offblock(cache, kkii, zzxx);
	labelset_unref(kk);
	labelset_unref(zz);
	g_list_free(kkii);
	g_list_free(zzxx);
	return suffstats;
}

gpointer sscache_get_offblock(SSCache *cache, GList * kkii, GList * zzxx) {
	gpointer suffstats;
	Labelset * kk;
	Labelset * zz;
	Offblock_Key * key;

	g_assert(kkii != NULL);
	g_assert(zzxx != NULL);

	if (cache->debug) {
		g_print("sscache_get_offblock: {");
		list_labelset_print(kkii);
		g_print("} ** {");
		list_labelset_print(zzxx);
		g_print("}\n");
	}

	/* first, let's union everything and see if that exists already */
	kk = labelset_new(cache->dataset);
	zz = labelset_new(cache->dataset);
	for (GList * jj = kkii; jj != NULL; jj = g_list_next(jj)) {
		labelset_union(kk, jj->data);
	}
	for (GList * yy = zzxx; yy != NULL; yy = g_list_next(yy)) {
		labelset_union(zz, yy->data);
	}
	key = offblock_key_new(kk, zz);
	suffstats = g_hash_table_lookup(cache->suffstats_offblocks, key);
	if (suffstats != NULL) {
		goto found_out;
	}

	/* if both are singletons, then let's go visit the full data matrix
	 * not really any way around that...
	 */
	if (suffstats == NULL && labelset_count(kk) == 1 && labelset_count(zz) == 1) {
		suffstats = sscache_lookup_offblock_full(cache,
				labelset_any_label(kk),
				labelset_any_label(zz));
		goto out;
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
	if (suffstats == NULL) {
		suffstats = sscache_lookup_offblock_ordered(cache, kk, zzxx);
	}
	if (suffstats == NULL) {
		suffstats = sscache_lookup_offblock_ordered(cache, zz, kkii);
	}
	if (suffstats == NULL) {
		suffstats = sscache_lookup_offblock_sparse(cache, kk, zz);
	}
	if (suffstats == NULL) {
		sscache_println(cache, "sscache: ");
		// thm failure
		g_print("failed offblock: ");
		list_labelset_print(kkii);
		g_print(" <-> ");
		list_labelset_print(zzxx);
		g_print("\n");
		g_error("theorem failure?!");
	}
out:
	if (suffstats != NULL) {
		g_hash_table_insert(cache->suffstats_offblocks, key, suffstats);
	} else {
found_out:
		offblock_key_free(key);
	}
	labelset_unref(kk);
	labelset_unref(zz);
	return suffstats;
}

static gpointer sscache_lookup_offblock_ordered(SSCache *cache, Labelset * kk, GList * zzxx) {
	gpointer suffstats = suffstats_new_empty();
	gboolean found = FALSE;
	for (GList * xx = zzxx; xx != NULL; xx = g_list_next(xx)) {
		gpointer offblock_suffstats = sscache_lookup_offblock_simple(cache, kk, xx->data);
		found = offblock_suffstats != NULL;
		if (found) {
			break;
		}
	}
	if (!found) {
		goto not_found;
	}
	for (; zzxx != NULL; zzxx = g_list_next(zzxx)) {
		gpointer offblock_suffstats = sscache_lookup_offblock_simple(cache, kk, zzxx->data);
		gboolean sparse = FALSE;
		if (offblock_suffstats == NULL) {
			offblock_suffstats = sscache_lookup_offblock_sparse(cache, kk, zzxx->data);
			sparse = TRUE;
		}
		if (offblock_suffstats == NULL) {
			goto not_found;
		}
		suffstats_add(suffstats, offblock_suffstats);
		if (sparse) {
			suffstats_unref(offblock_suffstats);
		}
	}
	return suffstats;
not_found:
	suffstats_unref(suffstats);
	return NULL;
}

static gpointer sscache_lookup_offblock_simple(SSCache *cache, Labelset * ii, Labelset * jj) {
	gpointer suffstats;
	Offblock_Key * key;

	if (cache->debug) {
		g_print("sscache_lookup_offblock_simple: {");
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
			g_print("sscache_lookup_offblock_simple end: fail\n");
		}
		return NULL;
	}
	if (cache->debug) {
		g_print("sscache_lookup_offblock_simple end: ");
		suffstats_print(suffstats);
		g_print("\n");
	}
	offblock_key_free(key);
	return suffstats;
}

static gpointer sscache_lookup_offblock_full(SSCache *cache, gconstpointer ii, gconstpointer jj) {
	Counts * suffstats;
	gboolean missing;
	gboolean value;

	dataset_label_assert(cache->dataset, ii);
	dataset_label_assert(cache->dataset, jj);
	g_assert(ii != jj);

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
		g_print("sscache_lookup_offblock_full: ");
		suffstats_print(suffstats);
		g_print("\n");
	}
	return suffstats;
}


static gpointer sscache_lookup_offblock_sparse(SSCache *cache, Labelset * kk, Labelset *zz) {
	Counts * counts;

	if (!cache->not_found_zero) {
		return NULL;
	}
	counts = suffstats_new_empty();
	counts->num_total = labelset_count(kk)*labelset_count(zz);
	return counts;
}


void sscache_println(SSCache * cache, const gchar * prefix) {
	GList * keys = g_hash_table_get_keys(cache->suffstats_offblocks);
	for (; keys != NULL; keys = g_list_next(keys)) {
		Offblock_Key *key = keys->data;
		g_print(" (");
		labelset_print(key->fst);
		g_print(", ");
		labelset_print(key->snd);
		g_print(")");
		if (g_list_next(keys) != NULL) {
			g_print(", ");
		}
	}
	g_print("\n");
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

