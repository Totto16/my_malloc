
#include <my_malloc.h>

#include <stdlib.h>

#include <gtest/gtest.h>

#define POOL_SIZE ((uint64_t)(1024U * 1024U * 256U))

// THIS IS hardcoded, since there's no better way of knowing this
#define STATIC_MEMORYBLOCK_OVERHEAD (24UL)

TEST(MyMalloc, initializeError) {

	EXPECT_EXIT({ my_allocator_init(POOL_SIZE * POOL_SIZE, true); }, ::testing::ExitedWithCode(1),
	            "ERROR: Failed to allocate memory in the allocator: Cannot allocate memory");
}
