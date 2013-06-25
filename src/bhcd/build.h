#ifndef	BUILD_H
#define	BUILD_H

#include <glib.h>
#include "params.h"
#include "tree.h"


struct Build_t;
typedef struct Build_t Build;

Build * build_new(GRand *rng, Params * params, guint num_restarts, gboolean sparse);
void build_free(Build *);
void build_once(Build *build);
void build_run(Build * build);
Tree * build_get_best_tree(Build * build);
void build_set_verbose(Build * build, gboolean value);

#endif /*BUILD_H*/
