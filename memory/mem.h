#ifndef MEM_H
#define MEM_H

#include <stdint.h>
#include <stddef.h>

void     mem_init(void);
void*    kmalloc(size_t size);
void*    kmalloc_aligned(size_t size, size_t align);
void     kfree(void* ptr);
uint32_t mem_get_used(void);
uint32_t mem_get_free(void);
uint32_t mem_get_total(void);

#endif
