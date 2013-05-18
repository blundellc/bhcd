#include <gsl/gsl_sf_gamma.h>
#include <math.h>
#include "lnbetacache.h"


// allocate at most ~1GB for the cache.
#define	MAX_MAX_ENTRIES	11585

struct LnBetaCache_t {
	guint max_num;
	guint size;
	guint hits;
	gdouble	alpha;
	gdouble beta;
	gdouble * evals;
};


LnBetaCache * lnbetacache_new(gdouble alpha, gdouble beta, guint max_num) {
	LnBetaCache * cache;

	cache = g_new(LnBetaCache, 1);
	cache->hits = 0;
	cache->alpha = alpha;
	cache->beta = beta;
	if (max_num > MAX_MAX_ENTRIES) {
		max_num = MAX_MAX_ENTRIES;
	}
	cache->size = max_num*max_num;
	cache->evals = g_new(gdouble, cache->size);
	cache->max_num = max_num;

	for (guint ii = 0; ii < cache->size; ii++) {
		cache->evals[ii] = INFINITY;
	}
	return cache;
}

void lnbeta_cache_free(LnBetaCache * cache) {
	g_free(cache->evals);
	g_free(cache);
}


gdouble lnbetacache_get(LnBetaCache * cache, guint num_ones, guint num_zeros) {
	gdouble val;
	guint offset;

	if (num_ones >= cache->max_num || num_zeros >= cache->max_num) {
		return gsl_sf_lnbeta(cache->alpha + num_ones, cache->beta + num_zeros);
	}

	offset = num_zeros + cache->max_num*num_ones;
	g_assert(offset < cache->size);
	val = cache->evals[offset];
	if (!isfinite(val)) {
		val = gsl_sf_lnbeta(cache->alpha + num_ones, cache->beta + num_zeros);
		cache->evals[offset] = val;
	} else {
		cache->hits++;
	}
	return val;
}

guint lnbetacache_get_num_hits(LnBetaCache * cache) {
	return cache->hits;
}

