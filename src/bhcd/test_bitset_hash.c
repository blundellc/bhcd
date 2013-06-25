#include <glib.h>
#include "bitset.h"

int main(int argc, char *argv[]) {
	Bitset * aa;
	guint32 hh;
	const guint size = 221;

	aa = bitset_new(size);
	for (guint32 ii = 1; ii < size; ii++) {
		bitset_set(aa, ii);
		hh = bitset_hash(aa);
		g_print("%d: %x\n", ii, hh);
		for (guint32 jj = ii+1; jj < size; jj++) {
			bitset_set(aa, jj);
			hh = bitset_hash(aa);
			g_print("%d, %d: %x\n", ii, jj, hh);
			for (guint32 kk = jj+1; kk < size; kk++) {
				bitset_set(aa, kk);
				hh = bitset_hash(aa);
				g_print("%d, %d, %d: %x\n", ii, jj, kk, hh);
				bitset_clear(aa, kk);
			}
			bitset_clear(aa, jj);
		}
		bitset_clear(aa, ii);
	}
	bitset_unref(aa);
	return 0;
}
