#ifndef UTIL_H
#define UTIL_H

#include <glib.h>
#include <math.h>
#include <gsl/gsl_math.h>
#include "labelset.h"

#define	EQFLOAT_DEFAULT_PREC	1e-4
#define	assert_eqfloat(expa, expb, prec)							\
	do {											\
		gdouble _assert_eqfloat_aa = (expa);						\
		gdouble _assert_eqfloat_bb = (expb);						\
		if (fabs(_assert_eqfloat_aa) < prec ||						\
		    fabs(_assert_eqfloat_bb) < prec) {						\
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

#define	assert_lefloat(expa, expb, prec)							\
	do {											\
		gdouble _assert_eqfloat_aa = (expa);						\
		gdouble _assert_eqfloat_bb = (expb);						\
		if (fabs(_assert_eqfloat_aa) < prec ||						\
		    fabs(_assert_eqfloat_bb) < prec) {						\
			if (_assert_eqfloat_aa-_assert_eqfloat_bb <= prec) {			\
				break;								\
			}									\
		} else {									\
			if (gsl_fcmp(_assert_eqfloat_aa, _assert_eqfloat_bb, prec) <= 0) { 	\
				break;								\
			}									\
		}										\
		g_error("%s:%d: assertion failed; %e not close to less or equal %e", 		\
				__FILE__, __LINE__,						\
				_assert_eqfloat_aa, _assert_eqfloat_bb);			\
	} while (0)

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
void io_stdout(IOFunc func, gpointer user_data);
void io_writefile(const gchar *fname, IOFunc func, gpointer user_data);

GList * list_new_full(gpointer last, ...);
#define list_new(...)   list_new_full(NULL, __VA_ARGS__, NULL)
void list_assert_sorted(GList * list, GCompareFunc cmp);
gboolean list_equal(GList *aa, GList *bb, GEqualFunc equal);
guint list_hash(GList * list, GHashFunc hash_func);

void list_labelset_print(GList *);
gchar * strip_quotes(gchar *str);

static inline guint32 pop_count(guint64 xx) {
	/* simple divide and conquer (hd) */
	xx = (xx & 0x5555555555555555ULL) + ((xx >>  1) & 0x5555555555555555ULL);
	xx = (xx & 0x3333333333333333ULL) + ((xx >>  2) & 0x3333333333333333ULL);
	xx = (xx & 0x0f0f0f0f0f0f0f0fULL) + ((xx >>  4) & 0x0f0f0f0f0f0f0f0fULL);
	xx = (xx & 0x00ff00ff00ff00ffULL) + ((xx >>  8) & 0x00ff00ff00ff00ffULL);
	xx = (xx & 0x0000ffff0000ffffULL) + ((xx >> 16) & 0x0000ffff0000ffffULL);
	xx = (xx & 0x00000000ffffffffULL) + ((xx >> 32) & 0x00000000ffffffffULL);
	return xx;
}

#endif  /*UTIL_H*/
