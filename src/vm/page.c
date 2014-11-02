#include <debug.h>
#include "page.h"
#include "frame.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include <string.h>

struct lock file_lock;
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

bool add_to_page_table(struct file* file, size_t read_bytes, size_t zero_bytes, uint8_t* upage, off_t ofs, bool writable)
{
	struct sup_page_elem *spe = malloc(sizeof(struct sup_page_elem));
	if(spe == NULL)
		return false;
	spe->file = file;
	spe->read_bytes = read_bytes;
	spe->zero_bytes = zero_bytes;
	spe->uaddr = upage;
	spe->offset = ofs;
	spe->writable = writable;
	spe->load = false;

	return(hash_insert(&thread_current()->sup, &spe->elem) == NULL);
}

bool load_lazy_page(struct sup_page_elem* spe)
{
	if(spe->load)
		return false;
	uint8_t *frame = frame_allocate();
	if(frame == NULL)
		return false;
	if(spe->read_bytes > 0)
	{
		lock_acquire(&file_lock);
		if(file_read_at(spe->file, frame, spe->read_bytes, spe->offset) != (int) spe->read_bytes)
		{
			lock_release(&open_lock);
			frame_deallocate(frame);
			return false;
		}
		lock_release(&file_lock);
		memset(frame + spe->read_bytes, 0, spe->zero_bytes);
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
