#ifndef ALLOCATOR_TESTS_H
#define ALLOCATOR_TESTS_H

#include <stddef.h>
#include <stdlib.h>

void my_allocator_init(uint64_t);
void my_allocator_destroy(void);
void* my_malloc(uint64_t);
void my_free(void*);

void test_best_fit_allocator(void);

#ifdef _WITH_REALLOC
void* my_realloc(void* ptr, uint64_t size);
void test_realloc(void);
#endif
#endif
