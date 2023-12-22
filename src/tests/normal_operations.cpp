
#include <my_malloc.h>

#include <stdlib.h>

#include <gtest/gtest.h>

#define POOL_SIZE ((uint64_t)(1024U * 1024U * 256U))

// THIS IS hardcoded, since there's no better way of knowing this
#define STATIC_MEMORYBLOCK_OVERHEAD (24UL)

TEST(MyMalloc, normalOperations) {
	my_allocator_init(POOL_SIZE, true);

	void* const ptr1 = my_malloc(1024);
	EXPECT_NE(ptr1, nullptr);
	memset(ptr1, 0xFF, 1024);

	void* ptr2 = my_malloc(1024);
	EXPECT_GT(ptr2, ptr1);
	memset(ptr2, 0xFF, 1024);
	const uint64_t overhead = (ptrdiff_t)ptr2 - (ptrdiff_t)ptr1 - 1024;

	void* const startOfRegion = (unsigned char*)ptr1 - STATIC_MEMORYBLOCK_OVERHEAD - overhead;

	my_free(ptr1);
	my_free(nullptr);

	// Reuse first block
	void* ptr3 = my_malloc(1024);
	EXPECT_EQ(ptr3, ptr1);

	// Create a 2048 byte hole
	void* ptr4 = my_malloc(3072);
	memset(ptr4, 0xFF, 3072);
	void* ptr5 = my_malloc(2048);
	memset(ptr5, 0xFF, 2048);
	void* ptr6 = my_malloc(2048);
	memset(ptr6, 0xFF, 2048);
	EXPECT_GT(ptr5, ptr4);
	EXPECT_GT(ptr6, ptr5);
	my_free(ptr5);

	// Fill 2048 byte hole with two new allocations
	void* ptr7 = my_malloc(1024);
	memset(ptr7, 0xFF, 1024);
	void* ptr8 = my_malloc(1024 - overhead);
	EXPECT_EQ(ptr7, ptr5);
	EXPECT_EQ((intptr_t)ptr8, (intptr_t)ptr5 + 1024 + (intptr_t)overhead);

	// Check that all blocks are merged
	my_free(ptr4);
	my_free(ptr8);
	my_free(ptr7);

	void* ptr9 = my_malloc(4096);
	EXPECT_EQ(ptr9, ptr4);

	my_free(ptr9);
	my_free(ptr6);
	my_free(ptr2);
	my_free(ptr3);

	// Lastly, allocate all available memory
	void* ptr10 = my_malloc(POOL_SIZE - overhead - STATIC_MEMORYBLOCK_OVERHEAD);
	EXPECT_NE(ptr10, nullptr);

	// Check new mapped block, so that this memory is after the first allocation
	void* ptr11 = my_malloc(1);
	EXPECT_TRUE((unsigned char*)ptr11 > (unsigned char*)startOfRegion + POOL_SIZE ||
	            (unsigned char*)ptr11 < (unsigned char*)startOfRegion);

	my_free(ptr10);
	my_free(ptr11);

	my_allocator_destroy();
}
