#include <gsl/gsl_sf_log.h>
#include <gsl/gsl_sf_gamma.h>
#include "counts.h"
#include "params.h"

static const guint max_cached_count = 100;

Params * params_new(Dataset * dataset, gdouble gamma, gdouble alpha, gdouble beta, gdouble delta, gdouble lambda) {
	Params * params = g_new(Params, 1);
	params->ref_count = 1;

	params->dataset = dataset;
	dataset_ref(dataset);
	params->sscache = sscache_new(dataset);

	params->gamma = gamma;
	params->loggamma = gsl_sf_log(1.0 - gamma);
	params->binary_only = FALSE;

	params->alpha = alpha;
	params->beta = beta;
	params->logbeta_alpha_beta = lnbetacache_new(alpha, beta, max_cached_count);

	params->delta = delta;
	params->lambda = lambda;
	params->logbeta_delta_lambda = lnbetacache_new(delta, lambda, max_cached_count);

	return params;
}

Params * params_default(Dataset * dataset) {
	return params_new(dataset,
			  0.4,
			  1.0, 0.2,
			  0.2, 1.0);
}

void params_reset_cache(Params *params) {
	sscache_unref(params->sscache);
	params->sscache = sscache_new(params->dataset);
}

void params_ref(Params * params) {
	params->ref_count++;
}

void params_unref(Params * params) {
	if (params->ref_count <= 1) {
		dataset_unref(params->dataset);
		sscache_unref(params->sscache);
		lnbeta_cache_free(params->logbeta_alpha_beta);
		lnbeta_cache_free(params->logbeta_delta_lambda);
		g_free(params);
	} else {
		params->ref_count--;
	}
}

gdouble params_logprob_on(Params * params, gpointer pcounts) {
	Counts * counts = pcounts;
	gdouble logprob;

	if (counts->num_total == 0) {
		return 0.0;
	}
	logprob = lnbetacache_get(params->logbeta_alpha_beta,
			counts->num_ones,
			counts->num_total - counts->num_ones) -
		  lnbetacache_get(params->logbeta_alpha_beta, 0, 0);
	return logprob;
}

gdouble params_logprob_off(Params * params, gpointer pcounts) {
	Counts * counts = pcounts;
	gdouble logprob;

	if (counts->num_total == 0) {
		return 0.0;
	}
	logprob = lnbetacache_get(params->logbeta_delta_lambda,
			counts->num_ones,
			counts->num_total - counts->num_ones) -
		  lnbetacache_get(params->logbeta_delta_lambda, 0, 0);
	return logprob;
}


gdouble params_logpred_on(Params * params, gpointer pcounts, gboolean value) {
	Counts * counts = pcounts;
	gdouble logpred;

	logpred = lnbetacache_get(params->logbeta_alpha_beta,
			counts->num_ones + value,
			counts->num_total - counts->num_ones + 1-value) -
		  lnbetacache_get(params->logbeta_alpha_beta,
			counts->num_ones,
			counts->num_total - counts->num_ones);
	return logpred;
}

gdouble params_logpred_off(Params * params, gpointer pcounts, gboolean value) {
	Counts * counts = pcounts;
	gdouble logpred;

	logpred = lnbetacache_get(params->logbeta_delta_lambda,
			counts->num_ones + value,
			counts->num_total - counts->num_ones + 1-value) -
		  lnbetacache_get(params->logbeta_delta_lambda,
			counts->num_ones,
			counts->num_total - counts->num_ones);
	return logpred;
}
