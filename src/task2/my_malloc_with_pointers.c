/*
Author: Tobias Niederbrunner - csba1761
Module: PS OS 10
*/

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>

#include <valgrind.h>

#include <utils.h>

#include "my_malloc.h"

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
enum __my_malloc_alloc_status {
	FREE = 0,
	ALLOCED = 1,
};

typedef struct {
	void* nextBlock;
	void* previousBlock;
	status_t status;
} BlockInformation;

typedef struct {
	void* data;
	uint64_t dataSize;
#if !defined(_ALLOCATOR_NOT_MT_SAVE) && _PER_THREAD_ALLOCATOR != 1
	pthread_mutex_t mutex;
#endif
} GlobalObject;

#if _PER_THREAD_ALLOCATOR == 0 || defined(_ALLOCATOR_NOT_MT_SAVE)
static GlobalObject __my_malloc_globalObject = { .data = NULL };
#else
// if _PER_THREAD_ALLOCATOR is 1 it allocates one such structure per Thread, this is done with the
// keyword "_Thread_local" (underscore Uppercase, and double underscore  + any case are reserved
// words for the c standard, so this was introduced in c11, there exists a typedef thread_local for
// that, but I rather use the Keyword directly )
// ATTENTION: each Thread also has to call my_allocator_init, otherwise this is NULL and I DON'T
// check if it's NULL ANYWHERE, meaning it will crash rather instantly trying to read from or store
// to address 0!! from the data entry in the struct, that is 0 initialized!)
static _Thread_local GlobalObject __my_malloc_globalObject = { .data = NULL };
#endif

/**
 * @note Needs to be called with the mutex locked, in order to be thread safe!
 *
 */
uint64_t size_of_double_pointer_block(BlockInformation* block) {
	if(block == NULL) {
		printSingleErrorAndExit("INTERNAL: This is an allocator ERROR, this shouldn't occur!\n");
	} else if(block->nextBlock == NULL) {
		return (((pseudoByte*)__my_malloc_globalObject.data + __my_malloc_globalObject.dataSize) -
		        (pseudoByte*)block) -
		       sizeof(BlockInformation);
	} else {
		return ((pseudoByte*)block->nextBlock - (pseudoByte*)block) - sizeof(BlockInformation);
	}
}

/**
 * @note Needs to be called with the mutex locked, in order to be thread safe!
 *
 */
static bool __my_malloc_block_fitsBetter(BlockInformation* toCompare,
                                         BlockInformation* currentBlock, uint64_t size) {

	if(toCompare->status != FREE) {
		return false;
	}

	if(currentBlock->status != FREE) {
		return true;
	}

	const uint64_t blockSize = size_of_double_pointer_block(toCompare);

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

	const uint64_t currentSize = size_of_double_pointer_block(currentBlock);

	if(currentSize > size + sizeof(BlockInformation)) {
		if(blockSize <= size + sizeof(BlockInformation)) {
			return false;
		}
	}
	return (blockSize - size) < (currentSize - size);
}

/**
 * @note MT-safe - with thread_local storage, this only accesses that, otherwise a mutex is used, if
 * this is called without initializing the underlying allocator beforehand, it is undefined
 * behaviour, however this function crashes the program in that case
 */
