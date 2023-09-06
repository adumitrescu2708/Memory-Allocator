// SPDX-License-Identifier: BSD-3-Clause
/**
 * @name Dumitrescu Alexandra
 * @date 10.04.2023
 * @for  Operating Systems - Memory Allocator
 */
#include <unistd.h>
#include "osmem.h"
#include "alignment_utils.h"
#include "allocator.h"
#include "../utils/printf.h"

/**
 * @param size - size of new payload
 *	| First align the memory, then check if there is a possible best
 *	| fit for it. If there is, then return the address of the block's
 *	| payload, if not add the block to the list.
 */
void *os_malloc(size_t size)
{
	if (size == 0)
		return NULL;
	size_t block_size = (size_t) align((size));
	struct block_meta *best_fit =  (struct block_meta *)find_best_fit(block_size);

	if (best_fit == NULL)
		return add_new_block(block_size);

	return (void *)((char *)best_fit + get_block_meta_size());
}

/**
 * @param adr - beginning address of a payload
 *	| First searches for the corresponding block in the list,
 *	| then splits into 2 cases: (A) if the block is alloced, then
 *	| just sets the status to free, (B) if the block is mapped
 *	| frees the memory and directly remove the node from list.
 */
void os_free(void *ptr)
{
	if (ptr != NULL) {
		struct block_meta *block = (struct block_meta *)find_block((char *)ptr - get_block_meta_size());

		// (A)
		if (block != NULL && block->status == STATUS_ALLOC) {
			block->status = STATUS_FREE;
			ptr = NULL;
		}

		// (B)
		if (block != NULL && block->status == STATUS_MAPPED)
			delete_node(block);
	}
}

/**
 * @param nmemb - number of elements
 * @param size - size of each element
 *	| We apply the same logic from malloc(), what differs is that we also
 *	| initialise the chunk of memory with 0 (A) and that for deciding wether
 *	| a block is mapped or alloced we use page_size instead of MMAP_TRESHOLD.
 *	| (B).
 */
void *os_calloc(size_t nmemb, size_t size)
{
	size_t total_size = nmemb * size;
	void *adr;

	if (total_size == 0)
		return NULL;

	size_t block_size = (size_t) align((total_size));
	struct block_meta *best_fit = NULL;

	// (B)
	if ((int) (size + get_block_meta_size()) < (int) getpagesize())
		best_fit = (struct block_meta *)find_best_fit(block_size);
	if (best_fit == NULL || (int) (size + get_block_meta_size()) >= (int) getpagesize())
		adr = add_new_block_calloc(block_size);
	else
		adr = (void *)((char *)best_fit + get_block_meta_size());

	// (A)
	if (adr != NULL) {
		char *ptr = (char *)adr;

		for (int i = 0; i < (int) total_size; i++, ptr++)
			(*ptr) = 0;
	}
	return adr;
}

/**
 * @param ptr - beginning adress of the payload
 * @param size - size of the new payload
 *	| (A) When trying to allocate 0 bytes we free the pointer
 *	| (B) When trying to realloc a NULL pointer, we call malloc on the
 *	|	  given size.
 *	| (C) When trying to realloc a freed block we return NULL
 *	| (D) When tring to realloc to a larger size than MMAP_TRESHOLD, we free
 *	|	  the block and call malloc.
 *	| (E) If a smaller size then we split the block if possibl, if not
 *	|	  we return the same block.
 *	| (F) We then try to expand the blocks if possible (if the block is the
 *	|	  last one in the list or if it is followed by multiple free blocks
 *	|	  that can be coalesced together to form the requested size)
 *	| (G) If expanding isn't possible, we apply the best fit rule and
 *	|	  coalesce all free blocks in the list to find a suitable
 *	|	  chunk of contiguous memory.
 *	| (H) Then we either move the block or expand the last free block
 *	|	  and copy the contents of the memory.
 */
void *os_realloc(void *ptr, size_t size)
{
	// (A)
	if (size == 0) {
		os_free(ptr);
		return NULL;
	}

	// (B)
	if (ptr == NULL)
		return os_malloc(size);

	struct block_meta *block = (struct block_meta *)find_block((char *)ptr - get_block_meta_size());

	// (C)
	if (block->status == STATUS_FREE)
		return NULL;

	size_t total_size = (size_t) align((size));

	// (D)
	if (block->status == STATUS_MAPPED) {
		int len = block->size;

		if (len > align(size))
			len = align(size);

		void *adr = os_malloc(size);

		memcpy(adr, (char *)ptr, len);

		os_free(ptr);
		return adr;
	}
	if (block->status == STATUS_ALLOC) {
		// (E)
		if ((int) block->size > (int) (total_size +  get_block_meta_size())) {
			split_block(block, total_size);
			return (void *)((char *)block + get_block_meta_size());
		}
		if (block->size >= total_size)
			return (void *)((char *)block + get_block_meta_size());

		// (F)
		struct block_meta *best_fit = (struct block_meta *)try_realloc_expanding(block, total_size);

		if (best_fit == NULL) {
			// (G)
			void *adr2 = find_free_block_realloc(block, total_size);

			if (adr2 == NULL) {
				struct block_meta *last = find_last();

				// (H)
				if (last->status == STATUS_FREE && total_size + get_block_meta_size() < MMAP_THRESHOLD) {
					void *adr3 = expand_last_free(last, total_size);

					memcpy(adr3, (char *)block + get_block_meta_size(), block->size);
					block->status = STATUS_FREE;
					return adr3;
				}
				return move_block_realloc(block, total_size);
			} else {
				return (char *)adr2 + get_block_meta_size();
			}
		} else {
			if (best_fit == block)
				return expand_last_block_realloc(block, total_size);

			return expand_block_realloc(block, total_size);
		}
	}
	return NULL;
}
