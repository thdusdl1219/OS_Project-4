#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "cache.h"
#include <stdio.h>
/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define FILE_MAX_SIZE 8466432



struct indirect_block
{
	block_sector_t direct_block[128];
};

struct double_indirect_block
{
	block_sector_t indirect_block[128];
};

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
		block_sector_t block_ptr[14];
		int index;
		int indirect_index;
		int double_indirect_index;
    uint32_t unused[109];               /* Not used. */
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  	return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
		off_t length;
		off_t read_l;
		block_sector_t block_ptr[14];
		int index;
		int indirect_index;
		int double_indirect_index;
  };

off_t inode_extend(struct inode* inode, off_t length);
bool inode_made(struct inode_disk* disk_inode);
void inode_destroy(struct inode* inode);
void inode_destroy_indirect(block_sector_t *p, size_t sectors);
/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t size, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos < size)
	{
		if(pos < 512*12)
    	return inode->block_ptr[pos / 512];
		else if(pos < 512*128 + 512*12)
		{
			struct indirect_block block;
			pos -= 512*12;
			block_read(fs_device, inode->block_ptr[12], &block);
			return block.direct_block[pos / 512];			
		}
		else
		{
			struct double_indirect_block d_block;
			struct indirect_block block;
			pos -= (512*128 + 512*12);
			block_read(fs_device, inode->block_ptr[13], &d_block);
			int index = pos / (512*128);
			block_read(fs_device, d_block.indirect_block[index], &block);
			pos %= 512*128;
			return block.direct_block[pos / 512];
		}
	}
  else
    return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      disk_inode->length = length;
			if(length > FILE_MAX_SIZE)
				disk_inode->length = FILE_MAX_SIZE;
      disk_inode->magic = INODE_MAGIC;
			if(inode_made(disk_inode))
			{
				block_write(fs_device, sector, disk_inode);
      	success = true; 
      } 
      free (disk_inode);
    }
  return success;
}

bool inode_made(struct inode_disk* disk_inode)
{
	struct inode inode;
	int i;
	inode.length = 0;
	inode.read_l = 0;
	for(i = 0; i < 14; i++)
		inode.block_ptr[i] = 0;
	inode.index = 0;
	inode.indirect_index = 0;
	inode.double_indirect_index = 0;
	inode_extend(&inode, disk_inode->length);
	for(i = 0; i < 14; i++)
		disk_inode->block_ptr[i] = inode.block_ptr[i];
	disk_inode->index = inode.index;
	disk_inode->indirect_index = inode.indirect_index;
	disk_inode->double_indirect_index = inode.double_indirect_index;
	return true;
}

