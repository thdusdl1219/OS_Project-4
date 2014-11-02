#include <list.h>
extern struct list frame_list;
struct lock frame_lock;

struct frame_elem
{
	struct list_elem elem;
	struct thread* cur_thread;
	void* frame;
};
void frame_init(void);
void* frame_allocate(struct thread* thd);
void frame_deallocate(struct thread* thd);
void* choose_victim(void);
void* find_frame(struct thread* thd);
