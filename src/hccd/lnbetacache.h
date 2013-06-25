#ifndef	LNBETACACHE_H
#define	LNBETACACHE_H

#include <glib.h>

struct LnBetaCache_t;
typedef struct LnBetaCache_t LnBetaCache;

LnBetaCache * lnbetacache_new(gdouble alpha, gdouble beta, guint max_num);
gdouble lnbetacache_get(LnBetaCache * cache, guint num_ones, guint num_zeros);
guint lnbetacache_get_num_hits(LnBetaCache * cache);
void lnbeta_cache_free(LnBetaCache * cache);

#endif /*LNBETACACHE_H*/
