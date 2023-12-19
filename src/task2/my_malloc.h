// header include guard
#ifndef _MY_MALLOC_H_
#define _MY_MALLOC_H_

#include <stdint.h>
#include <stdlib.h>

void* my_malloc(uint64_t size);
bool was_malloced(void* ptr);

void my_free(void* ptr);
void* my_realloc(void* ptr, uint64_t size);

void my_allocator_init(uint64_t size);
void my_allocator_destroy(void);

#endif
