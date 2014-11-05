#include "swap.h"
#include <stdio.h>
#include "userprog/syscall.h"
void swap_init()
{
	swap_block = block_get_role(BLOCK_SWAP);
	if(swap_block == NULL)
		return;

	swap_table = bitmap_create(block_size(swap_block) * BLOCK_SECTOR_SIZE / 0x1000);
	if(swap_table == NULL)
		return;
	bitmap_set_all (swap_table, false);
	lock_init(&block_lock);
}

size_t swap_out(void* frame)
{
	size_t index = bitmap_scan(swap_table, 0, 1, false);
	if(index == BITMAP_ERROR)
		PANIC("[Warning] swap block is full.");

	bitmap_mark (swap_table, index);

	int i;
	for(i = 0; i < 8; i++)
		block_write(swap_block, index * 8 + i, frame + i * BLOCK_SECTOR_SIZE);

	return index;

}

void swap_in(size_t index, void* frame)
{
	lock_acquire(&block_lock);
	if( bitmap_test(swap_table, index) == false )
		PANIC("[Warning] It is free swap slot.");

	bitmap_reset(swap_table, index);

	int i;
	for(i = 0; i < 8; i++)
	{
		block_read(swap_block, index * 8 + i, frame + i * BLOCK_SECTOR_SIZE);
	}
	lock_release(&block_lock);
}
