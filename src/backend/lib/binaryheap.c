/*-------------------------------------------------------------------------
 *
 * binaryheap.c
 *	  A simple binary heap implementation
 *
 * Portions Copyright (c) 2012-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/lib/binaryheap.c
 *
 *-------------------------------------------------------------------------
 */
/*-------------------------------------------------------------------------
 *
 * binaryheap.c
 *	  一个简单的二叉堆实现
 *
 * Portions Copyright (c) 2012-2019, PostgreSQL Global Development Group
 *
 * IDENTIFICATION
 *	  src/backend/lib/binaryheap.c
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include <math.h>

#include "lib/binaryheap.h"

static void sift_down(binaryheap *heap, int node_off);
static void sift_up(binaryheap *heap, int node_off);
static inline void swap_nodes(binaryheap *heap, int a, int b);

/*
 * binaryheap_allocate
 *
 * Returns a pointer to a newly-allocated heap that has the capacity to
 * store the given number of nodes, with the heap property defined by
 * the given comparator function, which will be invoked with the additional
 * argument specified by 'arg'.
 */
/*
 * binaryheap_allocate
 *
 * 返回一个指向新分配堆的指针，这个堆能够保存给定数量的节点。堆属性通过给定的比较函数来定义，
 * 调用这个函数时，会使用'arg'指定的额外参数。
 */
binaryheap *
binaryheap_allocate(int capacity, binaryheap_comparator compare, void *arg)
{
	int			sz;
	binaryheap *heap;

	sz = offsetof(binaryheap, bh_nodes) + sizeof(Datum) * capacity;
	heap = (binaryheap *) palloc(sz);
	heap->bh_space = capacity;
	heap->bh_compare = compare;
	heap->bh_arg = arg;

	heap->bh_size = 0;
	heap->bh_has_heap_property = true;

	return heap;
}

/*
 * binaryheap_reset
 *
 * Resets the heap to an empty state, losing its data content but not the
 * parameters passed at allocation.
 */
/*
 * binaryheap_reset
 *
 * 将堆重设为空状态，移除数据但保留分配时传入的参数。
 */
void
binaryheap_reset(binaryheap *heap)
{
	heap->bh_size = 0;
	heap->bh_has_heap_property = true;
}

/*
 * binaryheap_free
 *
 * Releases memory used by the given binaryheap.
 */
/*
 * binaryheap_free
 *
 * 释放给定二叉堆使用的内存。
 */
void
binaryheap_free(binaryheap *heap)
{
	pfree(heap);
}

/*
 * These utility functions return the offset of the left child, right
 * child, and parent of the node at the given index, respectively.
 *
 * The heap is represented as an array of nodes, with the root node
 * stored at index 0. The left child of node i is at index 2*i+1, and
 * the right child at 2*i+2. The parent of node i is at index (i-1)/2.
 */
/*
 * 这些工具函数分别返回给定索引号的节点的左子节点、右子节点、父节点的偏移量。
 *
 * 堆表示为节点的数组，根节点保存在索引0中。节点的左子节点索引为2*i+1，右子节点索引为
 * 2*i+2。节点i的父节点索引为(i-1)/2。
 */

static inline int
left_offset(int i)
{
	return 2 * i + 1;
}

static inline int
right_offset(int i)
{
	return 2 * i + 2;
}

static inline int
parent_offset(int i)
{
	return (i - 1) / 2;
}

/*
 * binaryheap_add_unordered
 *
 * Adds the given datum to the end of the heap's list of nodes in O(1) without
 * preserving the heap property. This is a convenience to add elements quickly
 * to a new heap. To obtain a valid heap, one must call binaryheap_build()
 * afterwards.
 */
/*
 * binaryheap_add_unordered
 *
 * 将给定的数据添加到堆的节点序列的末尾，时间复杂度为O(1)，不保持堆的属性。这便于将元素快速地添加到
 * 新堆中。为了获得一个有效的堆，必须在这之后调用binaryheap_build()。
 */
void
binaryheap_add_unordered(binaryheap *heap, Datum d)
{
	if (heap->bh_size >= heap->bh_space)
		elog(ERROR, "out of binary heap slots");
	heap->bh_has_heap_property = false;
	heap->bh_nodes[heap->bh_size] = d;
	heap->bh_size++;
}

/*
 * binaryheap_build
 *
 * Assembles a valid heap in O(n) from the nodes added by
 * binaryheap_add_unordered(). Not needed otherwise.
 */
/*
 * binaryheap_build
 *
 * 将通过binaryheap_add_unordered()函数添加的节点组合成一个有效的堆，
 * 时间复杂度为O(n)。否则的话不需要调用。
 */
void
binaryheap_build(binaryheap *heap)
{
	int			i;

	for (i = parent_offset(heap->bh_size - 1); i >= 0; i--)
		sift_down(heap, i);
	heap->bh_has_heap_property = true;
}

/*
 * binaryheap_add
 *
 * Adds the given datum to the heap in O(log n) time, while preserving
 * the heap property.
 */
/*
 * binaryheap_add
 *
 * 将给定的数据添加到堆中，时间复杂度为O(log n)，保持堆的属性。
 */
void
binaryheap_add(binaryheap *heap, Datum d)
{
	if (heap->bh_size >= heap->bh_space)
		elog(ERROR, "out of binary heap slots");
	heap->bh_nodes[heap->bh_size] = d;
	heap->bh_size++;
	sift_up(heap, heap->bh_size - 1);
}

/*
 * binaryheap_first
 *
 * Returns a pointer to the first (root, topmost) node in the heap
 * without modifying the heap. The caller must ensure that this
 * routine is not used on an empty heap. Always O(1).
 */
