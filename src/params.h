#ifndef PARAMS_H
#define PARAMS_H
#include <glib.h>
#include "dataset.h"


typedef struct {
	/* private: */
	guint		ref_count;
	/* public: */
	Dataset *	dataset;
	gdouble		gamma;
	gdouble		loggamma; /* really log(1-gamma) */
	gdouble		alpha;
	gdouble		beta;
	gdouble		delta;
	gdouble		lambda;
} Params;

Params * params_new(Dataset * dataset, gdouble gamma, gdouble alpha, gdouble beta, gdouble delta, gdouble lambda);
Params * params_default(Dataset * dataset);
void params_ref(Params * params);
void params_unref(Params * params);

gdouble params_logprob_off(Params * params, gpointer pcounts);
gdouble params_logprob_on(Params * params, gpointer pcounts);

gpointer suff_stats_empty(Params * params);
gpointer suff_stats_from_label(Params * params, gpointer label);
gpointer suff_stats_off_lookup(Params * params, GList * srcs, GList * dsts);
void suff_stats_add(gpointer pdst, gpointer psrc);
gpointer suff_stats_copy(gpointer src);
void suff_stats_unref(gpointer ss);

#endif
