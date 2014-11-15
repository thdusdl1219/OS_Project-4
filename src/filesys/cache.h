#ifndef CACHEH
#define CACHEH

#include "devices/block.h"
#include "filesys.h"
#include "threads/synch.h"
#include <list.h>

struct list buffer_cache;
struct lock cache_lock;
int cache_size;

struct cache_elem
{
	uint8_t block[BLOCK_SECTOR_SIZE];
	block_sector_t sector_index;
	bool accessed;
	bool dirty;
	struct list_elem elem;
};

void cache_init(void);
uint8_t* get_block_cache(block_sector_t sector, bool dirty, bool read);
struct cache_elem* get_elem_cache(block_sector_t sector);
uint8_t* choose_victim(block_sector_t sector, bool dirty, bool read);

void all_cache_go_back(void);

#endif

