
#include <my_malloc.h>

#include <stdlib.h>

#include <gtest/gtest.h>

#define POOL_SIZE ((uint64_t)(1024U * 1024U * 256U))

// THIS IS hardcoded, since there's no better way of knowing this
#define STATIC_MEMORYBLOCK_OVERHEAD (24UL)

TEST(MyMalloc, reallocEdgeCases) {

	my_allocator_init(POOL_SIZE, true);

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
