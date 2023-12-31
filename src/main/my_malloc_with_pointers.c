/*
Author: Totto16
*/

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>

#include <utils.h>

#include "my_malloc.h"

#if !defined(_PER_THREAD_ALLOCATOR)
#define _PER_THREAD_ALLOCATOR 0
#endif

#if _PER_THREAD_ALLOCATOR < 0 || _PER_THREAD_ALLOCATOR > 1
// this is a c preprocessor macro, it throws a compiler error with the given message
#error "NOT SUPPORTED PER_THREAD_ALLOCATOR: not between 0 and 1!"
#endif

#ifndef _TESTS_INTERNAL_FUNCTION
#define INTERNAL_FUNCTION static
#else
#define INTERNAL_FUNCTION

#endif

// the variables that start with __ can be visible globally, so they're  prefixed by __my_malloc_ so
// that it doesn't pollute the global scope additionally these are made static! (meaning no outside
// file can see them, on global variables this only makes them inivisible to other files )

typedef uint8_t status_t;

typedef uint32_t block_number_t;

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
	block_number_t blockNumber;
} BlockInformation;

// every MemoryBlock starts with this
// [ MemoryBlock | BlockInformation | .....  ]

typedef struct {
	uint64_t size;
	void* next;
	block_number_t number;
} MemoryBlockinformation;

typedef struct {
#if !defined(_ALLOCATOR_NOT_MT_SAVE) && _PER_THREAD_ALLOCATOR != 1
	pthread_mutex_t mutex;
#endif
	MemoryBlockinformation* block;
	uint64_t defaultMemoryBlockSize;
} GlobalObject;

#if _PER_THREAD_ALLOCATOR == 0 || defined(_ALLOCATOR_NOT_MT_SAVE)
static GlobalObject __my_malloc_globalObject = { .defaultMemoryBlockSize = 0 };
#else
// if _PER_THREAD_ALLOCATOR is 1 it allocates one such structure per Thread, this is done with the
// keyword "_Thread_local" (underscore Uppercase, and double underscore  + any case are reserved
// words for the c standard, so this was introduced in c11, there exists a typedef thread_local for
// that, but I rather use the Keyword directly )
// ATTENTION: each Thread also has to call my_allocator_init, otherwise this is NULL and I DON'T
// check if it's NULL ANYWHERE, meaning it will crash rather instantly trying to read from or store
// to address 0!! from the data entry in the struct, that is 0 initialized!)
static _Thread_local GlobalObject __my_malloc_globalObject = { .defaultMemoryBlockSize = 0 };
#endif

/**
 * @brief INTERNAL FUNCTION: DO NOT USE
 *
 * @note Needs to be called with the mutex locked, in order to be thread safe!
 *
 */
INTERNAL_FUNCTION MemoryBlockinformation* get_memory_block_by_number(block_number_t number) {

	MemoryBlockinformation* nextMemoryBlock =
	    (MemoryBlockinformation*)__my_malloc_globalObject.block; // may be NULL

	while(nextMemoryBlock != NULL) {

		if(nextMemoryBlock->number == number) {
			return nextMemoryBlock;
		}

		nextMemoryBlock = nextMemoryBlock->next;
	};

	return NULL;
}

/**
 * @brief INTERNAL FUNCTION: DO NOT USE
 *
 * @note Needs to be called with the mutex locked, in order to be thread safe!
 *
 */
INTERNAL_FUNCTION MemoryBlockinformation* get_last_memory_block() {

	MemoryBlockinformation* nextMemoryBlock =
	    (MemoryBlockinformation*)__my_malloc_globalObject.block; // may be NULL

	while(nextMemoryBlock != NULL) {

		if(nextMemoryBlock->next == NULL) {
			return nextMemoryBlock;
		}

		nextMemoryBlock = nextMemoryBlock->next;
	};

	return NULL;
}

/**
 * @brief INTERNAL FUNCTION: DO NOT USE
 *
 * @note Needs to be called with the mutex locked, in order to be thread safe!
 *
 */
INTERNAL_FUNCTION block_number_t get_next_free_memory_number() {

	block_number_t start = 0;

	while(true) {

		// we need to cover some potential strange pattern:
		// e.g. 0 1 3 4 5 2 needs to say 6
		// but 0 1 3 4 5 6 needs to say 2
		bool needsRestart = false;

		const block_number_t oldStart = start;

		MemoryBlockinformation* nextMemoryBlock =
		    (MemoryBlockinformation*)__my_malloc_globalObject.block;

		while(nextMemoryBlock != NULL) {

			if(nextMemoryBlock->number == start) {
				start = nextMemoryBlock->number + 1;
			} else if(nextMemoryBlock->number > start) {
				needsRestart = true;
			}

			nextMemoryBlock = nextMemoryBlock->next;
		};

		// no rerun needed => start is settled, if the rerun didn't change anything, it is settled
		if(!needsRestart || (needsRestart && start == oldStart)) {
			break;
		}
	}

	return start;
}

