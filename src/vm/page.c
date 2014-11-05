#include <debug.h>
#include "page.h"
#include "frame.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include <string.h>
#include "swap.h"

struct lock file_lock;
struct lock open_lock;
static unsigned 
hash_func (const struct hash_elem *a, void *aux UNUSED)
{
	struct sup_page_elem *spe = hash_entry(a, struct sup_page_elem, elem);
	return hash_int((int) spe->uaddr);
}

static bool 
less_func (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED)
{
	struct sup_page_elem *aa = hash_entry(a, struct sup_page_elem, elem);
	struct sup_page_elem *bb = hash_entry(b, struct sup_page_elem, elem);
	
	if(aa->uaddr < bb->uaddr)
		return true;
	return false;
}

static void
destruct_func(struct hash_elem *a, void *aux UNUSED)
{
	struct sup_page_elem *spe = hash_entry(a, struct sup_page_elem, elem);
	if(spe->load)
	{
		frame_deallocate(pagedir_get_page(thread_current()->pagedir, spe->uaddr));
		pagedir_clear_page(thread_current()->pagedir, spe->uaddr);
	}
	free(spe);
}

void sup_page_init(struct hash* sup)
{
	hash_init(sup, hash_func, less_func, NULL);
}

void sup_page_destroy(struct hash* sup)
{
	hash_destroy(sup, destruct_func);
}

void change_page_table_close(uint8_t* addr)
{
	struct sup_page_elem *spe = NULL;
	spe = get_page_elem(addr);
	if(spe == NULL)
		return;
	load_lazy_page(spe);
}

bool remove_page_table_unmmap(uint8_t* addr)
{
	struct sup_page_elem *spe = NULL;
	spe = get_page_elem(addr);
	if(spe == NULL)
		return false;
	if(spe->load == true)
	{
		void* frame = pagedir_get_page(thread_current()->pagedir, spe->uaddr);
		if(pagedir_is_dirty(thread_current()->pagedir, spe->uaddr))
		{
			lock_acquire(&open_lock);
			if(file_write_at(spe->file, frame, spe->read_bytes, spe->offset) != (int) spe->read_bytes)
			{
				lock_release(&open_lock);
				return false;
			}
			lock_release(&open_lock);
		
		}
		frame_deallocate(addr);
		pagedir_clear_page(thread_current()->pagedir, spe->uaddr);
	}
	hash_delete(&thread_current()->sup, &spe->elem);
	free(spe);
	return true;
}

bool add_to_page_table(struct file* file, size_t read_bytes, size_t zero_bytes, uint8_t* upage, off_t ofs, bool writable, bool mmap)
{
	struct sup_page_elem *spe = NULL;
	spe = get_page_elem(upage);
	if(spe != NULL)
		return false;
	spe = malloc(sizeof(struct sup_page_elem));
	if(spe == NULL)
		return false;
	spe->file = file;
	spe->read_bytes = read_bytes;
	spe->zero_bytes = zero_bytes;
	spe->uaddr = upage;
	spe->offset = ofs;
	spe->writable = writable;
	spe->load = false;
	spe->swap_index = -1;
	spe->swap = false;
	spe->reswap = false;
	spe->mmap = mmap;
	return(hash_insert(&thread_current()->sup, &spe->elem) == NULL);
}
bool add_to_page_table_in_stack(uint8_t* addr)
{
	struct sup_page_elem *spe = malloc(sizeof(struct sup_page_elem));
	if(spe == NULL)
		return false;
	spe->file = NULL;
	spe->read_bytes = 0;
	spe->zero_bytes = 0;
	spe->uaddr = pg_round_down(addr);
	spe->offset = 0;
	spe->writable = true;
	spe->load = false;
	spe->swap_index = -1;
	spe->swap = false;
	spe->reswap = false;
	spe->mmap = false;

	return(hash_insert(&thread_current()->sup, &spe->elem) == NULL);
}

bool load_stack_page(struct sup_page_elem* spe)
{
	if(spe->load)
		return false;
	uint8_t *frame = frame_allocate(spe->uaddr);
	if(frame == NULL)
		return false;
	if(spe->swap)
	{
		swap_in(spe->swap_index, frame);
		spe->swap = false;
	}

	if(!install_page (spe->uaddr, frame, spe->writable))
	{
		frame_deallocate(frame);
		return false;
	}
	spe->load = true;
	return true;
}

bool load_lazy_page(struct sup_page_elem* spe)
{
	if(spe->load)
		return false;
	uint8_t *frame = frame_allocate(spe->uaddr);
	if(frame == NULL)
		return false;
	if(spe->swap)
	{
		swap_in(spe->swap_index, frame);
		spe->swap = false;
	}
	else
	{
		if(spe->read_bytes > 0)
		{
			lock_acquire(&open_lock);
			if(file_read_at(spe->file, frame, spe->read_bytes, spe->offset) != (int) spe->read_bytes)
			{
				lock_release(&open_lock);
				frame_deallocate(frame);
				return false;
			}
			lock_release(&open_lock);
			memset(frame + spe->read_bytes, 0, spe->zero_bytes);
		}
	}

	if(!install_page (spe->uaddr, frame, spe->writable))
	{
		frame_deallocate(frame);
		return false;
	}
	spe->load = true;
	return true;
}

struct sup_page_elem* get_page_elem(void* addr)
{
	struct sup_page_elem spe;
	spe.uaddr = pg_round_down(addr);
	struct hash_elem *a = hash_find(&thread_current()->sup, &spe.elem);
	if(a == NULL)
		return NULL;
	return hash_entry(a, struct sup_page_elem, elem);
}
