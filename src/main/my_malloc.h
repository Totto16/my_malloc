// header include guard
#ifndef _MY_MALLOC_H_
#define _MY_MALLOC_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

void* my_malloc(uint64_t size);
void my_free(void* ptr);
void* my_realloc(void* ptr, uint64_t size);

void my_allocator_init(uint64_t size, bool force_alloc);
void my_allocator_destroy(void);

#ifdef __cplusplus
}
#endif

#endif
