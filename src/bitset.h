#ifndef	BITSET_H
#define	BITSET_H

#include <glib.h>

struct Bitset_t;
typedef struct Bitset_t Bitset;

Bitset * bitset_new(guint32 max_index);
void bitset_ref(Bitset *);
void bitset_unref(Bitset *);

gboolean bitset_equal(Bitset *aa, Bitset *bb);
guint bitset_hash(Bitset * bitset);

void bitset_set(Bitset *bitset, guint32 index);
void bitset_clear(Bitset *bitset, guint32 index);
gboolean bitset_contains(Bitset *bitset, guint32 index);
void bitset_union(Bitset *dst, Bitset *src);

typedef void (*BitsetFunc)(gpointer, guint32);
void bitset_foreach(Bitset *bitset, BitsetFunc func, gpointer user_data);

void bitset_println(Bitset *bitset, const gchar *prefix);
void bitset_tostring(Bitset * bitset, GString * out);

#endif /*BITSET_H*/
