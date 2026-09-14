#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 0 = success (including already ready), -1 = failure. */
int heap_port_init(void *mem, size_t size);

void *heap_alloc(size_t size);
void heap_free(void *ptr);
void *heap_realloc(void *ptr, size_t size);

#ifdef __cplusplus
}
#endif
