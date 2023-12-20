
#include <my_malloc.h>


#include <gtest/gtest.h>

#define POOL_SIZE ((uint64_t)(1024U * 1024U * 256U))

TEST(MyMallocCrashes, reallocBeforeInitializing) {

	EXPECT_EXIT({ my_realloc((void*)0xFFEEDDCC, 1024); }, ::testing::ExitedWithCode(1),
	            "INTERNAL: An Error occurred while trying to lock the mutex "
	            "in the internal allocator: Invalid argument");
}
