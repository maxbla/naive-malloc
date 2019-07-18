#include <naive_malloc.h>

static unsigned long get_page_size(void) {
	// initalize to invalid value
	static unsigned long page_size = 0;
	if (page_size == 0) {
		long tmp = sysconf(_SC_PAGESIZE);
		page_size = tmp < 0 ? 4096 : (unsigned long)tmp;
	}
	return page_size;
}

/**
 * Takes a void* pointer as returned by malloc and returns the corresponding
 * page_header. Fails if the initial pointer was not returned by malloc
 */
static struct page_header* void_to_page_header(void* ptr) {
	return (struct page_header*)((char*)ptr - sizeof(struct page_header));
}

static void naive_free_internal(struct page_header* header) {
	if (header == NULL) {
		return;
	}
	const size_t size = header -> pages * header -> page_size;
	int res = munmap(header, size);
	if (res == -1) {
		perror("Error freeing!");
	}
} 

/** The simplest free()-ish function possible 
 *  Fails if the user has managed to modify the malloc header 
 */
void naive_free(void* ptr) {
	if (ptr != NULL) {
		naive_free_internal(void_to_page_header(ptr));
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
	size_t page_size = get_page_size();
	/* size in bytes that needs to be allocated */
	size_t size_with_header = size + sizeof(struct page_header);
	/* number of pages to allocate */
	/* TODO: fix case where size_with_header == page_size */
	size_t pages = size_with_header / page_size + 1;
	size_t mmap_size = pages * page_size;
	void* ptr = mmap(0, mmap_size, PROT_READ|PROT_WRITE, 
	MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (ptr == MAP_FAILED) {
		return NULL;
	}
	struct page_header* page = ptr;
	/* copy values to page header */
	page->pages = pages;
	page->page_size = page_size;
	return page;
}

void* naive_malloc(size_t size) {
	struct page_header* result = naive_malloc_internal(size);
	return result == NULL ? NULL : result->page_data;
}

/** Helper function. Zero initalize memory */
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