/**
 * @brief INTERNAL FUNCTION: DO NOT USE
 *
 * @note Needs to be called with the mutex locked, in order to be thread safe!
 *
 */
INTERNAL_FUNCTION uint64_t size_of_double_pointer_block(BlockInformation* block) {
	if(block == NULL) {
		printSingleErrorAndExit(
		    "INTERNAL: This is an allocator ERROR, this shouldn't occur: block is NULL\n");
	} else if(block->nextBlock == NULL) {
		const MemoryBlockinformation* currentMemoryBlock =
		    get_memory_block_by_number(block->blockNumber);
		if(currentMemoryBlock == NULL) {
			printSingleErrorAndExit("INTERNAL: This is an allocator ERROR, this shouldn't occur: "
			                        "currentMemoryBlock is NULL\n");
		}

		return (((pseudoByte*)currentMemoryBlock + currentMemoryBlock->size) - (pseudoByte*)block) -
		       sizeof(BlockInformation);
	} else if(block->blockNumber == ((BlockInformation*)block->nextBlock)->blockNumber) {
		return ((pseudoByte*)block->nextBlock - (pseudoByte*)block) - sizeof(BlockInformation);
	} else {
		const MemoryBlockinformation* currentMemoryBlock =
		    get_memory_block_by_number(block->blockNumber);

		if(currentMemoryBlock == NULL) {
			printSingleErrorAndExit("INTERNAL: This is an allocator ERROR, this shouldn't occur: "
			                        "currentMemoryBlock is NULL\n");
		}

		// in this case the memory is the same, as if the nextBlock was NULL, since the contigious
		// block ends here, we assert a bunch of stuff regarding the position of the next block, it
		// HAS to be the first of the new memoryBlock, the number there doesn't have to be one
		// larger, it just has to be not the same. It is mostly one larger, but don't rely on that!
		// Also there must be a nextMemoryBlock, if the next Pointer is not NULL

		const MemoryBlockinformation* nextMemoryBlock =
		    (MemoryBlockinformation*)currentMemoryBlock->next;

		if(nextMemoryBlock->next == NULL) {
			printSingleErrorAndExit("INTERNAL: This is an allocator ERROR, this shouldn't occur: "
			                        "nextMemoryBlock is NULL\n");
		}

		if(currentMemoryBlock->number == nextMemoryBlock->number) {
			printSingleErrorAndExit(
			    "INTERNAL: This is an allocator ERROR, this shouldn't occur: "
			    "currentMemoryBlock and nextMemoryBlock have the same number\n");
		}

		if((pseudoByte*)nextMemoryBlock !=
		   ((pseudoByte*)block->nextBlock) - sizeof(MemoryBlockinformation)) {
			printSingleErrorAndExit("INTERNAL: This is an allocator ERROR, this shouldn't occur: "
			                        "nextBlock in another memory block isn't the first block\n");
		}

		return (((pseudoByte*)currentMemoryBlock + currentMemoryBlock->size) - (pseudoByte*)block) -
		       sizeof(BlockInformation);
	}
}

/**
 * @brief INTERNAL FUNCTION; DO NOT USE
 *
 * @note Needs to be called with the mutex locked, in order to be thread safe!
 *
 */
