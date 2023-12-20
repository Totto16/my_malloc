/*
Author: Totto16
*/

#include <stdio.h>
#include <stdlib.h>

#include <membench.h>

#include <main/my_malloc.h>

// this main executes the tests and the membench, it has to be linked with the necessary .c files
int main(void) {

	printf("Now running the memory benchmark with the best fit list allocator and thread local "
	       "memory:\n");

	run_membench_thread_local(my_allocator_init, my_allocator_destroy, my_malloc, my_free);
	return EXIT_SUCCESS;
}
