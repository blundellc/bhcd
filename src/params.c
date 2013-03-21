#include <gsl/gsl_sf_log.h>
#include <gsl/gsl_sf_gamma.h>
#include "counts.h"
#include "params.h"


Params * params_new(Dataset * dataset, gdouble gamma, gdouble alpha, gdouble beta, gdouble delta, gdouble lambda) {
	Params * params = g_new(Params, 1);
	params->ref_count = 1;
	params->dataset = dataset;
	dataset_ref(dataset);
	params->sscache = sscache_new(dataset);
	params->gamma = gamma;
	params->loggamma = gsl_sf_log(1.0 - gamma);
	params->alpha = alpha;
	params->beta = beta;
	params->delta = delta;
	params->lambda = lambda;
	return params;
}

Params * params_default(Dataset * dataset) {
	return params_new(dataset,
			  0.4,
			  1.0, 0.2,
			  0.2, 1.0);
}

void params_ref(Params * params) {
	params->ref_count++;
}

void params_unref(Params * params) {
	if (params->ref_count <= 1) {
		dataset_unref(params->dataset);
		sscache_unref(params->sscache);
		g_free(params);
	} else {
		params->ref_count--;
	}
}

gdouble params_logprob_on(Params * params, gpointer pcounts) {
	Counts * counts = pcounts;
	gdouble a1, b0, logprob;

	if (counts->num_total == 0) {
		return 0.0;
	}
	a1 = params->alpha + counts->num_ones;
	b0 = params->beta  + counts->num_total - counts->num_ones;
	logprob = gsl_sf_lnbeta(a1, b0) - gsl_sf_lnbeta(params->alpha, params->beta);
	return logprob;
}

gdouble params_logprob_off(Params * params, gpointer pcounts) {
	Counts * counts = pcounts;
	gdouble d1, l0, logprob;

	if (counts->num_total == 0) {
		return 0.0;
	}
	d1 = params->delta + counts->num_ones;
	l0 = params->lambda  + counts->num_total - counts->num_ones;
	logprob = gsl_sf_lnbeta(d1, l0) - gsl_sf_lnbeta(params->delta, params->lambda);
	return logprob;
}