INTERNAL_FUNCTION bool __my_malloc_block_fitsBetter(BlockInformation* toCompare,
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
 * @brief internal malloc, used by realloc and malloc, but doesn't lock mutexes, that is done by the
 * parent functions, DO NOT us outside of the internals of this file!
 */
INTERNAL_FUNCTION void* __internal__my_malloc(uint64_t size, BlockInformation* fixedBlock) {

	// calling my_malloc without initializing the allocator doesn't work, if that is the case,
	// likely the uninitialized mutex access before this will crash the program, but that is here
	// for safety measures! AND ALSO in the case of uninitialized allocator in the thread local case
	if(__my_malloc_globalObject.defaultMemoryBlockSize == 0) {
		fprintf(stderr, "Calling malloc before initializing the allocator is prohibited!\n");
		exit(1);
	}

	BlockInformation* bestFit = fixedBlock;
	if(bestFit == NULL && __my_malloc_globalObject.block != NULL) {

		bestFit = (BlockInformation*)(((pseudoByte*)__my_malloc_globalObject.block) +
		                              sizeof(MemoryBlockinformation));
		BlockInformation* nextFreeBlock = (BlockInformation*)bestFit->nextBlock;

		while(nextFreeBlock != NULL) {
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
	}

	const uint64_t blockSize =
	    __my_malloc_globalObject.block == NULL ? 0 : size_of_double_pointer_block(bestFit);

	// if the one that fit the best is not big enough, it means no block is big enough! If it's not
	// free, than there was no free block
	if(__my_malloc_globalObject.block == NULL || bestFit == NULL || bestFit->status != FREE ||
	   blockSize < size) {

		//  allocate a new memory block, if the size is bigger than pool size, just request a bigger
		//  one, we can do that here, try first to get a
		// continuos block, if that works, increase the size of the current one, otherwise just make
		// a new MemoryBlockInfo structure.

		MemoryBlockinformation* lastMemoryBlock = get_last_memory_block(); // may be NULL
		void* preferredAddress =
		    lastMemoryBlock == NULL ? NULL : ((pseudoByte*)lastMemoryBlock) + lastMemoryBlock->size;

		uint64_t preferredSize = __my_malloc_globalObject.defaultMemoryBlockSize;

		if(preferredSize - sizeof(MemoryBlockinformation) - sizeof(BlockInformation) < size) {
			preferredSize = size + sizeof(MemoryBlockinformation) - sizeof(BlockInformation);
		}

		void* newRegion = mmap(preferredAddress, preferredSize, PROT_READ | PROT_WRITE,
		                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

		if(newRegion == MAP_FAILED) {
			// don't fail, just return NULL ,indicating Out of memory
			return NULL;
		}

		MEMCHECK_REMOVE_INTERNAL_USE(newRegion, preferredSize);

		MemoryBlockinformation* newMemoryBlock = (MemoryBlockinformation*)newRegion;

		MEMCHECK_DEFINE_INTERNAL_USE(newMemoryBlock, sizeof(MemoryBlockinformation));

		newMemoryBlock->next = NULL;
		newMemoryBlock->size = preferredSize;

		block_number_t blockNumber = get_next_free_memory_number();
		newMemoryBlock->number = blockNumber;

		if(lastMemoryBlock == NULL) {
			__my_malloc_globalObject.block = newMemoryBlock;
		} else {
			lastMemoryBlock->next = newMemoryBlock;
		}

		BlockInformation* newBlock =
		    (BlockInformation*)((pseudoByte*)newRegion + sizeof(MemoryBlockinformation));

		// no adding the BlockInformation to the memoryBlock
		MEMCHECK_DEFINE_INTERNAL_USE(newBlock, sizeof(BlockInformation));

		newBlock->nextBlock = NULL;
		newBlock->status = FREE;
		newBlock->blockNumber = blockNumber;

		// FIND previous block, to set the nextBlock there to the newBlock and set the previousBlock
		// on the newBlock, if there is a previous block (not possible, if there where 0 blocks)

		if(lastMemoryBlock != NULL) {

			BlockInformation* lastBlockOfLastMemBlock =
			    (BlockInformation*)(((pseudoByte*)lastMemoryBlock) +
			                        sizeof(MemoryBlockinformation));

			while(lastBlockOfLastMemBlock->nextBlock != NULL) {
				lastBlockOfLastMemBlock = (BlockInformation*)lastBlockOfLastMemBlock->nextBlock;
			}

			newBlock->previousBlock = lastBlockOfLastMemBlock;
			lastBlockOfLastMemBlock->nextBlock = newBlock;
		}

		// Now call internal malloc with the new block as hint, to use it, without duplicating code
		// and searching for it, if we already have it

		return __internal__my_malloc(size, newBlock);
	};

	if(blockSize - size <= (sizeof(BlockInformation))) {
		// block size and size needed for allocation is the same, only need to set the status to
		// allocated

		// OR

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
		newBlock->blockNumber = bestFit->blockNumber;

		bestFit->status = ALLOCED;
		bestFit->nextBlock = newBlock;

		if(newBlock->nextBlock != NULL) {
			((BlockInformation*)newBlock->nextBlock)->previousBlock = newBlock;
		}
	}

	void* returnValue = (pseudoByte*)bestFit + sizeof(BlockInformation);

	MEMCHECK_DEFINE_INTERNAL_USE(bestFit, sizeof(BlockInformation));
	VALGRIND_ALLOC(returnValue, size, 0, false);

	return returnValue;
}

/**
 * @note MT-safe - with thread_local storage, this only accesses that, otherwise a mutex is
 * used, if this is called without initializing the underlying allocator beforehand, it is
 * undefined behaviour, however this function crashes the program in that case
 */
void* my_malloc(uint64_t size) {

#if !defined(_ALLOCATOR_NOT_MT_SAVE) && _PER_THREAD_ALLOCATOR != 1
	int result = pthread_mutex_lock(&__my_malloc_globalObject.mutex);

	if(__my_malloc_globalObject.defaultMemoryBlockSize == 0) {
		fprintf(stderr, "Calling malloc before initializing the allocator is prohibited!\n");
		exit(1);
	}

	// mutex errors are better when being asserted, since no real errors can occur, only when the
	// system is already malfunctioning
	checkResultForThreadErrorAndExit(
	    "INTERNAL: An Error occurred while trying to lock the mutex in the internal allocator");
#endif

	void* returnValue = __internal__my_malloc(size, NULL);

#if !defined(_ALLOCATOR_NOT_MT_SAVE) && _PER_THREAD_ALLOCATOR != 1
	result = pthread_mutex_unlock(&__my_malloc_globalObject.mutex);
	checkResultForThreadErrorAndExit(
	    "INTERNAL: An Error occurred while trying to unlock the internal allocator mutex");
#endif

	return returnValue;
}

/**
 * @brief internal free, used by realloc and free, but doesn't lock mutexes, that is done by the
 * parent functions, DO NOT us outside of the internals of this file!
 */
INTERNAL_FUNCTION void __internal__my_free(void* ptr) {

	// calling my_free without initializing the allocator doesn't work, if that is the case,
	// likely the uninitialized mutex access before this will crash the program, but that is here
	// for safety measures! AND ALSO in the case of uninitialized allocator in the thread local case
	if(__my_malloc_globalObject.defaultMemoryBlockSize == 0) {
		fprintf(stderr, "Calling free before initializing the allocator is prohibited!\n");
		exit(1);
	}

	BlockInformation* currentBlock =
	    (BlockInformation*)((pseudoByte*)ptr - sizeof(BlockInformation));

	if(__my_malloc_globalObject.block == NULL) {
		// no block is not free, since we have no block anymore xD
		printErrorAndExit("ERROR: You tried to free a already freed Block: %p\n", ptr);
	}

	if(currentBlock->status == FREE) {
		printErrorAndExit("ERROR: You tried to free a already freed Block: %p\n", ptr);
	}

	currentBlock->status = FREE;
	VALGRIND_FREE(ptr, 0);

	BlockInformation* nextBlock = (BlockInformation*)currentBlock->nextBlock;
	BlockInformation* previousBlock = (BlockInformation*)currentBlock->previousBlock;

	bool currentWasRemoved = false;

	// merge with previous free block, if in 5the same memory block!
	if(previousBlock != NULL && previousBlock->status == FREE &&
	   previousBlock->blockNumber == currentBlock->blockNumber) {

		// MERGE three free blocks into one: layout Previous | Current | Next => New Free one
		if(nextBlock != NULL && nextBlock->status == FREE &&
		   nextBlock->blockNumber == currentBlock->blockNumber) {
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

		MEMCHECK_REMOVE_INTERNAL_USE(currentBlock, sizeof(BlockInformation));
		currentWasRemoved = true;

		// merge next free block with current one, if in the same memory block
	} else if(nextBlock != NULL && nextBlock->status == FREE &&
	          nextBlock->blockNumber == currentBlock->blockNumber) {
		currentBlock->nextBlock = nextBlock->nextBlock; // can be NULL

		if(nextBlock->nextBlock != NULL) {
			((BlockInformation*)nextBlock->nextBlock)->previousBlock = currentBlock;
		}

		MEMCHECK_REMOVE_INTERNAL_USE(nextBlock, sizeof(BlockInformation));
	}

	// if a new EMPTY memory block was created munmap it!

	// step 1: get the potential start block of a memory block, this can't be the next, since that
	// would have deleted the memory block on his free, if it was totally free
	BlockInformation* potentialFirstBlock = currentBlock;

	// Case 1, previous was free and is in the same memory block
	if(previousBlock != NULL && previousBlock->status == FREE &&
	   (currentWasRemoved || previousBlock->blockNumber == currentBlock->blockNumber)) {
		potentialFirstBlock = previousBlock;
	}

	// case 2, current is the potential start (do nothing, as potentialFirstBlock is already
	// correct)

	// step 2: get the start of the current block
	MemoryBlockinformation* currentMemoryBlock =
	    get_memory_block_by_number(potentialFirstBlock->blockNumber);

	if(currentMemoryBlock == NULL) {
		printSingleErrorAndExit("INTERNAL: This is an allocator ERROR, this shouldn't occur: "
		                        "currentMemoryBlock is NULL\n");
	}

	// step 3: test if the potentialFirstBlock is the first block, and it also spans the whole block
	// (and is free, but that is already assured)
	if((pseudoByte*)currentMemoryBlock + sizeof(MemoryBlockinformation) ==
	       (pseudoByte*)potentialFirstBlock &&
	   (potentialFirstBlock->nextBlock == NULL ||
	    (pseudoByte*)potentialFirstBlock->nextBlock >
	        ((pseudoByte*)currentMemoryBlock + currentMemoryBlock->size) ||
	    (pseudoByte*)potentialFirstBlock->nextBlock < ((pseudoByte*)currentMemoryBlock))) {

		// now remove this block with munmap, pay attention to the last one, we have to adjust the
		// global value there!

		// since we have no double pointers, the linked list has to be traversed, and the previous
		// (if there is one) block has to be found and the next pointer has to be adjusted, it has
		// to be checked, if the global object (the first list header) is the holder and than adjust
		// that pointer too, also don't delete the last memory block, so at least one has to remain

		if(__my_malloc_globalObject.block == currentMemoryBlock) {
			if(currentMemoryBlock->next == NULL) {
				// the last memory block can be deleted
				__my_malloc_globalObject.block = NULL;

			} else {
				__my_malloc_globalObject.block = currentMemoryBlock->next; // may be NULL
			}

		} else {

			// not special case, the block is somewhere in the middle of this single linked list
			MemoryBlockinformation* previousMemoryBlock = NULL;

			{
				MemoryBlockinformation* nextMemoryBlock =
				    (MemoryBlockinformation*)__my_malloc_globalObject.block;

				while(nextMemoryBlock != NULL) {

					if(nextMemoryBlock->next == currentMemoryBlock) {
						previousMemoryBlock = nextMemoryBlock;
						break;
					}

					nextMemoryBlock = nextMemoryBlock->next;
				};
			}

			if(previousMemoryBlock != NULL) {
				previousMemoryBlock->next = currentMemoryBlock->next; // may be NULL
			}
		}

		//  remove the current FREE block, that is in this to be deleted memory block
		if(potentialFirstBlock->nextBlock != NULL) {
			((BlockInformation*)potentialFirstBlock->nextBlock)->previousBlock =
			    potentialFirstBlock->previousBlock; // may be NULL
		}

		if(potentialFirstBlock->previousBlock != NULL) {
			((BlockInformation*)potentialFirstBlock->previousBlock)->nextBlock =
			    potentialFirstBlock->nextBlock; // may be NULL
		}

		const uint64_t currentMemoryBlockSize = currentMemoryBlock->size;

		int result = munmap(currentMemoryBlock, currentMemoryBlockSize);
		checkResultForThreadErrorAndExit("INTERNAL: Failed to munmap for the allocator:");

		MEMCHECK_REMOVE_INTERNAL_USE(currentMemoryBlock, currentMemoryBlockSize);
	}
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

	__internal__my_free(ptr);

#if !defined(_ALLOCATOR_NOT_MT_SAVE) && _PER_THREAD_ALLOCATOR != 1
	result = pthread_mutex_unlock(&__my_malloc_globalObject.mutex);
	checkResultForThreadErrorAndExit(
	    "INTERNAL: An Error occurred while trying to unlock the internal allocator mutex");
#endif
}

/**
 * @brief If ptr is NULL, this behaves as my_malloc
 * If size == 0 it behaves as my_free and returns NULL
 *
 * Otherwise it reallocates the memory, it may have a different address than before, but the return
 * value may also be the same as ptr. If the new size is greater than the previous size, the whole
 * content of the previous ptr is preserves and copied to the new ptr, if needed, the rest of the
 * data is undefined. If the new size is smaller, the data beyond that is potentially overwritten,
 * at least it's not accessible anymore, the returned ptr can be the same, but doesn't have to be
 * the same, since realloc may chose a better suited block for it, if that'S the case, the data up
 * to the new size is the same as the old one
 */
void* my_realloc(void* ptr, uint64_t size) {

	// if ptr == NULL, it is the same as my_malloc(size);
	if(ptr == NULL) {
		return my_malloc(size);
	}

	// if size == 0, it is the same as my_free(size);
	if(size == 0) {
		my_free(ptr);
		return NULL;
	}

#if !defined(_ALLOCATOR_NOT_MT_SAVE) && _PER_THREAD_ALLOCATOR != 1
	int result = pthread_mutex_lock(&__my_malloc_globalObject.mutex);

	if(__my_malloc_globalObject.defaultMemoryBlockSize == 0) {
		fprintf(stderr, "Calling realloc before initializing the allocator is prohibited!\n");
		exit(1);
	}

	// mutex errors are better when being asserted, since no real errors can occur, only when the
	// system is already malfunctioning
	checkResultForThreadErrorAndExit(
	    "INTERNAL: An Error occurred while trying to lock the mutex in the internal allocator");
#endif

	// calling my_malloc without initializing the allocator doesn't work, if that is the case,
	// likely the uninitialized mutex access before this will crash the program, but that is here
	// for safety measures! AND ALSO in the case of uninitialized allocator in the thread local case
	if(__my_malloc_globalObject.defaultMemoryBlockSize == 0) {
		fprintf(stderr, "Calling realloc before initializing the allocator is prohibited!\n");
		exit(1);
	}

	BlockInformation* currentBlock =
	    (BlockInformation*)((pseudoByte*)ptr - sizeof(BlockInformation));

	if(__my_malloc_globalObject.block == NULL) {
		// no block is not free, since we have no block anymore xD
		printErrorAndExit("ERROR: You tried to realloc a freed Block: %p\n", ptr);
	}

	if(currentBlock->status == FREE) {
		printErrorAndExit("ERROR: You tried to realloc a freed Block: %p\n", ptr);
	}

	// ATTENTION: this size isn't always the correct size, of the previous alloc! since some amount
	// of dread space can be at the end, it can be between 0 and sizeof(BlockInformation) bytes,
	// since there's no room for a new block in there. So every calculation here has to pay
	// attention to that

	// It is fine, to copy the undefined memory, since it's  at the end, where the new memory would
	// be undefined nevertheless
	const uint64_t blockSize = size_of_double_pointer_block(currentBlock);

	// CASE 1: the new size is smaller or the same (it may be also the same, if the blockSize is
	// slightly bigger, since there might be end padding!)
	if(size <= blockSize) {

		// CASE 1.1: no new block can be placed after the new size, so just returning the old size
		// and doing some valgrind house keeping
		if(blockSize - size <= sizeof(BlockInformation)) {

			VALGRIND_FREE(ptr, 0);
			VALGRIND_ALLOC(ptr, size, 0, false);

#if !defined(_ALLOCATOR_NOT_MT_SAVE) && _PER_THREAD_ALLOCATOR != 1
			int result = pthread_mutex_unlock(&__my_malloc_globalObject.mutex);
			checkResultForThreadErrorAndExit("INTERNAL: An Error occurred while trying to "
			                                 "unlock the internal allocator mutex");
#endif
			// just return the old pointer
			return ptr;

		} else {

			// CASE 1.2: make a new block, that is free, and is at the end of size

			// If applicable we search a better block, that is better suited for that cause, that
			// is computational intensive, but it may create less holes in the end also pay
			// attention to padding, so it has to be at least sizeof(BlockInformation) smaller, to
			// allocate a new block!

			// CASE 1.2.1: the new areas is significantly smaller than the last one, so using free +
			// malloc to get a better spot for the significantly smaller size, use 50% as threshold,
			// so that if it's 50% smaller, use malloc to get a new block
			// small NOTE: since we don't free this block before issuing a malloc, this block might
			// be suited better, but we have to use another xD, it might even return NULL, so it'S
			// out of memory xD
			if(size * 2 < blockSize) {

				void* newRegion = __internal__my_malloc(size, NULL);

				// out of memory, so just use the current nevertheless xD
				// ATTENTION: code duplication, since no good pattern emerges, to reuse code via
				// call or control flow :(
				if(newRegion == NULL) {

					VALGRIND_FREE(ptr, 0);

					BlockInformation* newBlock =
					    (BlockInformation*)(((pseudoByte*)currentBlock + sizeof(BlockInformation)) +
					                        size);
					MEMCHECK_DEFINE_INTERNAL_USE(newBlock, sizeof(BlockInformation));

					newBlock->nextBlock = currentBlock->nextBlock; // may be NULL
					newBlock->previousBlock = currentBlock;
					newBlock->status = FREE;
					newBlock->blockNumber = currentBlock->blockNumber;

					currentBlock->nextBlock = newBlock;
					if(newBlock->nextBlock != NULL) {
						((BlockInformation*)newBlock->nextBlock)->previousBlock = newBlock;
					}

					VALGRIND_ALLOC(ptr, size, 0, false);

#if !defined(_ALLOCATOR_NOT_MT_SAVE) && _PER_THREAD_ALLOCATOR != 1
					int result = pthread_mutex_unlock(&__my_malloc_globalObject.mutex);
					checkResultForThreadErrorAndExit("INTERNAL: An Error occurred while trying to "
					                                 "unlock the internal allocator mutex");
#endif
					// just return the old pointer
					return ptr;
				}

				// copy the subset of data into the new region
				void* dest = memcpy(newRegion, ptr, size);

				if(dest != newRegion) {
					fprintf(stderr,
					        "Error during memcpy, dest pointer is not the same as the given dest "
					        "pointer: "
					        "%p != %p\n",
					        newRegion, dest);
					exit(1);
				}

				// free the previous section
				__internal__my_free(ptr);

#if !defined(_ALLOCATOR_NOT_MT_SAVE) && _PER_THREAD_ALLOCATOR != 1
				int result = pthread_mutex_unlock(&__my_malloc_globalObject.mutex);
				checkResultForThreadErrorAndExit("INTERNAL: An Error occurred while trying to "
				                                 "unlock the internal allocator mutex");
#endif
				// return the new region
				return newRegion;

			} else {

				// CASE 1.2.2: just divide the block and use the current One

				VALGRIND_FREE(ptr, 0);

				BlockInformation* newBlock =
				    (BlockInformation*)(((pseudoByte*)currentBlock + sizeof(BlockInformation)) +
				                        size);
				MEMCHECK_DEFINE_INTERNAL_USE(newBlock, sizeof(BlockInformation));

				newBlock->nextBlock = currentBlock->nextBlock; // may be NULL
				newBlock->previousBlock = currentBlock;
				newBlock->status = FREE;
				newBlock->blockNumber = currentBlock->blockNumber;

				currentBlock->nextBlock = newBlock;
				if(newBlock->nextBlock != NULL) {
					((BlockInformation*)newBlock->nextBlock)->previousBlock = newBlock;
				}

				VALGRIND_ALLOC(ptr, size, 0, false);

#if !defined(_ALLOCATOR_NOT_MT_SAVE) && _PER_THREAD_ALLOCATOR != 1
				int result = pthread_mutex_unlock(&__my_malloc_globalObject.mutex);
				checkResultForThreadErrorAndExit("INTERNAL: An Error occurred while trying to "
				                                 "unlock the internal allocator mutex");
#endif
				// just return the old pointer
				return ptr;
			}
		}

	} else {
		// CASE 2: the size is bigger

		// it is enough to look forward one block, since there is per guarantee no block that is
		// also free, after a free block

		BlockInformation* nextBlock = (BlockInformation*)currentBlock->nextBlock; // may be NULL

		// Case 2.1: the current block with the next block can fit the new size!
		if(nextBlock != NULL && nextBlock->status == FREE) {
			const uint64_t nextBlockSize = size_of_double_pointer_block(nextBlock);

			const uint64_t totalPotentialSize =
			    nextBlockSize + sizeof(BlockInformation) + blockSize;

			if(totalPotentialSize >= size) {

				// figure out, if theres space for another block inside the new larger area!

				// CASE 2.1.1: no new block can be placed inside the new larger area, just deleting
				// the old in the middle (nextBlock)
				if(totalPotentialSize - size <= (sizeof(BlockInformation))) {

					currentBlock->nextBlock = nextBlock->nextBlock; // can be NULL
					if(nextBlock->nextBlock != NULL) {
						((BlockInformation*)nextBlock->nextBlock)->previousBlock = currentBlock;
					}

					MEMCHECK_REMOVE_INTERNAL_USE(nextBlock, sizeof(BlockInformation));
					VALGRIND_ALIGN_ALLOC_TO_GREATER_BLOCK(ptr, size);

#if !defined(_ALLOCATOR_NOT_MT_SAVE) && _PER_THREAD_ALLOCATOR != 1
					int result = pthread_mutex_unlock(&__my_malloc_globalObject.mutex);
					checkResultForThreadErrorAndExit("INTERNAL: An Error occurred while trying to "
					                                 "unlock the internal allocator mutex");
#endif
					// just return the old pointer, it has now space for the size
					return ptr;

				} else {

					// CASE 2.1.2: delete the current one and create a new one at the end, that is
					// free

					BlockInformation* newBlock =
					    (BlockInformation*)(((pseudoByte*)currentBlock + sizeof(BlockInformation)) +
					                        size);
					MEMCHECK_DEFINE_INTERNAL_USE(newBlock, sizeof(BlockInformation));

					newBlock->previousBlock = currentBlock;
					newBlock->nextBlock = nextBlock->nextBlock; // can be NULL
					newBlock->status = FREE;
					newBlock->blockNumber = currentBlock->blockNumber;

					currentBlock->nextBlock = newBlock;

					if(nextBlock->nextBlock != NULL) {
						((BlockInformation*)nextBlock->nextBlock)->previousBlock = newBlock;
					}

					MEMCHECK_REMOVE_INTERNAL_USE(nextBlock, sizeof(BlockInformation));
					VALGRIND_ALIGN_ALLOC_TO_GREATER_BLOCK(ptr, size);

#if !defined(_ALLOCATOR_NOT_MT_SAVE) && _PER_THREAD_ALLOCATOR != 1
					int result = pthread_mutex_unlock(&__my_malloc_globalObject.mutex);
					checkResultForThreadErrorAndExit("INTERNAL: An Error occurred while trying to "
					                                 "unlock the internal allocator mutex");
#endif
					// just return the old pointer, it has now space for the size
					return ptr;
				}
			}
		}

		// CASE 2.2 we need to issue a new malloc and copy the data over
		void* newRegion = __internal__my_malloc(size, NULL);

		if(newRegion == NULL) {
#if !defined(_ALLOCATOR_NOT_MT_SAVE) && _PER_THREAD_ALLOCATOR != 1
			int result = pthread_mutex_unlock(&__my_malloc_globalObject.mutex);
			checkResultForThreadErrorAndExit("INTERNAL: An Error occurred while trying to "
			                                 "unlock the internal allocator mutex");
#endif

			return NULL;
		}

		// copy the data into the new region, the blockSize is not 100%% accurate, but as said
		// above, the rest is undefined memory, as the rest of the newRegion region
		void* dest = memcpy(newRegion, ptr, blockSize);

		if(dest != newRegion) {
			fprintf(stderr,
			        "Error during memcpy, dest pointer is not the same as the given dest pointer: "
			        "%p != %p\n",
			        newRegion, dest);
			exit(1);
		}

		// free the previous section
		__internal__my_free(ptr);

#if !defined(_ALLOCATOR_NOT_MT_SAVE) && _PER_THREAD_ALLOCATOR != 1
		int result = pthread_mutex_unlock(&__my_malloc_globalObject.mutex);
		checkResultForThreadErrorAndExit("INTERNAL: An Error occurred while trying to "
		                                 "unlock the internal allocator mutex");
#endif
		// return the new region
		return newRegion;
	}
}

/**
 * @note NOT MT-safe. this function HAS TO BE called exactly once at the start of every program,
 * that uses this. If using thread_local storage, you have to call it once per thread. After that
 * every call to free and malloc is thread safe in both cases. If this fails, the program crashes.
 * No error is returned
 *
 * By default the allocator doesn't allocate a memory block, it creates a block in the first called
 * malloc. But you can force the creation of, one, if you wish so, but free may remove the last one,
 * no guarantee there. This whole thing would largely benefit programs, that don't use any dynamic
 * memory, but use this malloc, so no mmap call will be issued and no memory is required, if they
 * don't opt in into it
 *
 */

void my_allocator_init(uint64_t size, bool force_alloc) {
	__my_malloc_globalObject.block = NULL;
	__my_malloc_globalObject.defaultMemoryBlockSize = size;
	// MAP_ANONYMOUS means, that
	//  "The mapping is not backed by any file; its contents are initialized to zero.  The fd
	//  argument is ignored; however, some implementations require fd to be -1 if MAP_ANONYMOUS (or
	//  MAP_ANON)  is  specified, and portable applications should ensure this.  The offset
	//  argument should be zero." ~ man page

	if(force_alloc) {

		// this region is initalized with 0s
		__my_malloc_globalObject.block =
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
		if(__my_malloc_globalObject.block == MAP_FAILED) {
			__my_malloc_globalObject.block = NULL;
			printErrorAndExit("ERROR: Failed to allocate memory in the allocator: %s\n",
			                  strerror(errno));
		}

		// FREE is set with the 0 initialized region automatically (only here!)

		MEMCHECK_REMOVE_INTERNAL_USE(__my_malloc_globalObject.block, size);

		// initialize the first memoryBlock
		MemoryBlockinformation* firstMemoryBlock =
		    (MemoryBlockinformation*)__my_malloc_globalObject.block;

		MEMCHECK_DEFINE_INTERNAL_USE(firstMemoryBlock, sizeof(MemoryBlockinformation));

		firstMemoryBlock->size = size;
		firstMemoryBlock->number = 0;
		firstMemoryBlock->next = NULL;

		// initialize the first block
		BlockInformation* firstBlock =
		    (BlockInformation*)((pseudoByte*)firstMemoryBlock + sizeof(MemoryBlockinformation));

		MEMCHECK_DEFINE_INTERNAL_USE(firstBlock, sizeof(BlockInformation));

		firstBlock->nextBlock = NULL;
		firstBlock->previousBlock = NULL;
		firstBlock->status = FREE;
		firstBlock->blockNumber = 0;
	}

#if !defined(_ALLOCATOR_NOT_MT_SAVE) && _PER_THREAD_ALLOCATOR != 1
	// initialize the mutex, use default as attr
	int result = pthread_mutex_init(&__my_malloc_globalObject.mutex, NULL);
	checkResultForThreadErrorAndExit("INTERNAL: An Error occurred while trying to initializing the "
	                                 "internal mutex for the allocator");
#endif

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

/**
 * @note NOT MT-safe,in the thread local case it is however, this function has to be called at the
 * end of every program, except when using thread locals, than you are free to call it, but don#t
 * have to, the initializer creates an atexit handler, that destroys this, but calling it manually
 * is a safe noop
 *
 */
void my_allocator_destroy(void) {
	if(__my_malloc_globalObject.block == NULL) {
		return;
	}

	MemoryBlockinformation* nextMemoryBlock =
	    (MemoryBlockinformation*)__my_malloc_globalObject.block;

	__my_malloc_globalObject.block = NULL;

	// unmap the memory blocks in order
	while(nextMemoryBlock != NULL) {

		MemoryBlockinformation* currentMemoryBlock = nextMemoryBlock;
		const uint64_t currentMemoryBlockSize = currentMemoryBlock->size;

		nextMemoryBlock = nextMemoryBlock->next;

		int result = munmap(currentMemoryBlock, currentMemoryBlockSize);
		checkResultForThreadErrorAndExit("INTERNAL: Failed to munmap for the allocator:");

		MEMCHECK_REMOVE_INTERNAL_USE(currentMemoryBlock, currentMemoryBlockSize);
	};

#if !defined(_ALLOCATOR_NOT_MT_SAVE) && _PER_THREAD_ALLOCATOR != 1
	int result = pthread_mutex_destroy(&__my_malloc_globalObject.mutex);
	checkResultForThreadErrorAndExit(
	    "INTERNAL: An Error occurred while trying to destroy the internal mutex "
	    "in cleaning up for the allocator");
#endif
}

#ifdef __cplusplus
}
#endif
