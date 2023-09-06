// SPDX-License-Identifier: BSD-3-Clause
/**
 * @name Dumitrescu Alexandra
 * @date 10.04.2023
 * @for  Operating Systems - Memory Allocator
 */
#include <unistd.h>
#include "allocator.h"
#include "alignment_utils.h"
#include "helpers.h"

/**
 * Heap head - start of the linked list
 */
struct block_meta *heap_head;
/**
 * Value set when the list is first used
 */
short int initialised;
/**
 * @param size - aligned size of the new block
 *	| This method is used when allocating a chunk of memory
 *	| that is larger than the MMAP_TRESHOLD.
 *	| Case (A): The block is the first one in the list, in
 *	|			which case it becomes the heap head.
 *	| Case (B): The block is added at the end of the list.
 *	| Returns the memory moved with size_of_header bytes
 */
void *add_new_mapped_block(size_t size)
{
	void *new_mem = NULL;

	new_mem = mmap(NULL, size + get_block_meta_size(), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	DIE(new_mem == (void *) -1, "Mmap syscall failed!\n");

	struct block_meta *new_block = new_mem, *ptr = NULL;

	new_block->status = STATUS_MAPPED;
	new_block->size = size;
	new_block->next = NULL;

	if (heap_head == NULL) {
		// (A)
		initialised = 1;
		heap_head = new_mem;
		heap_head->size = size;
		heap_head->next = NULL;
		heap_head->status = STATUS_MAPPED;
	} else {
		// (B)
		ptr = heap_head;

		while (ptr->next != NULL)
			ptr = ptr->next;
		ptr->next = new_block;
	}
	return (char *)new_block + (int)get_block_meta_size();
}
/**
 * @param block - block of memory realoced
 * @param size - aligned size of the realoced block
 *	| It first coalesces free blocks into contiguous free blocks
 *	| of memory and searches for a large enough free block where
 *	| to replace the block. It implements the best fit strategy, searches
 *	| for the smallest larger block of free memory. (A)
 *	| After this, it tries to split the block if possible. (B)
 */
void *find_free_block_realloc(struct block_meta *block, size_t total_size)
{
	coalesce();
	struct block_meta *minim = (struct block_meta *)NULL;
	int found = -1;
	struct block_meta *ptr = NULL;

	// (A)
	for (ptr = heap_head; ptr != NULL; ptr = ptr->next) {
		if (ptr->status == STATUS_FREE && ptr->size >= total_size) {
			if (found == -1) {
				minim = ptr;
			} else {
				if (minim->size > ptr->size)
					minim = ptr;
			}
			found = 1;
		}
	}
	/* (B) */
	if (found == 1) {
		if (minim->size > get_block_meta_size() + total_size)
			split_block(minim, total_size);
		else
			minim->status = STATUS_ALLOC;

		block->status = STATUS_FREE;
		memmove((char *)minim + get_block_meta_size(), (char *)block + get_block_meta_size(), block->size);
	}
	return (void *)minim;
}

/**
 * | This method returns the last alloced block of memory in the list.
 */
struct block_meta *find_last(void)
{
	struct block_meta *ptr = heap_head;

	while (ptr->next != NULL && ptr->next->status != STATUS_MAPPED)
		ptr = ptr->next;
	return ptr;
}

/**
 * @param block - the block that will be realoced
 * @param size - the aligned size of the new chunk
 *	| Checks wether the new chunk of memory will be alloced
 *	| using mmap() syscall in which case it expands the block,
 *	| otherwise it adds the block at the end of the list of
 *	| alloced blocks of memory.
 */
void *move_block_realloc(struct block_meta *block, size_t size)
{
	block->status = STATUS_FREE;
	if (size + get_block_meta_size() < MMAP_THRESHOLD) {
		struct block_meta *last_alloced_block = heap_head;

		while (1) {
			struct block_meta *next_alloced_block = last_alloced_block->next;

			if (next_alloced_block == NULL)
				break;
			if (next_alloced_block->status == STATUS_MAPPED)
				break;
			last_alloced_block = last_alloced_block->next;
		}
		struct block_meta *new_block = (struct block_meta *)((char *)last_alloced_block
				+ last_alloced_block->size + get_block_meta_size());
		void *new_addr = (char *)new_block + size + get_block_meta_size();

		int res = brk(new_addr);

		DIE(res == -1, "Brk syscall failed!\n");
		new_block->next = last_alloced_block->next;
		last_alloced_block->next = new_block;
		new_block->status = STATUS_ALLOC;
		new_block->size = size;

		memcpy((char *)new_block + get_block_meta_size(), (char *)block + get_block_meta_size(), block->size);

		return (char *)new_block + get_block_meta_size();
	}
	void *new = add_new_mapped_block(size);

	memcpy(new, (char *)block + get_block_meta_size(), block->size);
	return new;
}
/**
 * @param block - last block that will be extended
 * @param size - new size
 * | This metod extands the last block in realloc().
 */
void *expand_last_block_realloc(struct block_meta *block, size_t size)
{
	void *new_mem = (char *)block + size + get_block_meta_size();

	int res = brk(new_mem);

	DIE(res == -1, "Brk syscall failed!\n");

	block->status = STATUS_ALLOC;
	block->size = size;
	return (char *)block + get_block_meta_size();
}

/**
 * @param block - block that will be realloced
 * @para size - new aligned size of the block
 * | This method expands the given block to the adjacent free blocks (A)
 * | after coalescing them and splits the result block, if possible. (B)
 */
void *expand_block_realloc(struct block_meta *block, size_t size)
{
	struct block_meta *next_block = block->next;
	// (A)
	block->size += next_block->size + get_block_meta_size();
	block->next = next_block->next;

	// (B)
	if (block->size > size + get_block_meta_size())
		split_block(block, size);
	return (char *)block + get_block_meta_size();
}

/**
 * @param block - last freed block in list
 * @param size - new size of the alloced block
 * | This method expands the last freed block in realloc()
 */
void *expand_last_free(struct block_meta *block, size_t size)
{
	void *adr = (char *)block + size + get_block_meta_size();

	int res = brk(adr);

	DIE(res == -1, "Brk syscall failed!\n");
	block->size = size;
	block->status = STATUS_ALLOC;
	return (char *)block + get_block_meta_size();
}
/**
 * @param block - last freed block in list
 * @param size - new size of the alloced block
 *	| This method checks sequentially wether the adjacent blocks
 *	| can be coalesced until the new size is reached. In case
 *	| the size isn't reached, the blocks will remain coalesced.
 */
void coalesce_realloc(struct block_meta *block, size_t size)
{
	if (block == NULL)
		return;
	struct block_meta *block_current = block;
	struct block_meta *block_next = block->next;

	if (block_current->status == STATUS_FREE) {
		size_t total_size = block_current->size;

		if (block_next != NULL) {
			while (block_next->status == STATUS_FREE) {
				total_size += block_next->size + get_block_meta_size();
				block_next = block_next->next;
				block_current->next = block_next;
				if (block_next == NULL || total_size >= size)
					break;
			}
		}
		block_current->size = total_size;
	}
}

/**
 * @param block - last freed block in list
 * @param size - new size of the alloced block
 *	| This method returns the address of the block if it can be extanded (A)
 *	| or if it is the last one in the list (B), otherwise it returns NULL
 */
void *try_realloc_expanding(struct block_meta *block, size_t size)
{
	coalesce_realloc(block->next, size - block->size - get_block_meta_size());
	struct block_meta *next_block = block->next;
	struct block_meta *last_block = heap_head;

	while (last_block->next != NULL && last_block->next->status != STATUS_MAPPED)
		last_block = last_block->next;
	// (A)
	if (next_block != NULL && block->size + next_block->size + get_block_meta_size() >= size
		&& next_block->status == STATUS_FREE)
		return next_block;
	// (B)
	else if (block == last_block)
		return last_block;
	return NULL;
}
/**
 *	| This method iterates through the list and merges
 *	| free blocks of adjacent chunks of memory into a contiguous
 *	| block.
 */
void coalesce(void)
{
	if (heap_head == NULL)
		return;

	struct block_meta *block_current = heap_head;
	struct block_meta *block_next = heap_head->next;

	while (block_next != NULL) {
		if (block_current->status == STATUS_FREE) {
			size_t total_size = block_current->size;

			while (block_next->status == STATUS_FREE) {
				total_size += block_next->size + get_block_meta_size();
				block_next = block_next->next;
				block_current->next = block_next;
				if (block_next == NULL)
					break;
			}
			block_current->size = total_size;
			block_current = block_next;
			if (block_next != NULL)
				block_next = block_next->next;
		} else {
			block_current = block_current->next;
			block_next = block_next->next;
		}
	}
}
/**
 * @param block - the block of memory that will be removed
 *	| This method is used for deleting a node in the list in
 *	| case of a free() call on a mapped block.
 */
void delete_node(struct block_meta *block)
{
	struct block_meta *ptr = heap_head;
	int result;

	if (heap_head == block) {
		heap_head = heap_head->next;
		result = munmap(block, ptr->size + get_block_meta_size());
		DIE(result == -1, "Munmap failed!\n");
	} else {
		while (ptr->next != block)
			ptr = ptr->next;
		ptr->next = ptr->next->next;
		result = munmap(block, block->size + get_block_meta_size());
		DIE(result == -1, "Munmap failed!\n");
	}
}

/**
 * @param block - block thaat will be split
 * @param size - size of the alloced block
 *	| This method splits the block into 2 new blocks,
 *	| the first one being size bytes long and alloced,
 *	| and the second one remaining freed with the rest of
 *	| the bytes.
 */
void split_block(struct block_meta *block, size_t size)
{
	struct block_meta *new_block = (struct block_meta *)((char *)block + size + get_block_meta_size());

	new_block->size = block->size - size - get_block_meta_size();
	new_block->next = block->next;
	new_block->status = STATUS_FREE;
	block->next = new_block;
	block->size = size;
	block->status = STATUS_ALLOC;
}

/**
 * @size - the size of the contiguous free chunk of memory
 *	| This method first coalesces adjacent free blocks, then
 *	| searches using best-fit ideology the smallest larger
 *	| freed block and then tries to split it if possible.
 */
void *find_best_fit(size_t size)
{
	coalesce();
	if (heap_head == NULL)
		return NULL;

	struct block_meta *best_fit = NULL;
	struct block_meta *ptr = NULL;

	for (ptr = heap_head; ptr != NULL; ptr = ptr->next) {
		if (ptr->status == STATUS_FREE && ptr->size >= size) {
			if (best_fit != NULL) {
				if (best_fit->size > ptr->size)
					best_fit = ptr;
			} else {
				best_fit = ptr;
			}
		}
	}
	if (best_fit != NULL) {
		if ((int) (best_fit->size - size) > (int)get_block_meta_size())
			split_block(best_fit, size);
		else
			best_fit->status = STATUS_ALLOC;
	}

	return best_fit;
}

/**
 * @param size - aligned size of memory
 *	| This method allocates memory using brk() syscall
 *	| and checks wether the block is the first one in the list, in
 *	| which case it updates the heap head, or if it needs to be
 *	| placed at the end of the alloced blocks.
 */
void *add_new_alloced_block(size_t size)
{
	void *new_mem = NULL;
	struct block_meta *last_alloced_block = heap_head;
	struct block_meta *new_block;

	if (initialised != 0) {
		while (1) {
			struct block_meta *next_alloced_block = last_alloced_block->next;

			if (next_alloced_block == NULL)
				break;
			if (next_alloced_block->status == STATUS_MAPPED)
				break;
			last_alloced_block = last_alloced_block->next;
		}
		if (last_alloced_block->status == STATUS_FREE) {
			last_alloced_block->status = STATUS_ALLOC;
			size_t remaining_size = align(size - last_alloced_block->size);

			new_mem = (char *)last_alloced_block + last_alloced_block->size + get_block_meta_size() + remaining_size;
			int res = brk(new_mem);

			DIE(res == -1, "Brk syscall failed!\n");
			last_alloced_block->size = size;
			return (char *)last_alloced_block + get_block_meta_size();
		}
		if (last_alloced_block->status == STATUS_MAPPED) {
			struct block_meta *new;
			void *start = sbrk(0);

			DIE(start == (void *)-1, "Sbrk failed!\n");

			void *res = sbrk(MMAP_THRESHOLD);

			DIE(res == (void *)-1, "Sbrk syscall failed!\n");

			new = (struct block_meta *)start;
			DIE(new == (void *) -1, "Sbrk syscall failed!");

			new->size = size;
			new->next = last_alloced_block;
			new->status = MMAP_THRESHOLD;
			heap_head = new;
			return (char *)start + get_block_meta_size();
		}
		new_mem = (char *)last_alloced_block + last_alloced_block->size + size + 2 * get_block_meta_size();
		int res = brk(new_mem);

		DIE(res == -1, "Brk syscall() failed!\n");
		new_block =  (struct block_meta *) ((char *)last_alloced_block
				+ last_alloced_block->size + get_block_meta_size());
		new_block->size = size;
		new_block->next = last_alloced_block->next;
		new_block->status = STATUS_ALLOC;
		last_alloced_block->next = new_block;
		return (char *)new_block + get_block_meta_size();
	}
	initialised = 1;

	void *start = sbrk(0);

	DIE(start == (void *)-1, "Sbrk syscall failed!\n");

	heap_head = (struct block_meta *)start;

	void *res = sbrk(MMAP_THRESHOLD);

	DIE(res == (void *)-1, "Sbrk syscall failed!\n");

	heap_head->size = MMAP_THRESHOLD - get_block_meta_size();
	heap_head->next = NULL;
	heap_head->status = STATUS_ALLOC;

	if (size + 2 * get_block_meta_size() < MMAP_THRESHOLD)
		split_block(heap_head, size);

	return (char *)start + get_block_meta_size();
}

/**
 * @param size - aligned size of the new block
 *	| This method checks it the new block will be mapped or alloced
 *	| in calloc() call
 */
void *add_new_block_calloc(size_t size)
{
	int page_size = getpagesize();

	if ((int) (size + get_block_meta_size()) >= (int) page_size)
		return add_new_mapped_block(size);
	else
		return add_new_alloced_block(size);
}

/**
 * @param size - aligned size of the new block
 *	| This method checks it the new block will be mapped or alloced
 *	| in malloc() call
 */
void *add_new_block(size_t size)
{
	if (size >= MMAP_THRESHOLD)
		return add_new_mapped_block(size);
	else
		return add_new_alloced_block(size);
}
/**
 * @param adr - starting address of the block
 *	| This method iterates through the list and returns
 *	| the block starting there or NULL in case it doesn't exist.
 */
void *find_block(void *adr)
{
	for (struct block_meta *ptr = heap_head; ptr != NULL; ptr = ptr->next) {
		if (ptr == adr)
			return (void *)ptr;
	}
	return NULL;
}
