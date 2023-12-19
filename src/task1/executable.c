/*
Author: Tobias Niederbrunner - csba1761
Module: PS OS 10
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "my_malloc.h"

#include "../tests/allocator_tests.h"

#include "../tests/membench.h"

// prints the usage, if argc is not the right amount!
void printUsage(const char* programName) {
	printf("usage: %s --<mode>\n\t mode: test, bench, all\n", programName);
}

// this main executes the tests and the membench, it has to be linked with the three .c files it
// requires namely "my_malloc.c", "../tests/allocator_tests.c", "../tests/membench.c"
// you can choose which mode to run with the command line parameter
int main(int argc, char const* argv[]) {

	if(argc != 2) {
		printUsage(argv[0]);
		exit(EXIT_FAILURE);
	}

	const char* mode = argv[1];
	unsigned char modeMap = 0;

	if(strcmp(mode, "--test") == 0) {
		modeMap = 1; // 0b01

	} else if(strcmp(mode, "--bench") == 0) {
		modeMap = 2; // 0b10
	} else if(strcmp(mode, "--all") == 0) {
		modeMap = 3; // 0b11
	} else {
		printUsage(argv[0]);
		exit(EXIT_FAILURE);
	}

	if(modeMap & 1) { // 0b01
		printf("Now testing the free list allocator:\n");
		test_free_list_allocator();
	}

	if(modeMap & 2) { // 0b10
		printf("Now running the memory benchmark:\n");
		run_membench_global(my_allocator_init, my_allocator_destroy, my_malloc, my_free);
	}

	return EXIT_SUCCESS;
}
