#include <stddef.h>
#include <sys/mman.h>

#include <stdio.h>

/** The simplest malloc()-ish function possible */
void* naive_malloc(size_t bytes) {
	if (bytes == 0) {
		return NULL;
	}
	size_t size_with_header = bytes + sizeof(size_t);
	void* ptr = mmap(0, size_with_header,
		PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	if (ptr == MAP_FAILED) {
		//handle error
	}
	*(size_t*)ptr = bytes; //save size of allocation for freeing
	return (size_t*)ptr+1;
}

/** The simplest free()-ish function possible 
 * Fails only if th user has managed to modify the malloc header 
 */
void naive_free(void* ptr) {
	void* header_start = (size_t*)ptr - 1;
	munmap(header_start, *(size_t*)ptr);
}

int main() {
	int* i = naive_malloc(sizeof(int));
	printf("Initial values:\t%d,\t%d\n", *i, *(i+1));
	printf("Saved size:\t%ld\n", *((size_t*)i-1));
	*i = 123;
	*(i+1) = 234;
	printf("Initial values:\t%d,\t%d\n", *i, *(i+1));
	printf("Saved size:\t%ld\n", *((size_t*)i-1));
	naive_free(i);
	while(1) {;}
}
