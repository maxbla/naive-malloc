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

enum test_result {
	TEST_SUCCESS = 0,
	ALLOCATION_LARGE = 1,	//Allocation was larger than expected
	ALLOCATION_SMALL = 2,	//Allocation was smaller than expected
	NOT_FREED = 4,			//Call to free didn't work
	ALLOC_FAILED = 8,		//Allocation failed
};

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
	naive_free_internal(void_to_page_header(ptr));
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

/** number of pages of memory used 
 *  used to determine if call to free() worked */ 
static unsigned long get_used_memory(void) {
	FILE* mem_info = fopen("/proc/self/statm", "r");
	/*
	Documentation on statm:
	Provides information about memory usage, measured in pages.
		The columns are:
		size		(1) total program size
					(same as VmSize in /proc/[pid]/status)
		resident	(2) resident set size
					(same as VmRSS in /proc/[pid]/status)
		shared		(3) number of resident shared pages (i.e., backed by a file)
					(same as RssFile+RssShmem in /proc/[pid]/status)
		text		(4) text (code)
		lib			(5) library (unused since Linux 2.6; always 0)
		data		(6) data + stack
		dt			(7) dirty pages (unused since Linux 2.6; always 0)
	 */
	if (mem_info == NULL) {
		perror("Failed to open /proc/self/statm in get_used_memory()");
		assert(0);
	}
	unsigned long size, resident, shared, text, lib, data, dt;
	// each field has a maximum length of 19 = log_10(2^(64-1)) and there are 
	//7 fields in /proc/self/statm
	// With space delimiters and a trailing \0 this comes out to (19+1)*7 + 1 = 141
	// Round to 150 just to be safe
	char buf[150];
	fgets(buf, sizeof(buf), mem_info);
	sscanf(buf, "%lu %lu %lu %lu %lu %lu %lu",
		&size, &resident, &shared, &text, &lib, &data, &dt);
	fclose(mem_info);
	return size;
}

static enum test_result test_variable_malloc(size_t size) {
	// const unsigned ALLOCATION_LARGE = 1;
	// const unsigned ALLOCATION_SMALL = 2;
	// const unsigned NOT_FREED = 4;
	// const unsigned MALLOC_FAILED = 8;
	unsigned ret = TEST_SUCCESS;
	unsigned long page_size = get_page_size();
	// size rounded down to the nearest multiple of page_size
	size_t min_expected_page_allocations = (size + sizeof(struct page_header)) / page_size;
	// size rounded up to the nearest multiple of page_size
	// TODO: deal with overflow
	size_t max_expected_page_allocations = (size + sizeof(struct page_header)) / page_size + 1;

	unsigned long before_malloc = get_used_memory();
	void* ptr = naive_malloc(size);
	if (ptr == NULL) {
		return ALLOC_FAILED;
	}
	unsigned long after_malloc = get_used_memory();
	unsigned long allocated = after_malloc - before_malloc;
	if (allocated < min_expected_page_allocations) {
		ret |= ALLOCATION_SMALL;
	}
	if (allocated > max_expected_page_allocations) {
		ret |= ALLOCATION_LARGE;
	}
	naive_free(ptr);
	unsigned long after_free = get_used_memory();
	if (before_malloc != after_free) {
		ret |= NOT_FREED;
	}
	return ret;
}

static int test_malloc_zero(void) {
	return test_variable_malloc(0) == TEST_SUCCESS;
}

// allocations of less than one page of memory
static int test_malloc_small(void) {
	for (size_t i=1; i<get_page_size()-sizeof(struct page_header); i++) {
		unsigned int result = test_variable_malloc(i);
		fflush(stdout);
		if (result) {
			printf("result: %d\n", result);
			fflush(stdout);
			assert(!result);
		}
	}
	return 0;
}

static int test_malloc_large(void) {
	const unsigned long long _2pow64 =  18446744073709551615ull;
	const unsigned long long _2pow64div10 = _2pow64/10;
	unsigned long long size = get_page_size()-sizeof(struct page_header);
	unsigned increment = 2; //multiply by this number each time
	for (; size<_2pow64div10; size*=increment) {
		unsigned int result = test_variable_malloc((size_t)size);
		if (result & ALLOC_FAILED) {
			break;
		}
		if (result != TEST_SUCCESS) {
			printf("Large Alloc Failed. Result:%d\n", result);
			assert(result == TEST_SUCCESS);
		}
	}
	const unsigned long long largest_sucessful_alloc = size/increment;
	if (largest_sucessful_alloc > 1000ull*1000ull*1000ull) {
		printf("largest successfull alloc: %llu GB\n", largest_sucessful_alloc/1000ull/1000ull/1000ull);
	} else if (largest_sucessful_alloc > 1000ull*1000ull) {
		printf("largest successfull alloc: %llu MB\n", largest_sucessful_alloc/1000ull/1000ull);
	} else if (largest_sucessful_alloc > 1000ull) {
		printf("largest successfull alloc: %llu KB\n", largest_sucessful_alloc/1000ull);
	} else {
		printf("largest successfull alloc: %llu B\n", largest_sucessful_alloc);
	}
	return 0;
}

int main(void) {
	test_malloc_zero();
	test_malloc_small();
	test_malloc_large();
	return 0;
}
