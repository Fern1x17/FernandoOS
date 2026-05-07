#include "mem.h"
#include "../libc/string.h"

/*
 * Heap del kernel: free-list doblemente enlazada con coalescing.
 * Soporta kmalloc() y kfree() reales.
 * Rango: 0x400000 – 0x7FFFFF (4 MiB).
 */

#define HEAP_START  0x400000U
#define HEAP_SIZE   (4U * 1024U * 1024U)
#define MIN_SPLIT   8U   /* payload mínimo tras split */

typedef struct block_hdr {
    uint32_t          size;   /* bytes de payload */
    uint32_t          free;   /* 1 = libre, 0 = ocupado */
    struct block_hdr* next;
    struct block_hdr* prev;
} block_hdr_t;

#define HDR ((uint32_t)sizeof(block_hdr_t))   /* 16 bytes */

static block_hdr_t* heap_head;

void mem_init(void) {
    heap_head       = (block_hdr_t*)HEAP_START;
    heap_head->size = HEAP_SIZE - HDR;
    heap_head->free = 1;
    heap_head->next = 0;
    heap_head->prev = 0;
}

/* Parte un bloque b en [size | resto], si el resto es útil */
static void split_block(block_hdr_t* b, uint32_t size) {
    if (b->size < size + HDR + MIN_SPLIT) return;
    block_hdr_t* rest = (block_hdr_t*)((uint8_t*)b + HDR + size);
    rest->size = b->size - size - HDR;
    rest->free = 1;
    rest->next = b->next;
    rest->prev = b;
    if (b->next) b->next->prev = rest;
    b->next = rest;
    b->size = size;
}

void* kmalloc(size_t size) {
    if (!size) return 0;
    size = (size + 3U) & ~3U;   /* alinear a 4 bytes */

    for (block_hdr_t* b = heap_head; b; b = b->next) {
        if (b->free && b->size >= size) {
            split_block(b, size);
            b->free = 0;
            return (void*)((uint8_t*)b + HDR);
        }
    }
    return 0;   /* heap agotado */
}

void* kmalloc_aligned(size_t size, size_t align) {
    /* Para alineaciones simples delegamos en kmalloc (alineación a 4 bytes).
       Alineaciones de página deben usar pmm_alloc_frame() directamente. */
    (void)align;
    return kmalloc(size);
}

/* Coalesce: fusiona b con sus vecinos libres */
static void coalesce(block_hdr_t* b) {
    /* Fusionar con siguiente */
    if (b->next && b->next->free) {
        b->size += HDR + b->next->size;
        b->next  = b->next->next;
        if (b->next) b->next->prev = b;
    }
    /* Fusionar con anterior */
    if (b->prev && b->prev->free) {
        b->prev->size += HDR + b->size;
        b->prev->next  = b->next;
        if (b->next) b->next->prev = b->prev;
    }
}

void kfree(void* ptr) {
    if (!ptr) return;
    block_hdr_t* b = (block_hdr_t*)((uint8_t*)ptr - HDR);
    if (b->free) return;   /* doble liberación → ignorar */
    b->free = 1;
    coalesce(b);
}

/* ── Estadísticas ─────────────────────────────────────────── */

uint32_t mem_get_used(void) {
    uint32_t n = 0;
    for (block_hdr_t* b = heap_head; b; b = b->next)
        if (!b->free) n += b->size + HDR;
    return n;
}

uint32_t mem_get_free(void) {
    uint32_t n = 0;
    for (block_hdr_t* b = heap_head; b; b = b->next)
        if (b->free) n += b->size;
    return n;
}

uint32_t mem_get_total(void) { return HEAP_SIZE; }

uint32_t mem_get_blocks(void) {
    uint32_t n = 0;
    for (block_hdr_t* b = heap_head; b; b = b->next) n++;
    return n;
}

uint32_t mem_get_largest_free(void) {
    uint32_t max = 0;
    for (block_hdr_t* b = heap_head; b; b = b->next)
        if (b->free && b->size > max) max = b->size;
    return max;
}
