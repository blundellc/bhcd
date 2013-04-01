#ifndef	LABELSET_H
#define	LABELSET_H

#include <glib.h>
#include "dataset.h"
#include "bitset.h"
#include "util.h"

struct Labelset_t;
typedef struct Labelset_t Labelset;

Labelset * labelset_new_full(Dataset * dataset, ...);
#define	labelset_new(...)	labelset_new_full(__VA_ARGS__, NULL)
Labelset * labelset_copy(Labelset *);
void labelset_ref(Labelset * lset);
void labelset_unref(Labelset * lset);

guint32 labelset_count(Labelset * lset);
gboolean labelset_equal(Labelset *aa, Labelset *bb);
guint labelset_hash(Labelset * lset);
gint labelset_cmp(gconstpointer paa, gconstpointer pbb);

gpointer labelset_any_label(Labelset * lset);
void labelset_add(Labelset *lset, gconstpointer label);
gboolean labelset_contains(Labelset *lset, gconstpointer label);
void labelset_del(Labelset *lset, gconstpointer label);
void labelset_union(Labelset *aa, Labelset *bb);

void labelset_print(Labelset * lset);
void labelset_tostring(Labelset * lset, GString * out);
void labelset_union(Labelset *aa, Labelset *bb);
gboolean labelset_disjoint(Labelset *aa, Labelset *bb);


#endif /*LABELSET_H*/
