#ifndef	BUILD_H
#define	BUILD_H

#include <glib.h>
#include "params.h"
#include "tree.h"

Tree * build(GRand *, Params * params);
Tree * build_repeat(GRand *, Params * params, guint num_repeats);

#endif /*BUILD_H*/
