#include "frame.h"
#include "threads/synch.h"
#include <debug.h>
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "swap.h"
struct lock open_lock;
struct list frame_list;

static bool
spe_same(const struct list_elem *a_, const struct list_elem *b_ UNUSED, void *aux)
{
	const struct frame_elem *a = list_entry(a_, struct frame_elem, elem);
	return a->spe == (struct sup_page_elem* )aux;
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

void* frame_allocate(struct sup_page_elem* spe)
{
	void* frame;
	struct frame_elem* fte = malloc(sizeof(struct frame_elem));
	
	frame = palloc_get_page(PAL_USER);
	if(frame == NULL)
	{
		frame = choose_victim();
		lock_release(&frame_lock);
	}
	
	fte->cur_thread = thread_current();
	fte->frame = frame;
	fte->spe = spe;
	lock_acquire(&frame_lock);
	list_push_back(&frame_list, &fte->elem);
	lock_release(&frame_lock);
	return frame;

}
void frame_deallocate(void* frame)
{
	struct frame_elem* t;
	lock_acquire(&frame_lock);
	while(1)
	{
		t = list_entry(list_find(&frame_list, frame_same, frame), struct frame_elem, elem);
		if(t == NULL)
		{
			lock_release(&frame_lock);
			return;
		}
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
	lock_acquire(&frame_lock);
	struct list_elem* e = list_begin(&frame_list);
	while(1)
	{
		struct frame_elem *fe = list_entry(e, struct frame_elem, elem);
		struct thread *t = fe->cur_thread;
		void* frame;
		if(t->pagedir != NULL)
		{
		if(pagedir_is_accessed(t->pagedir, fe->spe->uaddr))
			pagedir_set_accessed(t->pagedir, fe->spe->uaddr, false);
		else
		{
//			struct sup_page_elem* spe = get_page_elem(fe->uaddr);
			lock_acquire(fe->spe->lock);
			if(pagedir_is_dirty(t->pagedir, fe->spe->uaddr) || fe->spe->reswap)
			{
				if(fe->spe->mmap)
				{
					lock_acquire(&open_lock);
					file_write_at(fe->spe->file, fe->frame, fe->spe->read_bytes, fe->spe->offset);
					lock_release(&open_lock);
				}
				else
				{
					fe->spe->reswap = true;
					fe->spe->swap = true;
					fe->spe->swap_index = swap_out(fe->frame);
				}
			}

			pagedir_clear_page(t->pagedir, fe->spe->uaddr);
//			palloc_free_page(fe->frame);
			list_remove(&fe->elem);
			fe->spe->load = false;
			frame = fe->frame;
			lock_release(fe->spe->lock);
			free(fe);
			return frame;
		}
		}
		e = list_next(e);
		if(e == list_end(&frame_list))
		{
			e = list_begin(&frame_list);
		}
	}
}


struct frame_elem* find_frame(struct sup_page_elem* spe)
{
	struct frame_elem* t;
	lock_acquire(&frame_lock);
	t = list_entry(list_find(&frame_list, spe_same, (void *)spe), struct frame_elem, elem);
	lock_release(&frame_lock);
	if(t == NULL)
		return NULL;
	else
		return t;
}
