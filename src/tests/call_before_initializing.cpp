
#include <my_malloc.h>

#include <stdlib.h>

#include <gtest/gtest.h>

#define POOL_SIZE ((uint64_t)(1024U * 1024U * 256U))

// THIS IS hardcoded, since there's no better way of knowing this
#define STATIC_MEMORYBLOCK_OVERHEAD (24UL)

TEST(MyMalloc, callBeforeInitializing) {

	EXPECT_EXIT({ my_malloc(1024); }, ::testing::ExitedWithCode(1),
	            "Calling malloc before initializing the allocator is prohibited!");
}
