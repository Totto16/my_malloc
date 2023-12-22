
#include <my_malloc.h>

#include <stdlib.h>

#include <gtest/gtest.h>

#define POOL_SIZE ((uint64_t)(1024U * 1024U * 256U))

// THIS IS hardcoded, since there's no better way of knowing this
#define STATIC_MEMORYBLOCK_OVERHEAD (24UL)

TEST(MyMalloc, doubleFree) {

	my_allocator_init(POOL_SIZE, true);

	void* const ptr1 = my_malloc(1024);
	EXPECT_NE(ptr1, nullptr);

	my_free(ptr1);

	EXPECT_EXIT({ my_free(ptr1); }, ::testing::ExitedWithCode(1),
	            "ERROR: You tried to free a already freed Block: 0x[0-9a-fA-F]{2,16}");
}