void* my_malloc(uint64_t size) {
	// TODO: if the size is to high for that, than use mmap to get another block, with double
	// TODO: pointers the whole allocated region doesn't have to be continuous, check if nothing
	// TODO: expects that to be the case, especially the sizeof block or similar functions!

#if !defined(_ALLOCATOR_NOT_MT_SAVE) && _PER_THREAD_ALLOCATOR != 1
	int result = pthread_mutex_lock(&__my_malloc_globalObject.mutex);
	// mutex errors are better when being asserted, since no real errors can occur, only when the
	// system is already malfunctioning
	checkResultForThreadErrorAndExit(
	    "INTERNAL: An Error occurred while trying to lock the mutex in the internal allocator");
#endif

	// calling my_malloc without initializing the allocator doesn't work, if that is the case,
	// likely the uninitialized mutex access before this will crash the program, but that is here
	// for safety measures! AND ALSO in the case of uninitialized allocator in the thread local case
	if(__my_malloc_globalObject.data == NULL) {
		fprintf(stderr, "Calling malloc before initializing the allocator is prohibited!\n");
		exit(1);
	}

	BlockInformation* bestFit = (BlockInformation*)__my_malloc_globalObject.data;
	BlockInformation* nextFreeBlock = (BlockInformation*)bestFit->nextBlock;

	while(nextFreeBlock != NULL) {
		// TODO: this is extremely slow, this WIP tries to make some cases faster!!

		/*
		        size_of_double_pointer_block(blockSize, bestFit);
		        if(blockSize + sizeof(BlockInformation) <= size) {
		            break;
		        }

		        size_of_double_pointer_block(nextSize, nextFreeBlock);
		        if(nextSize + sizeof(BlockInformation) <= size) {
		            bestFit = nextFreeBlock;
		        } */

		if(__my_malloc_block_fitsBetter(nextFreeBlock, bestFit, size)) {
			bestFit = nextFreeBlock;
			// shorthand evaluation, so if it fits perfectly don't look for a better one
			const uint64_t blockSize = size_of_double_pointer_block(bestFit);
			if(blockSize == size) {
				break;
			}
		}
		nextFreeBlock = nextFreeBlock->nextBlock;
	}
	// if the one that fit the best is not big enough, it means no block is big enough! If it's not
	// free, than there was no free block
	const uint64_t blockSize = size_of_double_pointer_block(bestFit);

	if(bestFit == NULL || blockSize < size ||
	   (blockSize != size && blockSize < size + sizeof(BlockInformation) &&
	    bestFit->nextBlock == NULL) ||
	   bestFit->status != FREE) {
#if !defined(_ALLOCATOR_NOT_MT_SAVE) && _PER_THREAD_ALLOCATOR != 1
		result = pthread_mutex_unlock(&__my_malloc_globalObject.mutex);
		checkResultForThreadErrorAndExit("INTERNAL: An Error occurred while trying to "
		                                 "unlock the internal allocator mutex");
#endif

		return NULL;
	};

	if(blockSize == size) {
		// block size and size needed for allocation is the same, only need to set the status to
		// allocated

		bestFit->status = ALLOCED;
	} else if(blockSize - size <= (sizeof(BlockInformation))) {
		// block size and size needed for allocation is nearly the same, but can't allocate a new
		// block at the end, since it hasn't enough space for another BlockInformation, so only need
		// to set the status to allocated, but some size is wasted, it can create a gap of 1 or
		// more, that is fine, but gaps of 0 or less just "waste" that memory -- this handling
		// implicates, that no position of previous or next block may be calculated by using the
		// size!!

		bestFit->status = ALLOCED;

	} else {
		BlockInformation* newBlock =
		    (BlockInformation*)(((pseudoByte*)bestFit + sizeof(BlockInformation)) + size);
		MEMCHECK_DEFINE_INTERNAL_USE(newBlock, sizeof(BlockInformation));

		// the new gap is at least 1 byte big, see above!

		newBlock->status = FREE;
		newBlock->nextBlock = bestFit->nextBlock; // can be NULL
		newBlock->previousBlock = bestFit;

		bestFit->status = ALLOCED;
		bestFit->nextBlock = newBlock;

		if(newBlock->nextBlock != NULL) {
			((BlockInformation*)newBlock->nextBlock)->previousBlock = newBlock;
		}
	}

	void* returnValue = (pseudoByte*)bestFit + sizeof(BlockInformation);

	MEMCHECK_DEFINE_INTERNAL_USE(bestFit, sizeof(BlockInformation));
	VALGRIND_ALLOC(returnValue, size, 0, false);

#if !defined(_ALLOCATOR_NOT_MT_SAVE) && _PER_THREAD_ALLOCATOR != 1
	result = pthread_mutex_unlock(&__my_malloc_globalObject.mutex);
	checkResultForThreadErrorAndExit(
	    "INTERNAL: An Error occurred while trying to unlock the internal allocator mutex");
#endif

	return returnValue;
}

