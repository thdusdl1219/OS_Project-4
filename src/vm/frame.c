#include "frame.h"
#include "threads/synch.h"
#include <debug.h>
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "page.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "swap.h"
struct lock open_lock;
struct list frame_list;

static bool
thread_same(const struct list_elem *a_, const struct list_elem *b_ UNUSED, void *aux)
{
	const struct frame_elem *a = list_entry(a_, struct frame_elem, elem);
	return a->cur_thread == aux;
}

static bool
frame_same(const struct list_elem *a_, const struct list_elem *b_ UNUSED, void *aux)
{
	const struct frame_elem *a = list_entry(a_, struct frame_elem, elem);
	return a->frame == aux;
}

void frame_init()
{
	list_init(&frame_list);
	lock_init(&frame_lock);
}

void* frame_allocate(uint8_t* uaddr)
{
	void* frame;
	struct frame_elem* fte = malloc(sizeof(struct frame_elem));
	
	frame = palloc_get_page(PAL_USER);
	if(frame == NULL)
		frame = choose_victim();
	
	fte->cur_thread = thread_current();
	fte->frame = frame;
	fte->uaddr = uaddr;
	list_push_back(&frame_list, &fte->elem);
	return frame;

}
void frame_deallocate(void* frame)
{
	struct frame_elem* t;
	while(1)
	{
		t = list_entry(list_find(&frame_list, frame_same, frame), struct frame_elem, elem);
		if(t == NULL)
			return;
		else
		{
			palloc_free_page(t->frame);
			list_remove(&t->elem);
			free(t);
		}
	}
}
void* choose_victim()
{
	struct list_elem* e = list_begin(&frame_list);
	while(1)
	{
		struct frame_elem *fe = list_entry(e, struct frame_elem, elem);
		struct thread *t = fe->cur_thread;
		if(pagedir_is_accessed(t->pagedir, fe->uaddr))
			pagedir_set_accessed(t->pagedir, fe->uaddr, false);
		else
		{
			struct sup_page_elem* spe = get_page_elem(fe->uaddr);
			if(pagedir_is_dirty(t->pagedir, fe->uaddr) || spe->reswap)
			{
				if(spe->mmap)
				{
					lock_acquire(&open_lock);
					file_write_at(spe->file, fe->frame, spe->read_bytes, spe->offset);
					lock_release(&open_lock);
				}
				else
				{
					spe->reswap = true;
					spe->swap = true;
					spe->swap_index = swap_out(fe->frame);
				}
			}

			pagedir_clear_page(t->pagedir, fe->uaddr);
			palloc_free_page(fe->frame);
			list_remove(&fe->elem);
			free(fe);
			spe->load = false;
			return palloc_get_page(PAL_USER);
		}
	}
	PANIC("I don't have more frame");
}


struct frame_elem* find_frame()
{
	struct frame_elem* t;
	t = list_entry(list_find(&frame_list, thread_same, (void *)thread_current()), struct frame_elem, elem);
	if(t == NULL)
		return NULL;
	else
		return t;
}
