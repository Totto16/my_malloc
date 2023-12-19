/*
Author: Tobias Niederbrunner - csba1761
Module: PS OS 10
*/

// FREE LIST has only FREE Elements in the list :( 🤦‍♂️

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>

#include "../shared/utils.h"

// statically defined Block Size
#define BLOCK_SIZE 1024

// for exercise 3, see the explanation down below!
#if !defined(_PER_THREAD_ALLOCATOR)
#define _PER_THREAD_ALLOCATOR 0
#endif

// simple preprocessor checker for valid value!
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

// FREE is 0, so each mmap (so initialized to 0) memory has set every block to FREE
enum __my_malloc_status { FREE = 0, ALLOCED = 1 };

typedef struct {
	status_t status;
	// later identify this block as meta information, so that isValidBlock is better at recognizing
	// if it's valid void* next_block; NOT needed, since fixed block sizes are used!
} BlockInformation;

// "cheating" since the pointer to the nextFreeBlock is stored here :)
typedef struct {
	void* data;
	size_t dataSize;
	pthread_mutex_t mutex;
	void* nextFreeBlock;
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

// could be a macro, but for more complicated use later thats not that well suited
// checks wether a block can really be one, not that reliable, but the behavior of the programm is
// undefined, if not passing valid adresses into the malloc functions, so that is just a little
// security mechanims
static bool __my_malloc_isValidBlock(void* blockPointer) {
	BlockInformation blockInformation = *((BlockInformation*)blockPointer);
	return (blockInformation.status == FREE || blockInformation.status == ALLOCED);
}

// alloc a certain size, here only BLOCK_SIZE is possible, everything greater then that return a
// NULL pointer, indicating failure, if no space was in the global data struct a NULL pointer is
// returned, rather then allocating more memory, this malloc can't grow it's internal buffer
// dynamically!

void* my_malloc(size_t size) {
	if(size > BLOCK_SIZE) {
		return NULL;
	}
	// lock mutex, so it's thread safe!
	int result = pthread_mutex_lock(&__my_malloc_globalObject.mutex);
	// mutex errors are better when being asserted, since no real errors can occur, only when the
	// system is already malfunctioning
	checkResultForThreadErrorAndExit(
	    "INTERNAL: An Error occurred while trying to lock the mutex in the internal allocator");

	// simply get (by "cheating" alias storing the next Free Block globally :))  the next free
	// block, if it's NULl there's nothing free, this is assured to be 100% always true, since
	// my_free() assures that if it frees something that this nextFreeBlock pointer is updated
	// properly
	BlockInformation* allocedBlock =
	    __my_malloc_globalObject.nextFreeBlock; // can be NULL, if the previous malloc id figure,
	                                            // that nothing is free :)
	if(allocedBlock == NULL) {

		// unlocking mutex before returning
		result = pthread_mutex_unlock(&__my_malloc_globalObject.mutex);
		checkResultForThreadErrorAndExit(
		    "INTERNAL: An Error occurred while trying to unlock the internal allocator mutex");

		return NULL;
	}

	// setting the status of the Block to ALLOCED
	allocedBlock->status = ALLOCED;

	// now adjust the nextFreeBlock, this is done by iterating over all available Blocks and then
	// seeing if the block is free, this is really simple, because the size of the blocks is
	// constanly BLOCK_SIZE
	size_t dataLength = BLOCK_SIZE + sizeof(BlockInformation);
	pseudoByte* nextFreeBlock = (pseudoByte*)allocedBlock + dataLength;
	while(true) {
		// if no more block is free it's NULL
		if(nextFreeBlock >= (pseudoByte*)__my_malloc_globalObject.data +
		                        __my_malloc_globalObject.dataSize - dataLength) {
			nextFreeBlock = NULL;
			break;
		}

		if(((BlockInformation*)nextFreeBlock)->status == FREE) {
			break;
		}
		nextFreeBlock = (pseudoByte*)nextFreeBlock + dataLength;
	}

	// now setting the found block, it can be NULL
	__my_malloc_globalObject.nextFreeBlock = nextFreeBlock;

	// unlocking mutex before returning
	result = pthread_mutex_unlock(&__my_malloc_globalObject.mutex);
	checkResultForThreadErrorAndExit(
	    "INTERNAL: An Error occurred while trying to unlock the internal allocator mutex");

	// returning the area that is designed to store the data ( it has an offset of
	// sizeof(BlockInformation))
	return (pseudoByte*)allocedBlock + sizeof(BlockInformation);
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
	if(!__my_malloc_isValidBlock(information)) {
		printErrorAndExit("INTERNAL: you tried to free a invalid Block at address: %p\n", ptr);
	}
	if(information->status == FREE) {
		printErrorAndExit("INTERNAL: you tried to free a already freed Block: %p\n", ptr);
	}
	// finally setting the status to FREE
	information->status = FREE;

	// now checking if the nextFreeBlock has to be adjusted
	if((void*)information < __my_malloc_globalObject.nextFreeBlock ||
	   __my_malloc_globalObject.nextFreeBlock == NULL) {
		__my_malloc_globalObject.nextFreeBlock = information;
	}

	// unlocking mutex before returning
	result = pthread_mutex_unlock(&__my_malloc_globalObject.mutex);
	checkResultForThreadErrorAndExit(
	    "INTERNAL: An Error occurred while trying to unlock the internal allocator mutex");
}

void my_allocator_init(size_t size) {
	// first checking if the structureSize canbe stored at least one time!
	size_t structureSize = BLOCK_SIZE + sizeof(BlockInformation);
	if(size < structureSize) {
		printErrorAndExit("INTERNAL: The given size to the allocator was to small for even one "
		                  "block, given: %ld, minimal : %ld\n",
		                  size, structureSize);
	}
	size_t numberOfBlocks = size / structureSize;
	// allocate that much memory, so that the size isn't overshot, but the most blocks can be
	// stored! also store this size globally, this is needed for the munmap later on destroying
	size_t sizeOfMemory = numberOfBlocks * BLOCK_SIZE;
	__my_malloc_globalObject.dataSize = sizeOfMemory;

	// MAP_ANONYMOUS means, that
	//  "The mapping is not backed by any file; its contents are initialized to zero.  The fd
	//  argument is ignored; however, some implementations require fd to be -1 if MAP_ANONYMOUS (or
	//  MAP_ANON)  is  specified, and portable applications should ensure this.  The offset
	//  argument should be zero." ~ man page

	// this memory region is initalized with 0s
	__my_malloc_globalObject.data =
	    mmap(NULL, sizeOfMemory, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	if(__my_malloc_globalObject.data == MAP_FAILED) {
		printErrorAndExit("INTERNAL: Failed to mmap for the allocator: %s\n", strerror(errno));
	}

	__my_malloc_globalObject.nextFreeBlock = __my_malloc_globalObject.data;

	// initialize the mutex, use default as attr
	int result = pthread_mutex_init(&__my_malloc_globalObject.mutex, NULL);
	checkResultForThreadErrorAndExit("INTERNAL: An Error occurred while trying to initializing the "
	                                 "internal mutex for the allocator");
}

void my_allocator_destroy(void) {
	// un,mapping the mapped memory and destroy the mutex

	// memset everything to 0 is a good idea
	int result = munmap(__my_malloc_globalObject.data, __my_malloc_globalObject.dataSize);
	checkResultForThreadErrorAndExit("INTERNAL: Failed to munmap for the allocator:");

	result = pthread_mutex_destroy(&__my_malloc_globalObject.mutex);
	checkResultForThreadErrorAndExit(
	    "INTERNAL: An Error occurred while trying to destroy the internal mutex "
	    "in cleaning up for the allocator");
}