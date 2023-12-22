/*
Author: Totto16
*/

#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>

#include <utils.h>

// some possible configurations, these can be done with defines during compilation
#if !defined(_USE_BIFIELDS)
#define _USE_BIFIELDS 1
#endif

#if _USE_BIFIELDS < 0 || _USE_BIFIELDS > 1
// this is a c preprocessor macro, it throws a compiler error with the given message
#error "NOT SUPPORTED USE_BIFIELDS: not between 0 and 1!"
#endif

#if !defined(_VALIDATE_BLOCKS)
#define _VALIDATE_BLOCKS 0
#endif

#if _VALIDATE_BLOCKS < 0 || _VALIDATE_BLOCKS > 1
// this is a c preprocessor macro, it throws a compiler error with the given message
#error "NOT SUPPORTED VALIDATE_BLOCKS: not between 0 and 1!"
#endif

// for exercise 3, see the explanation down below!
#if !defined(_PER_THREAD_ALLOCATOR)
#define _PER_THREAD_ALLOCATOR 0
#endif

#if _PER_THREAD_ALLOCATOR < 0 || _PER_THREAD_ALLOCATOR > 1
// this is a c preprocessor macro, it throws a compiler error with the given message
#error "NOT SUPPORTED PER_THREAD_ALLOCATOR: not between 0 and 1!"
#endif

// the variables that start with __ can be visible globally, so they're  prefixed by __my_malloc_
// (normally a standard naming, that is reserved for the standard, but here I "define my own
// standard" meaning. that this "malloc" is part of that) so that it doesn't pollute the global
// scope

// for calculations and also internal status
typedef uint8_t status_t;

typedef uint8_t pseudoByte;

// FREE is 0, so each mmap (so initialized to 0) region has set every block to FREE
enum __my_malloc_status { FREE = 0, ALLOCED = 1 };

#if _USE_BIFIELDS == 1

typedef struct {
	status_t status : 1;
	// later identify this block as meta information, so that isValidBlock is better at recognizing
	// void* next_block
	// using only 63 and 1 bit, so it all fits into one byte  and the max real value is the same as
	// int64_max alias 9,223,372,036,854,775,807, if you have more memory then that to allocate then
	// you sure can afford to not use this bitfield
	uint64_t size : 63;
	// here either the size or the pointer to next can be used, depending on what
	// is choosen, the calculations to make are different
} BlockInformation;

#else

// bitfield access is slow, but here the storage takes 16 BYets  9 + alignment, that is double the
// bytes from the one with bitfield, but its really faster, so you can choose which one you want
// (generally you don't make thousands of mallocs, so it is done with a bitfield as default)
typedef struct {
	status_t status;
	uint64_t size;
} BlockInformation;

#endif

