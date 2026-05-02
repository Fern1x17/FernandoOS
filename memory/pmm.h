#ifndef PMM_H
#define PMM_H

#include <stdint.h>

#define PAGE_SIZE       4096U
#define PMM_MAX_FRAMES  8192U   /* cubre hasta 32 MB (32*1024*1024 / 4096) */

/*
 * El área 0x000000–0x7FFFFF (primeros 8 MB) se reserva íntegramente
 * para el kernel: código, BSS, page tables estáticas y heap de kmalloc.
 * Los frames gestionados dinámicamente empiezan en 0x800000.
 */
#define PMM_KERN_END     0x800000U
#define PMM_KERN_FRAMES  (PMM_KERN_END / PAGE_SIZE)  /* 2048 */

void     pmm_init(uint32_t total_mem_kb);

uint32_t pmm_alloc_frame(void);                  /* 1 frame → dirección física */
uint32_t pmm_alloc_frames(uint32_t n);           /* n frames contiguos */
void     pmm_free_frame(uint32_t phys);
void     pmm_free_frames(uint32_t phys, uint32_t n);

uint32_t pmm_get_total(void);  /* total de frames gestionados */
uint32_t pmm_get_used(void);
uint32_t pmm_get_free(void);

#endif
