#include "bitset.h"
#include "util.h"

#define	BITS_PER_ELEM		64
#define	MASK_ELEM		(BITS_PER_ELEM-1)
/* log2(BITS_PER_ELEM) */
#define SHIFT_ELEM		6

struct Bitset_t {
	guint ref_count;
	/* size of elems */
	guint32 size;
	guint64 *elems;
};

static inline void bitset_get_bit(guint32 index, guint32 * elem_index, guint64 * bit);

Bitset * bitset_new(guint32 max_index) {
	Bitset * bitset;

	bitset = g_new(Bitset, 1);
	bitset->ref_count = 1;
	bitset->size = 1 + max_index/BITS_PER_ELEM;
	bitset->elems = g_new0(guint64, bitset->size);
	return bitset;
}

Bitset * bitset_copy(Bitset *other) {
	Bitset * bitset = bitset_new((other->size-1)*BITS_PER_ELEM);
	g_assert(bitset->size == other->size);
	for (guint32 ii = 0; ii < bitset->size; ii++) {
		bitset->elems[ii] = other->elems[ii];
	}
	return bitset;
}

void bitset_ref(Bitset * bitset) {
	bitset->ref_count++;
}

void bitset_unref(Bitset * bitset) {
	if (bitset->ref_count <= 1) {
		g_free(bitset->elems);
		g_free(bitset);
	} else {
		bitset->ref_count--;
	}
}

gboolean bitset_equal(Bitset *aa, Bitset *bb) {
	if (aa->size != bb->size) {
		return FALSE;
	}
	for (guint32 ii = 0; ii < aa->size; ii++) {
		if (aa->elems[ii] != bb->elems[ii]) {
			return FALSE;
		}
	}
	return TRUE;
}

guint32 bitset_count(Bitset *bitset) {
	guint32 count = 0;
	for (guint32 ii = 0; ii < bitset->size; ii++) {
		count += pop_count(bitset->elems[ii]);
	}
	return count;
}

guint bitset_hash(Bitset * bitset) {
	guint32 hash = 0;

	for (guint32 ii = 0; ii < bitset->size; ii++) {
		const guint64 elem = bitset->elems[ii];
		hash ^= elem & 0xffffff;
		hash ^= elem >> 32;
	}
	return hash;
}

static inline void bitset_get_bit(guint32 index, guint32 * elem_index, guint64 * bit) {
	guint8 offset;

	offset = index & MASK_ELEM;
	/* because of type rules, need to cast the 1 to something big enough,
	 * else we get wrap around...
	 */
	*bit = ((guint64)1) << offset;
	*elem_index = index >> SHIFT_ELEM;
}


guint32 bitset_any(Bitset *bitset) {
	for (guint32 ii = 0; ii < bitset->size; ii++) {
		guint64 elem = bitset->elems[ii];
		gint offset = g_bit_nth_lsf(elem, -1);
		if (offset != -1) {
			return offset + ii*BITS_PER_ELEM;
		}
	}
	g_assert_not_reached();
	return 0;
}

void bitset_set(Bitset *bitset, guint32 index) {
	guint64 bit;
	guint32 elem_index;

	bitset_get_bit(index, &elem_index, &bit);
	bitset->elems[elem_index] |= bit;
}

void bitset_clear(Bitset *bitset, guint32 index) {
	guint64 bit;
	guint32 elem_index;

	bitset_get_bit(index, &elem_index, &bit);
	bitset->elems[elem_index] &= ~bit;
}

gboolean bitset_contains(Bitset *bitset, guint32 index) {
	guint64 bit;
	guint32 elem_index;

	bitset_get_bit(index, &elem_index, &bit);
	return (bitset->elems[elem_index] & bit) != 0;
}

void bitset_union(Bitset *dst, Bitset *src) {
	g_assert(dst->size >= src->size);
	for (guint32 ii = 0; ii < src->size; ii++) {
		dst->elems[ii] |= src->elems[ii];
	}
}

gboolean bitset_disjoint(Bitset *aa, Bitset *bb) {
	guint32 min_size = MIN(aa->size, bb->size);
	for (guint ii = 0; ii < min_size; ii++) {
		if ((aa->elems[ii] & bb->elems[ii]) != 0) {
			return FALSE;
		}
	}
	return TRUE;
}

void bitset_foreach(Bitset *bitset, BitsetFunc func, gpointer user_data) {
	for (guint32 ii = 0; ii < bitset->size; ii++) {
		guint64 elem = bitset->elems[ii];
		guint32 bit;
		gint offset = g_bit_nth_lsf(elem, -1);
		for (;offset != -1; offset = g_bit_nth_lsf(elem, offset)) {
			bit = offset + ii*BITS_PER_ELEM;
			func(user_data, bit);
		}
	}
}

void bitset_print(Bitset * bitset) {
        GString * out;

        out = g_string_new("");
        bitset_tostring(bitset, out);
        g_print("%s", out->str);
        g_string_free(out, TRUE);
}

void bitset_tostring_append(gpointer pout, guint32 ii) {
	GString * out = pout;
	g_string_append_printf(out, "%d ", ii);
}


void bitset_tostring(Bitset * bitset, GString * out) {
	bitset_foreach(bitset, bitset_tostring_append, out);
}

