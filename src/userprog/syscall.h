#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <list.h>
#include "filesys/file.h"
void syscall_init (void);

void close_file_mmap(struct file* file);
void munmap(int mapping);
//static struct list open_exe;


/*struct exe_elem
{
	char* name;
	int pid;
	struct list_elem elem;
};*/

#endif /* userprog/syscall.h */
