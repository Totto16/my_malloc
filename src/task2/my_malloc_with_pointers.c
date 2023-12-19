/*
Author: Tobias Niederbrunner - csba1761
Module: PS OS 10
*/

#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>

#include <utils.h>

#if !defined(_USE_POINTERS)
#define _USE_POINTERS 2
#endif

#if _USE_POINTERS < 1 || _USE_POINTERS > 2
// this is a c preprocessor macro, it throws a compiler error with the given message
#error "NOT SUPPORTED USE_POINTERS: not between 1 and 2!"
#endif

#if !defined(_VALIDATE_BLOCKS)
#define _VALIDATE_BLOCKS 0
#endif

#if _VALIDATE_BLOCKS < 0 || _VALIDATE_BLOCKS > 1
// this is a c preprocessor macro, it throws a compiler error with the given message
#error "NOT SUPPORTED VALIDATE_BLOCKS: not between 0 and 1!"
#endif

#if !defined(_PER_THREAD_ALLOCATOR)
#define _PER_THREAD_ALLOCATOR 0
#endif

#if _PER_THREAD_ALLOCATOR < 0 || _PER_THREAD_ALLOCATOR > 1
// this is a c preprocessor macro, it throws a compiler error with the given message
#error "NOT SUPPORTED PER_THREAD_ALLOCATOR: not between 0 and 1!"
#endif

// the variables that start with __ can be visible globally, so they're  prefixed by __my_malloc_ so
// that it doesn't pollute the global scope additionally these are made static! (meaning no outside
// file can see them, on global variables this only makes them inivisible to other files )

typedef uint8_t status_t;

typedef uint8_t pseudoByte;

// FREE is 0, so each mmap (so initialized to 0) region has set every block to FREE
enum __my_malloc_status { FREE = 0, ALLOCED = 1 };

#if _USE_POINTERS == 2
typedef struct {
	status_t status;
	void* nextBlock;
	void* previousBlock;
} BlockInformation;

#define SIZE_OF_DOUBLE_POINTER_BLOCK(variableName, block) \
	size_t variableName; \
	do { \
		if(block == NULL) { \
			printSingleErrorAndExit( \
			    "INTERNAL: This is an allocator ERROR, this shouldn't occur!\n"); \
		} else if(block->nextBlock == NULL) { \
			variableName = (pseudoByte*)__my_malloc_globalObject.data + \
			               __my_malloc_globalObject.dataSize - sizeof(BlockInformation) - \
			               (pseudoByte*)block; \
		} else { \
			variableName = \
			    ((pseudoByte*)block->nextBlock) - (pseudoByte*)block - sizeof(BlockInformation); \
		} \
	} while(false)

#endif

typedef struct {
	void* data;
	size_t dataSize;
	pthread_mutex_t mutex;
} GlobalObject;

#if _PER_THREAD_ALLOCATOR == 0
static GlobalObject __my_malloc_globalObject;
#else
// if _PER_THREAD_ALLOCATOR is 1 it allocates one such structure per Thread, this is done with teh
// keyword "_Thread_local" (underscore Uppercase, and double underscore  + any case are reserved
// words for the c standard, so this was introduced in c11, there exists a typedef thread_local for
// that, but I rather use the Keyword directly )
// ATTENTION: each Thread also has to call my_allocator_init, otherwise this is NULL and I DON'T
// check if it's NULL ANYWHERE, meaning it will crash rather instantly trying to read from or store
// to address 0!! from the data entry in the struct, that is 0 initialized!)
static _Thread_local GlobalObject __my_malloc_globalObject;
#endif

