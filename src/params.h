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


gpointer suffstats_empty(Params * params);
gpointer suffstats_from_label(Params * params, gpointer label);
gpointer suffstats_copy(gpointer src);
void suffstats_unref(gpointer ss);

gdouble suffstats_logprob_off(gpointer, Params *);
gdouble suffstats_logprob_on(gpointer, Params * );
gpointer suffstats_off_lookup(Params * params, GList * srcs, GList * dsts);
void suffstats_add(gpointer pdst, gpointer psrc);

#endif
