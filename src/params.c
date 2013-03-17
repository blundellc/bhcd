#include <gsl/gsl_sf_log.h>
#include <gsl/gsl_sf_gamma.h>
#include "counts.h"
#include "params.h"

Params * params_new(Dataset * dataset, gdouble gamma, gdouble alpha, gdouble beta, gdouble delta, gdouble lambda) {
	Params * params = g_new(Params, 1);
	params->ref_count = 1;
	params->dataset = dataset;
	dataset_ref(dataset);
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
		g_free(params);
	} else {
		params->ref_count--;
	}
}

gdouble params_logprob_on(Params * params, gpointer pcounts) {
	Counts * counts = pcounts;
	gdouble a1 = params->alpha + counts->num_ones;
	gdouble b0 = params->beta  + counts->num_total - counts->num_ones;
	return gsl_sf_lnbeta(a1, b0) - gsl_sf_lnbeta(params->alpha, params->beta);
}

gdouble params_logprob_off(Params * params, gpointer pcounts) {
	Counts * counts = pcounts;
	gdouble a1 = params->delta + counts->num_ones;
	gdouble b0 = params->lambda  + counts->num_total - counts->num_ones;
	return gsl_sf_lnbeta(a1, b0) - gsl_sf_lnbeta(params->delta, params->lambda);
}

gpointer suff_stats_from_label(Params * params, gpointer label) {
	gboolean missing;
	gboolean value;

	value = dataset_get(params->dataset, label, label, &missing);
	if (missing) {
		return counts_new(0, 0);
	}
	return counts_new(value, 1);
}

gpointer suff_stats_copy(gpointer src) {
	return counts_copy(src);
}

gpointer suff_stats_empty(Params * params) {
	return counts_new(0, 0);
}

void suff_stats_add(gpointer pdst, gpointer psrc) {
	Counts * dst = pdst;
	Counts * src = psrc;
	counts_add(dst, src);
}


gpointer suff_stats_off_lookup(Params * params, GList * srcs, GList * dsts) {
	Counts * counts;
	GList * src;
	GList * dst;
	gboolean missing;
	gboolean value;

	counts = counts_new(0, 0);

	for (src = srcs; src != NULL; src = g_list_next(src)) {
		for (dst = g_list_first(dsts); dst != NULL; dst = g_list_next(dst)) {
			value = dataset_get(params->dataset, src->data, dst->data, &missing);
			if (!missing) {
				counts->num_ones += value;
				counts->num_total++;
			}
		}
	}
	return counts;
}

void suff_stats_unref(gpointer ss) {
	counts_unref(ss);
}


