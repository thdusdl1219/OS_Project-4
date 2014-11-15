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
#include "filesys/inode.h"
#include "filesys/directory.h"

static void syscall_handler (struct intr_frame *);
static bool fd_same (const struct list_elem *a_, const struct list_elem *b_ UNUSED, void *aux);

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

static void
syscall_handler (struct intr_frame *f) 
{
	int pid;
	bool bool_;
	struct open_elem* t;
	struct file* open_file;
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
				lock_acquire(&open_lock);
				bool_ = filesys_create(*(char **)(f->esp + 16), *(unsigned int*)(f->esp + 20), false);
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
			if(strlen(*(char **)(f->esp + 4)) == 0)
			{
				f->eax = -1;
				break;
			}
			lock_acquire(&open_lock);
			open_file = filesys_open(*(char **)(f->esp + 4));
			lock_release(&open_lock);
			if(open_file == NULL)
			{
				f->eax = -1;
				break;
			}
			else
			{

				t = malloc (sizeof(*t));
				memset(t, 0, sizeof(*t));
				if(open_file->inode->dir)
				{
					t->dir = dir_open(open_file->inode);
					t->ddir = true;
				}
				else
					t->ddir = false;
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
				if(t->ddir)
				{
					f->eax = -1;
					break;
				}
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
			t = list_entry(list_find(&thread_current()->open_list, fd_same, (f->esp + 4)), struct open_elem, elem);
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
			if(t->ddir)
				dir_close(t->dir);

			lock_acquire(&open_lock);
			file_close(t->open_file);
			lock_release(&open_lock);
			list_remove(list_find(&thread_current()->open_list, fd_same, (f->esp + 4)));
			free(t);
			break;
		case SYS_CHDIR:
			if(*(char **)(f->esp + 4) == NULL)
			{
				thread_current () -> exit_status = -1;
				thread_exit();
			}
			if(strlen(*(char **)(f->esp + 4)) == 0)
			{
				f->eax = false;
				break;
			}
				struct dir* dir1;
				dir1 = get_real_dir(*(char **)(f->esp + 4),false);
				if(dir1 == NULL)
				{
					f->eax = false;
					break;
				}
				else
				{
//					dir1->inode->up_dir = thread_current()->pwd;
//					if(thread_current()->pwd == NULL)
//						dir1->inode->up_dir = dir_open_root ();
					if(thread_current()->pwd != NULL)
						dir_close(thread_current()->pwd);
					thread_current()->pwd = dir1;
					f->eax = true;
				}
			break;
		case SYS_MKDIR:
			if(*(char **)(f->esp + 4) == NULL)
			{
				thread_current () -> exit_status = -1;
				thread_exit();
			}
			if(strlen(*(char **)(f->esp + 4)) == 0)
			{
				f->eax = false;
				break;
			}
			else
			{
			char n[strlen(*(char**)(f->esp+4))+1];
			char m[strlen(*(char**)(f->esp+4))+1];
			memset (n, 0, strlen(*(char**)(f->esp + 4)) +1);
			memset (m, 0, strlen(*(char**)(f->esp + 4)) + 1);
			strlcpy (n, *(char **)(f->esp + 4), strlen(*(char**)(f->esp +4))+1);
			strlcpy (m, *(char **)(f->esp + 4), strlen(*(char **)(f->esp + 4))+1);

			struct dir* dir;

			if(n[0] == '/' || dir == NULL)
				dir = dir_open_root ();
			else
				dir = dir_reopen(thread_current()->pwd);
			char *token, *save_ptr, *real_name = NULL;

			for(token = strtok_r(n, "/", &save_ptr); token != NULL; token = strtok_r(NULL, "/", &save_ptr))
					real_name = token;
			

			for(token = strtok_r(m, "/", &save_ptr); token != NULL; token = strtok_r(NULL, "/", &save_ptr))
			{
				if(!strcmp(token, "."))
					continue;
				else if(!strcmp(token, ".."))
					dir = dir_open(inode_open(dir->inode->up_dir));
				else
				{
					struct inode* in;
//					char tmp[strlen(token)+1];
//					memset(tmp, 0, strlen(token)+1);
//					strlcpy(tmp, token, strlen(token)+1);
					if(save_ptr[0] == '\0')
					{
						if(dir_lookup(dir, real_name, &in))
						{
							f->eax = false;
//							dir_close(dir);
							break;
						}
						else
						{
							lock_acquire(&open_lock);
							f->eax = filesys_create (*(char**)(f->esp + 4), 0, true);
							lock_release(&open_lock);
//							if(dir != thread_current()->pwd)
							dir_close(dir);
							break;
						}
					}
					else
					{
						if(dir_lookup(dir, token, &in))
						{
							if(in->dir)
							{
								dir_close(dir);
								dir = dir_open(in);
							}
							else
							{
								f->eax = false;
//								dir_close(dir);
								break;
							}
						}
						else
						{
							f->eax = false;
	//						dir_close(dir);
							break;
						}
					}
				}
			}
			}
			break;
		case SYS_READDIR:
			t = list_entry(list_find(&thread_current()->open_list, fd_same, (f->esp + 16)), struct open_elem, elem);
			if(t == NULL)
			{
				thread_current ()->exit_status = -1;
				thread_exit ();
			}
			if(!t->ddir)
			{
				f->eax = false;
				break;
			}
			else
				f->eax = dir_readdir( t->dir, *(char **)(f->esp + 20));
		break;
	case SYS_ISDIR:
			t = list_entry(list_find(&thread_current()->open_list, fd_same, (f->esp + 4)), struct open_elem, elem);
			if(t == NULL)
			{
				thread_current ()->exit_status = -1;
				thread_exit ();
			}
			if(!t->ddir)
			{
				f->eax = false;
				break;
			}
			else
				f->eax = true;
		break;
	case SYS_INUMBER:
			t = list_entry(list_find(&thread_current()->open_list, fd_same, (f->esp + 4)), struct open_elem, elem);
			if(t == NULL)
			{
				thread_current ()->exit_status = -1;
				thread_exit ();
			}
			if(!t->ddir)
			{
				f->eax = inode_get_inumber(t->open_file->inode);
				break;
			}
			else
			{
				f->eax = inode_get_inumber(t->dir->inode);
				break;
			}
		break;
//		default:
//			printf ("system call!\n");
//			break;
	}
}
