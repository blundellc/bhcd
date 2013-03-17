#ifndef	BUILD_H
#define	BUILD_H

#include <glib.h>
#include "params.h"
#include "tree.h"

Tree * build(Params * params, GList * labels);
void build_add_merges(Params * params, GSequence * merges, GPtrArray * trees, Tree * tkk);
void build_greedy(Params * params, GPtrArray * trees, GSequence * merges);
GSequence * build_init_merges(Params * params, GPtrArray * trees);
GPtrArray * build_init_trees(Params * params, GList * labels);
void build_remove_tree(GPtrArray * trees, guint ii);

#endif /*BUILD_H*/
