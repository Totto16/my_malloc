/*
Author: Totto16
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "my_malloc.h"

#include <allocator_tests.h>
#include <membench.h>

// prints the usage, if argc is not the right amount!
void printUsage(const char* programName) {
	printf("usage: %s --<mode>\n\t mode: test, bench, realloc, all\n", programName);
}

// this main executes the tests and the membench, it has to be linked with the three .c files it
// requires namely "my_malloc.c", "allocator_tests.c", "membench.c"
// you can choose which mode to run with the command line parameter
int main(int argc, char const* argv[]) {

	if(argc != 2) {
		printUsage(argv[0]);
		exit(EXIT_FAILURE);
	}

	const char* mode = argv[1];
	unsigned char modeMap = 0;

	if(strcmp(mode, "--test") == 0) {
		modeMap = 1; // 0b001

	} else if(strcmp(mode, "--bench") == 0) {
		modeMap = 2; // 0b010
	} else if(strcmp(mode, "--realloc") == 0) {
		modeMap = 4; // 0b100
	} else if(strcmp(mode, "--all") == 0) {
		modeMap = 7; // 0b111
	} else {
		printUsage(argv[0]);
		exit(EXIT_FAILURE);
	}

	if(modeMap & 1) { // 0b001
		printf("Now testing the free list allocator:\n");
		test_best_fit_allocator();
	}

#ifdef _WITH_REALLOC
	if(modeMap & 4) { // 0b100
		printf("Now testing realloc\n");
		test_realloc();
	}
#endif

	if(modeMap & 2) { // 0b010
		printf("Now running the memory benchmark:\n");
#if defined(_PER_THREAD_ALLOCATOR) && _PER_THREAD_ALLOCATOR == 1
		run_membench_thread_local
#else
		run_membench_global
#endif
		    (my_allocator_init, my_allocator_destroy, my_malloc, my_free);
	}

	return EXIT_SUCCESS;
}
