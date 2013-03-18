#ifndef UTIL_H
#define UTIL_H

#include <glib.h>
#include <math.h>
#include <gsl/gsl_math.h>


typedef union {
	gpointer ptr;
	gconstpointer const_ptr;
} UnionPtr;

gdouble log_add_exp(gdouble, gdouble);
gchar * num_to_string(guint);
gint cmp_quark(gconstpointer, gconstpointer);

#define	assert_eqfloat(expa, expb, prec)							\
	do {											\
		gdouble _assert_eqfloat_aa = (expa);						\
		gdouble _assert_eqfloat_bb = (expb);						\
		if (fabs(_assert_eqfloat_aa) < prec ||					\
		    fabs(_assert_eqfloat_bb) < prec) {					\
			if (fabs(_assert_eqfloat_aa-_assert_eqfloat_bb) < prec) {		\
				break;								\
			}									\
		} else {									\
			if (gsl_fcmp(_assert_eqfloat_aa, _assert_eqfloat_bb, prec) == 0) { 	\
				break;								\
			}									\
		}										\
		g_error("%s:%d: assertion failed; %e too close to %e", 				\
				__FILE__, __LINE__,						\
				_assert_eqfloat_aa, _assert_eqfloat_bb);			\
	} while (0)

#endif
