#ifndef VMM_H
#define VMM_H

#include <stdint.h>
#include <stddef.h>

/* Flags de página (compatibles con x86 hardware) */
#define VMM_PRESENT  0x001U
#define VMM_WRITE    0x002U
#define VMM_USER     0x004U

void      vmm_init(void);

/* ── Asignación de páginas (espacio del kernel) ──────────── */
void*     vmm_alloc_pages(uint32_t count);          /* count páginas contiguas */
void      vmm_free_pages(void* virt, uint32_t count);

/* ── Manipulación de page directories ───────────────────── */
uint32_t* vmm_create_pd(void);         /* nuevo PD con mapa del kernel copiado */
void      vmm_destroy_pd(uint32_t* pd);
void      vmm_switch_pd(uint32_t* pd);
uint32_t* vmm_kernel_pd(void);         /* devuelve el PD del kernel */

/* ── Mapeo explícito ─────────────────────────────────────── */
void      vmm_map_page(uint32_t* pd, uint32_t virt,
                       uint32_t phys, uint32_t flags);
void      vmm_unmap_page(uint32_t* pd, uint32_t virt);
uint32_t  vmm_get_phys(uint32_t* pd, uint32_t virt); /* 0 si no mapeado */

#endif
