#include "userprog/syscall.h"
#include <stdio.h>
#include <string.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "filesys/file.h"
#include "threads/vaddr.h"
#include "devices/input.h"
#include "threads/synch.h"
#include "vm/page.h"

static void syscall_handler (struct intr_frame *);
static bool fd_same (const struct list_elem *a_, const struct list_elem *b_ UNUSED, void *aux);
void check_pointer(const void* ptr);
void check_string(const void* str);
struct lock open_lock;

char zz[256];
int fd_ = 2;
void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}
static bool
fd_same (const struct list_elem *a_, const struct list_elem *b_ UNUSED, void *aux)
{
	  const struct open_elem *a = list_entry(a_, struct open_elem, elem);
		return a ->file_d == *(int *)aux;
}

void check_pointer(const void* ptr)
{
	if(!is_user_vaddr((const void *)ptr) || ptr < (void *) 0x08048000)
	{
		thread_current() ->exit_status = -1;
		thread_exit();
	}
//	struct sup_page_elem *spe = get_page_elem((void *)ptr);
//	if(spe == NULL)
//	{
//		thread_current() -> exit_status = -1;
//		thread_exit();
//	}
}

void check_string(const void* str)
{
//	printf("string : %s\n",str);
//	PANIC("address : %x\n",str);

	check_pointer(str);
	while(* (char *) str != 0)
	{
		str = (char *) str + 1;
		check_pointer(str);
	}
}

void check_buffer(void* buffer, unsigned size)
{
	unsigned i;
	char* buf = (char *) buffer;
	for(i = 0; i < size ; i++)
	{
		check_pointer((const void*)buf);
		buf++;
	}
}
static void
syscall_handler (struct intr_frame *f) 
{
	int pid;
	bool bool_;
	struct open_elem* t;
	struct file* open_file;
	
	check_pointer((const void*) f->esp);
	
	switch(*(int*)(f->esp))
	{
		case SYS_HALT:
			shutdown_power_off();
			break;
		case SYS_EXIT:
			if((unsigned int)f->esp > f->ebp)
			{
				thread_current ()->exit_status = -1;
				f->eax = -1;
			}
			else
			{
				thread_current()->exit_status = *(int *)(f->esp + 4);
				f->eax = *(int *)(f->esp + 4);
			}
			thread_exit();
			break;
		case SYS_EXEC:
				pid = process_execute((*(char **)(f->esp + 4)));
				f->eax = pid;
			break;
		case SYS_WAIT:
				f->eax = process_wait(*(tid_t *)(f->esp + 4));
			 break;
		case SYS_CREATE:
			if(*(char **)(f->esp + 16) == NULL)
			{
				thread_current ()->exit_status = -1;
				thread_exit();
			}
			else
			{
				check_string((const void*)(*(char **)(f->esp + 16)));
				lock_acquire(&open_lock);
				bool_ = filesys_create(*(char **)(f->esp + 16), *(unsigned int*)(f->esp + 20));
				lock_release(&open_lock);
				f->eax = bool_;
			}
			break;
		case SYS_REMOVE:
			if(*(char **)(f->esp + 4) == NULL)
			{
				thread_current ()->exit_status = -1;
				thread_exit();
			}
				check_string((const void*)(*(char **)(f->esp+4)));
				lock_acquire(&open_lock);
				f->eax = filesys_remove(*(char **)(f->esp + 4));
				lock_release(&open_lock);
			break;
		case SYS_OPEN:
			if(*(char **)(f->esp + 4) == NULL)
			{
				thread_current () -> exit_status = -1;
				thread_exit();
			}
			check_string((const void*)(*(char **)(f->esp+4)));
			lock_acquire(&open_lock);
			open_file = filesys_open(*(char **)(f->esp + 4));
			lock_release(&open_lock);
			if(open_file == NULL)
				f->eax = -1;
			else
			{
				t = malloc (sizeof(*t));
				t->open_file = open_file;
				t->file_d = fd_;
				f->eax = fd_++;
				list_push_back(&(thread_current () -> open_list), &(t->elem));
			}
			break;
		case SYS_FILESIZE:
			t = list_entry(list_find(&thread_current()->open_list, fd_same, (f->esp + 4)), struct open_elem, elem);
			f->eax = file_length(t->open_file);
			break;
		case SYS_READ:
				if(*(void **)(f->esp + 24) > PHYS_BASE)
				{
					thread_current ()->exit_status = -1;
					thread_exit();
				}

			if(*(int *)(f->esp + 20) == 0)
			{
				int i = 0;
				for(; i < *(int *)(f->esp + 28); i++)
				{
					(*(char **)(f->esp + 24))[i] = input_getc();
				}
			}
			else
			{
//				check_buffer(*(void **)(f->esp+24), *(unsigned *)(f->esp + 28));
				t = list_entry(list_find(&thread_current()->open_list, fd_same, (f->esp + 20)), struct open_elem, elem);
				lock_acquire(&open_lock);
				f->eax = file_read (t->open_file, *(void **)(f->esp + 24), *(int32_t *)(f->esp + 28));	
				lock_release(&open_lock);
			}
			break;
		case SYS_WRITE:
			if(*(int *)(f->esp + 20) == 1)
			{
				putbuf(*(char **)(f->esp + 24), *(size_t *)(f->esp+28));
			}
			else
			{
				t = list_entry(list_find(&thread_current()->open_list, fd_same, (f->esp + 20)), struct open_elem, elem);
				lock_acquire(&open_lock);
				f->eax = file_write (t->open_file, *(void **)(f->esp + 24), *(int32_t *)(f->esp + 28));
				lock_release(&open_lock);
			}
			break;
		case SYS_SEEK:
			t = list_entry(list_find(&thread_current()->open_list, fd_same, (f->esp + 16)), struct open_elem, elem);
			if(t == NULL)
			{
				thread_current ()->exit_status = -1;
				thread_exit ();
			}
			lock_acquire(&open_lock);
			file_seek(t->open_file, *(off_t *)(f->esp + 20));
			lock_release(&open_lock);
			break;
		case SYS_TELL:
			t = list_entry(list_find(&thread_current()->open_list, fd_same, (f->esp + 16)), struct open_elem, elem);
			if(t == NULL)
			{
				thread_current ()->exit_status = -1;
				thread_exit ();
			}
			lock_acquire(&open_lock);
			f->eax = file_tell (t->open_file);
			lock_release(&open_lock);
			break;
		case SYS_CLOSE:
			t = list_entry(list_find(&thread_current()->open_list, fd_same, (f->esp + 4)), struct open_elem, elem);
			if(t == NULL)
			{
				thread_current ()-> exit_status = -1;
				thread_exit();
			}
			lock_acquire(&open_lock);
			file_close(t->open_file);
			lock_release(&open_lock);
			list_remove(list_find(&thread_current()->open_list, fd_same, (f->esp + 4)));
			free(t);
			break;
//		default:
//			printf ("system call!\n");
//			break;
	}
}
