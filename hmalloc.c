
#include <stdlib.h>
#include <sys/mman.h>
#include <stdio.h>

#include "hmalloc.h"

// linked list for free blocks
typedef struct free_block {
	size_t size;
    struct free_block* next;
} free_block;

const size_t PAGE_SIZE = 4096;
static hm_stats stats; // This initializes the stats to 0.
static free_block* free_list;

// accumulates each block
long free_list_counter(free_block* list, long length) {
	if (!list)
		return length;
	else
		return free_list_counter(list->next, length + 1);
}

// returns the number of blocks
long
free_list_length()
{
    return free_list_counter(free_list, 0);
}

// returns stats for the program
hm_stats*
hgetstats()
{
    stats.free_length = free_list_length();
    return &stats;
}

// prints stats
void
hprintstats()
{
    stats.free_length = free_list_length();
    fprintf(stderr, "\n== husky malloc stats ==\n");
    fprintf(stderr, "Mapped:   %ld\n", stats.pages_mapped);
    fprintf(stderr, "Unmapped: %ld\n", stats.pages_unmapped);
    fprintf(stderr, "Allocs:   %ld\n", stats.chunks_allocated);
    fprintf(stderr, "Frees:    %ld\n", stats.chunks_freed);
    fprintf(stderr, "Freelen:  %ld\n", stats.free_length);
}

// returns the number of pages to fit xx bytes with yy bytes per page
static
size_t
div_up(size_t xx, size_t yy)
{
    // This is useful to calculate # of pages
    // for large allocations.
    size_t zz = xx / yy;

    if (zz * yy == xx) {
        return zz;
    }
    else {
        return zz + 1;
    }
}

// combines blocks that are connected
void coalesce(free_block* blk1, free_block* blk2) {
	
	if ((void*)blk1 + (blk1->size) == (void*)blk2) {
		
		blk1->size += blk2->size;
		blk1->next = blk2->next;
	
		coalesce(blk1, blk2->next);
	}
}

// inserts size bytes into the list of blocks
void freelist_insert(void* ptr, size_t size, free_block* list, free_block* previous) {

	if (list == NULL || ptr <= (void*) list) { // add as first block
		free_list = ptr;
		free_list->size = size;
		free_list->next = list;

		coalesce(free_list, list);
	}
	else if ((void*) list < ptr && (ptr <= (void*) list->next || list->next == NULL)) {
		free_block* new = ptr;
		new->next = list->next;
		new->size = size;
		list->next = new;

		coalesce(list, new);
	}
	else
		freelist_insert(ptr, size, list->next, list);
}

// finds the first block that is at least size bytes big
void* find_block(size_t size, free_block* list, free_block* previous) {

	if (!list)
		return NULL;
	else if (list->size >= size) {
		void* ptr = list;
		if (previous == NULL)
			free_list = list->next;
		else
			previous->next = list->next;
		return ptr;
	}
	else
		return find_block(size, list->next, list);
}

// allocates size memory for the user
void*
hmalloc(size_t size)
{
	if (stats.chunks_allocated == 0) {
		free_list = NULL;
	}

    stats.chunks_allocated += 1;
    size += sizeof(size_t); // add space for block size

	void *ptr;

	if (size < PAGE_SIZE) {
		ptr = find_block(size, free_list, NULL);

		size_t leftover;
		if (!ptr) {
			ptr = mmap(NULL, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
			stats.pages_mapped += 1;
			leftover = PAGE_SIZE - size;
		}
		else {
			leftover = ((free_block*) ptr)->size - size;
		}

		if (leftover > sizeof(free_block))
			freelist_insert(ptr + size, leftover, free_list, NULL);

	}
	else {
		size_t page_num = div_up(size, PAGE_SIZE);
		ptr = mmap(NULL, page_num * PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
		stats.pages_mapped += page_num;
	}

	size_t* tmp = ((size_t*) ptr);
	*tmp = size;	

    return ptr + sizeof(size_t); // pointer after size
}

// frees the given pointer
void
hfree(void* item)
{

    stats.chunks_freed += 1;
	item -= sizeof(size_t); // start ptr

	size_t size = *(size_t*) item; // size

	if (size < PAGE_SIZE) {
		freelist_insert(item, size, free_list, NULL);
	}
	else {
		munmap(item - sizeof(size_t), size);
		stats.pages_unmapped += div_up(size, PAGE_SIZE);
	}
}

