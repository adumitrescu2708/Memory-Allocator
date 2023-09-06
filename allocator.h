/* SPDX-License-Identifier: BSD-3-Clause */
#pragma once
#include "helpers.h"
/*
    @name Dumitrescu Alexandra
    @date 10.04.2023
    @for  Operating Systems - Memory Allocator
*/

/*
    @param size - aligned size of the new memory block

    | Method called by malloc() function, adds a new block of memory
    | in the memory allocator's linked list and treats possible cases.
*/
void *add_new_block(size_t size);
/*
    @param size - aligned size of the new memory block

    | Method called by calloc() function, adds a new block of memory
    | in the memory allocator's linked list and treats possible cases.
    | This method is similar to add_new_block() called by malloc()
*/
void *add_new_block_calloc(size_t size);
/*
    @param size - aligned size of the new memory block

    | This method is used to implement block reuse in the memory allocator.
    | Given a size, it finds the best fit in the linked list. The best fit
    | reffers to the smallest block larger than the required size.
*/
void *find_best_fit(size_t size);
/*
    @param adr - starting address of a block

    | Method used to iterate in the list and find the corresponding
    | block at the given address.
*/
void *find_block(void *adr);
/*
    | Method used for merging adjacent free blocks in the linked list into
    | a contiguous chunk of free memory, used for preventing fragmentations.
*/
void coalesce(void);
/*
    @param block - block of memory realloced()
    @param size - new size of the memory block

    | This method coalesced the following free blocks adjacent to the given
    | block until the size limit is reached. If the limit isn't reached, then
    | the coalesced blocks will remain coalesced.
*/
void coalesce_realloc(struct block_meta *block, size_t size);
/*
    @param block - block that will be removed

    | This method is used to remove a block in the linked list when free()
    | method is called on a mapped memory allocation.
*/
void delete_node(struct block_meta *block);
/*
    @param block - block that will be split
    @param size - aligned size of the new memory block

    | This method is used to split the given block into 2 blocks,
    | the first one containing size bytes and the second one the rest ones.
*/
void split_block(struct block_meta *block, size_t size);
/*
    @param block - block of memory that needs to be realloced
    @param size - new size requested by the realloc() call

    | This method is first step when reallocing a chunk of memory.
    | It checks wether the block of memory is the last one in the list, in
    | which case it needs to be extended, or it is followed by a large
    | enough chunk of freed memory blocks. It calls coalesce_realloc()
    | to expand only the adjacent free blocks until the limit of size
    | is completed. This method returns NULL if the expanding can not
    | take place, or the address of the block if not.
*/
void *try_realloc_expanding(struct block_meta *block, size_t size);
/*
    @param block - the block that is realloced
    @param size - new aligned size of the block

    | This method expands the block and splits the resulting block
    | if possible.
*/
void *expand_block_realloc(struct block_meta *block, size_t size);
/*
    @param block - the block that is realloced
    @param size - new aligned size of the block

    | This method is used when reallocating and finding the possibility
    | to extand the last block that is freed instead of adding a new one
*/
void *expand_last_free(struct block_meta *block, size_t size);
/*
    @param block - the block that is realloced
    @param size - new aligned size of the block

    | Function used when expanding the last block in the list in the
    | realloc() function
*/
void *expand_last_block_realloc(struct block_meta *block, size_t size);
/*
    @param block - block that is realloced
    @param size - new aligned size of the block

    | Function that moves the realloced block to a new contiguous
    | chunk of memory found in previous method calls.
*/
void *move_block_realloc(struct block_meta *block, size_t size);
/*
    @param block - block that is realloced
    @param total_size - new aligned size of the block

    | Function that implements best fit method in realloc() call
*/
void *find_free_block_realloc(struct block_meta *block, size_t total_size);
/*
    | Function that iterates through the list and searches for the last
    | alloced block of memeory (using brk syscall)
*/
struct block_meta *find_last(void);
