#include <glib.h>
#include "minheap.h"

struct MinHeap_t {
	GPtrArray * elems;
	guint num_elems;
	MinHeapCompare elem_cmp;
	MinHeapFree elem_free;
};
#define MINHEAP_PARENT(index)	(index == 0? 0: ((index-1)/2))
#define	MINHEAP_LEFT(index)	(2*index+1)
#define	MINHEAP_RIGHT(index)	(2*index+2)

static void minheap_append(MinHeap * heap, gpointer elem);
static void minheap_bubble_down(MinHeap * heap, guint index);
static void minheap_bubble_up(MinHeap * heap, guint index);


MinHeap * minheap_new(guint hint_size, MinHeapCompare elem_cmp, MinHeapFree elem_free) {
	MinHeap * heap;

	heap = g_new(MinHeap, 1);
	heap->elems = g_ptr_array_sized_new(hint_size);
	heap->num_elems = 0;
	heap->elem_cmp = elem_cmp;
	heap->elem_free = elem_free;
	return heap;
}

MinHeap * minheap_copy(MinHeap * orig, MinHeapCopy elem_copy, MinHeapFree elem_free) {
	MinHeap * heap;
	gpointer elem;

	heap = minheap_new(orig->num_elems, orig->elem_cmp, elem_free);
	g_ptr_array_set_size(heap->elems, orig->num_elems);
	for (guint ii = 0; ii < orig->num_elems; ii++) {
		elem = elem_copy(g_ptr_array_index(orig->elems, ii));
		g_ptr_array_index(heap->elems, ii) = elem;
	}
	heap->num_elems = orig->num_elems;
	return heap;
}

void minheap_free(MinHeap * heap) {
	if (heap->elem_free != NULL) {
		for (guint ii = 0; ii < heap->num_elems; ii++) {
			heap->elem_free(g_ptr_array_index(heap->elems, ii));
		}
	}
	g_free(g_ptr_array_free(heap->elems, TRUE));
	g_free(heap);
}

gpointer minheap_elem_no_copy(gpointer pp) {
	return pp;
}

void minheap_elem_no_free(gpointer pp) {
}


void minheap_print(MinHeap * heap) {
	guint index, left_index, right_index;
	gpointer left, right;

	for (index = 0; index < heap->num_elems; index++) {
		left_index = MINHEAP_LEFT(index);
		right_index = MINHEAP_RIGHT(index);
		left = (left_index < heap->num_elems? g_ptr_array_index(heap->elems, left_index): 0);
		right = (right_index < heap->num_elems? g_ptr_array_index(heap->elems, right_index): 0);
		g_print("%d: %ld, left(%d): %ld, right(%d): %ld\n",
			index, (unsigned long)g_ptr_array_index(heap->elems, index),
			left_index, (unsigned long)left,
			right_index, (unsigned long)right);
	}
}


guint minheap_size(MinHeap * heap) {
	return heap->num_elems;
}

void minheap_enq(MinHeap * heap, gpointer elem) {
	minheap_append(heap, elem);
	minheap_bubble_up(heap, heap->num_elems-1);
}

gpointer minheap_deq(MinHeap * heap) {
	gpointer root;

	g_assert(heap->num_elems > 0);

	root = g_ptr_array_index(heap->elems, 0);
	if (heap->num_elems > 1) {
		g_ptr_array_index(heap->elems, 0) = g_ptr_array_index(heap->elems, heap->num_elems-1);
		g_ptr_array_index(heap->elems, heap->num_elems-1) = NULL;
		heap->num_elems--;
		minheap_bubble_down(heap, 0);
	} else {
		g_ptr_array_index(heap->elems, 0) = NULL;
		heap->num_elems = 0;
	}
	return root;
}



static void minheap_append(MinHeap * heap, gpointer elem) {
	if (heap->num_elems < heap->elems->len) {
		g_ptr_array_index(heap->elems, heap->num_elems) = elem;
	} else {
		g_ptr_array_add(heap->elems, elem);
	}
	heap->num_elems++;
}

static void minheap_bubble_up(MinHeap * heap, guint index) {
	gpointer cur, parent;
	guint parent_index;

	while (index > 0) {
		cur = g_ptr_array_index(heap->elems, index);
		parent_index = MINHEAP_PARENT(index);
		parent = g_ptr_array_index(heap->elems, parent_index);
		if (heap->elem_cmp(cur, parent) >= 0) {
			return;
		}
		g_ptr_array_index(heap->elems, index) = parent;
		g_ptr_array_index(heap->elems, parent_index) = cur;
		index = parent_index;
	}
}

static void minheap_bubble_down(MinHeap * heap, guint index) {
	guint left_index, right_index, smallest_index;
	gpointer tmp;

	while (1) {
		left_index = MINHEAP_LEFT(index);
		right_index = MINHEAP_RIGHT(index);
		smallest_index = index;
		if (left_index < heap->num_elems &&
			heap->elem_cmp(
				g_ptr_array_index(heap->elems, left_index),
				g_ptr_array_index(heap->elems, smallest_index)
				) < 0) {
			smallest_index = left_index;
		}
		if (right_index < heap->num_elems &&
			heap->elem_cmp(
				g_ptr_array_index(heap->elems, right_index),
				g_ptr_array_index(heap->elems, smallest_index)
				) < 0) {
			smallest_index = right_index;
		}
		if (smallest_index == index) {
			break;
		}
		tmp = g_ptr_array_index(heap->elems, index);
		g_ptr_array_index(heap->elems, index) = g_ptr_array_index(heap->elems, smallest_index);
		g_ptr_array_index(heap->elems, smallest_index) = tmp;
		index = smallest_index;
	}
}


void minheap_iter_init(MinHeap * heap, MinHeapIter * iter) {
	iter->heap = heap;
	iter->index = 0;
}

gboolean minheap_iter_next(MinHeapIter * iter, gpointer * pelem) {
	if (iter->index >= iter->heap->num_elems) {
		return FALSE;
	}
	*pelem = g_ptr_array_index(iter->heap->elems, iter->index);
	iter->index++;
	return TRUE;
}

