#ifndef	SSCACHE_H
#define	SSCACHE_H

#include <glib.h>
#include "dataset.h"
#include "labelset.h"

struct SSCache_t;
typedef struct SSCache_t SSCache;

SSCache * sscache_new(Dataset *);
gpointer sscache_get_label(SSCache *cache, gconstpointer label);
gpointer sscache_get_offblock(SSCache *cache, GList * xx, GList * yy);
gpointer sscache_get_offblock_simple(SSCache *cache, Labelset * xx, Labelset * yy);
gpointer sscache_get_offblock_full(SSCache *cache, gconstpointer ii, gconstpointer jj);
void sscache_unref(SSCache *cache);

gpointer suffstats_new_empty(void);
gpointer suffstats_copy(gpointer src);
void suffstats_ref(gpointer ss);
void suffstats_unref(gpointer ss);
void suffstats_add(gpointer pdst, gpointer psrc);
void suffstats_print(gpointer ss);

#endif /*SSCACHE_H*/
