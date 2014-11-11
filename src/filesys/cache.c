#include "cache.h"
#include "threads/malloc.h"


void cache_init()
{
	list_init(&buffer_cache);
	cache_size = 0;
}

struct cache_elem* get_elem_cache(block_sector_t sector)
{
	struct list_elem* e;
	for(e = list_begin(&buffer_cache); e != list_end(&buffer_cache); e = list_next(e))
	{
		struct cache_elem* ce = list_entry(e, struct cache_elem, elem);
		if(ce->sector_index == sector)
			return ce;
	}
	return NULL;
}

uint8_t* choose_victim(block_sector_t sector, bool dirty, bool read)
{
	bool check = true;
	struct cache_elem* ce;
	struct list_elem* e;
	while(check)
	{
		for(e = list_begin(&buffer_cache); e != list_end(&buffer_cache); e = list_next(e))
		{
			ce = list_entry(e, struct cache_elem, elem);
			if(ce->accessed)
				ce->accessed = false;
			else
			{
				if(ce->dirty)
					block_write(fs_device, ce->sector_index, &ce->block);
				check = false;
				break;
			}
		}
	}
	block_read(fs_device, sector, &ce->block);
	ce->sector_index = sector;
	ce->accessed = true;
	ce->dirty = dirty;
	if(read)
		get_block_cache(sector + 1, dirty, false);
	return (uint8_t *)&ce->block;
}

uint8_t* get_block_cache(block_sector_t sector, bool dirty, bool read)
{
	struct cache_elem* ce = get_elem_cache(sector);
	if(ce != NULL)
	{
		ce->accessed = true;
		ce->dirty |= dirty;
		if(read)
			get_block_cache(sector + 1, dirty, false);
		return (uint8_t *)&ce->block;
	}
	else
	{
		if(cache_size < 64)
		{
			cache_size++;
			ce = malloc(sizeof(struct cache_elem));
			if(ce == NULL)
				return NULL;
			list_push_back(&buffer_cache, &ce->elem);
			block_read(fs_device, sector, &ce->block);
			ce->sector_index = sector;
			ce->accessed = true;
			ce->dirty = dirty;
			if(read)
				get_block_cache(sector + 1, dirty, false);
			return (uint8_t *)&ce->block;
		}
		else
			return choose_victim(sector, dirty, read);
	}
}

void all_cache_go_back()
{
	struct list_elem* e;
	for(e = list_begin(&buffer_cache); e != list_end(&buffer_cache);)
	{
		struct cache_elem* ce = list_entry(e, struct cache_elem, elem);
		e = list_next(e);
		if(ce->dirty)
			block_write(fs_device, ce->sector_index, &ce->block);
		list_remove(&ce->elem);
		free(ce);
	}
}
