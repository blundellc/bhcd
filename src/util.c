#include <gsl/gsl_sf_log.h>
#include <gsl/gsl_sf_exp.h>
#include "util.h"


gdouble log_add_exp(gdouble xx, gdouble yy) {
	gdouble diff = xx - yy;
	if (diff >= 0) {
		return xx + gsl_sf_log_1plusx(gsl_sf_exp(-diff));
	} else if (diff < 0) {
		return yy + gsl_sf_log_1plusx(gsl_sf_exp(diff));
	} else {
		/* inf/nan with same sign */
		return xx + yy;
	}
}

gchar * num_to_string(guint ii) {
	static gchar base[] = "0123456789abcdefghijklmnopqrstuvwxyz";
	GString *out;

	out = g_string_new("");

	do {
		g_string_append_c(out, base[ii % sizeof(base)]);
		ii /= sizeof(base);
	} while (ii != 0);

	return g_string_free(out, FALSE);
}


gint cmp_quark(gconstpointer paa, gconstpointer pbb) {
	GQuark aa;
	GQuark bb;

	aa = GPOINTER_TO_INT(paa);
	bb = GPOINTER_TO_INT(pbb);
	return (gint)aa - (gint)bb;
}

