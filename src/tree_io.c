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
	GQueue * qq;
	Dataset * dataset;
	Params * params;
	gint next_index;

	params = tree_get_params(root);
	dataset = params->dataset;

	io_printf(io, "data [ file \"%s\" ]\n", dataset_get_filename(dataset));
	/* IEEE754 numbers have at most 17 dp of precision. */
	io_printf(io, "params [ gamma %1.17e alpha %1.17e beta %1.17e delta %1.17e lambda %1.17e ]\n",
			params->gamma,
			params->alpha, params->beta,
			params->delta, params->lambda
			);
	io_printf(io, "fit [ logprob %1.17e ]\n", tree_get_logprob(root));
	io_printf(io, "tree [\n");

	qq = g_queue_new();
	next_index = -1;
	g_queue_push_head(qq, pair_new(GINT_TO_POINTER(next_index), root));
	next_index++;

	while (!g_queue_is_empty(qq)) {
		Pair *cur;
		Tree *tree;
		gint parent_index;

		cur = g_queue_pop_head(qq);
		parent_index = GPOINTER_TO_INT(cur->fst);
		tree = cur->snd;

		if (tree_is_leaf(tree)) {
			io_printf(io, "\tleaf [ logprob %1.17e logresp %1.17e parent %d label \"%s\" ]\n",
					tree_get_logprob(tree),
					tree_get_logresponse(tree),
					parent_index,
					dataset_label_to_string(dataset, leaf_get_label(tree))
				);
		} else {
			GList * child;
			guint my_index;

			my_index = next_index;
			next_index++;

			if (parent_index == -1) {
				io_printf(io, "\troot [ logprob %1.17e logresp %1.17e id %d ]\n",
						tree_get_logprob(tree),
						tree_get_logresponse(tree),
						my_index
					);
			} else {
				io_printf(io, "\tstem [ logprob %1.17e logresp %1.17e parent %d child %d ]\n",
						tree_get_logprob(tree),
						tree_get_logresponse(tree),
						parent_index,
						my_index
					);
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

