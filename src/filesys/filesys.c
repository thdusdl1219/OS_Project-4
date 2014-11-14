#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "cache.h"
#include "threads/thread.h"
#include "threads/malloc.h"
/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();
	cache_init();
  if (format) 
    do_format ();

  free_map_open ();
//	thread_current() ->pwd = dir_open_root ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
	all_cache_go_back();
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, bool ddir) 
{
  block_sector_t inode_sector = 0;
//	struct dir *dir = dir_open_root();
  struct dir *dir = get_real_dir (name, true);
	char n[strlen(name) + 1];
	memset (n, 0, strlen(name) + 1);
	memcpy (n, name, strlen(name));
	char *token, *save_ptr;
	char *real_name = NULL;
	for(token = strtok_r(n, "/", &save_ptr); token != NULL; token = strtok_r(NULL, "/", &save_ptr))
			real_name = token;
	char* real_file_name = malloc(strlen(real_name) + 1);
	memcpy(real_file_name, real_name, strlen(real_name)+1);

  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, ddir)
                  && dir_add (dir, real_file_name, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
}

struct dir* get_real_dir(const char* name, bool open)
{
	char n[strlen(name) + 1];
	memset (n, 0, strlen(name) + 1);
	memcpy (n, name, strlen(name));
	struct dir* dir;
	if(n[0] == '/' || thread_current()->pwd == NULL)
		dir = dir_open_root ();
	else
	{
		if(!thread_current()->pwd->inode->removed)
			dir = dir_reopen(thread_current()->pwd);
		else
			return NULL;
	}

	char *token, *save_ptr;
	for(token = strtok_r(n, "/", &save_ptr); token != NULL; token = strtok_r(NULL, "/", &save_ptr))
	{
		if(!strcmp(token, "."))
			continue;
		else if(!strcmp(token, ".."))
		{
			dir = dir_reopen(dir->inode->up_dir); 
		}
		else
		{
			struct inode* in;
			if(save_ptr[0] == '\0' && open)
				return dir;
			if(dir_lookup(dir, token, &in))
			{
				if(in->dir)
				{
					dir_close(dir);
					dir = dir_open(in);
					if(dir->inode->removed)
						return NULL;
				}
				else
				{
					inode_close(in);
				}
			}
			else if(!open)
			{
				return NULL;
			}
		}
	}
	return dir;	
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
//  struct dir *dir = dir_open_root ();
  struct dir *dir = get_real_dir (name, true);
  struct inode *inode = NULL;
	char n[strlen(name) + 1];
	memset (n, 0, strlen(name) + 1);
	memcpy (n, name, strlen(name));
	char *token, *save_ptr;
	char *real_name = (char *)name;
	for(token = strtok_r(n, "/", &save_ptr); token != NULL; token = strtok_r(NULL, "/", &save_ptr))
			real_name = token;
	char* real_file_name = malloc(strlen(real_name) + 1);
	memcpy(real_file_name, real_name, strlen(real_name) + 1);

	if(dir == NULL)
		return NULL;
	if(!strcmp(name, "/"))
		return file_open (dir->inode);

	if(!strcmp(name, "."))
		return file_open (dir->inode);

  if (dir != NULL)
    dir_lookup (dir, real_file_name, &inode);
  dir_close (dir);


  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
//  struct dir *dir = dir_open_root ();
  struct dir *dir = get_real_dir (name, true);
	char n[strlen(name) + 1];
	memset (n, 0, strlen(name) + 1);
	memcpy (n, name, strlen(name));
	if(!strcmp(name, "/"))
		return false;
	char *token, *save_ptr, *real_name = NULL;
	for(token = strtok_r(n, "/", &save_ptr); token != NULL; token = strtok_r(NULL, "/", &save_ptr))
			real_name = token;
	char* real_file_name = malloc(strlen(real_name) + 1);
	memcpy(real_file_name, real_name, strlen(real_name)+1);

  bool success = dir != NULL && dir_remove (dir, real_file_name);
  dir_close (dir); 

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 55))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
