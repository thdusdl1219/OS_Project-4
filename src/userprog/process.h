#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);
bool install_page (void *upage, void *kpage, bool writable);
extern struct lock load_lock;
extern struct lock file_lock;
extern struct lock open_lock;
extern struct semaphore sema3;
#endif /* userprog/process.h */
