#ifndef UTIL_H
#define UTIL_H

#include <glib.h>

typedef union {
	gpointer ptr;
	gconstpointer const_ptr;
} UnionPtr;

gdouble log_add_exp(gdouble, gdouble);
gchar * num_to_string(guint);
gint cmp_quark(gconstpointer, gconstpointer);

#endif
