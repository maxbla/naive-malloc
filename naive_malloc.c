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
	/* size in bytes of a page of memory */
	size_t page_size = sysconf(_SC_PAGESIZE) == -1 ? 4096 : sysconf(_SC_PAGESIZE);
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
	return ptr->page_data;
}

/** Helper function. Zero initalized memory */
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
	struct page_header* header = naive_malloc(size)-sizeof(struct page_header);
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
	struct page_header* header = ptr-sizeof(struct page_header);
	int res = munmap(header, header->pages*header->page_size);
	if (res == -1) {
		puts("Error freeing!");
	}
}

/** number of bytes of memory used 
 *  used to determine if call to free() worked */ 
static long get_used_memory_info() {
	FILE* mem_info = fopen("/proc/self/statm", "r");
	if (mem_info == NULL) {
		return -1;
	}
	const int max_digits = 19; /*maximum binary digits of 64-bit numbers*/
	const int num_fields = 7; /* integer fields in /proc/self/statm */
	const int buf_size = (max_digits+1)*num_fields+2;
	long size, resident, shared, text, lib, data, dt;
	char buf[buf_size];

	fgets(buf, buf_size, mem_info);
	sscanf(buf, "%ld %ld %ld %ld %ld %ld %ld",
		&size, &resident, &shared, &text, &lib, &data, &dt);
	fclose(mem_info);
	return size;
}

int main() {
	long before_malloc = get_used_memory_info();
	printf("used pages of memory before allocation: %ld\n", before_malloc);
	size_t array_size = 100000000l;
	int* ptr = naive_malloc(array_size*sizeof(int));
	long before_free = get_used_memory_info();
	printf("used pages of memory after allocation: %ld\n", before_free);
	naive_free(ptr);
	long after_free = get_used_memory_info();
	printf("used pages of memory after free: %ld\n", after_free);
	if(after_free >= before_free) {
		perror("Call to naive_free() failed. This is unacceptable!");
		return -1;
	}
	return 0;
}
