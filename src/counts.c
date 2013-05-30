#include "counts.h"

Counts * counts_new(guint num_ones, guint num_total) {
	Counts * counts;

	counts = g_slice_new(Counts);
	counts->ref_count = 1;
	counts->num_ones = num_ones;
	counts->num_total = num_total;
	return counts;
}

Counts * counts_copy(Counts * orig) {
	return counts_new(orig->num_ones, orig->num_total);
}

void counts_add(Counts * dst, Counts * src) {
	dst->num_ones += src->num_ones;
	dst->num_total += src->num_total;
}

guint counts_num_zeros(Counts * counts) {
	return counts->num_total - counts->num_ones;
}

guint counts_num_ones(Counts * counts) {
	return counts->num_ones;
}

void counts_ref(Counts * counts) {
	counts->ref_count++;
}

void counts_unref(Counts * counts) {
	if (counts->ref_count <= 1) {
		g_slice_free(Counts, counts);
	} else {
		counts->ref_count--;
	}
}