/**
 * @brief frees a pointer, a NULL pointer is ignored and a safe noop,
 * if the pointer is not allocated with my_malloc, this call is undefined behaviour. It likely will
 * crash or create a blockInformation structure, that will crash in later stages, since it tries to
 * interpret some random garbage memory as block-structure, so be aware of that!
 * DOUBLE Frees crash the program, so remember to always set freed pointer sto NULL :)
 *
 * @note MT-safe, using the mutex, or the thread local storage, the same principles as in my_malloc
 * apply, so calling this with an uninitialized allocator is undefined behaviour and crashes the
 * program
 *
 */
void my_free(void* ptr) {

	// so that if you pass a wrong argument just nothing happens!
	if(ptr == NULL) {
		return;
	}

#if !defined(_ALLOCATOR_NOT_MT_SAVE) && _PER_THREAD_ALLOCATOR != 1
	int result = pthread_mutex_lock(&__my_malloc_globalObject.mutex);
	// mutex errors are better when being asserted, since no real errors can occur, only when the
	// system is already malfunctioning
	checkResultForThreadErrorAndExit(
	    "INTERNAL: An Error occurred while trying to lock the mutex in the internal allocator");
#endif

	// calling my_free without initializing the allocator doesn't work, if that is the case,
	// likely the uninitialized mutex access before this will crash the program, but that is here
	// for safety measures! AND ALSO in the case of uninitialized allocator in the thread local case
	if(__my_malloc_globalObject.data == NULL) {
		fprintf(stderr, "Calling free before initializing the allocator is prohibited!\n");
		exit(1);
	}

	BlockInformation* information =
	    (BlockInformation*)((pseudoByte*)ptr - sizeof(BlockInformation));

	if(information->status == FREE) {
		printErrorAndExit("INTERNAL: you tried to free a already freed Block: %p\n", ptr);
	}

	information->status = FREE;
	VALGRIND_FREE(ptr, 0);

	BlockInformation* nextBlock = (BlockInformation*)information->nextBlock;
	BlockInformation* previousBlock = (BlockInformation*)information->previousBlock;

	// merge with previous free block
	if(previousBlock != NULL && previousBlock->status == FREE) {

		// MERGE three free blocks into one: layout Previous | Current | Next => New Free one
		if(nextBlock != NULL && nextBlock->status == FREE) {
			previousBlock->nextBlock = nextBlock->nextBlock; // Can be NULL

			if(nextBlock->nextBlock != NULL) {
				((BlockInformation*)nextBlock->nextBlock)->previousBlock = previousBlock;
			}
			// merge previous free block with current one
		} else {

			previousBlock->nextBlock = nextBlock; // can be NULL
			if(nextBlock != NULL) {
				nextBlock->previousBlock = previousBlock;
			}
		}

		MEMCHECK_REMOVE_INTERNAL_USE(information, sizeof(BlockInformation));

		// merge next free block with current one
	} else if(nextBlock != NULL && nextBlock->status == FREE) {
		information->nextBlock = nextBlock->nextBlock; // can be NULL

		if(nextBlock->nextBlock != NULL) {
			((BlockInformation*)nextBlock->nextBlock)->previousBlock = information;
		}

		MEMCHECK_REMOVE_INTERNAL_USE(nextBlock, sizeof(BlockInformation));
	}

#if !defined(_ALLOCATOR_NOT_MT_SAVE) && _PER_THREAD_ALLOCATOR != 1
	result = pthread_mutex_unlock(&__my_malloc_globalObject.mutex);
	checkResultForThreadErrorAndExit(
	    "INTERNAL: An Error occurred while trying to unlock the internal allocator mutex");
#endif
}

/**
 * @note NOT MT-safe. this function HAS TO BE called exactly once at the start of every program,
 * that uses this. If using thread_local storage, you have to call it once per thread. After that
 * every call to free and malloc is thread safe in both cases. If this fails, the program crashes.
 * No error is returned
 *
 */
