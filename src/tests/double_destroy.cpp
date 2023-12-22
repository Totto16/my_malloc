
#include <my_malloc.h>

#include <stdlib.h>

#include <gtest/gtest.h>

#define POOL_SIZE ((uint64_t)(1024U * 1024U * 256U))

// THIS IS hardcoded, since there's no better way of knowing this
#define STATIC_MEMORYBLOCK_OVERHEAD (24UL)

TEST(MyMalloc, doubleDestroy) {
	my_allocator_init(POOL_SIZE, true);

	void* const ptr1 = my_malloc(1024);
	EXPECT_NE(ptr1, nullptr);

	my_free(ptr1);

	my_allocator_destroy();
	my_allocator_destroy();
}
