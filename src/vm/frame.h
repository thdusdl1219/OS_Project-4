#ifndef FRAMEH
#define FRAMEH

#include <list.h>
extern struct list frame_list;
struct lock frame_lock;

struct frame_elem
{
	struct list_elem elem;
	struct thread* cur_thread;
	void* frame;
	void* uaddr;
};
void frame_init(void);
void* frame_allocate(uint8_t* uaddr);
void frame_deallocate(void* frame);
void* choose_victim(void);
struct frame_elem* find_frame(void);

#endif
