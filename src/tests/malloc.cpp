
#include <my_malloc.h>

#include <stdlib.h>

#include <gtest/gtest.h>

#define POOL_SIZE ((uint64_t)(1024U * 1024U * 256U))

TEST(MyMalloc, normalOperations) {
	my_allocator_init(POOL_SIZE);

	void* const ptr1 = my_malloc(1024);
	EXPECT_NE(ptr1, nullptr);
	memset(ptr1, 0xFF, 1024);

	void* ptr2 = my_malloc(1024);
	EXPECT_GT(ptr2, ptr1);
	memset(ptr2, 0xFF, 1024);
	const uint64_t overhead = (ptrdiff_t)ptr2 - (ptrdiff_t)ptr1 - 1024;

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
	void* ptr10 = my_malloc(POOL_SIZE - overhead);
	EXPECT_NE(ptr10, nullptr);

	// Check OOM result
	void* ptr11 = my_malloc(1);
	EXPECT_EQ(ptr11, nullptr);

	my_free(ptr10);

	my_allocator_destroy();
}

TEST(MyMalloc, reallocOperations) {
	my_allocator_init(POOL_SIZE);

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
	void* ptr5 = my_malloc(POOL_SIZE - overhead);
	EXPECT_NE(ptr5, nullptr);

	// Check OOM result
	void* ptr6 = my_malloc(1);
	EXPECT_EQ(ptr6, nullptr);

	my_free(ptr5);

	my_allocator_destroy();
}

TEST(MyMalloc, callBeforeInitializing) {

	EXPECT_EXIT({ my_malloc(1024); }, ::testing::ExitedWithCode(1),
	            "Calling malloc before initializing the allocator is prohibited!");
}

TEST(MyMalloc, doubleFree) {

	my_allocator_init(POOL_SIZE);

	void* const ptr1 = my_malloc(1024);
	EXPECT_NE(ptr1, nullptr);

	my_free(ptr1);

	EXPECT_EXIT({ my_free(ptr1); }, ::testing::ExitedWithCode(1),
	            "ERROR: You tried to free a already freed Block: 0x[0-9a-fA-F]{2,16}");
}

TEST(MyMalloc, reallocFreedBlock) {

	my_allocator_init(POOL_SIZE);

	void* const ptr1 = my_malloc(1024);
	EXPECT_NE(ptr1, nullptr);

	my_free(ptr1);

	EXPECT_EXIT({ my_realloc(ptr1, 10); }, ::testing::ExitedWithCode(1),
	            "ERROR: You tried to realloc a freed Block: 0x[0-9a-fA-F]{2,16}");
}

TEST(MyMalloc, initializeError) {

	EXPECT_EXIT({ my_allocator_init(POOL_SIZE * POOL_SIZE); }, ::testing::ExitedWithCode(1),
	            "INTERNAL: Failed to mmap for the allocator: Cannot allocate memory\n");
}

TEST(MyMalloc, doubleDestroy) {
	my_allocator_init(POOL_SIZE);

	my_allocator_init(POOL_SIZE);

	void* const ptr1 = my_malloc(1024);
	EXPECT_NE(ptr1, nullptr);

	my_free(ptr1);

	my_allocator_destroy();
	my_allocator_destroy();
}

TEST(MyMalloc, reallocEdgeCases) {

	my_allocator_init(POOL_SIZE);

	// the same as my_malloc
	void* ptr1 = my_realloc(nullptr, 1010);
	EXPECT_NE(ptr1, nullptr);
	memset(ptr1, 0xEE, 1000);

	my_free(ptr1);

	void* const ptr2 = my_realloc(nullptr, 1000);
	EXPECT_EQ(ptr2, ptr1);
	ptr1 = nullptr;

	void* const ptr3 = my_realloc(ptr2, 1005);

	EXPECT_EQ(ptr2, ptr3);

	my_allocator_destroy();
}
