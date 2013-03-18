#ifndef	TREE_H
#define	TREE_H

#include "params.h"
#include <glib.h>

struct Tree_t;
typedef struct Tree_t Tree;

Tree * leaf_new(Params * params, gpointer label);
Tree * branch_new(Params * params);
Tree * tree_copy(Tree * orig);
void tree_ref(Tree * tree);
void tree_unref(Tree * tree);

void tree_assert(Tree * tree);

void branch_add_child(Tree * branch, Tree * child);

gboolean tree_is_leaf(Tree * tree);
GList * tree_get_labels(Tree * tree);
Params * tree_get_params(Tree * tree);
gdouble tree_get_logprob(Tree *tree);
guint tree_num_intern(Tree * tree);
guint tree_num_leaves(Tree * tree);
void tree_println(Tree * tree, const gchar *prefix);
void tree_tostring(Tree * tree, GString *str);
void tree_struct_print(Tree * tree, GString *str);

#endif /*TREE_H*/