void my_allocator_init(uint64_t size) {
	__my_malloc_globalObject.dataSize = size;

	// MAP_ANONYMOUS means, that
	//  "The mapping is not backed by any file; its contents are initialized to zero.  The fd
	//  argument is ignored; however, some implementations require fd to be -1 if MAP_ANONYMOUS (or
	//  MAP_ANON)  is  specified, and portable applications should ensure this.  The offset
	//  argument should be zero." ~ man page

	// this region is initalized with 0s
	__my_malloc_globalObject.data =
	    mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

	// see: https://github.com/torvalds/linux/blob/master/tools/include/nolibc/sys.h#L698-L708
	// example:
	// https://github.com/Apress/low-level-programming/blob/master/listings/chap4/mmap/mmap.asm
	// with sysvall, the result is an address, but with this you can check for errors!

	/* 0xfffffffffffff000

	if ((unsigned long)ret >= -4095UL) { // 0xfffffffffffff000
	    SET_ERRNO(-(long)ret);
	    ret = MAP_FAILED;
	}
	 */
	if(__my_malloc_globalObject.data == MAP_FAILED) {
		__my_malloc_globalObject.data = NULL;
		__my_malloc_globalObject.dataSize = 0;
		printErrorAndExit("INTERNAL: Failed to mmap for the allocator: %s\n", strerror(errno));
	}
	// FREE is set with the 0 initialized region automatically (only here!)

#if !defined(_ALLOCATOR_NOT_MT_SAVE) && _PER_THREAD_ALLOCATOR != 1
	// initialize the mutex, use default as attr
	int result = pthread_mutex_init(&__my_malloc_globalObject.mutex, NULL);
	checkResultForThreadErrorAndExit("INTERNAL: An Error occurred while trying to initializing the "
	                                 "internal mutex for the allocator");
#endif
	// initialize the first block, this sets everything to 0, but that isn't guaranteed in the
	// future and atm this is already done by mmap
	BlockInformation* firstBlock = (BlockInformation*)__my_malloc_globalObject.data;

	MEMCHECK_REMOVE_INTERNAL_USE(firstBlock, size);
	MEMCHECK_DEFINE_INTERNAL_USE(firstBlock, sizeof(BlockInformation));

	firstBlock->nextBlock = NULL;
	firstBlock->previousBlock = NULL;
	firstBlock->status = FREE;

// my_allocator_destroy is not always MT safe, in the thread local it is, but in the mutex case, it
// isn't so not using it there, in the mutex case, the caller that called the initialization HAS to
// call it manually, in the thread_local case, every thread cleans up the allocator themselves, a
// manual call to destroy is a safe noop, but not needed
#if _PER_THREAD_ALLOCATOR == 1
	int result2 = atexit(my_allocator_destroy);
	checkForThreadError(result2,
	                    "INTERNAL: An Error occurred while trying to register the atexit function",
	                    exit(EXIT_FAILURE););

#endif
}

// TODO: add realloc function

void* my_realloc(void* ptr, uint64_t size) {
	(void)ptr;
	(void)size;
	return NULL;
}

/**
 * @note NOT MT-safe,in the thread local case it is however, this function has to be called at the
 * end of every program, except when using thread locals, than you are free to call it, but don#t
 * have to, the initializer creates an atexit handler, that destroys this, but calling it manually
 * is a safe noop
 *
 */
void my_allocator_destroy(void) {
	if(__my_malloc_globalObject.data == NULL) {
		return;
	}

	MEMCHECK_REMOVE_INTERNAL_USE(__my_malloc_globalObject.data, __my_malloc_globalObject.dataSize);

	int result = munmap(__my_malloc_globalObject.data, __my_malloc_globalObject.dataSize);
	checkResultForThreadErrorAndExit("INTERNAL: Failed to munmap for the allocator:");

	__my_malloc_globalObject.data = NULL;
	__my_malloc_globalObject.dataSize = 0;

#if !defined(_ALLOCATOR_NOT_MT_SAVE) && _PER_THREAD_ALLOCATOR != 1
	result = pthread_mutex_destroy(&__my_malloc_globalObject.mutex);
	checkResultForThreadErrorAndExit(
	    "INTERNAL: An Error occurred while trying to destroy the internal mutex "
	    "in cleaning up for the allocator");
#endif
}
