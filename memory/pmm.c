#include "pmm.h"
#include "../libc/string.h"

/*
 * Bitmap de frames físicos.
 * Bit = 1 → frame ocupado.  Bit = 0 → frame libre.
 * Cada uint32_t cubre 32 frames (32 × 4 KB = 128 KB).
 */
#define BITMAP_WORDS  (PMM_MAX_FRAMES / 32U)   /* 256 palabras = 1 KB */

static uint32_t bitmap[BITMAP_WORDS];
static uint32_t total_frames;
static uint32_t used_count;

/* ── Macros de bit ────────────────────────────────────────── */
#define BIT_SET(f)  (bitmap[(f) >> 5] |=  (1U << ((f) & 31U)))
#define BIT_CLR(f)  (bitmap[(f) >> 5] &= ~(1U << ((f) & 31U)))
#define BIT_TST(f)  (bitmap[(f) >> 5] &   (1U << ((f) & 31U)))

/* ── Inicialización ───────────────────────────────────────── */

void pmm_init(uint32_t total_mem_kb) {
    total_frames = (total_mem_kb * 1024U) / PAGE_SIZE;
    if (total_frames > PMM_MAX_FRAMES)
        total_frames = PMM_MAX_FRAMES;

    /* Marcar todo como ocupado */
    memset(bitmap, 0xFF, sizeof(bitmap));
    used_count = total_frames;

    /* Liberar frames por encima del área reservada al kernel */
    for (uint32_t f = PMM_KERN_FRAMES; f < total_frames; f++) {
        BIT_CLR(f);
        used_count--;
    }
}

/* ── Asignación ───────────────────────────────────────────── */

uint32_t pmm_alloc_frame(void) {
    /* Búsqueda first-fit desde el primer grupo no totalmente ocupado */
    uint32_t start_word = PMM_KERN_FRAMES / 32U;
    for (uint32_t w = start_word; w < BITMAP_WORDS; w++) {
        if (bitmap[w] == 0xFFFFFFFFU) continue;
        for (uint32_t b = 0; b < 32U; b++) {
            uint32_t f = w * 32U + b;
            if (f >= total_frames) return 0;
            if (!BIT_TST(f)) {
                BIT_SET(f);
                used_count++;
                return f * PAGE_SIZE;
            }
        }
    }
    return 0;   /* sin memoria */
}

uint32_t pmm_alloc_frames(uint32_t n) {
    if (n == 0) return 0;
    if (n == 1) return pmm_alloc_frame();

    uint32_t start = 0, run = 0;
    for (uint32_t f = PMM_KERN_FRAMES; f < total_frames; f++) {
        if (!BIT_TST(f)) {
            if (run == 0) start = f;
            if (++run == n) {
                /* Encontrado: marcar todos */
                for (uint32_t i = start; i < start + n; i++) {
                    BIT_SET(i);
                    used_count++;
                }
                return start * PAGE_SIZE;
            }
        } else {
            run = 0;
        }
    }
    return 0;   /* sin memoria contigua */
}

/* ── Liberación ───────────────────────────────────────────── */

void pmm_free_frame(uint32_t phys) {
    uint32_t f = phys / PAGE_SIZE;
    if (f < PMM_KERN_FRAMES || f >= total_frames) return;
    if (!BIT_TST(f)) return;   /* doble liberación → ignorar */
    BIT_CLR(f);
    used_count--;
}

void pmm_free_frames(uint32_t phys, uint32_t n) {
    for (uint32_t i = 0; i < n; i++)
        pmm_free_frame(phys + i * PAGE_SIZE);
}

/* ── Estadísticas ─────────────────────────────────────────── */

uint32_t pmm_get_total(void) { return total_frames; }
uint32_t pmm_get_used(void)  { return used_count; }
uint32_t pmm_get_free(void)  { return total_frames - used_count; }
