#ifndef UTIL_H
#define UTIL_H

#include <glib.h>
#include <math.h>
#include <gsl/gsl_math.h>


typedef union {
	gpointer ptr;
	gconstpointer const_ptr;
} UnionPtr;

typedef struct {
	gpointer fst;
	gpointer snd;
} Pair;

typedef void (*IOFunc)(gpointer, GIOChannel *);

Pair *pair_new(gpointer fst, gpointer snd);
void pair_free(Pair *pair);

gdouble log_add_exp(gdouble, gdouble);
gchar * num_to_string(guint);
gint cmp_quark(gconstpointer, gconstpointer);
void io_printf(GIOChannel *io, const gchar *fmt, ...);

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
		g_error("%s:%d: assertion failed; %e not close to %e", 				\
				__FILE__, __LINE__,						\
				_assert_eqfloat_aa, _assert_eqfloat_bb);			\
	} while (0)

#endif
