#ifndef PARAMS_H
#define PARAMS_H
#include <glib.h>
#include "dataset.h"
#include "sscache.h"


typedef struct {
	/* private: */
	guint		ref_count;
	/* public: */
	Dataset *	dataset;
	SSCache *	sscache;
	gdouble		gamma;
	gdouble		loggamma; /* really log(1-gamma) */
	gdouble		alpha;
	gdouble		beta;
	gdouble		delta;
	gdouble		lambda;
	gboolean	binary_only;
} Params;

Params * params_new(Dataset * dataset, gdouble gamma, gdouble alpha, gdouble beta, gdouble delta, gdouble lambda);
Params * params_default(Dataset * dataset);
void params_reset_cache(Params *);
void params_ref(Params * params);
void params_unref(Params * params);

gdouble params_logprob_off(Params *, gpointer);
gdouble params_logprob_on(Params *, gpointer);
gdouble params_logpred_off(Params *, gpointer, gboolean);
gdouble params_logpred_on(Params *, gpointer, gboolean);

#endif
