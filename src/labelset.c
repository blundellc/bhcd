#include <stdarg.h>
#include "labelset.h"

struct Labelset_t {
	guint ref_count;
	Bitset * bits;
	Dataset * dataset;
};

static const gboolean labelset_debug = FALSE;

Labelset * labelset_new_full(Dataset * dataset, ...) {
	Labelset * lset;
	va_list ap;
	guint max_label;
	gpointer label;

	lset = g_new(Labelset, 1);
	lset->ref_count = 1;
	lset->dataset = dataset;
	dataset_ref(dataset);
	label = dataset_get_max_label(dataset);
	max_label = GPOINTER_TO_INT(label);
	lset->bits = bitset_new(max_label+1);

	va_start(ap, dataset);
	for (label = va_arg(ap, gpointer); label != NULL; label = va_arg(ap, gpointer)) {
		labelset_add(lset, label);
	}
	va_end(ap);
	return lset;
}


Labelset * labelset_copy(Labelset *other) {
	Labelset * lset = labelset_new(other->dataset);
	bitset_unref(lset->bits);
	lset->bits = bitset_copy(other->bits);
	return lset;
}


void labelset_ref(Labelset * lset) {
	lset->ref_count++;
}

void labelset_unref(Labelset * lset) {
	if (lset->ref_count <= 1) {
		dataset_unref(lset->dataset);
		bitset_unref(lset->bits);
		g_free(lset);
	} else {
		lset->ref_count--;
	}
}

guint32 labelset_count(Labelset * lset) {
	return bitset_count(lset->bits);
}

gpointer labelset_any_label(Labelset * lset) {
	guint32 any_int = bitset_any(lset->bits);
	return GINT_TO_POINTER(any_int);
}

gboolean labelset_equal(Labelset *aa, Labelset *bb) {
	return bitset_equal(aa->bits, bb->bits);
}

guint labelset_hash(Labelset * lset) {
	return bitset_hash(lset->bits);
}

gint labelset_cmp(gconstpointer paa, gconstpointer pbb) {
	const Labelset * aa = paa;
	const Labelset * bb = pbb;
	return bitset_cmp(aa->bits, bb->bits);
}


void labelset_add(Labelset *lset, gconstpointer label) {
    bitset_set(lset->bits, GPOINTER_TO_INT(label));
}

void labelset_del(Labelset *lset, gconstpointer label) {
    bitset_clear(lset->bits, GPOINTER_TO_INT(label));
}

gboolean labelset_contains(Labelset *lset, gconstpointer label) {
    return bitset_contains(lset->bits, GPOINTER_TO_INT(label));
}

void labelset_union(Labelset *aa, Labelset *bb) {
	bitset_union(aa->bits, bb->bits);
}


void labelset_print(Labelset * lset) {
	GString * out;
	
	out = g_string_new("");
	labelset_tostring(lset, out);
	g_print("%s", out->str);
	g_string_free(out, TRUE);
}

static void labelset_tostring_append(gpointer pargs, guint32 labelint) {
	Pair *args = pargs;
	Dataset * dataset = args->fst;
	GString * out = args->snd;
	gpointer label = GINT_TO_POINTER(labelint);
	if (labelset_debug) {
		g_string_append_printf(out, "%s(%d) ",
				dataset_label_to_string(dataset, label),
				labelint);
	} else {
		g_string_append(out, dataset_label_to_string(dataset, label));
		g_string_append(out, " ");
	}
}

void labelset_tostring(Labelset * lset, GString * out) {
	Pair *args = pair_new(lset->dataset, out);
	bitset_foreach(lset->bits, labelset_tostring_append, args);
	pair_free(args);
	if (labelset_debug) {
		g_string_append_printf(out, "(h:%x)", labelset_hash(lset));
	}
}

gboolean labelset_disjoint(Labelset *aa, Labelset *bb) {
	return bitset_disjoint(aa->bits, bb->bits);
}

