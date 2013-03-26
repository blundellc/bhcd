#ifndef	ISLANDS_H
#define	ISLANDS_H

#include <glib.h>
#include "dataset.h"

struct Islands_t;
typedef struct Islands_t Islands;

Islands * islands_new(Dataset *, GPtrArray * trees);
void islands_add_edge(Islands *, guint, guint);
void islands_merge(Islands *, guint, guint, guint);
GList * islands_get_neigh(Islands *, guint);
void islands_get_neigh_free(GList *);
GList * islands_get_edges(Islands *);
void islands_get_edges_free(GList *);
void islands_free(Islands *);


#endif /*ISLANDS_H*/
