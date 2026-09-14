#include "modules/runtime/heap.h"

#include <stdint.h>
#include <string.h>
#include "tx_api.h"

typedef struct {
    size_t size;
    uint32_t magic;
} heap_header;

#define HEAP_MAGIC 0x48425031U

static TX_BYTE_POOL heap_pool;
static TX_MUTEX heap_lock;
static int heap_ready;

int heap_port_init(void *mem, size_t size)
{
    if (heap_ready)
        return 0;
    if (mem == 0 || size < 1024U)
        return -1;
    if (tx_byte_pool_create(&heap_pool, "heap", mem, (ULONG)size) != TX_SUCCESS)
        return -1;
    if (tx_mutex_create(&heap_lock, "heap lock", TX_INHERIT) != TX_SUCCESS)
        return -1;
    heap_ready = 1;
    return 0;
}

void *heap_alloc(size_t size)
{
    VOID *raw = 0;
    heap_header *header;
    if (!heap_ready || size == 0U)
        return 0;
    (void)tx_mutex_get(&heap_lock, TX_WAIT_FOREVER);
    if (tx_byte_allocate(&heap_pool, &raw, (ULONG)(sizeof(heap_header) + size), TX_NO_WAIT) != TX_SUCCESS)
        raw = 0;
    if (raw != 0) {
        header = (heap_header *)raw;
        header->size = size;
        header->magic = HEAP_MAGIC;
        raw = header + 1;
    }
    (void)tx_mutex_put(&heap_lock);
    return raw;
}

void heap_free(void *ptr)
{
    heap_header *header;
    if (!heap_ready || ptr == 0)
        return;
    header = (heap_header *)ptr - 1;
    if (header->magic != HEAP_MAGIC)
        return;
    (void)tx_mutex_get(&heap_lock, TX_WAIT_FOREVER);
    header->magic = 0U;
    (void)tx_byte_release(header);
    (void)tx_mutex_put(&heap_lock);
}

void *heap_realloc(void *ptr, size_t size)
{
    heap_header *header;
    void *next;
    size_t copy_size;
    if (ptr == 0)
        return heap_alloc(size);
    if (size == 0U) {
        heap_free(ptr);
        return 0;
    }
    header = (heap_header *)ptr - 1;
    if (header->magic != HEAP_MAGIC)
        return 0;
    if (size <= header->size) {
        header->size = size;
        return ptr;
    }
    next = heap_alloc(size);
    if (next == 0)
        return 0;
    copy_size = header->size < size ? header->size : size;
    memcpy(next, ptr, copy_size);
    heap_free(ptr);
    return next;
}
