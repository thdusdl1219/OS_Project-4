#ifndef PAGEH
#define PAGEH

#include <hash.h>
#include "filesys/file.h"

struct sup_page_elem
{
	struct hash_elem elem;
	bool load;

	struct file* file;
	size_t read_bytes;
	size_t zero_bytes;
	off_t offset;
	bool writable;

	size_t swap_index;
	bool swap;

	uint8_t* uaddr;
};
void sup_page_init(struct hash* sup);
void sup_page_destroy(struct hash* sup);
struct sup_page_elem* get_page_elem(void* addr);

void change_page_table_close(uint8_t* addr);
bool add_to_page_table(struct file* file, size_t read_bytes, size_t zero_bytes, uint8_t* upage, off_t ofs, bool writable);
bool add_to_page_table_in_stack(uint8_t* addr);
bool remove_page_table_unmmap(uint8_t* addr);

bool load_stack_page(struct sup_page_elem* spe);
bool load_lazy_page(struct sup_page_elem* spe);

#endif
