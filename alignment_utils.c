// SPDX-License-Identifier: BSD-3-Clause
/**
 * @name Dumitrescu Alexandra
 * @date 10.04.2023
 * @for  Operating Systems - Memory Allocator
 */
#include "alignment_utils.h"

int align(size_t size)
{
return size % ALIGNMENT == 0 ? (int)size : (int)((size / ALIGNMENT + 1) * ALIGNMENT);
}

int get_block_meta_size(void)
{
return align(sizeof(struct block_meta));
}
