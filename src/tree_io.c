#include "tree_io.h"
#include "util.h"


Tree * tree_io_load(const gchar *fname) {
	return NULL;
}

void tree_io_save(Tree *tree, const gchar *fname) {
	GIOChannel *io;
	GError *error;
	error = NULL;
	io = g_io_channel_new_file(fname, "w", &error);
	if (error != NULL) {
		g_error("open `%s': %s", fname, error->message);
	}
	tree_io_save_io(tree, io);
	g_io_channel_shutdown(io, TRUE, &error);
	if (error != NULL) {
		g_error("shutdown `%s': %s", fname, error->message);
	}
	g_io_channel_unref(io);
}

void tree_io_save_io(Tree *root, GIOChannel *io) {
	GQueue *qq;
	Dataset *dataset;
	gint next_index;

	io_printf(io, "tree [\n");
	qq = g_queue_new();
	next_index = -1;
	g_queue_push_head(qq, pair_new(GINT_TO_POINTER(next_index), root));
	dataset = tree_get_params(root)->dataset;
	next_index++;

	while (!g_queue_is_empty(qq)) {
		Pair *cur;
		Tree *tree;
		gint parent_index;

		cur = g_queue_pop_head(qq);
		parent_index = GPOINTER_TO_INT(cur->fst);
		tree = cur->snd;

		if (tree_is_leaf(tree)) {
			io_printf(io, "\tleaf [ parent %d label \"%s\" ]\n",
					parent_index,
					dataset_get_label_string(dataset, leaf_get_label(tree))
				);
		} else {
			GList * child;
			guint my_index;

			my_index = next_index;
			next_index++;

			if (parent_index == -1) {
				io_printf(io, "\troot [ id %d ]\n", my_index);
			} else {
				io_printf(io, "\tstem [ parent %d child %d ]\n", parent_index, my_index);
			}
			child = branch_get_children(tree);
			for (; child != NULL; child = g_list_next(child)) {
				g_queue_push_tail(qq, pair_new(GINT_TO_POINTER(my_index), child->data));
			}
		}
		pair_free(cur);
	}
	g_queue_free(qq);
	io_printf(io, "]\n");
}