#if _VALIDATE_BLOCKS == 1
// could be a macro, but for more complicated use later thats not that well suited
static bool __my_malloc_isValidBlock(void* blockPointer) {
	if(blockPointer == NULL) {
		return false;
	}

	BlockInformation* blockInformation = ((BlockInformation*)blockPointer);
	bool hasValidStatus = blockInformation->status == FREE || blockInformation->status == ALLOCED;

	bool nextIsInRange =
	    ((blockInformation->nextBlock == NULL ||
	      (blockInformation->nextBlock >= __my_malloc_globalObject.data &&
	       (pseudoByte*)blockInformation->nextBlock <=
	           (pseudoByte*)__my_malloc_globalObject.data + __my_malloc_globalObject.dataSize)) &&
	     (blockInformation->previousBlock == NULL ||
	      (blockInformation->previousBlock >= __my_malloc_globalObject.data &&
	       (pseudoByte*)blockInformation->previousBlock <=
	           (pseudoByte*)__my_malloc_globalObject.data + __my_malloc_globalObject.dataSize)));

	bool noSelfReference =
	    (blockInformation->nextBlock == NULL || blockInformation->nextBlock != blockInformation) &&
	    (blockInformation->previousBlock == NULL ||
	     blockInformation->previousBlock != blockInformation);

	SIZE_OF_DOUBLE_POINTER_BLOCK(blockSize, blockInformation);

	bool blockSizeIsInBoundaries =
	    blockSize <=
	    __my_malloc_globalObject.dataSize; // <= 0 on unsigned is not necessary, it underflows then!

#if _PRINT_DEBUG == 1
	if(!(nextIsInRange && hasValidStatus && noSelfReference && blockSizeIsInBoundaries)) {
		printf(
		    "nextIsInRange: %d, hasValidStatus: %d, noSelfReference: %d, blockSizeIsInBoundaries: "
		    "%d\n",
		    nextIsInRange, hasValidStatus, noSelfReference, blockSizeIsInBoundaries);
	}
#endif

	return nextIsInRange && hasValidStatus && noSelfReference && blockSizeIsInBoundaries;
}

#endif

// DEBUG
#if _USE_POINTERS == 2 && _PRINT_DEBUG == 1
static void __my_malloc_debug_printSegment(char* desc, BlockInformation* information);

static void __my_malloc_debug_printSegment(char* desc, BlockInformation* information) {
#if _VALIDATE_BLOCKS == 1
	if(information != NULL && !__my_malloc_isValidBlock(information)) {
		printSingleErrorAndExit("NOT VALID: in print\n");
	}
#endif

	SIZE_OF_DOUBLE_POINTER_BLOCK(blockSize, information);
	printf("%s%sBlock: |%c| %ld -> %s\n", desc != NULL ? "\n" : "\t", desc != NULL ? desc : "",
	       information->status == FREE ? ' ' : '#', blockSize,
	       information->nextBlock == NULL ? "END\n" : "next");
	if(information->nextBlock != NULL) {
		__my_malloc_debug_printSegment(NULL, information->nextBlock);
	}
}
#endif
// DEBUG

static bool __my_malloc_block_fitsBetter(BlockInformation* toCompare,
                                         BlockInformation* currentBlock, size_t size) {

	if(toCompare->status != FREE) {
		return false;
	}

	if(currentBlock->status != FREE) {
		return true;
	}

	SIZE_OF_DOUBLE_POINTER_BLOCK(blockSize, toCompare);

	// if a new block has to be "allocated" then there has to be space for that!
	if(toCompare->nextBlock == NULL) {

		if(blockSize == size) {
			return true;
		}

		if(blockSize < sizeof(BlockInformation) + size) {
			return false;
		}

		if(blockSize == sizeof(BlockInformation) + size) {
			return true;
		}
	}

	if(blockSize < size) {
		return false;
	}

	if(blockSize == size) {
		return true;
	}

	SIZE_OF_DOUBLE_POINTER_BLOCK(currentSize, currentBlock);

	if(currentSize > size + sizeof(BlockInformation)) {
		if(blockSize <= size + sizeof(BlockInformation)) {
			return false;
		}
	}
	return blockSize - size < currentSize - size;
}

