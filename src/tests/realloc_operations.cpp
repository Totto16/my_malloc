
#include <my_malloc.h>

#include <stdlib.h>

#include <gtest/gtest.h>

#define POOL_SIZE ((uint64_t)(1024U * 1024U * 256U))

// THIS IS hardcoded, since there's no better way of knowing this
#define STATIC_MEMORYBLOCK_OVERHEAD (24UL)

TEST(MyMalloc, reallocOperations) {
	my_allocator_init(POOL_SIZE, true);

	// the same as my_malloc(1024)
	void* const ptr1 = my_realloc(nullptr, 1024);
	EXPECT_NE(ptr1, nullptr);
	memset(ptr1, 0xEE, 1024);

	void* ptr2 = my_realloc(ptr1, 3072);

	EXPECT_EQ(ptr2, ptr1);
	for(size_t i = 0; i < 1024; ++i) {
		EXPECT_EQ(((unsigned char*)ptr2)[i], 0xEE);
	}
	memset(((unsigned char*)ptr2) + 1024, 0xFF, 2048);

	void* ptr3 = my_malloc(353534);
	EXPECT_NE(ptr3, nullptr);
	memset(ptr3, 0xDD, 353534);

	const uint64_t overhead = (ptrdiff_t)ptr3 - (ptrdiff_t)ptr2 - 3072;

	void* const startOfRegion = (unsigned char*)ptr1 - STATIC_MEMORYBLOCK_OVERHEAD - overhead;

	void* ptr4 = my_realloc(ptr1, 1024 * 1024);
	EXPECT_NE(ptr2, ptr4);
	for(size_t i = 0; i < 1024; ++i) {
		EXPECT_EQ(((unsigned char*)ptr4)[i], 0xEE);
	}
	for(size_t i = 1024; i < 3072; ++i) {
		EXPECT_EQ(((unsigned char*)ptr4)[i], 0xFF);
	}

	my_free(ptr3);
	// the same as my_free(ptr4)
	my_realloc(ptr4, 0);

	// Lastly, allocate all available memory
	void* ptr5 = my_malloc(POOL_SIZE - overhead - STATIC_MEMORYBLOCK_OVERHEAD);
	EXPECT_NE(ptr5, nullptr);

	// Check new mapped block, so that this memory is after the first allocation
	void* ptr6 = my_malloc(1);
	EXPECT_TRUE((unsigned char*)ptr6 > (unsigned char*)startOfRegion + POOL_SIZE ||
	            (unsigned char*)ptr6 < (unsigned char*)startOfRegion);

	my_free(ptr5);
	my_free(ptr6);

	my_allocator_destroy();
}
