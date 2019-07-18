#include <stddef.h> //size_t
#include <sys/mman.h> //mmap
#include <unistd.h> // sysconf(_SC_PAGESIZE)
#include <errno.h>

#include <stdio.h>
#include <assert.h>

struct page_header {
	size_t pages;
	size_t page_size;
	char page_data[]; //the rest of the page(s) of memory
};

/** The simplest free()-ish function possible 
 *  Fails if the user has managed to modify the malloc header 
 */
void naive_free(void* ptr);

void* naive_malloc(size_t size);

void* naive_calloc(size_t nmemb, size_t size);

void* naive_realloc(void* ptr, size_t size);