void* my_malloc(size_t size) {
	int result = pthread_mutex_lock(&__my_malloc_globalObject.mutex);
	// mutex errors are better when being asserted, since no real errors can occur, only when the
	// system is already malfunctioning
	checkResultForThreadErrorAndExit(
	    "INTERNAL: An Error occurred while trying to lock the mutex in the internal allocator");

	BlockInformation* bestFit = (BlockInformation*)__my_malloc_globalObject.data;
	BlockInformation* nextFreeBlock = (BlockInformation*)bestFit->nextBlock;

	while(nextFreeBlock != NULL) {
		// WIP!!!!!

		/* 	SIZE_OF_DOUBLE_POINTER_BLOCK(blockSize, bestFit);
		    if(blockSize + sizeof(BlockInformation) <= size) {
		        break;
		    }

		    SIZE_OF_DOUBLE_POINTER_BLOCK(nextSize, nextFreeBlock);
		    if(nextSize + sizeof(BlockInformation) <= size) {
		        bestFit = nextFreeBlock;
		    } */

		if(__my_malloc_block_fitsBetter(nextFreeBlock, bestFit, size)) {
			bestFit = nextFreeBlock;
			// shorthand evaluation, so if it fits perfectly don't look fort better
			SIZE_OF_DOUBLE_POINTER_BLOCK(blockSize, bestFit);
			if(blockSize == size ||
			   (blockSize == size + sizeof(BlockInformation) && bestFit->nextBlock == NULL)) {
				break;
			}
		}
		nextFreeBlock = nextFreeBlock->nextBlock;
	}
	// if the one that fit the best is not big enough, it means no block is big enough!
	SIZE_OF_DOUBLE_POINTER_BLOCK(blockSize, bestFit);

	if(bestFit == NULL || blockSize < size ||
	   (blockSize != size && blockSize < size + sizeof(BlockInformation) &&
	    bestFit->nextBlock == NULL) ||
	   bestFit->status != FREE) {

		result = pthread_mutex_unlock(&__my_malloc_globalObject.mutex);
		checkResultForThreadErrorAndExit("INTERNAL: An Error occurred while trying to "
		                                 "unlock the internal allocator mutex");

		return NULL;
	};

#if _PRINT_DEBUG == 1
	printf("best Fit to requested size of %ld\n", size);
	__my_malloc_debug_printSegment("best Fit: ", bestFit);
#endif

	if(blockSize == size) {
		bestFit->status = ALLOCED;
	} else if(blockSize - size < sizeof(BlockInformation)) {

		bestFit->status = ALLOCED;

	} else {
		BlockInformation* newBlock =
		    (BlockInformation*)((pseudoByte*)bestFit + sizeof(BlockInformation) + size);
		newBlock->status = FREE;
		newBlock->nextBlock = bestFit->nextBlock; // can be NULL
		newBlock->previousBlock = bestFit;

		bestFit->status = ALLOCED;
		bestFit->nextBlock = newBlock;
	}

#if _PRINT_DEBUG == 1
	__my_malloc_debug_printSegment("malloc: ", (BlockInformation*)__my_malloc_globalObject.data);
#endif

	void* returnValue = (pseudoByte*)bestFit + sizeof(BlockInformation);

	result = pthread_mutex_unlock(&__my_malloc_globalObject.mutex);
	checkResultForThreadErrorAndExit(
	    "INTERNAL: An Error occurred while trying to unlock the internal allocator mutex");

	return returnValue;
}

