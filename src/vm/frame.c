#include "frame.h"
#include "threads/synch.h"
#include <debug.h>
#include "threads/malloc.h"
#include "threads/palloc.h"
struct list frame_list;

static bool
thread_same(const struct list_elem *a_, const struct list_elem *b_ UNUSED, void *aux)
{
	const struct frame_elem *a = list_entry(a_, struct frame_elem, elem);
	return a->cur_thread == aux;
}

void frame_init()
{
	list_init(&frame_list);
	lock_init(&frame_lock);
}

void* frame_allocate(struct thread* thd)
{
	void* frame;
	struct frame_elem* fte;
	fte = malloc(sizeof(*fte));
	
	frame = palloc_get_page(PAL_USER);
	if(frame == NULL)
		frame = choose_victim();
	
	fte->cur_thread = thd;
	fte->frame = frame;
	list_push_back(&frame_list, &fte->elem);
	return frame;

}
void frame_deallocate(struct thread* thd)
{
	struct frame_elem* t;
	while(1)
	{
		t = list_entry(list_find(&frame_list, thread_same, (void *)thd), struct frame_elem, elem);
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
	PANIC("I don't have more frame");
}


void* find_frame(struct thread* thd)
{
	struct frame_elem* t;
	t = list_entry(list_find(&frame_list, thread_same, (void *)thd), struct frame_elem, elem);
	if(t == NULL)
		return NULL;
	else
		return t->frame;
}
