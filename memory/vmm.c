#include "vmm.h"
#include "pmm.h"
#include "paging.h"
#include "../libc/string.h"

/*
 * Con identity mapping completo de 32 MB, la dirección virtual de
 * cualquier frame físico gestionado por el PMM coincide con su dirección
 * física.  El VMM aprovecha este hecho para operar sin necesidad de
 * un segundo nivel de traducción durante la fase kernel-only.
 *
 * vmm_map_page / vmm_unmap_page sí modifican las page tables para que
 * cuando añadamos espacios de usuario (ring 3) el código no cambie.
 */

static uint32_t* kpd = 0;   /* page directory del kernel */

/* ── Utilidades internas ──────────────────────────────────── */

static inline void tlb_flush(uint32_t virt) {
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

/* Devuelve puntero a la page table que cubre 'virt' en 'pd'.
   Si no existe y create==1, la asigna via PMM. */
static uint32_t* get_pt(uint32_t* pd, uint32_t virt, int create) {
    uint32_t pdi = virt >> 22;

    if (!(pd[pdi] & VMM_PRESENT)) {
        if (!create) return 0;
        uint32_t pt_phys = pmm_alloc_frame();
        if (!pt_phys) return 0;
        memset((void*)pt_phys, 0, PAGE_SIZE);   /* identity map: virt == phys */
        pd[pdi] = pt_phys | VMM_PRESENT | VMM_WRITE;
    }

    return (uint32_t*)(pd[pdi] & 0xFFFFF000U);
}

/* ── Inicialización ───────────────────────────────────────── */

void vmm_init(void) {
    kpd = paging_get_kernel_dir();
}

/* ── Mapeo explícito ──────────────────────────────────────── */

void vmm_map_page(uint32_t* pd, uint32_t virt,
                  uint32_t phys, uint32_t flags)
{
    uint32_t* pt = get_pt(pd, virt, 1);
    if (!pt) return;

    uint32_t pti = (virt >> 12) & 0x3FFU;
    pt[pti] = (phys & 0xFFFFF000U) | (flags & 0xFFFU) | VMM_PRESENT;
    tlb_flush(virt);
}

void vmm_unmap_page(uint32_t* pd, uint32_t virt) {
    uint32_t* pt = get_pt(pd, virt, 0);
    if (!pt) return;

    uint32_t pti = (virt >> 12) & 0x3FFU;
    pt[pti] = 0;
    tlb_flush(virt);
}

uint32_t vmm_get_phys(uint32_t* pd, uint32_t virt) {
    uint32_t* pt = get_pt(pd, virt, 0);
    if (!pt) return 0;

    uint32_t pti = (virt >> 12) & 0x3FFU;
    if (!(pt[pti] & VMM_PRESENT)) return 0;
    return (pt[pti] & 0xFFFFF000U) | (virt & 0xFFFU);
}

/* ── Asignación de páginas del kernel ─────────────────────── */

/*
 * Encuentra 'count' frames físicos consecutivos via PMM.
 * Con identity mapping están automáticamente accesibles en la
 * misma dirección virtual, por lo que no necesitamos remapear nada.
 */
void* vmm_alloc_pages(uint32_t count) {
    if (!count) return 0;
    uint32_t phys = pmm_alloc_frames(count);
    if (!phys) return 0;
    return (void*)phys;   /* virt == phys gracias al identity map */
}

void vmm_free_pages(void* virt, uint32_t count) {
    if (!virt || !count) return;
    pmm_free_frames((uint32_t)virt, count);
}

/* ── Page directories ─────────────────────────────────────── */

/*
 * Crea un nuevo PD copiando las 8 entradas del kernel (0–32 MB).
 * El nuevo PD comparte las mismas page tables del kernel: no duplica
 * los datos, solo copia los punteros a ellas.
 */
uint32_t* vmm_create_pd(void) {
    uint32_t pd_phys = pmm_alloc_frame();
    if (!pd_phys) return 0;

    uint32_t* new_pd = (uint32_t*)pd_phys;   /* identity map */
    memset(new_pd, 0, PAGE_SIZE);

    /* Copiar entradas del kernel (primeras 8 = 32 MB) */
    for (int i = 0; i < 8; i++)
        new_pd[i] = kpd[i];

    return new_pd;
}

/*
 * Destruye un PD liberando sus page tables propias (entradas >= 8)
 * y el frame del PD mismo.  Las entradas del kernel no se tocan.
 */
void vmm_destroy_pd(uint32_t* pd) {
    if (!pd || pd == kpd) return;

    for (int i = 8; i < 1024; i++) {
        if (pd[i] & VMM_PRESENT)
            pmm_free_frame(pd[i] & 0xFFFFF000U);
    }
    pmm_free_frame((uint32_t)pd);
}

void vmm_switch_pd(uint32_t* pd) {
    __asm__ volatile("mov %0, %%cr3" : : "r"((uint32_t)pd) : "memory");
}

uint32_t* vmm_kernel_pd(void) {
    return kpd;
}
