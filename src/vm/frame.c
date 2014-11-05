#include "frame.h"
#include "threads/synch.h"
#include <debug.h>
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
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
