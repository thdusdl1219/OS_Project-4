#ifndef FRAMEH
#define FRAMEH

#include <list.h>
#include "page.h"
extern struct list frame_list;
struct lock frame_lock;

struct frame_elem
{
	struct list_elem elem;
	struct thread* cur_thread;
	void* frame;
	struct sup_page_elem* spe;
};
void frame_init(void);
void* frame_allocate(struct sup_page_elem* spe);
void frame_deallocate(void* frame);
void* choose_victim(void);
struct frame_elem* find_frame(struct sup_page_elem* spe);

#endif
