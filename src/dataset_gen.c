#include "dataset.h"
#include "util.h"

Dataset * dataset_gen_speckle(GRand * rng, guint num_items, gdouble prob_one) {
	Dataset * dd;
	guint ii, jj;

	dd = dataset_new(TRUE);
	for (ii = 0; ii < num_items; ii++) {
		gchar *name_ii = num_to_string(ii);
		gpointer label_ii = dataset_label_create(dd, name_ii);
		g_free(name_ii);
		for (jj = ii; jj < num_items; jj++) {
			gchar *name_jj = num_to_string(jj);
			gpointer label_jj = dataset_label_create(dd, name_jj);
			g_free(name_jj);
			dataset_set(dd, label_ii, label_jj,
					g_rand_double(rng) < prob_one);
		}
	}
	return dd;
}


Dataset * dataset_gen_blocks(GRand * rng, guint num_items, guint block_width, gdouble prob_flip) {
	Dataset *dd;
	guint ii, jj;
	guint value;

	dd = dataset_new(TRUE);
	for (ii = 0; ii < num_items; ii++) {
		gchar *name_ii = num_to_string(ii);
		gpointer label_ii = dataset_label_create(dd, name_ii);
		g_free(name_ii);
		for (jj = ii; jj < num_items; jj++) {
			gchar *name_jj = num_to_string(jj);
			gpointer label_jj = dataset_label_create(dd, name_jj);
			g_free(name_jj);

			value = (ii/block_width % 2) == (jj/block_width % 2)
				&& (guint)ABS((gint)ii-(gint)jj) < block_width;
			if (g_rand_double(rng) < prob_flip) {
				value = !value;
			}
			dataset_set(dd, label_ii, label_jj, value);
		}
	}
	return dd;
}


Dataset * dataset_gen_toy3(void) {
	Dataset * dataset;
	gpointer aa;
	gpointer bb;
	gpointer cc;

	dataset = dataset_new(TRUE);
	/*     aa   bb   cc
	 * aa   _    1    0
	 * bb   1    _    0
	 * cc   0    0    _
	 */
	aa = dataset_label_create(dataset, "aa");
	bb = dataset_label_create(dataset, "bb");
	cc = dataset_label_create(dataset, "cc");
	dataset_set_missing(dataset, aa, aa);
	dataset_set_missing(dataset, bb, bb);
	dataset_set_missing(dataset, cc, cc);
	dataset_set(dataset, aa, bb, TRUE);
	dataset_set(dataset, aa, cc, FALSE);
	dataset_set(dataset, bb, cc, FALSE);
	return dataset;
}


Dataset * dataset_gen_toy4(void) {
	Dataset * dataset;
	gpointer aa, bb, cc, dd;

	dataset = dataset_new(FALSE);
	/* from scala code
	 *     aa   bb   cc   dd
	 * aa   1    0    0    1
	 * bb   0    _    1    _
	 * cc   1    1    0    0
	 * dd   0    0    1    _
	 */
	aa = dataset_label_create(dataset, "aa");
	bb = dataset_label_create(dataset, "bb");
	cc = dataset_label_create(dataset, "cc");
	dd = dataset_label_create(dataset, "dd");

	/* missing */
	dataset_set_missing(dataset, bb, bb);
	dataset_set_missing(dataset, bb, dd);
	dataset_set_missing(dataset, dd, dd);

	/* ones */
	dataset_set(dataset, aa, aa, TRUE);
	dataset_set(dataset, aa, dd, TRUE);
	dataset_set(dataset, bb, cc, TRUE);
	dataset_set(dataset, cc, aa, TRUE);
	dataset_set(dataset, cc, bb, TRUE);
	dataset_set(dataset, dd, cc, TRUE);

	/* zeros */
	dataset_set(dataset, aa, bb, FALSE);
	dataset_set(dataset, aa, cc, FALSE);
	dataset_set(dataset, bb, aa, FALSE);
	dataset_set(dataset, cc, cc, FALSE);
	dataset_set(dataset, cc, dd, FALSE);
	dataset_set(dataset, dd, aa, FALSE);
	dataset_set(dataset, dd, bb, FALSE);

	return dataset;
}