/*
 * binaryheap_first
 *
 * 返回堆中第一个（根、最顶层）节点的指针，不修改这个堆。调用者必须保证调用这个函数的
 * 时候，堆不为空。时间复杂度为O(1)。
 */
Datum
binaryheap_first(binaryheap *heap)
{
	Assert(!binaryheap_empty(heap) && heap->bh_has_heap_property);
	return heap->bh_nodes[0];
}

/*
 * binaryheap_remove_first
 *
 * Removes the first (root, topmost) node in the heap and returns a
 * pointer to it after rebalancing the heap. The caller must ensure
 * that this routine is not used on an empty heap. O(log n) worst
 * case.
 */
/*
 * binaryheap_remove_first
 *
 * 移除堆的第一个（根、最顶层）节点，然后在重平衡这个堆之后，返回指向被移除的节点的
 * 指针。调用者必须保证调用这个函数的时候，堆不为空。最坏情况下，时间复杂度为O(log n)。
 */
Datum
binaryheap_remove_first(binaryheap *heap)
{
	Assert(!binaryheap_empty(heap) && heap->bh_has_heap_property);

	if (heap->bh_size == 1)
	{
		heap->bh_size--;
		return heap->bh_nodes[0];
	}

	/*
	 * Swap the root and last nodes, decrease the size of the heap (i.e.
	 * remove the former root node) and sift the new root node down to its
	 * correct position.
	 */
	/*
	 * 交换根节点和最后一个节点，将堆的大小减1（也就是移除先前的那个根节点）。然后将新的根节点
	 * 下推至它的新位置。
	 */
	swap_nodes(heap, 0, heap->bh_size - 1);
	heap->bh_size--;
	sift_down(heap, 0);

	return heap->bh_nodes[heap->bh_size];
}

/*
 * binaryheap_replace_first
 *
 * Replace the topmost element of a non-empty heap, preserving the heap
 * property.  O(1) in the best case, or O(log n) if it must fall back to
 * sifting the new node down.
 */
/*
 * binaryheap_replace_first
 *
 * 替换一个非空堆的最顶层元素，且保持堆属性。最好情况下的时间复杂度为O(1)。如果必须将
 * 新的节点下推，则时间复杂度为O(log n)。
 */
void
binaryheap_replace_first(binaryheap *heap, Datum d)
{
	Assert(!binaryheap_empty(heap) && heap->bh_has_heap_property);

	heap->bh_nodes[0] = d;

	if (heap->bh_size > 1)
		sift_down(heap, 0);
}

/*
 * Swap the contents of two nodes.
 */
/*
 * 交换两个节点的内容。
 */
static inline void
swap_nodes(binaryheap *heap, int a, int b)
{
	Datum		swap;

	swap = heap->bh_nodes[a];
	heap->bh_nodes[a] = heap->bh_nodes[b];
	heap->bh_nodes[b] = swap;
}

/*
 * Sift a node up to the highest position it can hold according to the
 * comparator.
 */
/*
 * 基于比较器，将一个节点上移至它所能到达的最高位置。
 */
static void
sift_up(binaryheap *heap, int node_off)
{
	while (node_off != 0)
	{
		int			cmp;
		int			parent_off;

		/*
		 * If this node is smaller than its parent, the heap condition is
		 * satisfied, and we're done.
		 */
		/*
		 * 如果当前节点小于它的父节点，那么堆的条件已经满足，我们完成了上移操作。
		 */
		parent_off = parent_offset(node_off);
		cmp = heap->bh_compare(heap->bh_nodes[node_off],
							   heap->bh_nodes[parent_off],
							   heap->bh_arg);
		if (cmp <= 0)
			break;

		/*
		 * Otherwise, swap the node and its parent and go on to check the
		 * node's new parent.
		 */
		/*
		 * 否则，交换当前节点和其父节点，继续检查父节点。
		 */
		swap_nodes(heap, node_off, parent_off);
		node_off = parent_off;
	}
}

/*
 * Sift a node down from its current position to satisfy the heap
 * property.
 */
/*
 * 将一个节点从当前位置下推以满足堆属性。
 */
static void
sift_down(binaryheap *heap, int node_off)
{
	while (true)
	{
		int			left_off = left_offset(node_off);
		int			right_off = right_offset(node_off);
		int			swap_off = 0;

		/* Is the left child larger than the parent? */
		/* 左子节点大于父节点？ */
		if (left_off < heap->bh_size &&
			heap->bh_compare(heap->bh_nodes[node_off],
							 heap->bh_nodes[left_off],
							 heap->bh_arg) < 0)
			swap_off = left_off;

		/* Is the right child larger than the parent? */
		/* 右子节点大于父节点？ */
		if (right_off < heap->bh_size &&
			heap->bh_compare(heap->bh_nodes[node_off],
							 heap->bh_nodes[right_off],
							 heap->bh_arg) < 0)
		{
			/* swap with the larger child */
			/* 需要跟更大的那个子节点交换 */
			if (!swap_off ||
				heap->bh_compare(heap->bh_nodes[left_off],
								 heap->bh_nodes[right_off],
								 heap->bh_arg) < 0)
				swap_off = right_off;
		}

		/*
		 * If we didn't find anything to swap, the heap condition is
		 * satisfied, and we're done.
		 */
		/*
		 * 没发现需要交换的节点，堆的条件已经满足，我们完成了下推操作。
		 */
		if (!swap_off)
			break;

		/*
		 * Otherwise, swap the node with the child that violates the heap
		 * property; then go on to check its children.
		 */
		/*
		 * 否则，交换当前节点和其违反了堆属性的子节点；然后继续检查子节点。
		 */
		swap_nodes(heap, swap_off, node_off);
		node_off = swap_off;
	}
}
