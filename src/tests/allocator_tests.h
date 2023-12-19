#ifndef ALLOCATOR_TESTS_H
#define ALLOCATOR_TESTS_H

#include <stddef.h>
#include <stdlib.h>

void my_allocator_init(uint64_t);
void my_allocator_destroy(void);
void* my_malloc(uint64_t);
void my_free(void*);

void test_free_list_allocator(void);
void test_best_fit_allocator(void);

#endif