typedef struct {
	void* data;
	uint64_t dataSize;
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
// checks wether a block can really be one, not that reliable, but the behavior of the programm is
// undefined, if not passing valid adresses into the malloc functions, so that is just a little
// security mechanims
static bool __my_malloc_isValidBlock(void* blockPointer) {
	if(blockPointer == NULL) {
		return false;
	}

	BlockInformation* blockInformation = ((BlockInformation*)blockPointer);
	bool hasValidStatus = blockInformation->status == FREE || blockInformation->status == ALLOCED;
	bool nextIsInRange =
	    __my_malloc_globalObject.dataSize - sizeof(BlockInformation) >= blockInformation->size;
	return nextIsInRange && hasValidStatus;
}

#endif

// some helpers to get the nextBlock, this has to be done, since this implementation is done without
// pointers, but with sizes, it also can return NUll if it has no next block
static BlockInformation* __my_malloc_nextBlock(BlockInformation* currentBlock) {

	if(((pseudoByte*)currentBlock) - (pseudoByte*)__my_malloc_globalObject.data +
	       currentBlock->size + sizeof(BlockInformation) ==
	   __my_malloc_globalObject.dataSize) {
		return NULL;
	}

	BlockInformation* nextBlock =
	    (BlockInformation*)((pseudoByte*)currentBlock + sizeof(BlockInformation) +
	                        currentBlock->size);

	return nextBlock;
}

// this is incredibly slow, but it has to be done like that without previous Pointer!
static BlockInformation* __my_malloc_previousBlock(BlockInformation* currentBlock) {

	if((pseudoByte*)currentBlock == (pseudoByte*)__my_malloc_globalObject.data) {
		return NULL;
	}

	BlockInformation* previousFreeBlock = (BlockInformation*)__my_malloc_globalObject.data;
	BlockInformation* nextFreeBlock = __my_malloc_nextBlock(previousFreeBlock);
	while(nextFreeBlock != NULL) {
		nextFreeBlock = __my_malloc_nextBlock(previousFreeBlock);
		if((pseudoByte*)nextFreeBlock == (pseudoByte*)currentBlock) {
			return previousFreeBlock;
		}
		previousFreeBlock = nextFreeBlock;
	}
	printSingleErrorAndExit("INTERNAL: FATAL: this is an implementation Error, if reached!");
}

// checks if the block toCompare fits better then the block currentBlock, with  requested size size
static bool __my_malloc_block_fitsBetter(BlockInformation* toCompare,
                                         BlockInformation* currentBlock, uint64_t size) {

	if(toCompare->status != FREE) {
		return false;
	}

	if(currentBlock->status != FREE) {
		return true;
	}

	uint64_t blockSize = toCompare->size;
	if(blockSize < size) {
		return false;
	}

	if(blockSize == size) {
		return true;
	}

	// if no BlockInformation can fit in there, but also the block isn't exactly the right size it
	// doesn't fit, e.g if there are 7 bytes between the size to search the no structure can fit in,
	// but also the next one can't be used (alias just be leaved unmodified and used as next, this
	// can be solved by using next pointers  to the next BlockInformation, which consumes another 8
	// bytes :( but also since the size is only 8 bytes this shouldn't occur that often!)

	// this can make an allocation impossible, or just use another block
	if(blockSize - size < sizeof(BlockInformation)) {
		// this warning is not the best, but shouldn't occur when normally using this!
		printf("INTERNAL: WARNING: could be solved (better) with more memory hungry "
		       "BlockInformation!\n");

		return false;
	}
	uint64_t currentSize = currentBlock->size;

	return blockSize - size < currentSize - size;
}

// alloc a certain size,  if no space was in the global data struct a NULL pointer is
// returned, rather then allocating more memory, this malloc can't grow it's internal buffer
// dynamically!

void* my_malloc(uint64_t size) {
	// lock mutex, so it's thread safe!
	int result = pthread_mutex_lock(&__my_malloc_globalObject.mutex);
	// mutex errors are better when being asserted, since no real errors can occur, only when the
	// system is already malfunctioning
	checkResultForThreadErrorAndExit(
	    "INTERNAL: An Error occurred while trying to lock the mutex in the internal allocator");

	// iterating over all blocks and saving the best fit, has shorthand computation, meaning that if
	// a block fits exactly it takes that block immediately!
	BlockInformation* bestFit = (BlockInformation*)__my_malloc_globalObject.data;
	BlockInformation* nextFreeBlock = __my_malloc_nextBlock(bestFit);

	while(nextFreeBlock != NULL) {

		if(__my_malloc_block_fitsBetter(nextFreeBlock, bestFit, size)) {
			bestFit = nextFreeBlock;
			// shorthand evaluation, so if it fits perfectly don't look for better
			if(bestFit->size == size) {
				break;
			}
		}
		nextFreeBlock = __my_malloc_nextBlock(nextFreeBlock);
	}
	// if the one that fit the best is not big enough, it means no block is big enough, or if that
	// block isn't free, so that means teh same
	if(bestFit->size < size || bestFit->status != FREE) {
		// unlocking mutex before returning
		result = pthread_mutex_unlock(&__my_malloc_globalObject.mutex);
		checkResultForThreadErrorAndExit(
		    "INTERNAL: An Error occurred while trying to unlock the internal allocator mutex");

		return NULL;
	};

	// now either making a new block or just setting the old to status ALLOCED, this depends on the
	// size that has to be malloced

	if(bestFit->size == size) {
		bestFit->status = ALLOCED;
	} else {
		// caluclate the position of teh new block, then store there the necessary infromation
		uint64_t blockSize = sizeof(BlockInformation) + size;
		BlockInformation* newBlock = (BlockInformation*)((pseudoByte*)bestFit + blockSize);
		newBlock->status = FREE;
		newBlock->size = bestFit->size - blockSize;

		bestFit->status = ALLOCED;
		bestFit->size = size;
	}

	void* returnValue = (pseudoByte*)bestFit + sizeof(BlockInformation);

	// unlocking mutex before returning
	result = pthread_mutex_unlock(&__my_malloc_globalObject.mutex);
	checkResultForThreadErrorAndExit(
	    "INTERNAL: An Error occurred while trying to unlock the internal allocator mutex");

	// returning the area that is designed to store the data ( it has an offset of
	// sizeof(BlockInformation))
	return returnValue;
}

void my_free(void* ptr) {
	// lock mutex, so it's thread safe!
	int result = pthread_mutex_lock(&__my_malloc_globalObject.mutex);
	// mutex errors are better when being asserted, since no real errors can occur, only when the
	// system is already malfunctioning
	checkResultForThreadErrorAndExit(
	    "INTERNAL: An Error occurred while trying to lock the mutex in the internal allocator");

	// get the 	BlockInformation*  where the status is stored, here some security checks are done,
	// the system malloc doesn't do that, but I do it nevertheless
	BlockInformation* information =
	    (BlockInformation*)((pseudoByte*)ptr - sizeof(BlockInformation));
#if _VALIDATE_BLOCKS == 1
	if(!__my_malloc_isValidBlock(information)) {
		printErrorAndExit("INTERNAL: you tried to free a invalid Block at address: %p\n", ptr);
	}
#endif
	if(information->status == FREE) {
		printErrorAndExit("INTERNAL: you tried to free a already freed Block: %p\n", ptr);
	}
	// finally setting the status to FREE
	information->status = FREE;

	// now checking if some  free blocks can be merged
	BlockInformation* nextBlock = __my_malloc_nextBlock(information);
	BlockInformation* previousBlock = __my_malloc_previousBlock(information);
	if(previousBlock != NULL && previousBlock->status == FREE) {
		if(nextBlock != NULL && nextBlock->status == FREE) {
			previousBlock->size = previousBlock->size + information->size + nextBlock->size +
			                      (sizeof(BlockInformation) * 2);
		} else {
			previousBlock->size =
			    previousBlock->size + information->size + sizeof(BlockInformation);
		}
	} else if(nextBlock != NULL && nextBlock->status == FREE) {
		information->size = information->size + nextBlock->size + sizeof(BlockInformation);
	}

	// unlocking mutex before returning
	result = pthread_mutex_unlock(&__my_malloc_globalObject.mutex);
	checkResultForThreadErrorAndExit(
	    "INTERNAL: An Error occurred while trying to unlock the internal allocator mutex");
}

void my_allocator_init(uint64_t size, bool force_alloc) {
	__my_malloc_globalObject.dataSize = size;

	// DOES NOTHING
	(void)force_alloc;

	// MAP_ANONYMOUS means, that
	//  "The mapping is not backed by any file; its contents are initialized to zero.  The fd
	//  argument is ignored; however, some implementations require fd to be -1 if MAP_ANONYMOUS (or
	//  MAP_ANON)  is  specified, and portable applications should ensure this.  The offset
	//  argument should be zero." ~ man page

	// this memory region is initalized with 0s
	__my_malloc_globalObject.data =
	    mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if(__my_malloc_globalObject.data == MAP_FAILED) {
		printErrorAndExit("INTERNAL: Failed to mmap for the allocator: %s\n", strerror(errno));
	}
	// FREE is set with the 0 initialized region automatically (only here the block is initialzed
	// with 0, not after freeing!)

	((BlockInformation*)__my_malloc_globalObject.data)->size = size - sizeof(BlockInformation);

	// initialize the mutex, use default as attr
	int result = pthread_mutex_init(&__my_malloc_globalObject.mutex, NULL);
	checkResultForThreadErrorAndExit("INTERNAL: An Error occurred while trying to initializing the "
	                                 "internal mutex for the allocator");
}

void my_allocator_destroy(void) {
	// un,mapping the mapped memory and destroy the mutex
	int result = munmap(__my_malloc_globalObject.data, __my_malloc_globalObject.dataSize);
	checkResultForThreadErrorAndExit("INTERNAL: Failed to munmap for the allocator:");

	result = pthread_mutex_destroy(&__my_malloc_globalObject.mutex);
	checkResultForThreadErrorAndExit(
	    "INTERNAL: An Error occurred while trying to destroy the internal mutex "
	    "in cleaning up for the allocator");
}
