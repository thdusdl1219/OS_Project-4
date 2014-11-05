#ifndef SWAPH
#define SWAPH

#include "devices/block.h"
#include "page.h"
#include <bitmap.h>
#include "threads/synch.h"

struct block *swap_block;
struct bitmap *swap_table;
struct lock block_lock;

void swap_init(void);
size_t swap_out(void* frame);
void swap_in(size_t index, void* frame);


#endif
