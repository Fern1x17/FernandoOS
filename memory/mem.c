#include "mem.h"

/*
 * Asignador por bump (arena). Sencillo pero efectivo para la
 * fase inicial del kernel. El heap arranca a 4 MB para evitar
 * solapamiento con el kernel cargado en 1 MB.
 */
#define HEAP_START  0x400000U
#define HEAP_SIZE   (4U * 1024U * 1024U)   /* 4 MiB */

static uint32_t heap_cur = HEAP_START;

void mem_init(void) {
    heap_cur = HEAP_START;
}

void* kmalloc(size_t size) {
    /* Alinear a 4 bytes */
    size = (size + 3U) & ~3U;
    if (heap_cur + size > HEAP_START + HEAP_SIZE)
        return (void*)0;
    void* ptr = (void*)heap_cur;
    heap_cur += (uint32_t)size;
    return ptr;
}

void* kmalloc_aligned(size_t size, size_t align) {
    if (align == 0) align = 1;
    uint32_t aligned = (heap_cur + (uint32_t)(align - 1)) & ~(uint32_t)(align - 1);
    if (aligned + size > HEAP_START + HEAP_SIZE)
        return (void*)0;
    heap_cur = aligned + (uint32_t)size;
    return (void*)aligned;
}

void kfree(void* ptr) {
    (void)ptr;  /* Bump allocator: no-op */
}

uint32_t mem_get_used(void)  { return heap_cur - HEAP_START; }
uint32_t mem_get_free(void)  { return (HEAP_START + HEAP_SIZE) - heap_cur; }
uint32_t mem_get_total(void) { return HEAP_SIZE; }
