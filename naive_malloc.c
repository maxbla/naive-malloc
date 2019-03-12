#include <stddef.h> //size_t
#include <sys/mman.h> //mmap
#include <unistd.h> // sysconf(_SC_PAGESIZE)
#include <errno.h>

#include <stdio.h>
#include <assert.h>

/**number of fields of size_t in the page header*/
static const size_t header_size = 2;

struct page_header {
	size_t pages;
	size_t page_size;
	char page_data[]; //the rest of the page(s) of memory
};

/** The simplest malloc()-ish function possible 
 *  
 *  Allocates the required number of pages of memory, places some header 
 *  information at the start of the first page, then returns a pointer to the
 *  first byte after the header
 *  Allocates at least one page of memory for each call to simplify the logic
 *  Very inefficient for many small mallocs, but effective for one big malloc
 *  returns
 *   ________ ____ ____ ____        Page boundary
 *  | number |    |more|    |       |
 *  |   of   |data|data|etc |  ...  |
 *  | pages  |    |    |    |       |
 *   -------- ---- ---- ----
 *
 *  |--------|----|
 *   size_t   sizeof(char)
 *         |----------------|
 *         sizeof(char) * bytes
 */
void* naive_malloc(size_t size) {
	if (size == 0) {
		return NULL;
	}
	/** size in bytes of a page of memory */
	size_t page_size = sysconf(_SC_PAGESIZE) == -1 ? 4096 : sysconf(_SC_PAGESIZE);
	size_t size_with_header = size + header_size*sizeof(size_t);
	size_t pages = size_with_header / page_size + 1;
	size_t mmap_size = pages * page_size;
	struct page_header* ptr = mmap(0, mmap_size,
		PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (ptr == MAP_FAILED) {
		return NULL;
	}
	ptr->pages = pages;
	ptr->page_size = page_size;
	return ptr->page_data;
}

void zero_ptr(char* start, size_t size) {
	for(char* ptr = start; ptr<start+size; ptr++) {
		*ptr = 0;
	}
}

void* naive_calloc(size_t nmemb, size_t size) {
	size_t bytes = nmemb * size;
	if (nmemb != 0 && bytes / nmemb != size) { // overflow check
		if (!errno) {
			errno = EINVAL;
		}
		return NULL;
	}
	struct page_header* header = (struct page_header*)
			((size_t*)naive_malloc(size)-header_size);
	zero_ptr(header->page_data, bytes);
	return header->page_data;
}

/** The simplest free()-ish function possible 
 *  Fails if the user has managed to modify the malloc header 
 */
void naive_free(void* ptr) {
	if (ptr == NULL) {
		return;
	}
	struct page_header* header = (struct page_header*)((size_t*)ptr-header_size);
	int res = munmap(header, header->pages*header->page_size);
	if (res == -1) {
		puts("Error freeing!");
	}
}

int main() {
	size_t array_size = 100000000l;
	int* ptr = naive_malloc(array_size*sizeof(int));
	for(size_t i=0; i<array_size; i++) {
		ptr[i] = (int)i;
	}
	for(size_t i=0; i<array_size; i++) {
		assert(ptr[i] == (int)i);
	}
	naive_free(ptr);
	sleep(60); //wait so you can make sure naive_free() actually worked
}
