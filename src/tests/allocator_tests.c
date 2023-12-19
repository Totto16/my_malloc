#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "allocator_tests.h"

#ifdef NDEBUG
#define ASSERT(x) \
	do { \
		if(!x) { \
			fprintf(stderr, "ASSERTION ERROR: %s: %s\n", __FILE__, __LINE__); \
			exit(1); \
		} \
	} while(0)
#else
#include <assert.h>
#define ASSERT(x) assert(x)
#endif

#define POOL_SIZE ((uint64_t)(1024U * 1024U * 256U))

void test_free_list_allocator(void) {
	my_allocator_init(POOL_SIZE);

	const void* too_large = my_malloc(POOL_SIZE);
	ASSERT(too_large == NULL);

	void* const ptr1 = my_malloc(1);
	ASSERT(ptr1 != NULL);
	printf("ptr1: %p\n", ptr1);

	const void* const base = (void*)((unsigned char*)ptr1 - sizeof(void*));

	void* const ptr2 = my_malloc(1);
	printf("ptr2: %p\n", ptr2);

	const uint64_t block_size = (ptrdiff_t)ptr2 - (ptrdiff_t)ptr1 - sizeof(void*);
	printf("block size is: %" PRIu64 " (assuming header size %" PRIu64 ")\n", block_size,
	       sizeof(void*));

	my_free(ptr1);
	void* const ptr3 = my_malloc(block_size);
	memset(ptr3, 0xFF, block_size);
	printf("ptr3: %p\n", ptr3);
	ASSERT(ptr1 == ptr3);

	void* const ptr4 = my_malloc(block_size);
	ASSERT(ptr4 > ptr2);

	// Figure out how large entire pool is
	void* end = NULL;
	while(1) {
		void* next = my_malloc(1);
		if(next != NULL) {
			end = (void*)((unsigned char*)next + block_size);
		} else {
			break;
		}
	}
	const uint64_t pool_size = (ptrdiff_t)end - (ptrdiff_t)base;
	// This is a lower bound, as the pool may not be evenly divisible by block + header size.
	ASSERT(pool_size <= POOL_SIZE);

	my_allocator_destroy();

	puts("All good!");
}

void test_best_fit_allocator(void) {
	my_allocator_init(POOL_SIZE);

	void* const ptr1 = my_malloc(1024);
	printf("ptr1: %p\n", ptr1);
	ASSERT(ptr1 != NULL);
	memset(ptr1, 0xFF, 1024);

	void* ptr2 = my_malloc(1024);
	printf("ptr2: %p\n", ptr2);
	ASSERT(ptr2 > ptr1);
	memset(ptr2, 0xFF, 1024);
	const uint64_t overhead = (ptrdiff_t)ptr2 - (ptrdiff_t)ptr1 - 1024;
	printf("Overhead (list header size) is %zu\n", overhead);

	my_free(ptr1);

	// Reuse first block
	void* ptr3 = my_malloc(1024);
	printf("ptr3: %p\n", ptr3);
	ASSERT(ptr3 == ptr1);

	// Create a 2048 byte hole
	void* ptr4 = my_malloc(3072);
	memset(ptr4, 0xFF, 3072);
	void* ptr5 = my_malloc(2048);
	memset(ptr5, 0xFF, 2048);
	void* ptr6 = my_malloc(2048);
	memset(ptr6, 0xFF, 2048);
	ASSERT(ptr5 > ptr4);
	ASSERT(ptr6 > ptr5);
	my_free(ptr5);

	// Fill 2048 byte hole with two new allocations
	void* ptr7 = my_malloc(1024);
	memset(ptr7, 0xFF, 1024);
	void* ptr8 = my_malloc(1024 - overhead);
	ASSERT(ptr7 == ptr5);
	ASSERT((intptr_t)ptr8 == (intptr_t)ptr5 + 1024 + (intptr_t)overhead);

	// Check that all blocks are merged
	my_free(ptr4);
	my_free(ptr8);
	my_free(ptr7);

	void* ptr9 = my_malloc(4096);
	ASSERT(ptr9 == ptr4);

	my_free(ptr9);
	my_free(ptr6);
	my_free(ptr2);
	my_free(ptr3);

	// Lastly, allocate all available memory
	void* ptr10 = my_malloc(POOL_SIZE - overhead);
	ASSERT(ptr10 != NULL);

	// Check OOM result
	void* ptr11 = my_malloc(1);
	ASSERT(ptr11 == NULL);

	my_free(ptr10);

	my_allocator_destroy();

	puts("All good!");
}
