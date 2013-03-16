#include <stdio.h>
#include <glib.h>



typedef struct {
	guint       refcnt;
	GPtrArray * children;
} Tree;

void tree_unref(Tree *);

Tree * leaf_new(gpointer datum) {
	Tree * leaf = g_new(Tree, 1);
	leaf->refcnt = 1;
	leaf->children = NULL;
	return leaf;
}

Tree * branch_new() {
	Tree * branch = g_new(Tree, 1);
	branch->refcnt = 1;
	branch->children = g_ptr_array_new_with_free_func((GDestroyNotify)tree_unref);
	return branch;
}

gboolean tree_is_leaf(Tree * tree) {
	return tree->children == NULL;
}

void tree_ref(Tree * tree) {
	tree->refcnt++;
}

void tree_unref(Tree * tree) {
	if (tree->refcnt <= 1) {
		if (!tree_is_leaf(tree)) {
			g_ptr_array_unref(tree->children);
		}

		g_free(tree);
	} else {
		tree->refcnt--;
	}
}

void branch_add_child(Tree * branch, Tree * child) {
	g_assert(!tree_is_leaf(branch));
	tree_ref(child);
	g_ptr_array_add(branch->children, child);
}

typedef struct {
	guint ii;
	guint jj;
	Tree * tree;
	gdouble score;
} Merge;

Merge * merge_new(guint ii, Tree * aa, guint jj, Tree * bb) {
	Merge * merge = g_new(Merge, 1);
	merge->ii = ii;
	merge->jj = jj;
	merge->tree = branch_new();
	branch_add_child(merge->tree, aa);
	branch_add_child(merge->tree, bb);
	merge->score = -(gdouble)(ii * jj);
	return merge;
}

void merge_free(Merge * merge) {
	tree_unref(merge->tree);
	g_free(merge);
}


gint cmp_score(gconstpointer paa, gconstpointer pbb, gpointer userdata) {
	Merge * aa = (Merge *)paa;
	Merge * bb = (Merge *)pbb;
	return aa->score - bb->score;
}


void build(GPtrArray * data) {
	GPtrArray * trees = g_ptr_array_new();
	guint live_trees = 0;

	for (guint ii = 0; ii < data->len; ii++) {
		Tree * leaf = leaf_new(g_ptr_array_index(data, ii));
		g_ptr_array_add(trees, leaf);
		live_trees++;
	}

	GSequence * merges = g_sequence_new(NULL);

	for (guint ii = 0; ii < trees->len; ii++) {
		Tree * aa = g_ptr_array_index(trees, ii);

		for (guint jj = ii + 1; jj < trees->len; jj++) {
			Tree * bb = g_ptr_array_index(trees, jj);
			Merge * new_merge = merge_new(ii, aa, jj, bb);
			g_sequence_insert_sorted(merges, new_merge, cmp_score, NULL);
		}
	}

	while (live_trees > 1) {
		g_assert(g_sequence_get_length(merges) > 0);
		GSequenceIter * head = g_sequence_get_begin_iter(merges);
		Merge * cur = (Merge *)g_sequence_get(head);
		g_sequence_remove(head);

		if (g_ptr_array_index(trees, cur->ii) == NULL ||
		    g_ptr_array_index(trees, cur->jj) == NULL) {
			goto again;
		}

		gpointer * tii = &g_ptr_array_index(trees, cur->ii);
		tree_unref(*tii);
		*tii = NULL;
		gpointer * tjj = &g_ptr_array_index(trees, cur->jj);
		tree_unref(*tjj);
		*tjj = NULL;
		Tree * tkk = cur->tree;
		int kk = trees->len;
		live_trees--;

		for (guint ll = 0; ll < trees->len; ll++) {
			if (g_ptr_array_index(trees, ll) == NULL) {
				continue;
			}

			Tree * tll = g_ptr_array_index(trees, ll);
			Merge * new_merge = merge_new(kk, tkk, ll, tll);
			g_sequence_insert_sorted(merges, new_merge, cmp_score, NULL);
		}

		g_ptr_array_add(trees, tkk);
		tree_ref(tkk);
again:
		merge_free(cur);
	}

	Tree * root = (Tree *)g_ptr_array_index(trees, trees->len - 1);
	g_assert(root != NULL);
	tree_unref(root);
	g_ptr_array_free(trees, TRUE);
	g_sequence_free(merges);
}

int main(int argc, char * argv[]) {
	GPtrArray * data = g_ptr_array_new();

	for (guint ii = 0; ii < 10; ii++) {
		g_ptr_array_add(data, GINT_TO_POINTER(ii));
	}

	build(data);
	g_ptr_array_unref(data);
	return 0;
}