off_t inode_extend(struct inode* inode, off_t length)
{
	static char zeros[BLOCK_SECTOR_SIZE];
	size_t sectors = bytes_to_sectors(length);
	if(sectors == 0)
		return length;
	while(inode->index < 12)
	{
		free_map_allocate(1, &inode->block_ptr[inode->index]);
		block_write(fs_device, inode->block_ptr[inode->index], zeros);
		inode->index++;
		sectors--;
		if(sectors == 0)
			return length;
	}
	if(inode->index == 12)
	{
		struct indirect_block block;
		if(inode->indirect_index == 0)
			free_map_allocate(1, &inode->block_ptr[12]);
		else
			block_read(fs_device, inode->block_ptr[12], &block);
		while(inode->indirect_index < 128)
		{
			free_map_allocate(1, &block.direct_block[inode->indirect_index]);
			block_write(fs_device, block.direct_block[inode->indirect_index], zeros);
			inode->indirect_index++;
			sectors--;
			if(sectors == 0)
				break;
		}
		block_write(fs_device, inode->block_ptr[12], &block);
		if(inode->indirect_index == 128)
		{
			inode->index = 13;
			inode->indirect_index = 0;
		}
	}
	if(inode->index == 13)
	{
		static struct double_indirect_block double_block;
		if(inode->double_indirect_index == 0 && inode->indirect_index == 0)
			free_map_allocate(1, &inode->block_ptr[13]);
		else
			block_read(fs_device, inode->block_ptr[13], &double_block);
		while(inode->double_indirect_index < 128)
		{
			static struct indirect_block indirect_block;
			if(inode->indirect_index == 0)
				free_map_allocate(1, &double_block.indirect_block[inode->double_indirect_index]);
			else
				block_read(fs_device, double_block.indirect_block[inode->double_indirect_index], &indirect_block);
			while(inode->indirect_index < 128)
			{
				free_map_allocate(1, &indirect_block.direct_block[inode->indirect_index]);
				block_write(fs_device, indirect_block.direct_block[inode->indirect_index], zeros);
				inode->indirect_index++;
				sectors--;
				if(sectors == 0)
					break;
			}
			block_write(fs_device, double_block.indirect_block[inode->double_indirect_index], &indirect_block);
			if(inode->indirect_index == 128)
			{
				inode->indirect_index = 0;
				inode->double_indirect_index++;
			}
			if(sectors == 0)
				break;
		}
		block_write(fs_device, inode->block_ptr[13], &double_block);
	}
	return length;
}
/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  block_read (fs_device, inode->sector, &inode->data);
	inode->length = inode->data.length;
	inode->read_l = inode->data.length;
	inode->index = inode->data.index;
	inode->indirect_index = inode->data.indirect_index;
	inode->double_indirect_index = inode->data.double_indirect_index;
	int i;
	for(i = 0; i < 14; i ++)
	{
		inode->block_ptr[i] = inode->data.block_ptr[i];
	}
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{

  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
					inode_destroy(inode);
//          free_map_release (inode->data.start,
  //                          bytes_to_sectors (inode->data.length)); 
        }
			else
			{
				static struct inode_disk disk_inode;
				disk_inode.length = inode->length;
				disk_inode.magic = INODE_MAGIC;
				disk_inode.index = inode->index;
				disk_inode.indirect_index = inode->indirect_index;
				disk_inode.double_indirect_index = inode->double_indirect_index;
				int i;
				for(i = 0; i < 14 ; i++ )
					disk_inode.block_ptr[i] = inode->block_ptr[i];
				block_write(fs_device, inode->sector, &disk_inode);
			}

      free (inode); 
    }
}

void inode_destroy(struct inode* inode)
{
	size_t sectors = bytes_to_sectors(inode->length);
	int index = 0;
	while(sectors > 0 && index < 12)
	{
		free_map_release(inode->block_ptr[index], 1);
		sectors--;
		index++;
	}
	if(index == 12)
	{
		size_t indirect_sectors = sectors;
		if(sectors > 128)
			indirect_sectors = 128;
		inode_destroy_indirect(&inode->block_ptr[12], indirect_sectors);
		sectors -= indirect_sectors;
		if(sectors != 0)
			index++;
	}
	if(index == 13)
	{
		int i;
		struct double_indirect_block d_block;
		block_read(fs_device, inode->block_ptr[13], &d_block);
		int num_of_indirect = sectors / 128 + 1;
		for(i = 0; i < num_of_indirect; i++)
		{
			size_t indirect_sectors = sectors;
			if(sectors > 128)
				indirect_sectors = 128;
			inode_destroy_indirect(&d_block.indirect_block[i], indirect_sectors);
			sectors -= indirect_sectors;
		}
		free_map_release(inode->block_ptr[13], 1);
	}
}

void inode_destroy_indirect(block_sector_t *p, size_t sectors)
{
	unsigned i;
	struct indirect_block block;
	block_read(fs_device, *p, &block);
	for(i = 0; i < sectors; i++)
		free_map_release(block.direct_block[i], 1);
	free_map_release(*p, 1);
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;
	

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, inode->read_l, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode->length - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

					bounce = get_block_cache(sector_idx, false, true);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;
	volatile int test[128];
  if (inode->deny_write_cnt)
    return 0;

	if(offset + size > inode->length)
		inode->length = inode_extend(inode, offset + size);

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, inode->length, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;


			bounce = get_block_cache(sector_idx, true, false);
			memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
			block_write (fs_device, sector_idx, bounce);
			block_read (fs_device, sector_idx, (void *)test);
//			printf("%d \n", bounce[1]);
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->length;
}
