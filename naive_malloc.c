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

/**
 * Takes a void* pointer as returned by malloc and returns the corresponding
 * page_header. Fails if the initial pointer was not returned by malloc
 */
static struct page_header* void_to_page_header(void* ptr) {
	return (struct page_header*)((char*)ptr - sizeof(struct page_header));
}

/** The simplest free()-ish function possible 
 *  Fails if the user has managed to modify the malloc header 
 */
void naive_free(void* ptr) {
	if (ptr == NULL) {
		return;
	}
	struct page_header* header = void_to_page_header(ptr);
	int res = munmap(header, header->pages*header->page_size);
	if (res == -1) {
		puts("Error freeing!");
	}
}

/** The simplest malloc()-ish function possible 
 *  
 *  Allocates the required number of pages of memory, places some header 
 *  information at the start of the first page, then returns a pointer to the
 *  first byte after the header
 *  Allocates at least one page of memory for each call to simplify the logic
 *  Very inefficient for many small mallocs, but effective for one big malloc
 *  returns
 *   ________ _______ _______ ____     Page boundary
 *  | number |       |       |    |    |
 *  |   of   |data[0]|data[1]|... |    |
 *  | pages  |       |       |    |    |
 *   -------- ------- ------- ----
 *
 *  |--------|-------|
 *   size_t   sizeof(char)
 *           |--------------------|
 *            sizeof(char) * bytes
 */
static struct page_header* naive_malloc_internal(size_t size) {
	if (size == 0) {
		return NULL;
	}
	/* size in bytes of a page of memory */
	size_t page_size;
	{
		long tmp_pagesize = sysconf(_SC_PAGESIZE);
		const size_t default_pagesize = 4096;
		page_size = tmp_pagesize < 0 ? default_pagesize : (size_t)tmp_pagesize;
	}
	/* size in bytes that needs to be allocated */
	size_t size_with_header = size + sizeof(struct page_header);
	/* number of pages to allocate */
	size_t pages = size_with_header / page_size + 1;
	size_t mmap_size = pages * page_size;
	struct page_header* ptr = mmap(0, mmap_size,
		PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (ptr == MAP_FAILED) {
		return NULL;
	}
	/* copy values to page header */
	ptr->pages = pages;
	ptr->page_size = page_size;
	return ptr;
}

void* naive_malloc(size_t size) {
	return naive_malloc_internal(size)->page_data;
}

/** Helper function. Zero initalized memory */
static void zero_ptr(char* start, size_t size) {
	for(char* ptr = start; ptr<start+size; ptr++) {
		*ptr = 0;
	}
}

void* naive_calloc(size_t nmemb, size_t size) {
	size_t bytes = nmemb * size;
	if (nmemb == 0 || size == 0) {
		return NULL;
	}
	if (bytes / nmemb != size) { // overflow check
		if (!errno) {
			errno = EINVAL;
		}
		return NULL;
	}
	struct page_header* header = naive_malloc_internal(bytes);
	zero_ptr((char*)header, bytes + sizeof(struct page_header));
	return header->page_data;
}

void* naive_realloc(void* ptr, size_t size) {
	// shortcut: if there is already enough allocated space,
	// return passed pointer
	struct page_header* page = void_to_page_header(ptr);
	size_t start_size = page->pages * page->page_size;
	if (size < start_size) {
		return ptr;
	}
	struct page_header* new_page = naive_malloc_internal(size);
	for (size_t i=0; i<start_size; i++) {
		new_page->page_data[i] = page->page_data[i];
	}
	naive_free(ptr);
	return new_page->page_data;
}

/** number of bytes of memory used 
 *  used to determine if call to free() worked */ 
static long get_used_memory(void) {
	FILE* mem_info = fopen("/proc/self/statm", "r");
	if (mem_info == NULL) {
		perror("Failed to open /proc/self/statm in get_used_memory()");
		return -1;
	}
	long size, resident, shared, text, lib, data, dt;
	// each field has a maximum length of 19 = log_10(2^(64-1)) and there are 
	//7 fields in /proc/self/statm
	// With space delimiters and a trailing \0 this comes out to (19+1)*7 + 1 = 141
	// Round to 150 just to be safe
	char buf[150];
	fgets(buf, sizeof(buf), mem_info);
	sscanf(buf, "%ld %ld %ld %ld %ld %ld %ld",
		&size, &resident, &shared, &text, &lib, &data, &dt);
	fclose(mem_info);
	return size;
}

int main(void) {
	long before_malloc = get_used_memory();
	printf("used pages of memory before allocation: %ld\n", before_malloc);
	size_t array_size = 100000000l;
	int* ptr = naive_malloc(array_size*sizeof(int));
	long before_free = get_used_memory();
	printf("used pages of memory after allocation: %ld\n", before_free);
	naive_free(ptr);
	long after_free = get_used_memory();
	printf("used pages of memory after free: %ld\n", after_free);
	if(after_free >= before_free) {
		perror("Call to naive_free() failed. This is unacceptable!");
		return -1;
	}
	return 0;
}