void my_free(void* ptr) {

	// so that if you pass a wrong argument just nothing happens!
	if(ptr == NULL) {
		return;
	}

	int result = pthread_mutex_lock(&__my_malloc_globalObject.mutex);
	// mutex errors are better when being asserted, since no real errors can occur, only when the
	// system is already malfunctioning
	checkResultForThreadErrorAndExit(
	    "INTERNAL: An Error occurred while trying to lock the mutex in the internal allocator");

	BlockInformation* information =
	    (BlockInformation*)((pseudoByte*)ptr - sizeof(BlockInformation));
#if _VALIDATE_BLOCKS == 1
	if(!__my_malloc_isValidBlock(information)) {
		printErrorAndExit("INTERNAL: you tried to free a invalid Block at address: %p\n", ptr);
	}
#endif
#if !defined(_IGNORE_SECURITY_CHECKS)

	if(information->status == FREE) {
		printErrorAndExit("INTERNAL: you tried to free a already freed Block: %p\n", ptr);
	}
#endif
	information->status = FREE;

	BlockInformation* nextBlock = (BlockInformation*)information->nextBlock;
	BlockInformation* previousBlock = (BlockInformation*)information->previousBlock;

	// no this occurs in the memset of the tests, it overwrites the region the pointer is stored!
	// TODO: in my_malloc some off by one error occurs!! (or the next block isn't checked properly
	// for NULL!)
	if((pseudoByte*)previousBlock >
	   (pseudoByte*)__my_malloc_globalObject.data + __my_malloc_globalObject.dataSize) {
		return;
	}

	if(previousBlock != NULL && previousBlock->status == FREE) {
		if(nextBlock != NULL && nextBlock->status == FREE) {
			previousBlock->nextBlock = nextBlock->nextBlock;

			if(nextBlock->nextBlock != NULL) {
				((BlockInformation*)nextBlock->nextBlock)->previousBlock = previousBlock;
			}
		} else {

			previousBlock->nextBlock = nextBlock; // can be NULL
			nextBlock->previousBlock = previousBlock;
		}
	} else if(nextBlock != NULL && nextBlock->status == FREE) {
		information->nextBlock = nextBlock->nextBlock; // can be NULL

		if(nextBlock->nextBlock != NULL) {
			((BlockInformation*)nextBlock->nextBlock)->previousBlock = information;
		}
	}

#if _PRINT_DEBUG == 1
	__my_malloc_debug_printSegment("free: ", (BlockInformation*)__my_malloc_globalObject.data);
#endif

	result = pthread_mutex_unlock(&__my_malloc_globalObject.mutex);
	checkResultForThreadErrorAndExit(
	    "INTERNAL: An Error occurred while trying to unlock the internal allocator mutex");
}
void my_allocator_init(size_t size) {
	__my_malloc_globalObject.dataSize = size;

	// MAP_ANONYMOUS means, that
	//  "The mapping is not backed by any file; its contents are initialized to zero.  The fd
	//  argument is ignored; however, some implementations require fd to be -1 if MAP_ANONYMOUS (or
	//  MAP_ANON)  is  specified, and portable applications should ensure this.  The offset
	//  argument should be zero." ~ man page

	// this region is initalized with 0s
	__my_malloc_globalObject.data =
	    mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if(__my_malloc_globalObject.data == MAP_FAILED) {
		printErrorAndExit("INTERNAL: Failed to mmap for the allocator: %s\n", strerror(errno));
	}
	// FREE is set with the 0 initialized region automatically (only here!)

	// done with setting everything to 0 implicitly
	//((BlockInformation*)__my_malloc_globalObject.data)->nextBlock = NULL;
	// ((BlockInformation*)__my_malloc_globalObject.data)->previousBlock = NULL;

	// initialize the mutex, use default as attr
	int result = pthread_mutex_init(&__my_malloc_globalObject.mutex, NULL);
	checkResultForThreadErrorAndExit("INTERNAL: An Error occurred while trying to initializing the "
	                                 "internal mutex for the allocator");
}

void my_allocator_destroy(void) {

	int result = munmap(__my_malloc_globalObject.data, __my_malloc_globalObject.dataSize);
	checkResultForThreadErrorAndExit("INTERNAL: Failed to munmap for the allocator:");

	result = pthread_mutex_destroy(&__my_malloc_globalObject.mutex);
	checkResultForThreadErrorAndExit(
	    "INTERNAL: An Error occurred while trying to destroy the internal mutex "
	    "in cleaning up for the allocator");
}

// NOTE: this is extremely slow for some reason!!
