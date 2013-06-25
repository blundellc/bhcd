#ifndef	BITSET_H
#define	BITSET_H

#include <glib.h>

struct Bitset_t;
typedef struct Bitset_t Bitset;
typedef struct {
	/* private */
	Bitset * bitset;
	guint32 elem_index;
	gint offset;
} BitsetIter;

Bitset * bitset_new(guint32 max_index);
Bitset * bitset_copy(Bitset *);
void bitset_ref(Bitset *);
void bitset_unref(Bitset *);

guint32 bitset_count(Bitset *);
gboolean bitset_equal(Bitset *aa, Bitset *bb);
guint bitset_hash(Bitset * bitset);
gint bitset_cmp(gconstpointer paa, gconstpointer pbb);

guint32 bitset_any(Bitset *);
void bitset_set(Bitset *bitset, guint32 index);
void bitset_clear(Bitset *bitset, guint32 index);
void bitset_clear_all(Bitset *bitset);
gboolean bitset_contains(Bitset *bitset, guint32 index);
void bitset_union(Bitset *dst, Bitset *src);
gboolean bitset_disjoint(Bitset *, Bitset *);

void bitset_iter_init(BitsetIter *, Bitset *);
gboolean bitset_iter_next(BitsetIter *, guint32 *);
typedef void (*BitsetFunc)(gpointer, guint32);
void bitset_foreach(const Bitset *bitset, BitsetFunc func, gpointer user_data);

void bitset_print(const Bitset *bitset);
void bitset_tostring(const Bitset * bitset, GString * out);

#endif /*BITSET_H*/
