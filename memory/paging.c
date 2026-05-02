#include "paging.h"
#include "../cpu/isr.h"
#include "../drivers/vga.h"

/*
 * Identity mapping de los primeros 32 MB.
 * 8 page tables × 1024 entries × 4 KB = 32 MB
 *
 * Tanto el page directory como las page tables se declaran estáticos
 * y alineados a 4096 bytes para que puedan cargarse directamente en CR3.
 */

/* 1024 entradas de 4 bytes = 4 KB por tabla */
uint32_t page_directory[1024] __attribute__((aligned(4096)));
static uint32_t page_tables[8][1024] __attribute__((aligned(4096)));

/* ── Page-fault handler (ISR 14) ──────────────────────────── */

static void page_fault_handler(registers_t* regs) {
    /* CR2 contiene la dirección virtual que causó el fallo */
    uint32_t fault_addr;
    __asm__ volatile("mov %%cr2, %0" : "=r"(fault_addr));

    vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_puts("\n*** KERNEL PANIC: PAGE FAULT ***\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    vga_puts("  Direccion:  0x"); vga_put_hex(fault_addr); vga_putchar('\n');
    vga_puts("  Error code: 0x"); vga_put_hex(regs->err_code); vga_putchar('\n');
    vga_puts("  EIP:        0x"); vga_put_hex(regs->eip);      vga_putchar('\n');

    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    vga_puts("  Causa: ");
    if (regs->err_code & 0x1)
        vga_puts("proteccion (pagina presente)");
    else
        vga_puts("pagina no presente");
    if (regs->err_code & 0x2) vga_puts(", escritura");
    if (regs->err_code & 0x4) vga_puts(", modo usuario");
    vga_putchar('\n');

    vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_puts("\nSistema detenido.\n");

    __asm__ volatile("cli");
    for (;;) __asm__ volatile("hlt");
}

/* ── Inicialización ───────────────────────────────────────── */

void paging_init(void) {
    /* Limpiar el directorio (todas las entradas no presentes) */
    for (int i = 0; i < 1024; i++)
        page_directory[i] = 0x00000002;   /* supervisor, read/write, not present */

    /* Identity map de los primeros 32 MB con 8 page tables */
    for (int t = 0; t < 8; t++) {
        for (int e = 0; e < 1024; e++) {
            uint32_t phys = (uint32_t)((t * 1024 + e) * PAGE_SIZE);
            page_tables[t][e] = phys | PAGE_PRESENT | PAGE_WRITE;
        }
        uint32_t pt_phys = (uint32_t)page_tables[t];
        page_directory[t] = pt_phys | PAGE_PRESENT | PAGE_WRITE;
    }

    /* Registrar manejador de page fault antes de habilitar paginación */
    register_interrupt_handler(14, page_fault_handler);

    /* Cargar CR3 con la dirección del page directory */
    __asm__ volatile("mov %0, %%cr3" :: "r"((uint32_t)page_directory));

    /* Activar bit PG (bit 31) en CR0 */
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000U;
    __asm__ volatile("mov %0, %%cr0" :: "r"(cr0));
}

/* ── Mapeo adicional ──────────────────────────────────────── */

void paging_map(uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t pd_idx = virt >> 22;             /* bits 31..22 */
    uint32_t pt_idx = (virt >> 12) & 0x3FF;  /* bits 21..12 */

    /* Si la entrada del page directory no está presente, no podemos crear
       una nueva page table dinámica en esta implementación simple */
    if (!(page_directory[pd_idx] & PAGE_PRESENT))
        return;

    uint32_t* pt = (uint32_t*)(page_directory[pd_idx] & 0xFFFFF000);
    pt[pt_idx] = (phys & 0xFFFFF000) | (flags & 0xFFF) | PAGE_PRESENT;

    /* Invalidar la TLB para esta dirección */
    __asm__ volatile("invlpg (%0)" :: "r"(virt) : "memory");
}

/* ── Traducción virtual → física ─────────────────────────── */

uint32_t* paging_get_kernel_dir(void) {
    return page_directory;
}

uint32_t paging_get_phys(uint32_t virt) {
    uint32_t pd_idx = virt >> 22;
    uint32_t pt_idx = (virt >> 12) & 0x3FF;
    uint32_t offset = virt & 0xFFF;

    if (!(page_directory[pd_idx] & PAGE_PRESENT))
        return 0;

    uint32_t* pt = (uint32_t*)(page_directory[pd_idx] & 0xFFFFF000);
    if (!(pt[pt_idx] & PAGE_PRESENT))
        return 0;

    return (pt[pt_idx] & 0xFFFFF000) | offset;
}
