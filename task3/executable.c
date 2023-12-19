/*
Author: Tobias Niederbrunner - csba1761
Module: PS OS 10
*/

#include <stdio.h>
#include <stdlib.h>

#include "../tests/membench.h"

// simple preprocessor checker for valid value!
#if !defined(_MALLOC)
#error "NOT TYPE PROVIDED: use -D_MALLOC=X"
#endif

// simple preprocessor checker for valid value!
#if _MALLOC < 0 || _MALLOC > 1
// this is a c preprocessor macro, it throws a compiler error with the given message
#error "NOT SUPPORTED MALLOC: not between 0 and 1!"
#endif

// define for which malloc to use, either free_list or best_fit, these two headers are the same, but
// can theoretically have something different in it!
#if _MALLOC == 0
#include "../task1/my_malloc.h"
#elif _MALLOC == 1
#include "../task2/my_malloc.h"
#endif

// this main executes the tests and the membench, it has to be linked with the necessary .c files it
// requires this depends on the _MALLOC value, see Makefile for exact needed files!
int main(void) {
	// now print something individual for each _MALLOC type, the rest is the same
#if _MALLOC == 0
	printf(
	    "Now running the memory benchmark with the free list allocator and thread local memory:\n");
#elif _MALLOC == 1
	printf("Now running the memory benchmark with the best fit list allocator and thread local "
	       "memory:\n");
#endif
	run_membench_thread_local(my_allocator_init, my_allocator_destroy, my_malloc, my_free);
	return EXIT_SUCCESS;
}
