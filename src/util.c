#include <string.h>
#include <gsl/gsl_sf_log.h>
#include <gsl/gsl_sf_exp.h>
#include <gsl/gsl_errno.h>
#include <glib/gprintf.h>
#include "util.h"


Pair *pair_new(gpointer fst, gpointer snd) {
	Pair * pair;

	pair = g_new(Pair, 1);
	pair->fst = fst;
	pair->snd = snd;
	return pair;
}

void pair_free(Pair *pair) {
	g_free(pair);
}


gdouble log_1plus_exp(gdouble xx) {
	gsl_sf_result exp_xx;
	gsl_error_handler_t *old_handler = gsl_set_error_handler_off();
	int status = gsl_sf_exp_e(xx, &exp_xx);
	gsl_set_error_handler(old_handler);
	if ((status == GSL_EUNDRFLW || status == GSL_EDOM || status == GSL_ERANGE) && xx < 0) {
		return 0.0;
	} else if (status) {
		g_error("GSL error: %s", gsl_strerror(status));
	}
	return gsl_sf_log_1plusx(exp_xx.val);
}

gdouble log_add_exp(gdouble xx, gdouble yy) {
	gdouble diff = xx - yy;
	if (diff >= 0) {
		return xx + log_1plus_exp(-diff);
	} else if (diff < 0) {
		return yy + log_1plus_exp(diff);
	} else {
		/* inf/nan with same sign */
		return xx + yy;
	}
}

gchar * num_to_string(guint ii) {
	static const gchar base[] = "0123456789abcdefghijklmnopqrstuvwxyz";
	static const guint len = 36;
	GString *out;

	out = g_string_new("");

	do {
		g_string_append_c(out, base[ii % len]);
		ii /= len;
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

void io_printf(GIOChannel *io, const gchar *fmt, ...) {
	va_list ap;
	gchar *msg;
	GError *error;
	
	va_start(ap, fmt);
	g_vasprintf(&msg, fmt, ap);
	va_end(ap);

	error = NULL;
	g_io_channel_write_chars(io, msg, -1, NULL, &error);
	if (error != NULL) {
		g_error("g_io_channel_write_chars: %s", error->message);
	}
	g_free(msg);

}


void io_stdout(IOFunc func, gpointer user_data) {
        GIOChannel *io;

	io = g_io_channel_unix_new(1);
	func(user_data, io);
	g_io_channel_unref(io);
}

void io_file(const gchar *fname, IOFunc func, gpointer user_data) {
        GIOChannel *io;
        GError *error;
        error = NULL;
        io = g_io_channel_new_file(fname, "w", &error);
        if (error != NULL) {
                g_error("open `%s': %s", fname, error->message);
        }
        func(user_data, io);
        g_io_channel_shutdown(io, TRUE, &error);
        if (error != NULL) {
                g_error("shutdown `%s': %s", fname, error->message);
        }
        g_io_channel_unref(io);
}

gchar * strip_quotes(gchar *str) {
	guint len;

	len = strlen(str);
	if (len > 1 && str[0] == '"' && str[len-1] == '"') {
		g_memmove(str, &str[1], len-1);
		str[len-2] = '\0';
	}
	return str;
}

GList * list_new_full(gpointer last, ...) {
	va_list ap;
	GList * list;

	list = NULL;
	va_start(ap, last);
	for (gpointer elem = va_arg(ap, gpointer);
	     elem != last;
	     elem = va_arg(ap, gpointer)) {

		list = g_list_prepend(list, elem);
	}
	va_end(ap);
	list = g_list_reverse(list);
	return list;
}


void list_assert_sorted(GList * list, GCompareFunc cmp) {
	GList *prev;

	prev = list;
	for (list = g_list_next(list); prev != NULL && list != NULL; list = g_list_next(list)) {
		g_assert(cmp(prev->data, list->data) <= 0);
		prev = list;
	}
}

guint list_hash(GList * list, GHashFunc hash_func) {
	guint hash;

	hash = 0;
	for (; list != NULL; list = g_list_next(list)) {
		hash ^= hash_func(list->data);
	}
	return hash;
}

gboolean list_equal(GList *aa, GList *bb, GEqualFunc equal) {
	for (; aa != NULL && bb != NULL; aa = g_list_next(aa), bb = g_list_next(bb)) {
		if (!equal(aa->data, bb->data))
			return FALSE;
	}
	return aa == bb;
}


void list_labelset_print(GList * list) {
	for (GList * pp = list; pp != NULL; pp = g_list_next(pp)) {
		g_print("{ ");
		labelset_print(pp->data);
		g_print("}");
		if (g_list_next(pp) != NULL) {
			g_print(", ");
		}
	}
}
