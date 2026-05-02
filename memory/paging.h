#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>
#include "pmm.h"   /* define PAGE_SIZE */

#define PAGE_PRESENT 0x1
#define PAGE_WRITE   0x2
#define PAGE_USER    0x4

void      paging_init(void);
void      paging_map(uint32_t virt, uint32_t phys, uint32_t flags);
uint32_t  paging_get_phys(uint32_t virt);
uint32_t* paging_get_kernel_dir(void);   /* usado por vmm_init() */

#endif
