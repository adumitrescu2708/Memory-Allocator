/* SPDX-License-Identifier: BSD-3-Clause */

/*
    @name Dumitrescu Alexandra
    @date 10.04.2023
    @for  Operating Systems - Memory Allocator
*/
#pragma once
#include "helpers.h"

/*
    Memory is aligned to 8 bytes as required by 64 bit systems.
*/
#define ALIGNMENT 8
/*
    MMAP treshold
*/
#define MMAP_THRESHOLD (128 * 1024)
/*
    Method that aligned a given size to a multiple of ALIGNMENT
*/
int align(size_t size);
/*
    Returns the aligned number of bytes ocupied by the header of one
    block in the memory allocator's linked list.
*/
int get_block_meta_size(void);
