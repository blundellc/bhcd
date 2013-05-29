#include <gsl/gsl_sf_log.h>
#include <gsl/gsl_sf_gamma.h>
#include "counts.h"
#include "params.h"

// allocate at most ~1GB for the cache.
#define	MAX_CACHE_SIZE	11585

typedef void (*ParamsSetFunc)(Params *, gdouble);

static void params_set_gamma(Params *, gdouble);
static void params_set_alpha(Params *, gdouble);
static void params_set_beta(Params *, gdouble);
static void params_set_delta(Params *, gdouble);
static void params_set_lambda(Params *, gdouble);
static Params * params_copy(const Params * );

typedef struct {
	gdouble lower;
	gdouble upper;
	ParamsSetFunc set_value;
} ParamsSpec;
static const ParamsSpec params_spec_gamma  = { .lower = 0.0, .upper = 1.0, .set_value = params_set_gamma  };
static const ParamsSpec params_spec_alpha  = { .lower = 0.0, .upper = 1.0, .set_value = params_set_alpha  };
static const ParamsSpec params_spec_beta   = { .lower = 0.0, .upper = 1.0, .set_value = params_set_beta   };
static const ParamsSpec params_spec_delta  = { .lower = 0.0, .upper = 1.0, .set_value = params_set_delta  };
static const ParamsSpec params_spec_lambda = { .lower = 0.0, .upper = 1.0, .set_value = params_set_lambda };
static const guint max_cached_count = 100;


Params * params_new(Dataset * dataset, gdouble gamma, gdouble alpha, gdouble beta, gdouble delta, gdouble lambda) {
	guint cache_size;
	Params * params = g_new(Params, 1);
	params->ref_count = 1;

	params->dataset = dataset;
	dataset_ref(dataset);
	params->sscache = sscache_new(dataset);

	params->binary_only = FALSE;

	params_set_gamma(params, gamma);

	cache_size = dataset_num_labels(dataset);
	if (cache_size > MAX_CACHE_SIZE) {
		cache_size = MAX_CACHE_SIZE;
	}
	/* SEE ALSO: params_set_alpha, params_set_beta */
	params->alpha = alpha;
	params->beta = beta;
	params->logbeta_alpha_beta = lnbetacache_new(alpha, beta, cache_size);

	/* SEE ALSO: params_set_delta, params_set_lambda */
	params->delta = delta;
	params->lambda = lambda;
	params->logbeta_delta_lambda = lnbetacache_new(delta, lambda, cache_size);

	return params;
}

static Params * params_copy(const Params * orig) {
	Params * params;

	params = params_new(orig->dataset, orig->gamma,
			orig->alpha, orig->beta,
			orig->delta, orig->lambda);
	params->binary_only = orig->binary_only;
	return params;
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

Params * params_default(Dataset * dataset) {
	return params_new(dataset,
			  0.4,
			  1.0, 0.2,
			  0.2, 1.0);
}

static void params_set_gamma(Params * params, gdouble gamma) {
	params->gamma = gamma;
	params->loggamma = gsl_sf_log(1.0 - gamma);
}

static void params_set_alpha(Params * params, gdouble alpha) {
	params->alpha = alpha;
	params->logbeta_alpha_beta = lnbetacache_new(params->alpha, params->beta, max_cached_count);
}

static void params_set_beta(Params * params, gdouble beta) {
	params->beta = beta;
	params->logbeta_alpha_beta = lnbetacache_new(params->alpha, params->beta, max_cached_count);
}

static void params_set_delta(Params * params, gdouble delta) {
	params->delta = delta;
	params->logbeta_delta_lambda = lnbetacache_new(params->delta, params->lambda, max_cached_count);
}

static void params_set_lambda(Params * params, gdouble lambda) {
	params->lambda = lambda;
	params->logbeta_delta_lambda = lnbetacache_new(params->delta, params->lambda, max_cached_count);
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

gdouble params_logprob_offscore(Params * params, gpointer pcounts) {
	Counts * counts = pcounts;
	gdouble logprob;

	if (counts->num_total == 0) {
		return 0.0;
	}
	if (1) {
		// marginal score
		logprob = lnbetacache_get(params->logbeta_delta_lambda,
				counts->num_ones,
				counts->num_total - counts->num_ones) -
			  lnbetacache_get(params->logbeta_delta_lambda, 0, 0);
	} else {
		// independents
		gdouble logprob_zero;
		gdouble logprob_one;
		gdouble zz = lnbetacache_get(params->logbeta_delta_lambda, 0, 0);

		logprob_zero = lnbetacache_get(params->logbeta_delta_lambda, 0, 1) - zz;
		logprob_one = lnbetacache_get(params->logbeta_delta_lambda, 1, 0) - zz;
		logprob = logprob_one*counts->num_ones +
			  logprob_zero*(counts->num_total - counts->num_ones);
	}
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


static Params * params_sample_full(GRand * rng, const Params * orig, gdouble cur, const ParamsSpec * spec, ParamsProbFunc func, gpointer user_data) {
	Params * params;
	gdouble lower, upper;
	gdouble yy, prop;

	params = params_copy(orig);
	lower = spec->lower;
	upper = spec->upper;
	yy = gsl_sf_log(g_rand_double(rng)) + func(params, user_data);

	while (1) {
		prop = g_rand_double_range(rng, lower, upper);
		spec->set_value(params, prop);
		if (yy < func(params, user_data)) {
			goto out;
		}
		if (prop < cur) {
			lower = prop;
		} else {
			upper = prop;
		}
	}
out:
	return params;
}


Params * params_sample(GRand * rng, Params * params, ParamsProbFunc func, gpointer user_data) {
	Params * new_params;

	new_params = params;
	for (guint ministep = 0; ministep < 10; ministep++) {
		new_params = params_sample_full(rng, new_params, params->gamma,      &params_spec_gamma, func, user_data);
		new_params = params_sample_full(rng, new_params, new_params->alpha,  &params_spec_alpha, func, user_data);
		new_params = params_sample_full(rng, new_params, new_params->beta,   &params_spec_beta, func, user_data);
		new_params = params_sample_full(rng, new_params, new_params->delta,  &params_spec_delta, func, user_data);
		new_params = params_sample_full(rng, new_params, new_params->lambda, &params_spec_lambda, func, user_data);
	}
	return new_params;
}


