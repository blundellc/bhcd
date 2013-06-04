#ifndef	MINHEAP_H
#define	MINHEAP_H

struct MinHeap_t;
typedef struct MinHeap_t MinHeap;
typedef struct MinHeapIter_t {
	/* private: */
	MinHeap *	heap;
	guint		index;
} MinHeapIter;

typedef gint (*MinHeapCompare)(gconstpointer, gconstpointer);
typedef void (*MinHeapFree)(gpointer);

MinHeap * minheap_new(MinHeapCompare cmp, MinHeapFree elem_free);
void minheap_free(MinHeap *);

guint minheap_size(MinHeap *);

void minheap_iter_init(MinHeap *, MinHeapIter *);
gboolean minheap_iter_next(MinHeapIter *, gpointer *);

void minheap_enq(MinHeap *, gpointer);
gpointer minheap_deq(MinHeap *);

#endif /*MINHEAP_H*/
