#include "sscache.h"
#include "util.h"
#include "counts.h"
#include "labelset.h"

static const gboolean cache_debug = FALSE;
static const gboolean cache_symmetric = FALSE;
static const gboolean cache_disable_offblock = FALSE;

struct SSCache_t {
	guint		ref_count;
	gboolean	enable_sparse;
	Dataset *	dataset;
	Labelset *	emptyset;
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
static gpointer sscache_lookup_offblock_merge(SSCache *cache, Labelset * xx, Labelset * yy_left, Labelset * yy_right);
static gpointer sscache_lookup_offblock_simple(SSCache *cache, Labelset * xx, Labelset * yy);
static gpointer sscache_lookup_offblock_full(SSCache *cache, gconstpointer ii, gconstpointer jj);
static gpointer sscache_lookup_offblock_naive(SSCache *cache, Labelset * xx, Labelset * yy);

SSCache * sscache_new(Dataset *dataset, gboolean sparse) {
	SSCache * cache;

	cache = g_new(SSCache, 1);
	cache->ref_count = 1;
	cache->enable_sparse = sparse;
	cache->dataset = dataset;
	dataset_ref(cache->dataset);
	cache->emptyset = labelset_new(cache->dataset);
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
		labelset_unref(cache->emptyset);
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
	Labelset * xx = labelset_new(cache->dataset, ii);
	Labelset * yy = labelset_new(cache->dataset, jj);
	gpointer suffstats = sscache_get_offblock(cache, xx, cache->emptyset, yy, cache->emptyset);
	labelset_unref(xx);
	labelset_unref(yy);
	return suffstats;
}

gpointer sscache_get_offblock(SSCache *cache, Labelset * xx_left, Labelset * xx_right, Labelset * yy_left, Labelset * yy_right) {
	gpointer suffstats;
	Offblock_Key * key;
	Labelset * xx;
	Labelset * yy;

	g_assert(xx_left != NULL);
	g_assert(xx_right != NULL);
	g_assert(yy_left != NULL);
	g_assert(yy_right != NULL);

	/* first, let's union everything and see if that exists already */
	xx = labelset_copy(xx_left);
	labelset_union(xx, xx_right);

	yy = labelset_copy(yy_left);
	labelset_union(yy, yy_right);

	key = offblock_key_new(xx, yy);

	if (!cache_disable_offblock) {
		suffstats = g_hash_table_lookup(cache->suffstats_offblocks, key);
	} else {
		suffstats = sscache_lookup_offblock_naive(cache, xx, yy);
		g_assert(suffstats != NULL);
	}

	if (suffstats != NULL) {
		goto free_key_out;
	}


	/* if both are singletons, then let's go visit the full data matrix
	 * not really any way around that...
	 */
	if (suffstats == NULL && labelset_count(xx) == 1 && labelset_count(yy) == 1) {
		if (cache_debug) {
			g_print("singleton\n");
		}
		suffstats = sscache_lookup_offblock_full(cache,
				labelset_any_label(xx),
				labelset_any_label(yy));
	}

	/*
	 * thm. if k = {i,j} and z = {x, y}
	 * then either sigma_{kx} and sigma_{ky} 
	 *          or sigma_{zi} and sigma_{zj}
	 * have been computed already.
	 *
	 * note that i,j,x,y and k_i and z_x are all sets of labels.
	 * ergo, we need two lists of sets of labels: kk and zz.
	 */
	if (suffstats == NULL) {
		if (cache_debug) {
			g_print("merge x y\n");
		}
		suffstats = sscache_lookup_offblock_merge(cache, xx, yy_left, yy_right);
	}
	if (suffstats == NULL) {
		if (cache_debug) {
			g_print("merge y x\n");
		}
		suffstats = sscache_lookup_offblock_merge(cache, yy, xx_left, xx_right);
	}
	if (suffstats == NULL) {
		if (cache_debug) {
			g_print("sparse\n");
		}
		suffstats = sscache_lookup_offblock_sparse(cache, xx, yy);
	}
	if (suffstats == NULL) {
		sscache_println(cache, "sscache: ");
		// thm failure
		g_print("failed offblock: ");
		labelset_print(xx);
		g_print(" <-> ");
		labelset_print(yy);
		g_print("\n");
		g_error("theorem failure?!");
	}
	if (suffstats != NULL) {
		if (cache_debug) {
			Counts * counts = suffstats;
			if (counts->num_total > 2*labelset_count(xx)*labelset_count(yy)) {
				g_print("too many elements! ");
				labelset_print(xx);
				g_print(" <-> ");
				labelset_print(yy);
				g_print("\n");
				g_assert(FALSE);
			}
		}
		g_hash_table_insert(cache->suffstats_offblocks, key, suffstats);
	} else {
free_key_out:
		offblock_key_free(key);
	}
	labelset_unref(xx);
	labelset_unref(yy);
	return suffstats;
}

static gpointer sscache_lookup_offblock_merge(SSCache *cache, Labelset * xx, Labelset * yy_left, Labelset * yy_right) {
	gpointer suffstats, off_left, off_right, off_sparse;

	off_sparse = NULL;
	suffstats = suffstats_new_empty();
	off_left  = sscache_lookup_offblock_simple(cache, xx, yy_left);
	off_right = sscache_lookup_offblock_simple(cache, xx, yy_right);
	/* if just one is missing, try doing it with sparsity. */
	if (off_left == NULL && off_right == NULL) {
		goto not_found;
	} else if (off_left == NULL) {
		off_sparse = sscache_lookup_offblock_sparse(cache, xx, yy_left);
		off_left = off_sparse;
	} else if (off_right == NULL) {
		off_sparse = sscache_lookup_offblock_sparse(cache, xx, yy_right);
		off_right = off_sparse;
	}
	/* if we still have one missing, give up */
	if (off_left == NULL || off_right == NULL) {
		if (cache_debug) {
			if (off_left == NULL) {
				g_print("merge not found: left: ");
				labelset_print(xx);
				g_print("/");
				labelset_print(yy_left);
				g_print("\n");
			}
			if (off_right == NULL) {
				g_print("merge not found: right: ");
				labelset_print(xx);
				g_print("/");
				labelset_print(yy_right);
				g_print("\n");
			}
		}
		goto not_found;
	}
	suffstats_add(suffstats, off_left);
	suffstats_add(suffstats, off_right);
	if (off_sparse != NULL) {
		suffstats_unref(off_sparse);
	}
	return suffstats;
not_found:
	g_assert(off_sparse == NULL);
	suffstats_unref(suffstats);
	return NULL;
}


static gpointer sscache_lookup_offblock_simple(SSCache *cache, Labelset * ii, Labelset * jj) {
	gpointer suffstats;
	Offblock_Key * key;

	if (cache_debug) {
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
		if (cache_debug) {
			g_print("sscache_lookup_offblock_simple end: fail\n");
		}
		return NULL;
	}
	if (cache_debug) {
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
	if (!cache_symmetric) {
		// now add in the opposing direction...
		value = dataset_get(cache->dataset, jj, ii, &missing);
		if (!missing) {
			suffstats->num_ones += value;
			suffstats->num_total += 1;
		}
	}
	if (cache_debug) {
		g_print("sscache_lookup_offblock_sparse: ");
		suffstats_print(suffstats);
		g_print("\n");
	}
	return suffstats;
}


static gpointer sscache_lookup_offblock_sparse(SSCache *cache, Labelset * kk, Labelset *zz) {
	Counts * counts;

	if (!cache->enable_sparse) {
		g_print("sparse not enabled\n");
		return NULL;
	}
	counts = suffstats_new_empty();
	counts->num_total = labelset_count(kk)*labelset_count(zz);
	if (!cache_symmetric) {
		counts->num_total *= 2;
	}
	if (cache_debug) {
		g_print("sparse: ");
		labelset_print(kk);
		g_print(" <-> ");
		labelset_print(zz);
		g_print(" %d\n", counts->num_total);
	}
	return counts;
}

static gpointer sscache_lookup_offblock_naive(SSCache *cache, Labelset * xx, Labelset * yy) {
	Counts * suffstats;
	LabelsetIter iter_xx, iter_yy;
	gpointer ii, jj;
	gboolean missing;
	gboolean value;

	suffstats = suffstats_new_empty();
	labelset_iter_init(&iter_xx, xx);
	while (labelset_iter_next(&iter_xx, &ii)) {
		dataset_label_assert(cache->dataset, ii);
		labelset_iter_init(&iter_yy, yy);
		while (labelset_iter_next(&iter_yy, &jj)) {
			dataset_label_assert(cache->dataset, jj);
			g_assert(ii != jj);

			value = dataset_get(cache->dataset, ii, jj, &missing);
			suffstats->num_total += (missing? 0: 1);
			suffstats->num_ones  += (missing||!value? 0: 1);

			if (!cache_symmetric) {
				// now add in the opposing direction...
				value = dataset_get(cache->dataset, jj, ii, &missing);
				suffstats->num_total += (missing? 0: 1);
				suffstats->num_ones  += (missing||!value? 0: 1);
			}
		}
	}
	if (cache_debug) {
		g_print("sscache_lookup_offblock_naive: ");
		suffstats_print(suffstats);
		g_print("\n");
	}
	return suffstats;
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

void suffstats_sub(gpointer pdst, gpointer psrc) {
	Counts * dst = pdst;
	Counts * src = psrc;
	counts_sub(dst, src);
}


void suffstats_print(gpointer ss) {
	Counts * cc = ss;
	g_print("<0: %d, 1: %d>", cc->num_total-cc->num_ones, cc->num_ones);
}

